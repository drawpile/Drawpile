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
#ifndef DRAWDANCE_CANVAS_RENDERER_H
#define DRAWDANCE_CANVAS_RENDERER_H
#include <dpcommon/common.h>

typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_LayerContent DP_LayerContent;


typedef struct DP_CanvasRenderer DP_CanvasRenderer;

DP_CanvasRenderer *DP_canvas_renderer_new(void);

void DP_canvas_renderer_free(DP_CanvasRenderer *cr);


void DP_canvas_renderer_transform(DP_CanvasRenderer *cr, double *out_x,
                                  double *out_y, double *out_scale,
                                  double *out_rotation_in_radians);

void DP_canvas_renderer_transform_set(DP_CanvasRenderer *cr, double x, double y,
                                      double scale, double rotation_in_radians);


void DP_canvas_renderer_render(DP_CanvasRenderer *cr, DP_LayerContent *lc,
                               int view_width, int view_height,
                               DP_CanvasDiff *diff_or_null);


#endif
