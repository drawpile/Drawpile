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
#ifndef DPENGINE_TEST_H
#define DPENGINE_TEST_H
#include <dpmsg_test.h> // IWYU pragma: export

typedef struct DP_CanvasHistory DP_CanvasHistory;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_ModelChanges DP_ModelChanges;


void push_canvas_history(void **state, DP_CanvasHistory *value);

void push_canvas_state(void **state, DP_CanvasState *value);

void push_draw_context(void **state, DP_DrawContext *value);

void push_image(void **state, DP_Image *value);

void push_model_changes(void **state, DP_ModelChanges *value);


#define assert_image_files_equal(state, a, b) \
    _assert_image_files_equal(state, a, b, __FILE__, __LINE__)

void _assert_image_files_equal(void **state, const char *a, const char *b,
                               const char *file, int line);


#endif
