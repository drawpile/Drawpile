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
#ifndef DP_ENGINE_CANVAS_DIFF
#define DP_ENGINE_CANVAS_DIFF
#include <dpcommon/common.h>


typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef bool (*DP_CanvasDiffCheckFn)(void *data, int tile_index);
typedef void (*DP_CanvasDiffEachIndexFn)(void *data, int tile_index);
typedef void (*DP_CanvasDiffEachPosFn)(void *data, int tile_x, int tile_y);

DP_CanvasDiff *DP_canvas_diff_new(void);

void DP_canvas_diff_free(DP_CanvasDiff *diff);

void DP_canvas_diff_begin(DP_CanvasDiff *diff, int old_width, int old_height,
                          int current_width, int current_height,
                          bool layer_props_changed);

void DP_canvas_diff_check(DP_CanvasDiff *diff, DP_CanvasDiffCheckFn fn,
                          void *data);

void DP_canvas_diff_check_all(DP_CanvasDiff *diff);

void DP_canvas_diff_each_index(DP_CanvasDiff *diff, DP_CanvasDiffEachIndexFn fn,
                               void *data);

void DP_canvas_diff_each_index_reset(DP_CanvasDiff *diff,
                                     DP_CanvasDiffEachIndexFn fn, void *data);

void DP_canvas_diff_each_pos(DP_CanvasDiff *diff, DP_CanvasDiffEachPosFn fn,
                             void *data);

void DP_canvas_diff_each_pos_reset(DP_CanvasDiff *diff,
                                   DP_CanvasDiffEachPosFn fn, void *data);

void DP_canvas_diff_bounds_clamp(DP_CanvasDiff *diff, int tile_left,
                                 int tile_top, int tile_right, int tile_bottom,
                                 int *out_left, int *out_top, int *out_right,
                                 int *out_bottom, int *out_xtiles);

void DP_canvas_diff_each_pos_tile_bounds_reset(DP_CanvasDiff *diff,
                                               int tile_left, int tile_top,
                                               int tile_right, int tile_bottom,
                                               DP_CanvasDiffEachPosFn fn,
                                               void *data);

bool DP_canvas_diff_layer_props_changed_reset(DP_CanvasDiff *diff);


#endif
