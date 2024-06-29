/*
 * Copyright (C) 2022 askmeaboutloom
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
#include "canvas_history.h"
#include "canvas_state.h"
#include "recorder.h"
#include "snapshots.h"
#include <dpcommon/atomic.h>
#include <dpcommon/binary.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>
#include <dpcommon/perf.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpmsg/acl.h>
#include <dpmsg/local_match.h>
#include <dpmsg/message.h>
#include <dpmsg/msg_internal.h>
#include <time.h>

#define DP_PERF_CONTEXT "canvas_history"


#define INITIAL_CAPACITY              1024
#define EXPAND_CAPACITY(OLD_CAPACITY) ((OLD_CAPACITY) * 2)

#define MAX_FALLBEHIND 10000

// We want to batch draw dabs commands when replaying messages, since they're so
// common and really benefit from combined handling. We'll use a fixed buffer of
// some reasonable size to store plenty of messages for that purpose.
#define REPLAY_BUFFER_CAPACITY 8192

typedef enum DP_ForkAction {
    DP_FORK_ACTION_CONCURRENT,
    DP_FORK_ACTION_ALREADY_DONE,
    DP_FORK_ACTION_ROLLBACK,
} DP_ForkAction;

struct DP_CanvasHistory {
    DP_Mutex *mutex;
    DP_CanvasState *current_state;
    DP_UserCursors ucs;
    DP_EffectiveUserCursors eucs;
    int undo_depth_limit;
    int offset;
    int capacity;
    int used;
    DP_CanvasHistoryEntry *entries;
    DP_AffectedIndirectAreas aia;
    bool mark_command_done;
    struct {
        bool starts_at_undo_point;
        int start;
        int fallbehind;
        DP_Queue queue;
    } fork;
    struct {
        DP_CanvasHistorySavePointFn fn;
        void *user;
    } save_point;
    struct {
        int used;
        DP_Message *buffer[REPLAY_BUFFER_CAPACITY];
    } replay;
    DP_Atomic local_drawing_in_progress;
    struct {
        bool want;
        char *dir;
        DP_Output *output;
        size_t buffer_size;
        unsigned char *buffer;
    } dump;
};

struct DP_CanvasHistorySnapshot {
    DP_Atomic refcount;
    struct {
        int undo_depth_limit;
        int offset;
        int count;
        DP_CanvasHistoryEntry *entries;
    } history;
    struct {
        int start;
        int fallbehind;
        int count;
        DP_ForkEntry *entries;
    } fork;
};


// History debug is very noisy, only enable it when requested.
#if defined(NDEBUG) || !defined(DRAWDANCE_HISTORY_DEBUG)
#    define HISTORY_DEBUG(...) /* nothing */
#    define dump_history(CH)   /* nothing */
#else

#    define HISTORY_DEBUG(...) DP_debug(__VA_ARGS__)

static const char *get_undo_name(DP_Undo undo)
{
    switch (undo) {
    case DP_UNDO_DONE:
        return "DONE";
    case DP_UNDO_UNDONE:
        return "UNDONE";
    case DP_UNDO_GONE:
        return "GONE";
    default:
        DP_UNREACHABLE();
    }
}

static void dump_history(DP_CanvasHistory *ch)
{
    HISTORY_DEBUG("--- begin canvas history dump ---");
    HISTORY_DEBUG("history %d used, %d capacity, %d offset", ch->used,
                  ch->capacity, ch->offset);
    for (int i = 0; i < ch->used; ++i) {
        DP_CanvasHistoryEntry *entry = &ch->entries[i];
        DP_Message *msg = entry->msg;
        HISTORY_DEBUG(
            "    H[%d] %s %s user %u state %p", i, get_undo_name(entry->undo),
            DP_message_type_enum_name_unprefixed(DP_message_type(msg)),
            DP_message_context_id(msg), (void *)entry->state);
    }
    HISTORY_DEBUG("fork %zu used, %zu capacity, %d start, %d fallbehind",
                  ch->fork.queue.used, ch->fork.queue.capacity, ch->fork.start,
                  ch->fork.fallbehind);
    for (size_t i = 0; i < ch->fork.queue.used; ++i) {
        DP_ForkEntry *fe = DP_queue_at(&ch->fork.queue, sizeof(*fe), i);
        DP_Message *msg = fe->msg;
        HISTORY_DEBUG(
            "    F[%zu] %s user %u", i,
            DP_message_type_enum_name_unprefixed(DP_message_type(msg)),
            DP_message_context_id(msg));
    }
    HISTORY_DEBUG("--- end canvas history dump ---");
}

#endif


static void dump_init(DP_CanvasHistory *ch)
{
    if (ch->dump.want && !ch->dump.output) {
        const char *dir = ch->dump.dir;
        char *dump_path =
            DP_format("%s%s%lu_%p.drawdancedump", dir ? dir : "",
                      dir ? "/" : "", (unsigned long)time(NULL), (void *)ch);
        ch->dump.output = DP_file_output_new_from_path(dump_path);
        if (ch->dump.output) {
            DP_warn("Writing canvas history dump to '%s'", dump_path);
        }
        else {
            DP_warn("Can't open dump file '%s': %s", dump_path, DP_error());
        }
        DP_free(dump_path);
    }
}

static void dump_close(DP_CanvasHistory *ch, DP_Output *output)
{
    DP_free(ch->dump.buffer);
    if (!DP_output_free(output)) {
        DP_warn("Error closing dump: %s", DP_error());
    }
    ch->dump.output = NULL;
    ch->dump.buffer = NULL;
    ch->dump.buffer_size = 0;
}

static void dump_dispose_error(DP_CanvasHistory *ch, DP_Output *output)
{
    DP_warn("Dump error, closing it: %s", DP_error());
    dump_close(ch, output);
}

static bool dump_check(DP_CanvasHistory *ch, DP_Output **out_output)
{
    DP_Output *output = ch->dump.output;
    if (output) {
        if (ch->dump.want) {
            if (out_output) {
                *out_output = output;
            }
            return true;
        }
        else {
            DP_warn("Dump no longer wanted, closing it");
            dump_close(ch, output);
        }
    }
    return false;
}

static unsigned char *dump_get_buffer(void *user, size_t size)
{
    DP_CanvasHistory *ch = user;
    if (ch->dump.buffer_size < size) {
        ch->dump.buffer = DP_realloc(ch->dump.buffer, size);
        ch->dump.buffer_size = size;
    }
    return ch->dump.buffer;
}

static bool dump_message_write(DP_CanvasHistory *ch, DP_Output *output,
                               DP_Message *msg)
{
    size_t size = DP_message_serialize(msg, true, dump_get_buffer, ch);
    return DP_OUTPUT_WRITE_BIGENDIAN(output, DP_OUTPUT_UINT32(size),
                                     DP_OUTPUT_END)
        && DP_output_write(output, ch->dump.buffer, size);
}

static bool dump_message_try(DP_CanvasHistory *ch, DP_Output *output,
                             DP_Message *msg, unsigned char type)
{
    return DP_OUTPUT_WRITE_BIGENDIAN(output, DP_OUTPUT_UINT8(type),
                                     DP_OUTPUT_END)
        && dump_message_write(ch, output, msg) && DP_output_flush(output);
}

static void dump_message(DP_CanvasHistory *ch, DP_Message *msg,
                         unsigned char type)
{
    DP_Output *output;
    if (dump_check(ch, &output)) {
        if (!dump_message_try(ch, output, msg, type)) {
            dump_dispose_error(ch, output);
        }
    }
}

static bool dump_multidab_try(DP_CanvasHistory *ch, DP_Output *output,
                              int count, DP_Message **msgs, unsigned char type)
{
    if (!DP_OUTPUT_WRITE_BIGENDIAN(output, DP_OUTPUT_UINT8(type),
                                   DP_OUTPUT_UINT32(count), DP_OUTPUT_END)) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (!dump_message_write(ch, output, msgs[i])) {
            return false;
        }
    }
    return true;
}

