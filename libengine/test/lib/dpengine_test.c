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
#include "dpengine_test.h"
#include "dpcommon_test.h"
#include "dpengine/draw_context.h"
#include <dpcommon/input.h>
#include <dpengine/canvas_history.h>
#include <dpengine/canvas_state.h>
#include <dpengine/image.h>
#include <endian.h>


static void destroy_canvas_history(void *value)
{
    DP_canvas_history_free(value);
}

void push_canvas_history(void **state, DP_CanvasHistory *value)
{
    destructor_push(state, value, destroy_canvas_history);
}

static void destroy_canvas_state(void *value)
{
    DP_canvas_state_decref(value);
}

void push_canvas_state(void **state, DP_CanvasState *value)
{
    destructor_push(state, value, destroy_canvas_state);
}

static void destroy_draw_context(void *value)
{
    DP_draw_context_free(value);
}

void push_draw_context(void **state, DP_DrawContext *value)
{
    destructor_push(state, value, destroy_draw_context);
}

static void destroy_image(void *value)
{
    DP_image_free(value);
}

void push_image(void **state, DP_Image *value)
{
    destructor_push(state, value, destroy_image);
}


static DP_Image *read_image(void **state, const char *path)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    assert_non_null(input);
    push_input(state, input);
    DP_Image *img = DP_image_new_from_file(input, DP_IMAGE_FILE_TYPE_GUESS);
    assert_non_null(img);
    push_image(state, img);
    destructor_run(state, input);
    return img;
}

void _assert_image_files_equal(void **state, const char *a, const char *b,
                               const char *file, int line)
{
    DP_Image *img_a = read_image(state, a);
    DP_Image *img_b = read_image(state, b);
    int width = DP_image_width(img_a);
    int height = DP_image_height(img_a);
    _assert_int_equal(cast_to_largest_integral_type(width),
                      cast_to_largest_integral_type(DP_image_width(img_b)),
                      file, line);
    _assert_int_equal(cast_to_largest_integral_type(height),
                      cast_to_largest_integral_type(DP_image_height(img_b)),
                      file, line);

    DP_Pixel *pixels_a = DP_image_pixels(img_a);
    DP_Pixel *pixels_b = DP_image_pixels(img_b);
    for (int i = 0; i < width * height; ++i) {
        _assert_int_equal(cast_to_largest_integral_type(pixels_a[i].color),
                          cast_to_largest_integral_type(pixels_b[i].color),
                          file, line);
    }

    destructor_run(state, img_a);
    destructor_run(state, img_b);
}
