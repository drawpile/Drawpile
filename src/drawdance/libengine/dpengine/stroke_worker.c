// SPDX-License-Identifier: GPL-3.0-or-later
#include "stroke_worker.h"
#include "brush.h"
#include "brush_engine.h"
#include "canvas_state.h"
#include <dpcommon/common.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>

struct DP_StrokeWorker {
    DP_Semaphore *sem;
    DP_Mutex *mutex;
    DP_Thread *thread;
    DP_Queue queue;
    DP_BrushEngine *be;
};

typedef enum DP_StrokeWorkerJobType {
    DP_STROKE_WORKER_JOB_NONE,
    DP_STROKE_WORKER_JOB_SIZE_LIMIT_SET,
    DP_STROKE_WORKER_JOB_CLASSIC_BRUSH_SET,
    DP_STROKE_WORKER_JOB_MYPAINT_BRUSH_SET,
    DP_STROKE_WORKER_JOB_DABS_FLUSH,
    DP_STROKE_WORKER_JOB_MESSAGE_PUSH,
    DP_STROKE_WORKER_JOB_STROKE_BEGIN,
    DP_STROKE_WORKER_JOB_STROKE_TO,
    DP_STROKE_WORKER_JOB_POLL,
    DP_STROKE_WORKER_JOB_STROKE_END,
    DP_STROKE_WORKER_JOB_OFFSET_ADD,
    DP_STROKE_WORKER_JOB_CANCEL,
} DP_StrokeWorkerJobType;

typedef struct DP_StrokeWorkerJobClassic {
    DP_ClassicBrush brush;
    DP_BrushEngineStrokeParams besp;
    DP_UPixelFloat color_override;
    bool have_color_override;
    bool eraser_override;
} DP_StrokeWorkerJobClassic;

typedef struct DP_StrokeWorkerJobMyPaint {
    DP_MyPaintBrush brush;
    DP_MyPaintSettings settings;
    DP_BrushEngineStrokeParams besp;
    DP_UPixelFloat color_override;
    bool have_color_override;
    bool eraser_override;
} DP_StrokeWorkerJobMyPaint;

typedef struct DP_StrokeWorkerJob {
    DP_StrokeWorkerJobType type;
    union {
        int size_limit;
        DP_StrokeWorkerJobClassic *classic;
        DP_StrokeWorkerJobMyPaint *mypaint;
        DP_Message *msg;
        struct {
            unsigned int context_id;
            bool compatibility_mode;
            bool push_undo_point;
            bool mirror;
            bool flip;
            float zoom;
            float angle;
        } stroke_begin;
        struct {
            DP_BrushPoint bp;
        } stroke_to;
        struct {
            long long time_msec;
        } poll;
        struct {
            long long time_msec;
            bool push_pen_up;
        } stroke_end;
        struct {
            float x;
            float y;
        } offset_add;
        struct {
            long long time_msec;
            bool push_pen_up;
        } cancel;
    };
} DP_StrokeWorkerJob;


static bool shift_job(DP_Mutex *queue_mutex, DP_Queue *queue,
                      DP_StrokeWorkerJob *out_job)
{
    DP_MUTEX_MUST_LOCK(queue_mutex);
    DP_StrokeWorkerJob *job = DP_queue_peek(queue, sizeof(*out_job));
    if (job) {
        *out_job = *job;
        DP_queue_shift(queue);
    }
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
    return job;
}

