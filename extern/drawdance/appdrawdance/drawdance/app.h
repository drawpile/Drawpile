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
#ifndef DRAWDANCE_APP_H
#define DRAWDANCE_APP_H
#include <dpcommon/common.h>
#include <SDL.h>

typedef struct DP_UserInputs DP_UserInputs;
typedef struct DP_Worker DP_Worker;


typedef struct DP_App DP_App;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Document DP_Document;

typedef void (*DP_AppWorkerJobFn)(void *user);

DP_App *DP_app_new(SDL_Window *window, SDL_GLContext gl_context);

void DP_app_free(DP_App *app);

void DP_app_run(DP_App *app);

void DP_app_document_set(DP_App *app, DP_Document *doc_or_null);

void DP_app_canvas_renderer_transform(DP_App *app, double *out_x, double *out_y,
                                      double *out_scale,
                                      double *out_rotation_in_radians);

void DP_app_canvas_renderer_transform_set(DP_App *app, double x, double y,
                                          double scale,
                                          double rotation_in_radians);

bool DP_app_worker_ready(DP_App *app);

void DP_app_worker_push(DP_App *app, DP_AppWorkerJobFn fn, void *user);

DP_UserInputs *DP_app_inputs(DP_App *app);


#endif
