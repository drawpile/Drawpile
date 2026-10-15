// SPDX-License-Identifier: GPL-3.0-or-later
#include "liquify_worker.h"
#include "brush_engine.h"
#include "liquify.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <math.h>
#include <helpers.h>


struct DP_LiquifyWorker {
    DP_Semaphore *sem;
    DP_Mutex *mutex;
    DP_Thread *thread;
    DP_Queue queue;
    DP_Queue undo_stack;
    int undo_depth;
    int undone;
    DP_Liquify *l;
    DP_DrawContext *dc;
    DP_LiquifyEngine *le;
    DP_LiquifyWorkerOperationParams lwop;
    DP_LiquifyWorkerCallbackFn fn;
    void *user;
    float last_x;
    float last_y;
    bool stroking;
    bool have_last;
    bool have_changes;
};

typedef enum DP_LiquifyWorkerJobType {
    DP_LIQUIFY_WORKER_JOB_NONE,
    DP_LIQUIFY_WORKER_JOB_OPERATION_PARAMS_SET,
    DP_LIQUIFY_WORKER_JOB_STROKE_PARAMS_SET,
    DP_LIQUIFY_WORKER_JOB_STROKE_BEGIN,
    DP_LIQUIFY_WORKER_JOB_STROKE_TO,
    DP_LIQUIFY_WORKER_JOB_STROKE_END,
    DP_LIQUIFY_WORKER_JOB_STROKE_CANCEL,
    DP_LIQUIFY_WORKER_JOB_UNDO,
    DP_LIQUIFY_WORKER_JOB_REDO,
    DP_LIQUIFY_WORKER_JOB_UPDATE_REQUEST,
} DP_LiquifyWorkerJobType;

typedef struct DP_LiquifyWorkerJob {
    DP_LiquifyWorkerJobType type;
    union {
        DP_Liquify *l;
        DP_LiquifyWorkerOperationParams lwop;
        DP_LiquifyEngineStrokeParams lesp;
        float zoom;
        long long time_msec;
        struct {
            DP_BrushPoint bp;
            unsigned int flags;
        } stroke_to;
    };
} DP_LiquifyWorkerJob;


static void emit_event(DP_LiquifyWorker *lw, DP_LiquifyWorkerEventType type)
{
    DP_LiquifyWorkerEvent event;
    event.type = type;
    lw->fn(lw->user, &event);
}


static DP_LiquifyState *undo_stack_top(DP_LiquifyWorker *lw)
{
    DP_LiquifyState **pp =
        DP_queue_peek_last(&lw->undo_stack, sizeof(DP_LiquifyState *));
    DP_ASSERT(pp);
    return *pp;
}

static DP_LiquifyState *undo_stack_bottom(DP_LiquifyWorker *lw)
{
    DP_LiquifyState **pp =
        DP_queue_peek(&lw->undo_stack, sizeof(DP_LiquifyState *));
    DP_ASSERT(pp);
    return *pp;
}

static DP_LiquifyState *undo_stack_current(DP_LiquifyWorker *lw)
{
    DP_LiquifyState **pp = DP_queue_at(
        &lw->undo_stack, sizeof(DP_LiquifyState *),
        lw->undo_stack.used - DP_int_to_size(lw->undone) - (size_t)1);
    DP_ASSERT(pp);
    return *pp;
}

static void undo_stack_push(DP_LiquifyWorker *lw)
{
    while (lw->undone != 0) {
        DP_liquify_state_decref(undo_stack_top(lw));
        DP_queue_pop(&lw->undo_stack);
        --lw->undone;
    }

    if (DP_size_to_int(lw->undo_stack.used) == lw->undo_depth) {
        DP_liquify_state_decref(undo_stack_bottom(lw));
        DP_queue_shift(&lw->undo_stack);
    }

    DP_LiquifyState **pp =
        DP_queue_push(&lw->undo_stack, sizeof(DP_LiquifyState *));
    *pp = DP_liquify_current_state_inc(lw->l);
}

static void undo_stack_element_dispose(void *element)
{
    DP_liquify_state_decref(*(DP_LiquifyState **)element);
}