static void dump_multidab(DP_CanvasHistory *ch, int count, DP_Message **msgs,
                          unsigned char type)
{
    DP_Output *output;
    if (dump_check(ch, &output)) {
        if (!dump_multidab_try(ch, output, count, msgs, type)) {
            dump_dispose_error(ch, output);
        }
    }
}

static void dump_internal(DP_CanvasHistory *ch, unsigned char type)
{
    DP_Output *output;
    if (dump_check(ch, &output)) {
        if (!DP_OUTPUT_WRITE_BIGENDIAN(output, DP_OUTPUT_UINT8(type),
                                       DP_OUTPUT_END)) {
            dump_dispose_error(ch, output);
        }
    }
}

static void dump_undo_depth_limit(DP_CanvasHistory *ch, int undo_depth_limit)
{
    DP_Output *output;
    if (dump_check(ch, &output)) {
        if (!DP_OUTPUT_WRITE_BIGENDIAN(
                output, DP_OUTPUT_UINT8(DP_MSG_UNDO_DEPTH),
                DP_OUTPUT_UINT8(undo_depth_limit), DP_OUTPUT_END)) {
            dump_dispose_error(ch, output);
        }
    }
}

static void dump_snapshot_message(void *user, DP_Message *msg)
{
    DP_CanvasHistory *ch = user;
    dump_message(ch, msg, DP_DUMP_REMOTE_MESSAGE);
    DP_message_decref(msg);
}

static void dump_snapshot(DP_CanvasHistory *ch, DP_CanvasState *cs)
{
    if (dump_check(ch, NULL)) {
        DP_reset_image_build(cs, 0, dump_snapshot_message, ch);
    }
}


static void set_fork_start(DP_CanvasHistory *ch, bool starts_at_undo_point)
{
    DP_ASSERT(ch->used > 0);
    DP_ASSERT(ch->offset >= 0);
    ch->fork.starts_at_undo_point = starts_at_undo_point;
    ch->fork.start = ch->offset + ch->used - 1;
    HISTORY_DEBUG("Set fork start to %d", ch->fork.start);
}

static void dispose_fork_entry(void *element)
{
    DP_ForkEntry *fe = element;
    DP_message_decref(fe->msg);
}

static void clear_fork_entries(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    HISTORY_DEBUG("Clear %zu fork entries", ch->fork.queue.used);
    DP_queue_clear(&ch->fork.queue, sizeof(DP_ForkEntry), dispose_fork_entry);
}

static void push_fork_entry_noinc(DP_CanvasHistory *ch, DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);
    HISTORY_DEBUG("Push fork element %zu", ch->fork.queue.used);
    DP_ForkEntry *fe = DP_queue_push(&ch->fork.queue, sizeof(DP_ForkEntry));
    *fe = (DP_ForkEntry){msg, DP_affected_area_make(msg, &ch->aia)};
}

static void push_fork_entry_inc(DP_CanvasHistory *ch, DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);
    push_fork_entry_noinc(ch, DP_message_incref(msg));
}

static bool have_local_fork(DP_CanvasHistory *ch)
{
    return ch->fork.queue.used != 0;
}

static DP_Message *peek_fork_entry_message(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    DP_ASSERT(have_local_fork(ch));
    DP_ForkEntry *fe = DP_queue_peek(&ch->fork.queue, sizeof(DP_ForkEntry));
    DP_ASSERT(fe);
    return fe->msg;
}

static void shift_fork_entry_nodec(DP_CanvasHistory *ch)
{
    DP_queue_shift(&ch->fork.queue);
    HISTORY_DEBUG("Shift fork element %zu", ch->fork.queue.used);
}

static bool fork_entry_concurrent_with(void *element, void *user)
{
    DP_ForkEntry *fe = element;
    DP_AffectedArea *aa = user;
    DP_AffectedArea *fork_aa = &fe->aa;
    if (DP_affected_area_concurrent_with(aa, fork_aa)) {
        return true;
    }
    else {
        DP_MessageType type = DP_message_type(fe->msg);
        DP_warn("Non-concurrent %s in local fork",
                DP_message_type_enum_name_unprefixed(type));
        DP_AFFECTED_AREA_PRINT(aa, "Received message", DP_warn);
        DP_AFFECTED_AREA_PRINT(fork_aa, "Message in fork", DP_warn);
        return false;
    }
}

static bool fork_entries_concurrent_with(DP_CanvasHistory *ch, DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);
    DP_AffectedArea aa = DP_affected_area_make(msg, &ch->aia);
    return DP_queue_all(&ch->fork.queue, sizeof(DP_ForkEntry),
                        fork_entry_concurrent_with, &aa);
}


static void call_save_point_fn(DP_CanvasHistory *ch, DP_CanvasState *cs,
                               bool snapshot)
{
    DP_CanvasHistorySavePointFn save_point_fn = ch->save_point.fn;
    if (save_point_fn) {
        save_point_fn(ch->save_point.user, cs, snapshot);
    }
}

static void set_initial_entry(DP_CanvasHistory *ch, DP_CanvasState *cs)
{
    HISTORY_DEBUG("Set initial history entry");
    ch->entries[0] = (DP_CanvasHistoryEntry){
        DP_UNDO_DONE, DP_msg_undo_point_new(0), DP_canvas_state_incref(cs)};
    call_save_point_fn(ch, cs, false);
}

static bool is_undo_point_entry(DP_CanvasHistoryEntry *entry)
{
    return DP_message_type(entry->msg) == DP_MSG_UNDO_POINT;
}

static bool is_valid_save_point_entry(DP_CanvasHistoryEntry *entry)
{
    switch (entry->undo) {
    case DP_UNDO_DONE:
        // Any done entry can hold a save point.
        return true;
    case DP_UNDO_UNDONE:
        // Undone undo points can still hold a save point for the sake of redos.
        return is_undo_point_entry(entry);
    case DP_UNDO_GONE:
        // Gone entries can never hold a save point.
        return false;
    default:
        HISTORY_DEBUG("Panic: invalid undo value %d", (int)entry->undo);
        DP_UNREACHABLE();
    }
}

static void validate_history(DP_UNUSED DP_CanvasHistory *ch,
                             DP_UNUSED bool check_fork)
{
    dump_history(ch);
#ifndef NDEBUG
    DP_CanvasHistoryEntry *entries = ch->entries;
    int used = ch->used;
    bool have_save_point = false;
    for (int i = 0; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        DP_ASSERT(entry->undo == DP_UNDO_DONE || entry->undo == DP_UNDO_UNDONE
                  || entry->undo == DP_UNDO_GONE);
        DP_Message *msg = entry->msg;
        DP_ASSERT(msg); // Message must not be null.
        DP_MessageType type = DP_message_type(msg);
        DP_ASSERT(type != DP_MSG_UNDO); // Undos and redos aren't historized.
        if (entry->state) {
            DP_ASSERT(is_valid_save_point_entry(entry));
            have_save_point = true;
        }
    }
    // There must exist at least one save point.
    DP_ASSERT(have_save_point);
    // If the local fork contains entries, it must also be consistent.
    if (check_fork && have_local_fork(ch)) {
        // Fork start can't be beyond the truncation point.
        DP_ASSERT(ch->fork.start >= ch->offset);
    }
#endif
}


DP_CanvasHistory *
DP_canvas_history_new(DP_CanvasHistorySavePointFn save_point_fn,
                      void *save_point_user, bool want_dump,
                      const char *dump_dir)
{
    return DP_canvas_history_new_inc(NULL, save_point_fn, save_point_user,
                                     want_dump, dump_dir);
}

