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
#include "resize_image.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>
#include <dpengine/image.h>
#include <dpengine/pixels.h>
#include <dpengine_test.h>


static const uint32_t empty[WIDTH * HEIGHT];

static DP_Image *generate_base_image(void **state)
{
    DP_Image *img = DP_image_new(WIDTH, HEIGHT);
    push_image(state, img);

    int width = DP_image_width(img);
    int height = DP_image_height(img);
    DP_Pixel *pixels = DP_image_pixels(img);
    assert_int_equal(width, WIDTH);
    assert_int_equal(height, HEIGHT);
    assert_memory_equal(pixels, empty, sizeof(empty));

    double wf = DP_int_to_double(width);
    double hf = DP_int_to_double(height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixels[y * width + x] = (DP_Pixel){
                .b = DP_double_to_uint8(DP_int_to_double(y) / hf * 255.0),
                .g = DP_double_to_uint8(DP_int_to_double(x) / wf * 255.0),
                .r = 255,
                .a = 255,
            };
        }
    }

    return img;
}

static void test_resize_image(void **state)
{
    ResizeTestCase *rtc = initial_state(state);
    char *out_path =
        push_format(state, "test/tmp/resize_image_%s.png", rtc->name);
    char *expected_path =
        push_format(state, "test/data/image_resize/%s.png", rtc->name);

    DP_Image *base = generate_base_image(state);
    DP_Image *resized = DP_image_new_subimage(
        base, rtc->x, rtc->y, WIDTH + rtc->dw, HEIGHT + rtc->dh);
    destructor_run(state, base);
    push_image(state, resized);

    DP_Output *output = DP_file_output_new_from_path(out_path);
    push_output(state, output);
    assert_true(DP_image_write_png(resized, output));
    destructor_run(state, output);
    destructor_run(state, resized);

    assert_image_files_equal(state, out_path, expected_path);
}


int main(void)
{
    struct CMUnitTest tests[DP_ARRAY_LENGTH(resize_test_cases)];
    int test_count = DP_ARRAY_LENGTH(tests);
    for (int i = 0; i < test_count; ++i) {
        ResizeTestCase *rtc = &resize_test_cases[i];
        tests[i] = (struct CMUnitTest){rtc->name, test_resize_image, setup,
                                       teardown, rtc};
    }
    return cmocka_run_group_tests(tests, NULL, NULL);
}