static void undo_stack_clear(DP_LiquifyWorker *lw)
{
    DP_queue_clear(&lw->undo_stack, sizeof(DP_LiquifyState *),
                   undo_stack_element_dispose);
    lw->undone = 0;
}


static void
handle_operation_params_set(DP_LiquifyWorker *lw,
                            const DP_LiquifyWorkerOperationParams *lwop)
{
    lw->lwop = *lwop;
}

static void handle_stroke_params_set(DP_LiquifyWorker *lw,
                                     const DP_LiquifyEngineStrokeParams *lesp)
{
    DP_liquify_engine_params_set(lw->le, lesp);
}

static void handle_stroke_begin(DP_LiquifyWorker *lw, float zoom)
{
    lw->have_last = false;
    lw->have_changes = false;
    lw->stroking = true;
    DP_liquify_engine_stroke_begin(lw->le, zoom);
}

static void handle_stroke_to(DP_LiquifyWorker *lw, DP_BrushPoint bp,
                             unsigned int flags)
{
    DP_liquify_engine_stroke_to(lw->le, bp, flags);
}

static void handle_stroke_end(DP_LiquifyWorker *lw, long long time_msec)
{
    DP_liquify_engine_stroke_end(lw->le, time_msec);
    lw->stroking = false;
    if (lw->have_changes) {
        undo_stack_push(lw);
    }
}

static void handle_stroke_cancel(DP_LiquifyWorker *lw, long long time_msec)
{
    if (lw->stroking) {
        handle_stroke_end(lw, time_msec);
    }
}

static void handle_undo(DP_LiquifyWorker *lw)
{
    int next_undone = lw->undone + 1;
    if (DP_int_to_size(next_undone) < lw->undo_stack.used) {
        lw->undone = next_undone;
        DP_liquify_current_state_set_inc(lw->l, undo_stack_current(lw));
        emit_event(lw, DP_LIQUIFY_WORKER_EVENT_UPDATE);
    }
    else {
        emit_event(lw, DP_LIQUIFY_WORKER_EVENT_UNDO_LIMIT_HIT);
    }
}

static void handle_redo(DP_LiquifyWorker *lw)
{
    if (lw->undone > 0) {
        --lw->undone;
        DP_liquify_current_state_set_inc(lw->l, undo_stack_current(lw));
        emit_event(lw, DP_LIQUIFY_WORKER_EVENT_UPDATE);
    }
}

static void handle_update_request(DP_LiquifyWorker *lw)
{
    emit_event(lw, DP_LIQUIFY_WORKER_EVENT_UPDATE);
}


static bool shift_job(DP_Mutex *queue_mutex, DP_Queue *queue,
                      DP_LiquifyWorkerJob *out_job)
{
    DP_MUTEX_MUST_LOCK(queue_mutex);
    DP_LiquifyWorkerJob *job = DP_queue_peek(queue, sizeof(*out_job));
    if (job) {
        *out_job = *job;
        DP_queue_shift(queue);
    }
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
    return job;
}

static void run_liquify_worker_thread(void *data)
{
    DP_LiquifyWorker *lw = data;
    DP_Semaphore *sem = lw->sem;
    DP_Mutex *mutex = lw->mutex;
    DP_Queue *queue = &lw->queue;
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem);
        DP_LiquifyWorkerJob job;
        if (shift_job(mutex, queue, &job)) {
            switch (job.type) {
            case DP_LIQUIFY_WORKER_JOB_NONE:
                continue;
            case DP_LIQUIFY_WORKER_JOB_OPERATION_PARAMS_SET:
                handle_operation_params_set(lw, &job.lwop);
                continue;
            case DP_LIQUIFY_WORKER_JOB_STROKE_PARAMS_SET:
                handle_stroke_params_set(lw, &job.lesp);
                continue;
            case DP_LIQUIFY_WORKER_JOB_STROKE_BEGIN:
                handle_stroke_begin(lw, job.zoom);
                continue;
            case DP_LIQUIFY_WORKER_JOB_STROKE_TO:
                handle_stroke_to(lw, job.stroke_to.bp, job.stroke_to.flags);
                continue;
            case DP_LIQUIFY_WORKER_JOB_STROKE_END:
                handle_stroke_end(lw, job.time_msec);
                continue;
            case DP_LIQUIFY_WORKER_JOB_UNDO:
                handle_undo(lw);
                continue;
            case DP_LIQUIFY_WORKER_JOB_REDO:
                handle_redo(lw);
                continue;
            case DP_LIQUIFY_WORKER_JOB_UPDATE_REQUEST:
                handle_update_request(lw);
                continue;
            case DP_LIQUIFY_WORKER_JOB_STROKE_CANCEL:
                handle_stroke_cancel(lw, job.time_msec);
                continue;
            }
            DP_UNREACHABLE();
        }
        else {
            break;
        }
    }
}


