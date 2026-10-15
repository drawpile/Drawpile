// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_LIQUIFY_WORKER_H
#define DPENGINE_LIQUIFY_WORKER_H
#include <dpcommon/common.h>

typedef struct DP_BrushPoint DP_BrushPoint;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Liquify DP_Liquify;
typedef struct DP_LiquifyEngine DP_LiquifyEngine;
typedef struct DP_LiquifyEngineStrokeParams DP_LiquifyEngineStrokeParams;


// These represent the user holding the constrain and center key. They don't
// constrain or center the liquify. Constrain reverses move, scale and rotate.
// Center makes move go sideways, perpendicular to the stroke direction.
#define DP_LIQUIFY_WORKER_FLAG_CONSTRAIN (1u << 0u)
#define DP_LIQUIFY_WORKER_FLAG_CENTER    (1u << 1u)

typedef struct DP_LiquifyWorker DP_LiquifyWorker;

typedef struct DP_LiquifyWorkerOperationParams {
    int type;
    union {
        bool reverse;
        float smoothe_kernel_radius;
    } DP_ANONYMOUS(arg);
} DP_LiquifyWorkerOperationParams;

typedef enum DP_LiquifyWorkerEventType {
    DP_LIQUIFY_WORKER_EVENT_UPDATE,
    DP_LIQUIFY_WORKER_EVENT_UNDO_LIMIT_HIT,
} DP_LiquifyWorkerEventType;

typedef struct DP_LiquifyWorkerEvent {
    DP_LiquifyWorkerEventType type;
} DP_LiquifyWorkerEvent;

typedef void (*DP_LiquifyWorkerCallbackFn)(void *user,
                                           const DP_LiquifyWorkerEvent *event);

DP_LiquifyWorker *DP_liquify_worker_new_inc(DP_Liquify *l, DP_DrawContext *dc,
                                            int undo_depth,
                                            DP_LiquifyWorkerCallbackFn fn,
                                            void *user);

void DP_liquify_worker_free(DP_LiquifyWorker *lw);

DP_Liquify *DP_liquify_worker_liquify_noinc(DP_LiquifyWorker *lw);

DP_DrawContext *DP_liquify_worker_draw_context(DP_LiquifyWorker *lw);

bool DP_liquify_worker_thread_active(DP_LiquifyWorker *lw);

bool DP_liquify_worker_thread_start(DP_LiquifyWorker *lw);

bool DP_liquify_worker_thread_finish(DP_LiquifyWorker *lw);

void DP_liquify_worker_operation_params_set(
    DP_LiquifyWorker *lw, const DP_LiquifyWorkerOperationParams *lwop);

void DP_liquify_worker_stroke_params_set(
    DP_LiquifyWorker *lw, const DP_LiquifyEngineStrokeParams *lesp);

void DP_liquify_worker_stroke_begin(DP_LiquifyWorker *lw, float zoom);

void DP_liquify_worker_stroke_to(DP_LiquifyWorker *lw, const DP_BrushPoint *bp,
                                 unsigned int flags);

void DP_liquify_worker_stroke_end(DP_LiquifyWorker *lw, long long time_msec);

void DP_liquify_worker_stroke_cancel(DP_LiquifyWorker *lw, long long time_msec);

void DP_liquify_worker_undo(DP_LiquifyWorker *lw);
void DP_liquify_worker_redo(DP_LiquifyWorker *lw);

void DP_liquify_worker_update_request(DP_LiquifyWorker *lw);

void DP_liquify_worker_abort(DP_LiquifyWorker *lw);

#endif