DP_CanvasHistory *DP_canvas_history_new_inc(
    DP_CanvasState *cs_or_null, DP_CanvasHistorySavePointFn save_point_fn,
    void *save_point_user, bool want_dump, const char *dump_dir)
{
    DP_Mutex *mutex = DP_mutex_new();
    if (!mutex) {
        return NULL;
    }

    DP_CanvasHistory *ch = DP_malloc(sizeof(*ch));
    DP_CanvasState *cs =
        cs_or_null ? DP_canvas_state_incref(cs_or_null) : DP_canvas_state_new();
    size_t entries_size = sizeof(*ch->entries) * INITIAL_CAPACITY;

    *ch = (DP_CanvasHistory){
        mutex,
        cs,
        {0},
        {0},
        DP_UNDO_DEPTH_DEFAULT,
        0,
        INITIAL_CAPACITY,
        1,
        DP_malloc(entries_size),
        {0},
        true,
        {false, 0, 0, DP_QUEUE_NULL},
        {save_point_fn, save_point_user},
        {0, {0}},
        DP_ATOMIC_INIT(0),
        {want_dump, DP_strdup(dump_dir), NULL, 0, NULL},
    };
    DP_user_cursors_init(&ch->ucs);
    DP_effective_user_cursors_init(&ch->eucs);
    DP_affected_indirect_areas_clear(&ch->aia);

    dump_init(ch);
    if (cs_or_null) {
        dump_snapshot(ch, cs);
    }

    DP_queue_init(&ch->fork.queue, INITIAL_CAPACITY, sizeof(DP_ForkEntry));
    set_initial_entry(ch, cs);
    validate_history(ch, true);
    return ch;
}


static void dispose_entry(DP_CanvasHistoryEntry *entry)
{
    DP_message_decref(entry->msg);
    DP_canvas_state_decref_nullable(entry->state);
}

static void truncate_history_without_fork_check(DP_CanvasHistory *ch, int until)
{
    HISTORY_DEBUG("Truncate history until %d", until);
    DP_ASSERT(until <= ch->used);
    DP_CanvasHistoryEntry *entries = ch->entries;
    for (int i = 0; i < until; ++i) {
        dispose_entry(&entries[i]);
    }
    ch->used -= until;
    ch->offset += until;
    size_t size = sizeof(*entries) * DP_int_to_size(ch->used);
    memmove(entries, entries + until, size);
}

static void truncate_history(DP_CanvasHistory *ch, int until)
{
    DP_ASSERT(until <= ch->used);
    DP_ASSERT(!have_local_fork(ch) || ch->fork.start >= ch->offset + until);
    truncate_history_without_fork_check(ch, until);
}

void DP_canvas_history_free(DP_CanvasHistory *ch)
{
    if (ch) {
        clear_fork_entries(ch);
        DP_queue_dispose(&ch->fork.queue);
        truncate_history(ch, ch->used);
        DP_free(ch->entries);
        DP_canvas_state_decref(ch->current_state);
        DP_mutex_free(ch->mutex);
        DP_free(ch->dump.buffer);
        DP_output_free(ch->dump.output);
        DP_free(ch->dump.dir);
        DP_free(ch);
    }
}

bool DP_canvas_history_local_drawing_in_progress(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    return DP_atomic_get(&ch->local_drawing_in_progress);
}

void DP_canvas_history_local_drawing_in_progress_set(
    DP_CanvasHistory *ch, bool local_drawing_in_progress)
{
    DP_ASSERT(ch);
    DP_atomic_set(&ch->local_drawing_in_progress, local_drawing_in_progress);
}

bool DP_canvas_history_want_dump(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    return ch->dump.want;
}

void DP_canvas_history_want_dump_set(DP_CanvasHistory *ch, bool want_dump)
{
    DP_ASSERT(ch);
    ch->dump.want = want_dump;
}

DP_CanvasState *DP_canvas_history_get(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    DP_Mutex *mutex = ch->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    DP_CanvasState *cs = DP_canvas_state_incref(ch->current_state);
    DP_MUTEX_MUST_UNLOCK(mutex);
    return cs;
}

DP_CanvasState *
DP_canvas_history_compare_and_get(DP_CanvasHistory *ch, DP_CanvasState *prev,
                                  DP_UserCursorBuffer *out_user_cursors)
{
    DP_ASSERT(ch);
    DP_Mutex *mutex = ch->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    DP_CanvasState *next = ch->current_state;
    DP_CanvasState *cs;
    if (next == prev) {
        cs = NULL;
    }
    else {
        cs = DP_canvas_state_incref(next);
        if (out_user_cursors) {
            DP_effective_user_cursors_retrieve(&ch->eucs, out_user_cursors);
        }
    }
    DP_MUTEX_MUST_UNLOCK(mutex);
    return cs;
}

static void set_current_state_noinc(DP_CanvasHistory *ch, DP_CanvasState *next)
{
    DP_CanvasState *current = ch->current_state;
    DP_Mutex *mutex = ch->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    ch->current_state = next;
    DP_MUTEX_MUST_UNLOCK(mutex);
    DP_canvas_state_decref(current);
}

static void set_current_state_with_cursors_noinc(DP_CanvasHistory *ch,
                                                 DP_CanvasState *next)
{
    DP_CanvasState *current = ch->current_state;
    DP_Mutex *mutex = ch->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    ch->current_state = next;
    DP_effective_user_cursors_apply(&ch->eucs, &ch->ucs);
    DP_MUTEX_MUST_UNLOCK(mutex);
    DP_canvas_state_decref(current);
}


static void reset_to_state_noinc(DP_CanvasHistory *ch, DP_CanvasState *cs,
                                 bool clear_fork)
{
    set_current_state_noinc(ch, cs);
    if (clear_fork) {
        clear_fork_entries(ch);
        truncate_history(ch, ch->used);
    }
    else {
        truncate_history_without_fork_check(ch, ch->used);
    }
    DP_affected_indirect_areas_clear(&ch->aia);
    set_initial_entry(ch, cs);
    ch->used = 1;
    ch->offset = 0;
    ch->mark_command_done = true;
    validate_history(ch, clear_fork);
}

void DP_canvas_history_reset(DP_CanvasHistory *ch)
{
    // Hard reset: erase history and start from an empty canvas state. Is
    // used when the history gets too large. After the reset, there will
    // usually be a canvas resize command followed by layer creation and put
    // tile commands to restore the canvas as it was before the reset. Kinda
    // like git squash.
    HISTORY_DEBUG("Hard reset");
    dump_internal(ch, DP_DUMP_RESET);
    // Create a snapshot here, since there may be a reset to a desynced state.
    call_save_point_fn(ch, ch->current_state, true);
    reset_to_state_noinc(ch, DP_canvas_state_new(), true);
    ch->undo_depth_limit = DP_UNDO_DEPTH_DEFAULT;
    dump_init(ch);
}

void DP_canvas_history_reset_to_state_noinc(DP_CanvasHistory *ch,
                                            DP_CanvasState *cs)
{
    HISTORY_DEBUG("Reset to state");
    dump_internal(ch, DP_DUMP_RESET);
    reset_to_state_noinc(ch, cs, true);
    dump_init(ch);
    dump_snapshot(ch, cs);
}

static bool is_draw_dabs_message_type(DP_MessageType type)
{
    switch (type) {
    case DP_MSG_DRAW_DABS_CLASSIC:
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
    case DP_MSG_DRAW_DABS_MYPAINT:
        return true;
    default:
        return false;
    }
}

static DP_CanvasState *flush_replay_buffer(DP_CanvasHistory *ch,
                                           DP_CanvasState *cs,
                                           DP_DrawContext *dc)
{
    DP_CanvasState *next = DP_canvas_state_handle_multidab(
        cs, dc, NULL, ch->replay.used, ch->replay.buffer);
    ch->replay.used = 0;
    if (next) {
        DP_canvas_state_decref(cs);
        return next;
    }
    else {
        DP_warn("Error replaying drawing commands: %s", DP_error());
        return cs;
    }
}