static void push_job(DP_LiquifyWorker *lw, DP_LiquifyWorkerJob job)
{
    DP_ASSERT(lw->thread);
    DP_ASSERT(lw->sem);
    DP_ASSERT(lw->mutex);
    DP_Mutex *mutex = lw->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    *(DP_LiquifyWorkerJob *)DP_queue_push(&lw->queue, sizeof(job)) = job;
    DP_MUTEX_MUST_UNLOCK(mutex);
    DP_SEMAPHORE_MUST_POST(lw->sem);
}

static float effective_amount(DP_LiquifyWorker *lw, DP_LiquifyEngineDab dab,
                              float multiplier)
{
    bool reverse = lw->lwop.reverse;
    bool constrain = dab.flags & DP_LIQUIFY_WORKER_FLAG_CONSTRAIN;
    return dab.amount * ((reverse ^ constrain) ? -multiplier : multiplier);
}

static bool handle_dab(DP_LiquifyWorker *lw, DP_LiquifyEngineDab dab)
{
    DP_LiquifyOpParams params;
    params.type = (DP_LiquifyOpType)lw->lwop.type;
    switch (params.type) {
    case DP_LIQUIFY_OP_TYPE_MOVE:
        if (lw->have_last) {
            float amount = effective_amount(lw, dab, 1.0f);
            float offset_length =
                DP_hypotf(dab.x - lw->last_x, dab.y - lw->last_y);
            float offset_angle = dab.direction_rad;
            if (dab.flags & DP_LIQUIFY_WORKER_FLAG_CENTER) {
                offset_angle += (float)M_PI / 2.0f;
            }
            params.move = (DP_LiquifyOpMoveParams){
                amount,
                cosf(offset_angle) * offset_length,
                sinf(offset_angle) * offset_length,
            };
            break;
        }
        else {
            return false; // Can't move without any distance.
        }
    case DP_LIQUIFY_OP_TYPE_SCALE: {
        float amount = effective_amount(lw, dab, 0.1f);
        params.scale = (DP_LiquifyOpScaleParams){amount};
        break;
    }
    case DP_LIQUIFY_OP_TYPE_ROTATE: {
        float angle = effective_amount(lw, dab, (float)M_PI * 0.1f);
        params.rotate = (DP_LiquifyOpRotateParams){angle};
        break;
    }
    case DP_LIQUIFY_OP_TYPE_SMOOTHE:
        params.smoothe = (DP_LiquifyOpSmootheParams){
            dab.amount, lw->lwop.smoothe_kernel_radius};
        break;
    case DP_LIQUIFY_OP_TYPE_ERASE:
        params.erase = (DP_LiquifyOpEraseParams){dab.amount};
        break;
    default:
        DP_UNREACHABLE();
    }

    params.x = dab.x;
    params.y = dab.y;
    params.radius = dab.size / 2.0f;
    params.hardness = dab.hardness;
    return DP_liquify_op(lw->l, lw->dc, &params);
}

static void push_dab(void *user, DP_LiquifyEngineDab dab)
{
    DP_LiquifyWorker *lw = user;
    if (handle_dab(user, dab)) {
        lw->have_changes = true;
        emit_event(lw, DP_LIQUIFY_WORKER_EVENT_UPDATE);
    }
    lw->last_x = dab.x;
    lw->last_y = dab.y;
    lw->have_last = true;
}

