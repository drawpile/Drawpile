/*
 * Copyright (C) 2022 askmeaboufoom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#include "recorder.h"
#include "canvas_state.h"
#include "snapshots.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/output.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpmsg/binary_writer.h>
#include <dpmsg/message.h>
#include <dpmsg/message_queue.h>
#include <dpmsg/text_writer.h>
#include <parson.h>

#define MIN_INTERVAL 500
#define MAX_INTERVAL UINT16_MAX

struct DP_Recorder {
    DP_RecorderType type;
    struct {
        DP_RecorderGetTimeMsFn fn;
        void *user;
    } get_time;
    union {
        DP_BinaryWriter *binary_writer;
        DP_TextWriter *text_writer;
    };
    JSON_Value *header;
    long long last_timestamp;
    DP_Queue queue;
    DP_Atomic running;
    DP_Mutex *mutex;
    DP_Semaphore *sem;
    DP_Thread *thread;
};

struct DP_RecorderThreadArgs {
    DP_Recorder *r;
    DP_CanvasState *cs_or_null;
};


static bool set_header_pair(JSON_Object *obj, const char *key,
                            const char *value)
{
    if (key && value
        && json_object_set_string(obj, key, value) == JSONSuccess) {
        return true;
    }
    else {
        DP_error_set("Error setting recording header %s = %s",
                     key ? key : "NULL", value ? value : "NULL");
        return false;
    }
}

JSON_Value *DP_recorder_header_new(const char *first, ...)
{
    JSON_Value *header = json_value_init_object();
    if (header) {
        JSON_Object *obj = json_value_get_object(header);
        bool ok = set_header_pair(obj, "version", DP_PROTOCOL_VERSION);
        if (ok && first) {
            va_list ap;
            va_start(ap, first);
            const char *key = first;
            ok = set_header_pair(obj, key, va_arg(ap, const char *));
            while (ok && (key = va_arg(ap, const char *))) {
                ok = set_header_pair(obj, key, va_arg(ap, const char *));
            }
            va_end(ap);
        }

        if (!ok) {
            json_value_free(header);
            header = NULL;
        }
    }
    else {
        DP_error_set("Error creating recording header object");
    }
    return header;
}

JSON_Value *DP_recorder_header_clone(JSON_Value *header)
{
    JSON_Value *copy = header ? json_value_deep_copy(header) : NULL;
    if (copy) {
        return copy;
    }
    else {
        DP_error_set("Error cloning recording header");
        return NULL;
    }
}


static long long get_timestamp(DP_Recorder *r)
{
    DP_RecorderGetTimeMsFn fn = r->get_time.fn;
    return fn ? fn(r->get_time.user) : 0;
}

static bool write_header(DP_Recorder *r)
{
    JSON_Object *header = json_value_get_object(r->header);
    bool ok;
    switch (r->type) {
    case DP_RECORDER_TYPE_BINARY:
        ok = DP_binary_writer_write_header(r->binary_writer, header);
        break;
    case DP_RECORDER_TYPE_TEXT:
        ok = DP_text_writer_write_header(r->text_writer, header);
        break;
    default:
        DP_UNREACHABLE();
    }
    return ok;
}

static bool write_message_dec(DP_Recorder *r, DP_Message *msg)
{
    bool ok;
    switch (r->type) {
    case DP_RECORDER_TYPE_BINARY:
        ok = DP_binary_writer_write_message(r->binary_writer, msg);
        break;
    case DP_RECORDER_TYPE_TEXT:
        ok = DP_message_write_text(msg, r->text_writer);
        break;
    default:
        DP_UNREACHABLE();
    }

    DP_message_decref(msg);

    if (ok) {
        return true;
    }
    else {
        DP_atomic_set(&r->running, 0);
        return false;
    }
}

static void write_reset_image_message(void *user, DP_Message *msg)
{
    DP_Recorder *r = user;
    if (DP_atomic_get(&r->running)) {
        write_message_dec(r, msg);
    }
}

static bool shift_message(DP_Recorder *r, DP_Message **out_msg)
{
    DP_Mutex *mutex = r->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    DP_Message *msg = DP_message_queue_shift(&r->queue);
    DP_MUTEX_MUST_UNLOCK(mutex);
    if (msg) {
        *out_msg = msg;
        return true;
    }
    else {
        return false;
    }
}

static void run_recorder(void *user)
{
    struct DP_RecorderThreadArgs *args = user;
    DP_Recorder *r = args->r;
    DP_CanvasState *cs_or_null = args->cs_or_null;
    DP_free(args);

    if (write_header(r)) {
        if (cs_or_null) {
            DP_reset_image_build(cs_or_null, 0, write_reset_image_message, r);
            DP_canvas_state_decref(cs_or_null);
        }
        DP_Semaphore *sem = r->sem;
        while (true) {
            DP_SEMAPHORE_MUST_WAIT(sem);
            DP_Message *msg;
            if (DP_atomic_get(&r->running) && shift_message(r, &msg)) {
                if (!write_message_dec(r, msg)) {
                    break;
                }
            }
            else {
                break;
            }
        }
    }
    else {
        DP_canvas_state_decref_nullable(cs_or_null);
    }
}


DP_Recorder *DP_recorder_new_inc(DP_RecorderType type, JSON_Value *header,
                                 DP_CanvasState *cs_or_null,
                                 DP_RecorderGetTimeMsFn get_time_fn,
                                 void *get_time_user, DP_Output *output)
{
    DP_ASSERT(header);
    DP_ASSERT(output);
    DP_Recorder *r = DP_malloc(sizeof(*r));
    *r = (DP_Recorder){type,
                       {get_time_fn, get_time_user},
                       {NULL},
                       header,
                       0,
                       DP_QUEUE_NULL,
                       DP_ATOMIC_INIT(1),
                       NULL,
                       NULL,
                       NULL};

    switch (type) {
    case DP_RECORDER_TYPE_BINARY:
        r->binary_writer = DP_binary_writer_new(output);
        break;
    case DP_RECORDER_TYPE_TEXT:
        r->text_writer = DP_text_writer_new(output);
        break;
    default:
        DP_error_set("Unknown recorder type %d", (int)type);
        DP_output_free(output);
        DP_recorder_free_join(r);
        return NULL;
    }

    DP_message_queue_init(&r->queue, 64);

    r->mutex = DP_mutex_new();
    if (!r->mutex) {
        DP_recorder_free_join(r);
        return NULL;
    }

    r->sem = DP_semaphore_new(0);
    if (!r->sem) {
        DP_recorder_free_join(r);
        return NULL;
    }

    struct DP_RecorderThreadArgs *args = DP_malloc(sizeof(*args));
    *args = (struct DP_RecorderThreadArgs){
        r, DP_canvas_state_incref_nullable(cs_or_null)};
    r->thread = DP_thread_new(run_recorder, args);
    if (!r->thread) {
        DP_recorder_free_join(r);
        return NULL;
    }

    r->last_timestamp = get_timestamp(r);
    return r;
}

void DP_recorder_free_join(DP_Recorder *r)
{
    if (r) {
        if (r->thread) {
            DP_SEMAPHORE_MUST_POST(r->sem);
            DP_thread_free_join(r->thread);
        }
        DP_semaphore_free(r->sem);
        DP_mutex_free(r->mutex);
        DP_message_queue_dispose(&r->queue);
        json_value_free(r->header);
        switch (r->type) {
        case DP_RECORDER_TYPE_BINARY:
            DP_binary_writer_free(r->binary_writer);
            break;
        case DP_RECORDER_TYPE_TEXT:
            DP_text_writer_free(r->text_writer);
            break;
        default:
            break;
        }
        DP_free(r);
    }
}

DP_RecorderType DP_recorder_type(DP_Recorder *r)
{
    DP_ASSERT(r);
    return r->type;
}

JSON_Value *DP_recorder_header(DP_Recorder *r)
{
    DP_ASSERT(r);
    return r->header;
}

void DP_recorder_message_push_initial_noinc(DP_Recorder *r,
                                            DP_Message *(*get_next)(void *),
                                            void *user)
{
    DP_ASSERT(r);
    DP_ASSERT(get_next);
    DP_Mutex *mutex = r->mutex;
    DP_Semaphore *sem = r->sem;
    DP_Queue *queue = &r->queue;
    DP_MUTEX_MUST_LOCK(mutex);
    int count = 0;
    DP_Message *msg;
    while ((msg = get_next(user))) {
        DP_message_queue_push_noinc(queue, msg);
        ++count;
    }
    DP_SEMAPHORE_MUST_POST_N(sem, count);
    DP_MUTEX_MUST_UNLOCK(mutex);
    r->last_timestamp = get_timestamp(r);
}

bool DP_recorder_message_push_inc(DP_Recorder *r, DP_Message *msg)
{
    DP_ASSERT(r);
    DP_ASSERT(msg);
    if (DP_atomic_get(&r->running)) {
        long long timestamp = get_timestamp(r);
        long long interval = timestamp - r->last_timestamp;
        DP_Mutex *mutex = r->mutex;
        DP_Semaphore *sem = r->sem;
        DP_Queue *queue = &r->queue;
        DP_MUTEX_MUST_LOCK(mutex);
        if (interval > MIN_INTERVAL) {
            uint16_t clamped_interval =
                interval > MAX_INTERVAL ? MAX_INTERVAL : (uint16_t)interval;
            DP_message_queue_push_noinc(
                queue, DP_msg_interval_new(0, clamped_interval));
            DP_SEMAPHORE_MUST_POST(sem);
        }
        DP_message_queue_push_inc(queue, msg);
        DP_SEMAPHORE_MUST_POST(sem);
        DP_MUTEX_MUST_UNLOCK(mutex);
        r->last_timestamp = timestamp;
        return true;
    }
    else {
        return false;
    }
}