static DP_CanvasState *replay_drawing_command_dec(DP_CanvasHistory *ch,
                                                  DP_CanvasState *cs,
                                                  DP_DrawContext *dc,
                                                  DP_Message *msg,
                                                  DP_MessageType type)
{
    if (is_draw_dabs_message_type(type)) {
        int index = ch->replay.used++;
        DP_ASSERT(index < REPLAY_BUFFER_CAPACITY);
        ch->replay.buffer[index] = msg;
        if (index < REPLAY_BUFFER_CAPACITY - 1) {
            return cs;
        }
        else {
            return flush_replay_buffer(ch, cs, dc);
        }
    }
    else {
        if (ch->replay.used != 0) {
            cs = flush_replay_buffer(ch, cs, dc);
        }
        DP_CanvasState *next = DP_canvas_state_handle(cs, dc, NULL, msg);
        if (next) {
            DP_canvas_state_decref(cs);
            return next;
        }
        else {
            DP_warn("Error replaying drawing command: %s", DP_error());
            return cs;
        }
    }
}

static int search_save_point_index(DP_CanvasHistory *ch, int target_index)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    for (int i = target_index; i >= 0; --i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (entry->state) {
            DP_ASSERT(is_valid_save_point_entry(entry));
            return i;
        }
    }
    return -1;
}

static void replay_fork_entry(void *element, void *user)
{
    DP_ForkEntry *fe = element;
    DP_Message *msg = fe->msg;
    DP_MessageType type = DP_message_type(msg);
    if (type != DP_MSG_UNDO && type != DP_MSG_UNDO_POINT) {
        DP_CanvasHistory *ch = ((void **)user)[0];
        DP_CanvasState **cs = ((void **)user)[1];
        DP_DrawContext *dc = ((void **)user)[2];
        *cs = replay_drawing_command_dec(ch, *cs, dc, msg, type);
        validate_history(ch, true);
    }
}

static DP_CanvasState *replay_fork_dec(DP_CanvasHistory *ch, DP_CanvasState *cs,
                                       DP_DrawContext *dc)
{
    bool starts_at_undo_point =
        DP_message_type(peek_fork_entry_message(ch)) == DP_MSG_UNDO_POINT;
    set_fork_start(ch, starts_at_undo_point);
    DP_queue_each(&ch->fork.queue, sizeof(DP_ForkEntry), replay_fork_entry,
                  (void *[]){ch, &cs, dc});
    return cs;
}

static void finish_replay(DP_CanvasHistory *ch, DP_CanvasState *cs,
                          DP_DrawContext *dc)
{
    if (ch->replay.used != 0) {
        cs = flush_replay_buffer(ch, cs, dc);
    }
    set_current_state_noinc(ch, cs);
}

static void replay_from_inc(DP_CanvasHistory *ch, DP_DrawContext *dc,
                            int start_index, DP_CanvasState *start_cs,
                            bool with_fork)
{
    DP_ASSERT(start_cs);
    DP_CanvasHistoryEntry *entries = ch->entries;
    DP_CanvasState *cs = DP_canvas_state_incref(start_cs);

    int used = ch->used;
    for (int i = start_index + 1; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        DP_Undo undo = entry->undo;
        if (undo != DP_UNDO_GONE) {
            DP_Message *msg = entry->msg;
            DP_MessageType type = DP_message_type(msg);
            // Update undo points even when they're undone so
            // they can serve as a starting point for redos.
            if (type == DP_MSG_UNDO_POINT) {
                if (ch->replay.used != 0) {
                    cs = flush_replay_buffer(ch, cs, dc);
                }
                DP_canvas_state_decref_nullable(entry->state);
                entry->state = DP_canvas_state_incref(cs);
            }
            else if (undo == DP_UNDO_DONE) {
                cs = replay_drawing_command_dec(ch, cs, dc, msg, type);
                validate_history(ch, with_fork);
            }
        }
    }

    if (with_fork && have_local_fork(ch)) {
        DP_ASSERT(ch->fork.start >= start_index + ch->offset);
        cs = replay_fork_dec(ch, cs, dc);
    }

    finish_replay(ch, cs, dc);
}

static bool search_and_replay_from(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                   int target_index, bool with_fork)
{
    int start_index = search_save_point_index(ch, target_index);
    HISTORY_DEBUG("Replay from target %d, start %d", target_index, start_index);
    if (start_index >= 0) {
        replay_from_inc(ch, dc, start_index, ch->entries[start_index].state,
                        with_fork);
        return true;
    }
    else {
        DP_error_set("Can't find save point at or before %d", target_index);
        return false;
    }
}

void DP_canvas_history_soft_reset(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                  unsigned int context_id,
                                  DP_CanvasHistorySoftResetFn fn, void *user)
{
    // Soft reset: erase history, but keep the current canvas state.
    // Is used to avoid resetting the whole session when a new user joins
    // and the server doesn't have the history available, which is the case
    // for the built-in, non-dedicated Drawpile server. It sends a hard
    // reset only to the newly joined client, all the other clients get a
    // soft reset so that they can't undo beyond the point that the new
    // client joined at.
    HISTORY_DEBUG("Soft reset");
    dump_internal(ch, DP_DUMP_SOFT_RESET);

    bool have_fork = have_local_fork(ch);
    if (have_fork) {
        search_and_replay_from(ch, dc, ch->fork.start - ch->offset, false);
    }

    DP_CanvasState *cs = DP_canvas_state_incref(ch->current_state);
    if (fn) {
        fn(user, context_id, cs);
    }
    reset_to_state_noinc(ch, cs, false);

    if (have_fork) {
        finish_replay(ch, replay_fork_dec(ch, DP_canvas_state_incref(cs), dc),
                      dc);
    }
}

int DP_canvas_history_undo_depth_limit(DP_CanvasHistory *ch)
{
    DP_ASSERT(ch);
    return ch->undo_depth_limit;
}

void DP_canvas_history_undo_depth_limit_set(DP_CanvasHistory *ch,
                                            DP_DrawContext *dc,
                                            int undo_depth_limit)
{
    DP_ASSERT(ch);

    bool have_fork = have_local_fork(ch);
    if (have_fork) {
        search_and_replay_from(ch, dc, ch->fork.start - ch->offset, false);
    }

    int effective_limit =
        DP_clamp_int(undo_depth_limit, DP_CANVAS_HISTORY_UNDO_DEPTH_MIN,
                     DP_CANVAS_HISTORY_UNDO_DEPTH_MAX);
    DP_debug("Set undo depth limit to %d (%d given)", effective_limit,
             undo_depth_limit);
    dump_undo_depth_limit(ch, effective_limit);
    ch->undo_depth_limit = effective_limit;

    HISTORY_DEBUG("Set undo depth limit");
    DP_CanvasState *cs = DP_canvas_state_incref(ch->current_state);
    reset_to_state_noinc(ch, cs, false);

    if (have_fork) {
        finish_replay(ch, replay_fork_dec(ch, DP_canvas_state_incref(cs), dc),
                      dc);
    }
}

static int find_save_point_index(DP_CanvasHistory *ch)
{
    for (int i = ch->used - 1; i >= 0; --i) {
        DP_CanvasHistoryEntry *entry = &ch->entries[i];
        if (is_valid_save_point_entry(entry)) {
            return i;
        }
    }
    HISTORY_DEBUG("Panic: no possible save point index found");
    dump_history(ch);
    DP_UNREACHABLE(); // The history can't be totally gone.
}

static void make_save_point(DP_CanvasHistory *ch, int index,
                            bool snapshot_requested)
{
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ch->used);
    // Save points based on local fork state are invalid.
    DP_ASSERT(!have_local_fork(ch));
    DP_CanvasHistoryEntry *entry = &ch->entries[index];
    // This must actually be a valid spot for a save point.
    DP_ASSERT(is_valid_save_point_entry(entry));
    // There might already be a save point here, don't create one again.
    if (!entry->state) {
        HISTORY_DEBUG("Create %s save point at %d",
                      snapshot_requested ? "requested" : "regular", index);
        DP_CanvasState *cs = ch->current_state;
        entry->state = DP_canvas_state_incref(cs);
        call_save_point_fn(ch, cs, snapshot_requested);
    }
}