DP_LiquifyWorker *DP_liquify_worker_new_inc(DP_Liquify *l, DP_DrawContext *dc,
                                            int undo_depth,
                                            DP_LiquifyWorkerCallbackFn fn,
                                            void *user)
{
    DP_ASSERT(l);
    DP_ASSERT(dc);
    DP_ASSERT(fn);
    DP_ASSERT(undo_depth > 0);
    DP_LiquifyWorker *lw = DP_malloc(sizeof(*lw));
    *lw = (DP_LiquifyWorker){
        NULL,
        NULL,
        NULL,
        DP_QUEUE_NULL,
        DP_QUEUE_NULL,
        undo_depth,
        0,
        DP_liquify_incref(l),
        dc,
        NULL,
        {DP_LIQUIFY_OP_TYPE_MOVE, {false}},
        fn,
        user,
        0.0f,
        0.0f,
        false,
        false,
        false,
    };
    lw->le = DP_liquify_engine_new(push_dab, lw);
    DP_queue_init(&lw->queue, 64, sizeof(DP_LiquifyWorkerJob));
    DP_queue_init(&lw->undo_stack, DP_int_to_size(undo_depth),
                  sizeof(DP_LiquifyState *));
    undo_stack_push(lw);
    return lw;
}

void DP_liquify_worker_free(DP_LiquifyWorker *lw)
{
    if (lw) {
        if (lw->thread) {
            DP_SEMAPHORE_MUST_POST(lw->sem);
            DP_thread_free_join(lw->thread);
            DP_mutex_free(lw->mutex);
            DP_semaphore_free(lw->sem);
        }
        DP_queue_dispose(&lw->queue);
        DP_liquify_engine_free(lw->le);
        undo_stack_clear(lw);
        DP_queue_dispose(&lw->undo_stack);
        DP_liquify_decref(lw->l);
        DP_free(lw);
    }
}

DP_Liquify *DP_liquify_worker_liquify_noinc(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    return lw->l;
}

DP_DrawContext *DP_liquify_worker_draw_context(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    return lw->dc;
}

bool DP_liquify_worker_thread_active(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    return lw->thread != NULL;
}

bool DP_liquify_worker_thread_start(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    if (!lw->thread) {
        DP_ASSERT(!lw->sem);
        DP_ASSERT(!lw->mutex);

        lw->sem = DP_semaphore_new(0);
        if (!lw->sem) {
            return false;
        }

        lw->mutex = DP_mutex_new();
        if (!lw->mutex) {
            DP_semaphore_free(lw->sem);
            lw->sem = NULL;
            return false;
        }

        lw->thread = DP_thread_new(run_liquify_worker_thread, lw);
        if (!lw->thread) {
            DP_mutex_free(lw->mutex);
            lw->mutex = NULL;
            DP_semaphore_free(lw->sem);
            lw->sem = NULL;
            return false;
        }
    }
    return true;
}

bool DP_liquify_worker_thread_finish(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    if (lw->thread) {
        DP_ASSERT(lw->sem);
        DP_ASSERT(lw->mutex);

        DP_SEMAPHORE_MUST_POST(lw->sem);
        DP_thread_free_join(lw->thread);
        lw->thread = NULL;

        DP_mutex_free(lw->mutex);
        lw->mutex = NULL;

        DP_semaphore_free(lw->sem);
        lw->sem = NULL;

        return true;
    }
    else {
        return false;
    }
}

void DP_liquify_worker_operation_params_set(
    DP_LiquifyWorker *lw, const DP_LiquifyWorkerOperationParams *lwop)
{
    DP_ASSERT(lw);
    DP_ASSERT(lwop);
    if (lw->thread) {
        push_job(
            lw, (DP_LiquifyWorkerJob){
                    DP_LIQUIFY_WORKER_JOB_OPERATION_PARAMS_SET, .lwop = *lwop});
    }
    else {
        handle_operation_params_set(lw, lwop);
    }
}

void DP_liquify_worker_stroke_params_set(
    DP_LiquifyWorker *lw, const DP_LiquifyEngineStrokeParams *lesp)
{
    DP_ASSERT(lw);
    DP_ASSERT(lesp);
    if (lw->thread) {
        push_job(lw,
                 (DP_LiquifyWorkerJob){DP_LIQUIFY_WORKER_JOB_STROKE_PARAMS_SET,
                                       .lesp = *lesp});
    }
    else {
        handle_stroke_params_set(lw, lesp);
    }
}