static void run_stroke_worker_thread(void *data)
{
    DP_StrokeWorker *sw = data;
    DP_Semaphore *sem = sw->sem;
    DP_Mutex *mutex = sw->mutex;
    DP_Queue *queue = &sw->queue;
    bool stroking = false;
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem);
        DP_StrokeWorkerJob job;
        if (shift_job(mutex, queue, &job)) {
            switch (job.type) {
            case DP_STROKE_WORKER_JOB_NONE: // Job got cancelled.
                continue;
            case DP_STROKE_WORKER_JOB_SIZE_LIMIT_SET:
                DP_brush_engine_size_limit_set(sw->be, job.size_limit);
                continue;
            case DP_STROKE_WORKER_JOB_CLASSIC_BRUSH_SET:
                DP_ASSERT(!stroking);
                DP_brush_engine_classic_brush_set(
                    sw->be, &job.classic->brush, &job.classic->besp,
                    job.classic->have_color_override
                        ? &job.classic->color_override
                        : NULL,
                    job.classic->eraser_override);
                DP_free(job.classic);
                continue;
            case DP_STROKE_WORKER_JOB_MYPAINT_BRUSH_SET:
                DP_ASSERT(!stroking);
                DP_brush_engine_mypaint_brush_set(
                    sw->be, &job.mypaint->brush, &job.mypaint->settings,
                    &job.mypaint->besp,
                    job.mypaint->have_color_override
                        ? &job.mypaint->color_override
                        : NULL,
                    job.mypaint->eraser_override);
                DP_free(job.mypaint);
                continue;
            case DP_STROKE_WORKER_JOB_DABS_FLUSH:
                DP_brush_engine_dabs_flush(sw->be);
                continue;
            case DP_STROKE_WORKER_JOB_MESSAGE_PUSH:
                DP_brush_engine_message_push_noinc(sw->be, job.msg);
                continue;
            case DP_STROKE_WORKER_JOB_STROKE_BEGIN:
                stroking = true;
                DP_brush_engine_stroke_begin(
                    sw->be, NULL, job.stroke_begin.context_id,
                    job.stroke_begin.compatibility_mode,
                    job.stroke_begin.push_undo_point, job.stroke_begin.mirror,
                    job.stroke_begin.flip, job.stroke_begin.zoom,
                    job.stroke_begin.angle);
                continue;
            case DP_STROKE_WORKER_JOB_STROKE_TO:
                DP_brush_engine_stroke_to(sw->be, job.stroke_to.bp, NULL);
                continue;
            case DP_STROKE_WORKER_JOB_POLL:
                DP_brush_engine_poll(sw->be, job.poll.time_msec, NULL);
                continue;
            case DP_STROKE_WORKER_JOB_STROKE_END:
                stroking = false;
                DP_brush_engine_stroke_end(sw->be, job.stroke_end.time_msec,
                                           NULL, job.stroke_end.push_pen_up);
                continue;
            case DP_STROKE_WORKER_JOB_OFFSET_ADD:
                DP_brush_engine_offset_add(sw->be, job.offset_add.x,
                                           job.offset_add.y);
                continue;
            case DP_STROKE_WORKER_JOB_CANCEL:
                if (stroking) {
                    stroking = false;
                    DP_brush_engine_stroke_end(sw->be, job.cancel.time_msec,
                                               NULL, job.cancel.push_pen_up);
                }
                continue;
            }
            DP_UNREACHABLE();
        }
        else {
            break;
        }
    }
}


static void push_job(DP_StrokeWorker *sw, DP_StrokeWorkerJob job)
{
    DP_ASSERT(sw->thread);
    DP_ASSERT(sw->sem);
    DP_ASSERT(sw->mutex);
    DP_Mutex *mutex = sw->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    *(DP_StrokeWorkerJob *)DP_queue_push(&sw->queue, sizeof(job)) = job;
    DP_MUTEX_MUST_UNLOCK(mutex);
    DP_SEMAPHORE_MUST_POST(sw->sem);
}


DP_StrokeWorker *DP_stroke_worker_new(DP_BrushEngine *be)
{
    DP_StrokeWorker *sw = DP_malloc(sizeof(*sw));
    *sw = (DP_StrokeWorker){NULL, NULL, NULL, DP_QUEUE_NULL, be};
    DP_queue_init(&sw->queue, 64, sizeof(DP_StrokeWorkerJob));
    return sw;
}

void DP_stroke_worker_free(DP_StrokeWorker *sw)
{
    if (sw) {
        if (sw->thread) {
            DP_SEMAPHORE_MUST_POST(sw->sem);
            DP_thread_free_join(sw->thread);
            DP_mutex_free(sw->mutex);
            DP_semaphore_free(sw->sem);
        }
        DP_queue_dispose(&sw->queue);
        DP_brush_engine_free(sw->be);
        DP_free(sw);
    }
}

bool DP_stroke_worker_thread_active(DP_StrokeWorker *sw)
{
    DP_ASSERT(sw);
    return sw->thread != NULL;
}

bool DP_stroke_worker_thread_start(DP_StrokeWorker *sw)
{
    DP_ASSERT(sw);
    if (!sw->thread) {
        DP_ASSERT(!sw->sem);
        DP_ASSERT(!sw->mutex);
        DP_debug("Start stroke worker thread");

        sw->sem = DP_semaphore_new(0);
        if (!sw->sem) {
            return false;
        }

        sw->mutex = DP_mutex_new();
        if (!sw->mutex) {
            DP_semaphore_free(sw->sem);
            sw->sem = NULL;
            return false;
        }

        sw->thread = DP_thread_new(run_stroke_worker_thread, sw);
        if (!sw->thread) {
            DP_mutex_free(sw->mutex);
            sw->mutex = NULL;
            DP_semaphore_free(sw->sem);
            sw->sem = NULL;
            return false;
        }
    }
    return true;
}