bool DP_canvas_history_save_point_make(DP_CanvasHistory *ch)
{
    if (!have_local_fork(ch)) {
        make_save_point(ch, find_save_point_index(ch), true);
        return true;
    }
    else {
        DP_error_set("Can't create save point when internal fork is present");
        return false;
    }
}

static bool handle_internal(DP_CanvasHistory *ch, DP_MsgInternal *mi)
{
    DP_MsgInternalType internal_type = DP_msg_internal_type(mi);
    switch (internal_type) {
    case DP_MSG_INTERNAL_TYPE_RESET:
        DP_canvas_history_reset(ch);
        return true;
    case DP_MSG_INTERNAL_TYPE_SNAPSHOT:
        return DP_canvas_history_save_point_make(ch);
    default:
        DP_error_set("Unhandled internal message type: %d", (int)internal_type);
        return false;
    }
}


static void ensure_append_capacity(DP_CanvasHistory *ch)
{
    int old_capacity = ch->capacity;
    if (ch->used == old_capacity) {
        int new_capacity = EXPAND_CAPACITY(old_capacity);
        size_t new_size = sizeof(*ch->entries) * DP_int_to_size(new_capacity);
        HISTORY_DEBUG("Resize history capacity to %d entries", ch->capacity);
        ch->entries = DP_realloc(ch->entries, new_size);
        ch->capacity = new_capacity;
    }
}

static int append_to_history_noinc(DP_CanvasHistory *ch, DP_Message *msg,
                                   DP_Undo undo)
{
    DP_ASSERT(DP_message_type(msg) != DP_MSG_UNDO);
    ensure_append_capacity(ch);
    int index = ch->used;
    HISTORY_DEBUG("Append history entry %d", index);
    ch->entries[index] = (DP_CanvasHistoryEntry){undo, msg, NULL};
    ch->used = index + 1;
    return index;
}

static int append_to_history_inc(DP_CanvasHistory *ch, DP_Message *msg,
                                 DP_Undo undo)
{
    DP_ASSERT(DP_message_type(msg) != DP_MSG_UNDO);
    return append_to_history_noinc(ch, DP_message_incref(msg), undo);
}


static int mark_undone_actions_gone(DP_CanvasHistory *ch, int index,
                                    int *out_depth)
{
    unsigned int context_id = DP_message_context_id(ch->entries[index].msg);
    DP_CanvasHistoryEntry *entries = ch->entries;
    int i = index - 1;
    int depth = 1;
    int undo_depth_limit = ch->undo_depth_limit;
    for (; i >= 0 && depth < undo_depth_limit; --i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (is_undo_point_entry(entry)) {
            ++depth;
        }

        DP_Message *msg = entry->msg;
        if (DP_message_context_id(msg) == context_id) {
            DP_Undo undo = entry->undo;
            if (undo == DP_UNDO_GONE) {
                break; // Everything beyond this point is already gone.
            }
            else if (undo == DP_UNDO_UNDONE) {
                entry->undo = DP_UNDO_GONE;
                // Undone undo points still have a state for redo purposes.
                DP_CanvasState *cs = entry->state;
                if (cs) {
                    DP_canvas_state_decref(cs);
                    entry->state = NULL;
                }
            }
        }
    }
    *out_depth = depth;
    return i;
}

static void truncate_unreachable(DP_CanvasHistory *ch, int i, int depth)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    int undo_depth_limit = ch->undo_depth_limit;
    for (; i >= 0 && depth < undo_depth_limit; --i) {
        if (is_undo_point_entry(&entries[i])) {
            ++depth;
        }
    }
    // The local fork start must remain reachable.
    if (have_local_fork(ch)) {
        int fork_start_index = ch->fork.start - ch->offset;
        if (i > fork_start_index) {
            i = fork_start_index;
        }
    }
    // There must be a save point at or before the furthest undo point.
    while (i > 0 && !entries[i].state) {
        --i;
    }
    // If we went to zero, everything is reachable.
    if (i > 0) {
        truncate_history(ch, i);
    }
}

static void handle_undo_point(DP_CanvasHistory *ch, int index)
{
    // Don't make save points while a local fork is present, since the local
    // state may be incongruent with what the server thinks is happening.
    if (!have_local_fork(ch)) {
        make_save_point(ch, index, false);
    }
    int depth;
    int i = mark_undone_actions_gone(ch, index, &depth);
    truncate_unreachable(ch, i, depth);
}


void DP_canvas_history_cleanup(DP_CanvasHistory *ch, DP_DrawContext *dc,
                               void (*push_message)(void *, DP_Message *),
                               void *user)
{
    DP_ASSERT(ch);
    DP_ASSERT(push_message);
    dump_internal(ch, DP_DUMP_CLEANUP);
    if (have_local_fork(ch)) {
        int target_index = ch->fork.start - ch->offset;
        while (ch->fork.queue.used) {
            DP_Message *msg = peek_fork_entry_message(ch);
            push_message(user, msg);
            shift_fork_entry_nodec(ch);
        }
        if (!search_and_replay_from(ch, dc, target_index, true)) {
            DP_warn("Cleanup: %s", DP_error());
        }
    }
    push_message(user, DP_msg_pen_up_new(0));
}


bool DP_canvas_history_local_fork_clear(DP_CanvasHistory *ch,
                                        DP_DrawContext *dc)
{
    DP_ASSERT(ch);
    if (ch->fork.queue.used == 0) {
        return true;
    }
    else {
        DP_warn("Clear local fork at %d", ch->offset + ch->used);
        clear_fork_entries(ch);
        return search_and_replay_from(ch, dc, ch->fork.start - ch->offset,
                                      true);
    }
}


static bool is_done_entry_by(DP_CanvasHistoryEntry *entry,
                             unsigned int context_id)
{
    return entry->undo == DP_UNDO_DONE
        && DP_message_context_id(entry->msg) == context_id;
}

static int find_first_undo_point(DP_CanvasHistory *ch, unsigned int context_id,
                                 int *out_depth)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    int i;
    int depth = 0;
    int undo_depth_limit = ch->undo_depth_limit;
    for (i = ch->used - 1; i >= 0 && depth <= undo_depth_limit; --i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (is_undo_point_entry(entry)) {
            ++depth;
            if (is_done_entry_by(entry, context_id)) {
                break;
            }
        }
    }
    *out_depth = depth;
    return i;
}

static void mark_entries_undone(DP_CanvasHistory *ch, unsigned int context_id,
                                int undo_start)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    int used = ch->used;
    for (int i = undo_start; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (is_done_entry_by(entry, context_id)) {
            entry->undo = DP_UNDO_UNDONE;
        }
        // Clear out any states that were left behind by local fork starts, they
        // happen too frequently to update them all on every undo/redo. Instead
        // only undo points get to keep their states and get updated.
        if (!is_undo_point_entry(entry)) {
            DP_CanvasState *cs = entry->state;
            if (cs) {
                DP_canvas_state_decref_nullable(cs);
                entry->state = NULL;
            }
        }
    }
}

static bool undo(DP_CanvasHistory *ch, DP_DrawContext *dc,
                 unsigned int context_id)
{
    int depth;
    int undo_start = find_first_undo_point(ch, context_id, &depth);
    HISTORY_DEBUG("Undo for user %u from %d", context_id, undo_start);
    int undo_depth_limit = ch->undo_depth_limit;
    if (depth > undo_depth_limit) {
        DP_error_set("Undo by user %u beyond history limit %d", context_id,
                     undo_depth_limit);
        return false;
    }
    else if (undo_start < 0) {
        DP_error_set("Nothing found to undo for user %u", context_id);
        return false;
    }
    else {
        mark_entries_undone(ch, context_id, undo_start);
        return search_and_replay_from(ch, dc, undo_start, true);
    }
}