void DP_liquify_worker_stroke_begin(DP_LiquifyWorker *lw, float zoom)
{
    DP_ASSERT(lw);
    if (lw->thread) {
        push_job(lw, (DP_LiquifyWorkerJob){DP_LIQUIFY_WORKER_JOB_STROKE_BEGIN,
                                           .zoom = zoom});
    }
    else {
        handle_stroke_begin(lw, zoom);
    }
}

void DP_liquify_worker_stroke_to(DP_LiquifyWorker *lw, const DP_BrushPoint *bp,
                                 unsigned int flags)
{
    DP_ASSERT(lw);
    DP_ASSERT(bp);
    if (lw->thread) {
        push_job(lw, (DP_LiquifyWorkerJob){DP_LIQUIFY_WORKER_JOB_STROKE_TO,
                                           .stroke_to = {*bp, flags}});
    }
    else {
        handle_stroke_to(lw, *bp, flags);
    }
}

void DP_liquify_worker_stroke_end(DP_LiquifyWorker *lw, long long time_msec)
{
    DP_ASSERT(lw);
    if (lw->thread) {
        push_job(lw, (DP_LiquifyWorkerJob){DP_LIQUIFY_WORKER_JOB_STROKE_END,
                                           .time_msec = time_msec});
    }
    else {
        handle_stroke_end(lw, time_msec);
    }
}

static void cancel_stroke_job(void *element, DP_UNUSED void *user)
{
    DP_LiquifyWorkerJob *job = element;
    switch (job->type) {
    case DP_LIQUIFY_WORKER_JOB_UNDO:
    case DP_LIQUIFY_WORKER_JOB_REDO:
        break;
    default:
        job->type = DP_LIQUIFY_WORKER_JOB_NONE;
        break;
    }
}

void DP_liquify_worker_stroke_cancel(DP_LiquifyWorker *lw, long long time_msec)
{
    DP_ASSERT(lw);
    if (lw->thread) {
        DP_Mutex *mutex = lw->mutex;
        DP_MUTEX_MUST_LOCK(mutex);
        DP_queue_each(&lw->queue, sizeof(DP_LiquifyWorkerJob),
                      cancel_stroke_job, NULL);
        *(DP_LiquifyWorkerJob *)DP_queue_push(
            &lw->queue, sizeof(DP_LiquifyWorkerJob)) = (DP_LiquifyWorkerJob){
            DP_LIQUIFY_WORKER_JOB_STROKE_CANCEL, {.time_msec = time_msec}};
        DP_MUTEX_MUST_UNLOCK(mutex);
        DP_SEMAPHORE_MUST_POST(lw->sem);
    }
    else {
        handle_stroke_cancel(lw, time_msec);
    }
}

void DP_liquify_worker_undo(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    if (lw->thread) {
        push_job(lw, (DP_LiquifyWorkerJob){DP_LIQUIFY_WORKER_JOB_UNDO, {0}});
    }
    else {
        handle_undo(lw);
    }
}

void DP_liquify_worker_redo(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    if (lw->thread) {
        push_job(lw, (DP_LiquifyWorkerJob){DP_LIQUIFY_WORKER_JOB_REDO, {0}});
    }
    else {
        handle_redo(lw);
    }
}

void DP_liquify_worker_update_request(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    if (lw->thread) {
        push_job(lw, (DP_LiquifyWorkerJob){DP_LIQUIFY_WORKER_JOB_UPDATE_REQUEST,
                                           {0}});
    }
    else {
        handle_update_request(lw);
    }
}

static void abort_job(void *element, DP_UNUSED void *user)
{
    DP_LiquifyWorkerJob *job = element;
    job->type = DP_LIQUIFY_WORKER_JOB_NONE;
}

void DP_liquify_worker_abort(DP_LiquifyWorker *lw)
{
    DP_ASSERT(lw);
    if (lw->thread) {
        DP_Mutex *mutex = lw->mutex;
        DP_MUTEX_MUST_LOCK(mutex);
        DP_queue_each(&lw->queue, sizeof(DP_LiquifyWorkerJob), abort_job, NULL);
        DP_MUTEX_MUST_UNLOCK(mutex);
    }
}
