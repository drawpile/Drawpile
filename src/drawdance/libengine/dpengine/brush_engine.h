/*
 * Copyright (C) 2022-2023 askmeaboutloom
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
#ifndef DPENGINE_BRUSHENGINE_H
#define DPENGINE_BRUSHENGINE_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_ClassicBrush DP_ClassicBrush;
typedef struct DP_Message DP_Message;
typedef struct DP_MyPaintBrush DP_MyPaintBrush;
typedef struct DP_MyPaintSettings DP_MyPaintSettings;

typedef struct DP_BrushPoint {
    float x, y;
    float pressure;
    float xtilt, ytilt;
    float rotation;
    long long time_msec;
} DP_BrushPoint;

typedef struct DP_StrokeEngineStrokeParams {
    int smoothing;
    int stabilizer_sample_count;
    bool interpolate;
    bool smoothing_finish_strokes;
    bool stabilizer_finish_strokes;
} DP_StrokeEngineStrokeParams;

typedef struct DP_BrushEngineStrokeParams {
    DP_StrokeEngineStrokeParams se;
    int layer_id;
    int selection_id;
    bool layer_alpha_lock;
    bool sync_samples;
} DP_BrushEngineStrokeParams;


typedef void (*DP_StrokeEnginePushPointFn)(void *user, DP_BrushPoint bp,
                                           DP_CanvasState *cs_or_null);
typedef void (*DP_StrokeEnginePollControlFn)(void *user, bool enable);
typedef void (*DP_BrushEnginePushMessageFn)(void *user, DP_Message *msg);
typedef void (*DP_BrushEnginePollControlFn)(void *user, bool enable);
typedef DP_CanvasState *(*DP_BrushEngineSyncFn)(void *user);

typedef struct DP_StrokeEngine DP_StrokeEngine;
typedef struct DP_BrushEngine DP_BrushEngine;


DP_StrokeEngine *
DP_stroke_engine_new(DP_StrokeEnginePushPointFn push_point,
                     DP_StrokeEnginePollControlFn poll_control_or_null,
                     void *user);

void DP_stroke_engine_free(DP_StrokeEngine *se);

void DP_stroke_engine_params_set(DP_StrokeEngine *se,
                                 const DP_StrokeEngineStrokeParams *sesp);

void DP_stroke_engine_stroke_begin(DP_StrokeEngine *se);

void DP_stroke_engine_stroke_to(DP_StrokeEngine *se, DP_BrushPoint bp,
                                DP_CanvasState *cs_or_null);

void DP_stroke_engine_poll(DP_StrokeEngine *se, long long time_msec,
                           DP_CanvasState *cs_or_null);

void DP_stroke_engine_stroke_end(DP_StrokeEngine *se, long long time_msec,
                                 DP_CanvasState *cs_or_null);


DP_BrushEngine *
DP_brush_engine_new(DP_BrushEnginePushMessageFn push_message,
                    DP_BrushEnginePollControlFn poll_control_or_null,
                    DP_BrushEngineSyncFn sync_or_null, void *user);

void DP_brush_engine_free(DP_BrushEngine *be);

void DP_brush_engine_size_limit_set(DP_BrushEngine *be, int size_limit);

void DP_brush_engine_classic_brush_set(DP_BrushEngine *be,
                                       const DP_ClassicBrush *brush,
                                       const DP_BrushEngineStrokeParams *besp,
                                       const DP_UPixelFloat *color_override,
                                       bool eraser_override);

void DP_brush_engine_mypaint_brush_set(DP_BrushEngine *be,
                                       const DP_MyPaintBrush *brush,
                                       const DP_MyPaintSettings *settings,
                                       const DP_BrushEngineStrokeParams *besp,
                                       const DP_UPixelFloat *color_override,
                                       bool eraser_override);

void DP_brush_engine_dabs_flush(DP_BrushEngine *be);

void DP_brush_engine_message_push_noinc(DP_BrushEngine *be, DP_Message *msg);

// Sets the context id for this stroke, optionally pushes an undo point message.
void DP_brush_engine_stroke_begin(DP_BrushEngine *be,
                                  DP_CanvasState *cs_or_null,
                                  unsigned int context_id,
                                  bool compatibility_mode, bool push_undo_point,
                                  bool mirror, bool flip, float zoom,
                                  float angle);

// Pushes draw dabs messages or fills up the stabilizer.
void DP_brush_engine_stroke_to(DP_BrushEngine *be, DP_BrushPoint bp,
                               DP_CanvasState *cs_or_null);

// Pushes draw dabs messages if stabilizing.
void DP_brush_engine_poll(DP_BrushEngine *be, long long time_msec,
                          DP_CanvasState *cs_or_null);

// Finishes a stroke, pushing draw dabs messages if stabilizing, optionally
// pushes a pen up message.
void DP_brush_engine_stroke_end(DP_BrushEngine *be, long long time_msec,
                                DP_CanvasState *cs_or_null, bool push_pen_up);

void DP_brush_engine_offset_add(DP_BrushEngine *be, float x, float y);


#endif