static int find_oldest_redo_point(DP_CanvasHistory *ch, unsigned int context_id,
                                  int *out_depth)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    int redo_start = -1;
    int depth = 0;
    int undo_depth_limit = ch->undo_depth_limit;
    for (int i = ch->used - 1; i >= 0 && depth <= undo_depth_limit; --i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (is_undo_point_entry(entry)) {
            ++depth;
            if (DP_message_context_id(entry->msg) == context_id) {
                DP_Undo undo = entry->undo;
                if (undo == DP_UNDO_UNDONE) {
                    redo_start = i;
                }
                else if (undo == DP_UNDO_DONE) {
                    break;
                }
            }
        }
    }
    *out_depth = depth;
    return redo_start;
}

static void mark_entries_redone(DP_CanvasHistory *ch, unsigned int context_id,
                                int redo_start)
{
    DP_CanvasHistoryEntry *entries = ch->entries;
    entries[redo_start].undo = DP_UNDO_DONE;
    int used = ch->used;
    for (int i = redo_start + 1; i < used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        if (DP_message_context_id(entry->msg) == context_id) {
            DP_Undo undo = entry->undo;
            if (is_undo_point_entry(entry) && undo != DP_UNDO_GONE) {
                break;
            }
            else if (undo == DP_UNDO_UNDONE) {
                entry->undo = DP_UNDO_DONE;
            }
        }
    }
}

static bool redo(DP_CanvasHistory *ch, DP_DrawContext *dc,
                 unsigned int context_id)
{
    int depth;
    int redo_start = find_oldest_redo_point(ch, context_id, &depth);
    HISTORY_DEBUG("Redo for user %u from %d", context_id, redo_start);
    int undo_depth_limit = ch->undo_depth_limit;
    if (depth > undo_depth_limit) {
        DP_error_set("Redo by user %u beyond history limit %d", context_id,
                     undo_depth_limit);
        return false;
    }
    else if (redo_start < 0) {
        DP_error_set("Nothing found to redo for user %u", context_id);
        return false;
    }
    else {
        mark_entries_redone(ch, context_id, redo_start);
        return search_and_replay_from(ch, dc, redo_start, true);
    }
}


static unsigned int get_undo_context_id(DP_Message *msg, DP_MsgUndo *mu)
{
    unsigned int override_id = DP_msg_undo_override_user(mu);
    return override_id == 0 ? DP_message_context_id(msg) : override_id;
}

static bool handle_undo(DP_CanvasHistory *ch, DP_DrawContext *dc,
                        DP_Message *msg)
{
    DP_MsgUndo *mu = DP_msg_undo_cast(msg);
    bool is_redo = DP_msg_undo_redo(mu);
    unsigned int context_id = get_undo_context_id(msg, mu);
    return is_redo ? redo(ch, dc, context_id) : undo(ch, dc, context_id);
}


static bool handle_drawing_command(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                   DP_Message *msg)
{
    DP_CanvasState *cs =
        DP_canvas_state_handle(ch->current_state, dc, &ch->ucs, msg);
    if (cs) {
        set_current_state_with_cursors_noinc(ch, cs);
        return true;
    }
    else {
        return false;
    }
}


static bool handle_command(DP_CanvasHistory *ch, DP_DrawContext *dc,
                           DP_Message *msg, DP_MessageType type, int index)
{
    DP_ASSERT(DP_message_type(msg) == type);
    switch (type) {
    case DP_MSG_UNDO_POINT:
        HISTORY_DEBUG("Undo point for user %u at %d",
                      DP_message_context_id(msg), index);
        handle_undo_point(ch, index);
        return true;
    case DP_MSG_UNDO:
        return handle_undo(ch, dc, msg);
    default:
        return handle_drawing_command(ch, dc, msg);
    }
}

static bool match_message(DP_Message *msg, DP_MessageType type,
                          DP_Message *peeked_msg, DP_MessageType peeked_type)
{
    if (type == peeked_type) {
        return DP_message_payload_equals(msg, peeked_msg);
    }
    else if (DP_msg_local_match_is_local_match(msg)) {
        return DP_msg_local_match_matches(peeked_msg, msg);
    }
    else {
        return false;
    }
}

static void clear_fork_fallbehind(DP_CanvasHistory *ch)
{
    HISTORY_DEBUG("Fork fallbehind cleared");
    ch->fork.fallbehind = 0;
    int index = ch->fork.start - ch->offset;
    DP_ASSERT(index >= 0 && index < ch->used);
    DP_CanvasHistoryEntry *entry = &ch->entries[index];
    if (!ch->fork.starts_at_undo_point && !is_undo_point_entry(entry)) {
        DP_canvas_state_decref_nullable(entry->state);
        entry->state = NULL;
    }
}

static void maybe_append_to_history_inc(DP_CanvasHistory *ch, DP_Message *msg,
                                        int *out_index)
{
    if (out_index) {
        *out_index = append_to_history_inc(ch, msg, DP_UNDO_DONE);
    }
}

static DP_ForkAction reconcile_remote_command(DP_CanvasHistory *ch,
                                              DP_Message *msg,
                                              DP_MessageType type,
                                              bool local_drawing_in_progress,
                                              int *out_index)
{
    DP_ASSERT(DP_message_type(msg) == type);

    size_t used = ch->fork.queue.used;
    if (used == 0) {
        maybe_append_to_history_inc(ch, msg, out_index);
        return DP_FORK_ACTION_CONCURRENT;
    }

    DP_Message *peeked_msg = peek_fork_entry_message(ch);
    if (DP_message_context_id(msg) == DP_message_context_id(peeked_msg)) {
        DP_MessageType peeked_type = DP_message_type(peeked_msg);
        if (match_message(msg, type, peeked_msg, peeked_type)) {
            shift_fork_entry_nodec(ch);
            if (out_index) {
                *out_index =
                    append_to_history_noinc(ch, peeked_msg, DP_UNDO_DONE);
            }
            else {
                DP_message_decref(peeked_msg);
            }

            if (used == 1) {
                clear_fork_fallbehind(ch);
            }

            if (type == DP_MSG_UNDO || type == DP_MSG_UNDO_POINT) {
                // Undo operations aren't handled locally.
                return DP_FORK_ACTION_CONCURRENT;
            }
            else {
                return DP_FORK_ACTION_ALREADY_DONE;
            }
        }
        else {
            maybe_append_to_history_inc(ch, msg, out_index);
            DP_warn("Rollback at %d: fork %s != received %s",
                    ch->offset + ch->used,
                    DP_message_type_enum_name_unprefixed(peeked_type),
                    DP_message_type_enum_name_unprefixed(type));
            ch->fork.fallbehind = 0;
            clear_fork_entries(ch);
            return DP_FORK_ACTION_ROLLBACK;
        }
    }
    else {
        maybe_append_to_history_inc(ch, msg, out_index);
        if (++ch->fork.fallbehind >= MAX_FALLBEHIND) {
            DP_warn("Rollback at %d: fork fallbehind %d >= max fallbehind %d",
                    ch->offset + ch->used, ch->fork.fallbehind, MAX_FALLBEHIND);
            ch->fork.fallbehind = 0;
            clear_fork_entries(ch);
            return DP_FORK_ACTION_ROLLBACK;
        }
        else if (fork_entries_concurrent_with(ch, msg)) {
            return DP_FORK_ACTION_CONCURRENT;
        }
        else {
            // Avoid a rollback storm by clearing the local fork, but not when
            // drawing is in progress, since that would cause a feedback loop.
            if (local_drawing_in_progress) {
                DP_warn("Rollback at %d: fork not concurrent with %s - "
                        "not clearing fork due to local drawing in progress",
                        ch->offset + ch->used,
                        DP_message_type_enum_name_unprefixed(type));
            }
            else {
                DP_warn("Rollback at %d: fork not concurrent with %s - "
                        "clearing fork",
                        ch->offset + ch->used,
                        DP_message_type_enum_name_unprefixed(type));
                clear_fork_entries(ch);
            }
            return DP_FORK_ACTION_ROLLBACK;
        }
    }
}