bool DP_stroke_worker_thread_finish(DP_StrokeWorker *sw)
{
    DP_ASSERT(sw);
    if (sw->thread) {
        DP_ASSERT(sw->sem);
        DP_ASSERT(sw->mutex);
        DP_debug("Finish stroke worker thread");

        DP_SEMAPHORE_MUST_POST(sw->sem);
        DP_thread_free_join(sw->thread);
        sw->thread = NULL;

        DP_mutex_free(sw->mutex);
        sw->mutex = NULL;

        DP_semaphore_free(sw->sem);
        sw->sem = NULL;

        return true;
    }
    else {
        return false;
    }
}

void DP_stroke_worker_size_limit_set(DP_StrokeWorker *sw, int size_limit)
{
    DP_ASSERT(sw);
    if (sw->thread) {
        push_job(sw, (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_SIZE_LIMIT_SET,
                                          {.size_limit = size_limit}});
    }
    else {
        DP_brush_engine_size_limit_set(sw->be, size_limit);
    }
}

static void start_or_stop_thread(DP_StrokeWorker *sw, bool sync_samples)
{
    if (sync_samples) {
        if (!sw->thread) {
            bool started = DP_stroke_worker_thread_start(sw);
            if (!started) {
                DP_warn("Error starting stroke worker thread: %s", DP_error());
            }
        }
    }
    else if (sw->thread) {
        DP_stroke_worker_thread_finish(sw);
    }
}

void DP_stroke_worker_classic_brush_set(DP_StrokeWorker *sw,
                                        const DP_ClassicBrush *brush,
                                        const DP_BrushEngineStrokeParams *besp,
                                        const DP_UPixelFloat *color_override,
                                        bool eraser_override)
{
    DP_ASSERT(sw);
    DP_ASSERT(brush);
    DP_ASSERT(besp);

    start_or_stop_thread(sw, besp->sync_samples);

    if (sw->thread) {
        DP_StrokeWorkerJobClassic *classic = DP_malloc(sizeof(*classic));
        *classic = (DP_StrokeWorkerJobClassic){
            *brush,
            *besp,
            color_override ? *color_override : DP_upixel_float_zero(),
            color_override,
            eraser_override,
        };
        push_job(sw,
                 (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_CLASSIC_BRUSH_SET,
                                      .classic = classic});
    }
    else {
        DP_brush_engine_classic_brush_set(sw->be, brush, besp, color_override,
                                          eraser_override);
    }
}

void DP_stroke_worker_mypaint_brush_set(DP_StrokeWorker *sw,
                                        const DP_MyPaintBrush *brush,
                                        const DP_MyPaintSettings *settings,
                                        const DP_BrushEngineStrokeParams *besp,
                                        const DP_UPixelFloat *color_override,
                                        bool eraser_override)
{
    DP_ASSERT(sw);
    DP_ASSERT(brush);
    DP_ASSERT(settings);
    DP_ASSERT(besp);

    start_or_stop_thread(sw, besp->sync_samples);

    if (sw->thread) {
        DP_StrokeWorkerJobMyPaint *mypaint = DP_malloc(sizeof(*mypaint));
        *mypaint = (DP_StrokeWorkerJobMyPaint){
            *brush,
            *settings,
            *besp,
            color_override ? *color_override : DP_upixel_float_zero(),
            color_override,
            eraser_override,
        };
        push_job(sw,
                 (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_MYPAINT_BRUSH_SET,
                                      .mypaint = mypaint});
    }
    else {
        DP_brush_engine_mypaint_brush_set(sw->be, brush, settings, besp,
                                          color_override, eraser_override);
    }
}

void DP_stroke_worker_dabs_flush(DP_StrokeWorker *sw)
{
    DP_ASSERT(sw);
    if (sw->thread) {
        push_job(sw,
                 (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_DABS_FLUSH, {0}});
    }
    else {
        DP_brush_engine_dabs_flush(sw->be);
    }
}

