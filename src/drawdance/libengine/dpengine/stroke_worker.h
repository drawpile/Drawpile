// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_STROKE_WORKER_H
#define DPENGINE_STROKE_WORKER_H
#include <dpcommon/common.h>

typedef struct DP_BrushEngine DP_BrushEngine;
typedef struct DP_BrushEngineStrokeParams DP_BrushEngineStrokeParams;
typedef struct DP_BrushPoint DP_BrushPoint;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_ClassicBrush DP_ClassicBrush;
typedef struct DP_Message DP_Message;
typedef struct DP_MyPaintBrush DP_MyPaintBrush;
typedef struct DP_MyPaintSettings DP_MyPaintSettings;
typedef struct DP_UPixelFloat DP_UPixelFloat;


typedef struct DP_StrokeWorker DP_StrokeWorker;

// Takes ownership of the given brush engine.
DP_StrokeWorker *DP_stroke_worker_new(DP_BrushEngine *be);

void DP_stroke_worker_free(DP_StrokeWorker *sw);

bool DP_stroke_worker_thread_active(DP_StrokeWorker *sw);

bool DP_stroke_worker_thread_start(DP_StrokeWorker *sw);

bool DP_stroke_worker_thread_finish(DP_StrokeWorker *sw);

void DP_stroke_worker_size_limit_set(DP_StrokeWorker *sw, int size_limit);

void DP_stroke_worker_classic_brush_set(DP_StrokeWorker *sw,
                                        const DP_ClassicBrush *brush,
                                        const DP_BrushEngineStrokeParams *besp,
                                        const DP_UPixelFloat *color_override,
                                        bool eraser_override);

void DP_stroke_worker_mypaint_brush_set(DP_StrokeWorker *sw,
                                        const DP_MyPaintBrush *brush,
                                        const DP_MyPaintSettings *settings,
                                        const DP_BrushEngineStrokeParams *besp,
                                        const DP_UPixelFloat *color_override,
                                        bool eraser_override);

void DP_stroke_worker_dabs_flush(DP_StrokeWorker *sw);

void DP_stroke_worker_message_push_noinc(DP_StrokeWorker *sw, DP_Message *msg);

void DP_stroke_worker_stroke_begin(DP_StrokeWorker *sw,
                                   DP_CanvasState *cs_or_null,
                                   unsigned int context_id,
                                   bool compatibility_mode,
                                   bool push_undo_point, bool mirror, bool flip,
                                   float zoom, float angle);

void DP_stroke_worker_stroke_to(DP_StrokeWorker *sw, const DP_BrushPoint *bp,
                                DP_CanvasState *cs_or_null);

void DP_stroke_worker_poll(DP_StrokeWorker *sw, long long time_msec,
                           DP_CanvasState *cs_or_null);

void DP_stroke_worker_stroke_end(DP_StrokeWorker *sw, long long time_msec,
                                 DP_CanvasState *cs_or_null, bool push_pen_up);

void DP_stroke_worker_stroke_cancel(DP_StrokeWorker *sw, long long time_msec,
                                    bool push_pen_up);

void DP_stroke_worker_offset_add(DP_StrokeWorker *sw, float x, float y);

#endif