static bool handle_remote_command(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                  DP_Message *msg, DP_MessageType type,
                                  bool local_drawing_in_progress,
                                  bool append_to_history)
{
    DP_ASSERT(DP_message_type(msg) == type);
    HISTORY_DEBUG("Handle remote %s command from user %u",
                  DP_message_type_enum_name_unprefixed(type),
                  DP_message_context_id(msg));
    int index;
    DP_ForkAction fork_action;
    if (append_to_history) {
        fork_action = reconcile_remote_command(
            ch, msg, type, local_drawing_in_progress, &index);
    }
    else {
        index = -1;
        fork_action = reconcile_remote_command(ch, msg, type,
                                               local_drawing_in_progress, NULL);
    }

    switch (fork_action) {
    case DP_FORK_ACTION_CONCURRENT: {
        return handle_command(ch, dc, msg, type, index);
    }
    case DP_FORK_ACTION_ROLLBACK:
        return search_and_replay_from(ch, dc, ch->fork.start - ch->offset,
                                      true);
    default:
        DP_ASSERT(fork_action == DP_FORK_ACTION_ALREADY_DONE);
        return true;
    }
}

static bool handle_remote_message(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                  DP_Message *msg, DP_MessageType type,
                                  bool local_drawing_in_progress)
{
    switch (type) {
    case DP_MSG_INTERNAL:
        return handle_internal(ch, DP_msg_internal_cast(msg));
    case DP_MSG_UNDO:
        // An undo of user 0 means the next command should be appended as an
        // undone state. This is so we can get a proper undo history when
        // starting a recording or jumping to a recording index.
        if (get_undo_context_id(msg, DP_message_internal(msg)) == 0) {
            ch->mark_command_done = false;
            return true;
        }
        else {
            return handle_remote_command(ch, dc, msg, type,
                                         local_drawing_in_progress, false);
        }
    default:
        if (ch->mark_command_done) {
            return handle_remote_command(ch, dc, msg, type,
                                         local_drawing_in_progress, true);
        }
        else {
            ch->mark_command_done = true;
            append_to_history_inc(ch, msg, DP_UNDO_UNDONE);
            return true;
        }
    }
}

bool DP_canvas_history_handle(DP_CanvasHistory *ch, DP_DrawContext *dc,
                              DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);
    HISTORY_DEBUG("Handle remote %s command from user %u",
                  DP_message_type_enum_name_unprefixed(DP_message_type(msg)),
                  DP_message_context_id(msg));

    bool local_drawing_in_progress =
        DP_canvas_history_local_drawing_in_progress(ch);
    dump_message(ch, msg,
                 local_drawing_in_progress
                     ? DP_DUMP_REMOTE_MESSAGE_LOCAL_DRAWING_IN_PROGRESS
                     : DP_DUMP_REMOTE_MESSAGE);

    DP_MessageType type = DP_message_type(msg);
    DP_PERF_BEGIN_DETAIL(fn, "handle", "type=%d,local_drawing=%d", (int)type,
                         local_drawing_in_progress);
    bool ok =
        handle_remote_message(ch, dc, msg, type, local_drawing_in_progress);
    validate_history(ch, true);
    DP_PERF_END(fn);
    return ok;
}


bool DP_canvas_history_handle_local(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                    DP_Message *msg)
{
    DP_ASSERT(ch);
    DP_ASSERT(msg);
    HISTORY_DEBUG("Handle local %s command from user %u",
                  DP_message_type_enum_name_unprefixed(DP_message_type(msg)),
                  DP_message_context_id(msg));
    dump_message(ch, msg, DP_DUMP_LOCAL_MESSAGE);

    DP_MessageType type = DP_message_type(msg);
    DP_PERF_BEGIN_DETAIL(fn, "handle_local", "type=%d", (int)type);

    if (!have_local_fork(ch)) {
        set_fork_start(ch, type == DP_MSG_UNDO_POINT);
        make_save_point(ch, find_save_point_index(ch), false);
    }
    push_fork_entry_inc(ch, msg);

    bool ok = type == DP_MSG_UNDO || type == DP_MSG_UNDO_POINT
           || handle_drawing_command(ch, dc, msg);
    validate_history(ch, true);

    DP_PERF_END(fn);
    return ok;
}


void DP_canvas_history_handle_multidab_dec(DP_CanvasHistory *ch,
                                           DP_DrawContext *dc, int count,
                                           DP_Message **msgs)
{
    DP_ASSERT(ch);
    DP_ASSERT(count > 1);
    DP_ASSERT(msgs);

    bool local_drawing_in_progress =
        DP_canvas_history_local_drawing_in_progress(ch);
    dump_multidab(ch, count, msgs,
                  local_drawing_in_progress
                      ? DP_DUMP_REMOTE_MULTIDAB_LOCAL_DRAWING_IN_PROGRESS
                      : DP_DUMP_REMOTE_MULTIDAB);
    DP_PERF_BEGIN_DETAIL(fn, "handle_multidab", "count=%d,local_drawing=%d",
                         count, local_drawing_in_progress);

    int offset = 0;
    for (int i = 0; i < count; ++i) {
        DP_Message *msg = msgs[i];
        append_to_history_noinc(ch, msg, DP_UNDO_DONE);
        DP_ForkAction fork_action = reconcile_remote_command(
            ch, msg, DP_message_type(msg), local_drawing_in_progress, false);
        switch (fork_action) {
        case DP_FORK_ACTION_ROLLBACK:
            search_and_replay_from(ch, dc, ch->fork.start - ch->offset, true);
            offset = i + 1; // Those messages are already replayed now.
            break;
        case DP_FORK_ACTION_ALREADY_DONE:
            if (offset == i) {
                ++offset; // Remove message from the beginning.
            }
            else {
                msgs[i] = NULL; // Rub out message in the middle.
            }
            break;
        default:
            DP_ASSERT(fork_action == DP_FORK_ACTION_CONCURRENT);
            break;
        }
    }
    validate_history(ch, true);

    if (offset != count) {
        DP_CanvasState *cs = DP_canvas_state_handle_multidab(
            ch->current_state, dc, &ch->ucs, count - offset, msgs + offset);
        if (cs) {
            set_current_state_with_cursors_noinc(ch, cs);
        }
    }

    DP_PERF_END(fn);
}

void DP_canvas_history_handle_local_multidab_dec(DP_CanvasHistory *ch,
                                                 DP_DrawContext *dc, int count,
                                                 DP_Message **msgs)
{
    DP_ASSERT(ch);
    DP_ASSERT(count > 0);
    DP_ASSERT(msgs);
    dump_multidab(ch, count, msgs, DP_DUMP_LOCAL_MULTIDAB);
    DP_PERF_BEGIN_DETAIL(fn, "handle_local_multidab", "count=%d", count);

    if (!have_local_fork(ch)) {
        set_fork_start(ch, false);
        make_save_point(ch, find_save_point_index(ch), false);
    }

    for (int i = 0; i < count; ++i) {
        push_fork_entry_noinc(ch, msgs[i]);
    }

    DP_CanvasState *cs = DP_canvas_state_handle_multidab(ch->current_state, dc,
                                                         &ch->ucs, count, msgs);
    if (cs) {
        set_current_state_with_cursors_noinc(ch, cs);
    }

    validate_history(ch, true);
    DP_PERF_END(fn);
}


static int find_first_reachable_state_index(DP_CanvasHistoryEntry *entries,
                                            int entries_used,
                                            int undo_depth_limit,
                                            DP_CanvasState **out_cs)
{
    int start = entries_used - 1;
    int depth = 0;
    for (; start > 0 && depth < undo_depth_limit; --start) {
        if (is_undo_point_entry(&entries[start])) {
            ++depth;
        }
    }
    // There must be a save point at or before the furthest undo point.
    while (start > 0 && !entries[start].state) {
        --start;
    }

    DP_CanvasHistoryEntry *entry = &entries[start];
    DP_ASSERT(entry->state);
    *out_cs = entry->state;
    // If the starting point is not an undo point, the command inside the entry
    // has already been applied to the canvas state, so don't record it again.
    return is_undo_point_entry(entry) ? start : start + 1;
}