void DP_stroke_worker_message_push_noinc(DP_StrokeWorker *sw, DP_Message *msg)
{
    DP_ASSERT(sw);
    DP_ASSERT(msg);
    if (sw->thread) {
        push_job(sw, (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_MESSAGE_PUSH,
                                          .msg = msg});
    }
    else {
        DP_brush_engine_message_push_noinc(sw->be, msg);
    }
}

void DP_stroke_worker_stroke_begin(DP_StrokeWorker *sw,
                                   DP_CanvasState *cs_or_null,
                                   unsigned int context_id,
                                   bool compatibility_mode,
                                   bool push_undo_point, bool mirror, bool flip,
                                   float zoom, float angle)
{
    DP_ASSERT(sw);
    if (sw->thread) {
        push_job(sw, (DP_StrokeWorkerJob){
                         DP_STROKE_WORKER_JOB_STROKE_BEGIN,
                         {.stroke_begin = {context_id, compatibility_mode,
                                           push_undo_point, mirror, flip, zoom,
                                           angle}}});
    }
    else {
        DP_brush_engine_stroke_begin(sw->be, cs_or_null, context_id,
                                     compatibility_mode, push_undo_point,
                                     mirror, flip, zoom, angle);
    }
}

void DP_stroke_worker_stroke_to(DP_StrokeWorker *sw, const DP_BrushPoint *bp,
                                DP_CanvasState *cs_or_null)
{
    DP_ASSERT(sw);
    DP_ASSERT(bp);
    if (sw->thread) {
        push_job(sw, (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_STROKE_TO,
                                          {.stroke_to = {*bp}}});
    }
    else {
        DP_brush_engine_stroke_to(sw->be, *bp, cs_or_null);
    }
}

void DP_stroke_worker_poll(DP_StrokeWorker *sw, long long time_msec,
                           DP_CanvasState *cs_or_null)
{
    DP_ASSERT(sw);
    if (sw->thread) {
        push_job(sw, (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_POLL,
                                          {.poll = {time_msec}}});
    }
    else {
        DP_brush_engine_poll(sw->be, time_msec, cs_or_null);
    }
}

void DP_stroke_worker_stroke_end(DP_StrokeWorker *sw, long long time_msec,
                                 DP_CanvasState *cs_or_null, bool push_pen_up)
{
    DP_ASSERT(sw);
    if (sw->thread) {
        push_job(
            sw, (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_STROKE_END,
                                     {.stroke_end = {time_msec, push_pen_up}}});
    }
    else {
        DP_brush_engine_stroke_end(sw->be, time_msec, cs_or_null, push_pen_up);
    }
}

static void cancel_job(void *element, DP_UNUSED void *user)
{
    DP_StrokeWorkerJob *job = element;
    switch (job->type) {
    case DP_STROKE_WORKER_JOB_MESSAGE_PUSH:
        return; // Keep this message.
    case DP_STROKE_WORKER_JOB_CLASSIC_BRUSH_SET:
        DP_free(job->classic);
        break;
    case DP_STROKE_WORKER_JOB_MYPAINT_BRUSH_SET:
        DP_free(job->mypaint);
        break;
    default:
        break;
    }
    job->type = DP_STROKE_WORKER_JOB_NONE;
}

void DP_stroke_worker_stroke_cancel(DP_StrokeWorker *sw, long long time_msec,
                                    bool push_pen_up)
{
    DP_ASSERT(sw);
    if (sw->thread) {
        DP_ASSERT(sw->sem);
        DP_ASSERT(sw->mutex);
        DP_Mutex *mutex = sw->mutex;
        DP_MUTEX_MUST_LOCK(mutex);
        DP_queue_each(&sw->queue, sizeof(DP_StrokeWorkerJob), cancel_job, NULL);
        *(DP_StrokeWorkerJob *)DP_queue_push(
            &sw->queue, sizeof(DP_StrokeWorkerJob)) = (DP_StrokeWorkerJob){
            DP_STROKE_WORKER_JOB_CANCEL, {.cancel = {time_msec, push_pen_up}}};
        DP_MUTEX_MUST_UNLOCK(mutex);
        DP_SEMAPHORE_MUST_POST(sw->sem);
    }
}

void DP_stroke_worker_offset_add(DP_StrokeWorker *sw, float x, float y)
{
    DP_ASSERT(sw);
    if (sw->thread) {
        push_job(sw, (DP_StrokeWorkerJob){DP_STROKE_WORKER_JOB_OFFSET_ADD,
                                          {.offset_add = {x, y}}});
    }
    else {
        DP_brush_engine_offset_add(sw->be, x, y);
    }
}
