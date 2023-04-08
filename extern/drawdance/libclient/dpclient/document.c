/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "document.h"
#include <dpcommon/common.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpengine/canvas_history.h>
#include <dpengine/draw_context.h>
#include <dpmsg/message.h>
#include <dpmsg/message_queue.h>

typedef struct DP_Message DP_Message;


#define INITIAL_CAPACITY 64

struct DP_Document {
    size_t title_length;
    char *title;
    bool running;
    DP_Queue queue;
    DP_DrawContext *draw_context;
    DP_CanvasHistory *canvas_history;
    DP_Mutex *mutex_queue;
    DP_Semaphore *sem_queue_ready;
    DP_Thread *dequeue_thread;
};

static void run_command_thread(void *data)
{
    DP_Document *doc = data;
    DP_Queue *queue = &doc->queue;
    DP_DrawContext *dc = doc->draw_context;
    DP_CanvasHistory *ch = doc->canvas_history;
    DP_Mutex *mutex_queue = doc->mutex_queue;
    DP_Semaphore *sem_queue_ready = doc->sem_queue_ready;
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem_queue_ready);
        if (doc->running) {
            DP_MUTEX_MUST_LOCK(mutex_queue);
            DP_Message *msg = DP_message_queue_shift(queue);
            DP_MUTEX_MUST_UNLOCK(mutex_queue);
            if (msg) {
                if (!DP_canvas_history_handle(ch, dc, msg)) {
                    DP_warn("Error handling drawing command: %s", DP_error());
                }
                DP_message_decref(msg);
            }
            else {
                DP_warn("Dequeue got NULL message");
                DP_ASSERT(false); // Shouldn't happen.
            }
        }
        else {
            break;
        }
    }
}

DP_Document *DP_document_new(void)
{
    DP_Document *doc = DP_malloc(sizeof(*doc));
    *doc = (DP_Document){0,    NULL, true, DP_QUEUE_NULL, NULL,
                         NULL, NULL, NULL, NULL};
    DP_message_queue_init(&doc->queue, INITIAL_CAPACITY);
    if (!(doc->draw_context = DP_draw_context_new())) {
        DP_document_free(doc);
        return NULL;
    }
    if (!(doc->canvas_history = DP_canvas_history_new(NULL, NULL, false, NULL))) {
        DP_document_free(doc);
        return NULL;
    }
    if (!(doc->mutex_queue = DP_mutex_new())) {
        DP_document_free(doc);
        return NULL;
    }
    if (!(doc->sem_queue_ready = DP_semaphore_new(0))) {
        DP_document_free(doc);
        return NULL;
    }
    if (!(doc->dequeue_thread = DP_thread_new(run_command_thread, doc))) {
        DP_document_free(doc);
        return NULL;
    }
    return doc;
}

void DP_document_free(DP_Document *doc)
{
    if (doc) {
        doc->running = false;
        if (doc->dequeue_thread) {
            DP_SEMAPHORE_MUST_POST(doc->sem_queue_ready);
            DP_thread_free_join(doc->dequeue_thread);
        }
        if (doc->sem_queue_ready) {
            DP_semaphore_free(doc->sem_queue_ready);
        }
        if (doc->mutex_queue) {
            DP_mutex_free(doc->mutex_queue);
        }
        DP_message_queue_dispose(&doc->queue);
        DP_canvas_history_free(doc->canvas_history);
        DP_draw_context_free(doc->draw_context);
        DP_free(doc->title);
        DP_free(doc);
    }
}


const char *DP_document_title(DP_Document *doc, size_t *out_length)
{
    DP_ASSERT(doc);
    if (out_length) {
        *out_length = doc->title_length;
    }
    return doc->title;
}

DP_Queue *DP_document_queue(DP_Document *doc)
{
    DP_ASSERT(doc);
    return &doc->queue;
}

DP_CanvasState *DP_document_canvas_state_compare_and_get(DP_Document *doc,
                                                         DP_CanvasState *prev)
{
    DP_ASSERT(doc);
    return DP_canvas_history_compare_and_get(doc->canvas_history, prev, NULL);
}

void DP_document_command_push_noinc(DP_Document *doc, DP_Message *msg)
{
    DP_ASSERT(doc);
    DP_ASSERT(msg);
    DP_Mutex *mutex_queue = doc->mutex_queue;
    DP_MUTEX_MUST_LOCK(mutex_queue);
    DP_message_queue_push_noinc(&doc->queue, msg);
    DP_MUTEX_MUST_UNLOCK(mutex_queue);
    DP_SEMAPHORE_MUST_POST(doc->sem_queue_ready);
}

void DP_document_command_push_inc(DP_Document *doc, DP_Message *msg)
{
    DP_ASSERT(doc);
    DP_ASSERT(msg);
    DP_document_command_push_noinc(doc, DP_message_incref(msg));
}