bool DP_canvas_history_reset_image_new(
    DP_CanvasHistory *ch, DP_CanvasHistoryAcceptResetStateFn accept_state,
    DP_CanvasHistoryAcceptResetMessageFn accept_message, void *user)
{
    DP_ASSERT(ch);
    DP_ASSERT(accept_state);
    DP_ASSERT(accept_message);

    DP_CanvasHistoryEntry *entries = ch->entries;
    int entries_used = ch->used;
    int undo_depth_limit = ch->undo_depth_limit;

    DP_CanvasState *cs;
    int start = find_first_reachable_state_index(entries, entries_used,
                                                 undo_depth_limit, &cs);
    if (!accept_state(user, cs)) {
        return false;
    }

    if (!accept_message(user, DP_msg_undo_depth_new(
                                  0, DP_int_to_uint8(undo_depth_limit)))) {
        return false;
    }

    for (int i = start; i < entries_used; ++i) {
        DP_CanvasHistoryEntry *entry = &entries[i];
        switch (entry->undo) {
        case DP_UNDO_UNDONE:
            // If this is an undone entry, prefix it with an undo by user 0.
            // That's never a valid user for an undo otherwise, so it instead
            // gets interpreted as meaning to mark the next command undone.
            if (!accept_message(user, DP_msg_undo_new(0, 0, false))) {
                return false;
            }
            DP_FALLTHROUGH();
        case DP_UNDO_DONE: {
            DP_Message *msg = entry->msg;
            // Local match messages are for local synchronization and don't
            // contain any useful information, so we don't need to record them.
            if (!DP_msg_local_match_is_local_match(msg)) {
                if (!accept_message(user, DP_message_incref(msg))) {
                    return false;
                }
            }
            break;
        }
        default:
            break;
        }
    }

    return true;
}


struct DP_CanvasHistoryRecorderParams {
    DP_CanvasHistory *ch;
    DP_RecorderType type;
    JSON_Value *header;
    DP_RecorderGetTimeMsFn get_time_fn;
    void *get_time_user;
    DP_Output *output;
    DP_Recorder *r;
};

static bool accept_recorder_state(void *user, DP_CanvasState *cs)
{
    struct DP_CanvasHistoryRecorderParams *params = user;
    DP_ASSERT(!params->r);
    params->r = DP_recorder_new_inc(params->type, params->header, cs,
                                    params->get_time_fn, params->get_time_user,
                                    params->output);
    return params->r != NULL;
}

static bool accept_recorder_message(void *user, DP_Message *msg)
{
    struct DP_CanvasHistoryRecorderParams *params = user;
    DP_ASSERT(params->r);
    return DP_recorder_message_push_noinc(params->r, msg);
}

DP_Recorder *DP_canvas_history_recorder_new(
    DP_CanvasHistory *ch, DP_RecorderType type, JSON_Value *header,
    DP_RecorderGetTimeMsFn get_time_fn, void *get_time_user, DP_Output *output)
{
    DP_ASSERT(ch);
    DP_ASSERT(header);
    DP_ASSERT(output);
    struct DP_CanvasHistoryRecorderParams params = {
        ch, type, header, get_time_fn, get_time_user, output, NULL};
    bool ok = DP_canvas_history_reset_image_new(
        ch, accept_recorder_state, accept_recorder_message, &params);
    if (ok && params.r) {
        return params.r;
    }
    else {
        DP_recorder_free_join(params.r, NULL);
        return NULL;
    }
}


static DP_CanvasHistoryEntry *snapshot_history(DP_CanvasHistory *ch)
{
    int count = ch->used;
    DP_CanvasHistoryEntry *entries =
        count == 0 ? NULL : DP_malloc(sizeof(*entries) * DP_int_to_size(count));
    for (int i = 0; i < count; ++i) {
        DP_CanvasHistoryEntry *entry = &ch->entries[i];
        entries[i] = (DP_CanvasHistoryEntry){
            entry->undo, DP_message_incref(entry->msg),
            DP_canvas_state_incref_nullable(entry->state)};
    }
    return entries;
}

static DP_ForkEntry *snapshot_fork(DP_CanvasHistory *ch)
{
    size_t count = ch->fork.queue.used;
    DP_ForkEntry *fes = count == 0 ? NULL : DP_malloc(sizeof(*fes) * count);
    for (size_t i = 0; i < count; ++i) {
        DP_ForkEntry *fe =
            DP_queue_at(&ch->fork.queue, sizeof(DP_ForkEntry), i);
        fes[i] = (DP_ForkEntry){DP_message_incref(fe->msg), fe->aa};
    }
    return fes;
}

DP_CanvasHistorySnapshot *DP_canvas_history_snapshot_new(DP_CanvasHistory *ch)
{
    DP_CanvasHistorySnapshot *chs = DP_malloc(sizeof(*chs));
    *chs = (DP_CanvasHistorySnapshot){
        DP_ATOMIC_INIT(1),
        {ch->undo_depth_limit, ch->offset, ch->used, snapshot_history(ch)},
        {ch->fork.start, ch->fork.fallbehind,
         DP_size_to_int(ch->fork.queue.used), snapshot_fork(ch)}};
    return chs;
}

DP_CanvasHistorySnapshot *
DP_canvas_history_snapshot_incref(DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    DP_atomic_inc(&chs->refcount);
    return chs;
}

DP_CanvasHistorySnapshot *DP_canvas_history_snapshot_incref_nullable(
    DP_CanvasHistorySnapshot *chs_or_null)
{
    return chs_or_null ? DP_canvas_history_snapshot_incref(chs_or_null) : NULL;
}

void DP_canvas_history_snapshot_decref(DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    if (DP_atomic_dec(&chs->refcount)) {
        int fork_count = chs->fork.count;
        for (int i = 0; i < fork_count; ++i) {
            DP_message_decref(chs->fork.entries[i].msg);
        }
        DP_free(chs->fork.entries);
        int history_count = chs->history.count;
        for (int i = 0; i < history_count; ++i) {
            DP_CanvasHistoryEntry *entry = &chs->history.entries[i];
            DP_canvas_state_decref_nullable(entry->state);
            DP_message_decref(entry->msg);
        }
        DP_free(chs->history.entries);
        DP_free(chs);
    }
}

void DP_canvas_history_snapshot_decref_nullable(
    DP_CanvasHistorySnapshot *chs_or_null)
{
    if (chs_or_null) {
        DP_canvas_history_snapshot_decref(chs_or_null);
    }
}

int DP_canvas_history_snapshot_refs(DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    return DP_atomic_get(&chs->refcount);
}

int DP_canvas_history_snapshot_history_undo_depth_limit(
    DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    return chs->history.undo_depth_limit;
}

int DP_canvas_history_snapshot_history_offset(DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    return chs->history.offset;
}

int DP_canvas_history_snapshot_history_count(DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    return chs->history.count;
}

const DP_CanvasHistoryEntry *
DP_canvas_history_snapshot_history_entry_at(DP_CanvasHistorySnapshot *chs,
                                            int index)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < chs->history.count);
    return &chs->history.entries[index];
}

int DP_canvas_history_snapshot_fork_start(DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    return chs->fork.start;
}

int DP_canvas_history_snapshot_fork_fallbehind(DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    return chs->fork.fallbehind;
}

int DP_canvas_history_snapshot_fork_count(DP_CanvasHistorySnapshot *chs)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    return chs->fork.count;
}

const DP_ForkEntry *
DP_canvas_history_snapshot_fork_entry_at(DP_CanvasHistorySnapshot *chs,
                                         int index)
{
    DP_ASSERT(chs);
    DP_ASSERT(DP_atomic_get(&chs->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < chs->fork.count);
    return &chs->fork.entries[index];
}
