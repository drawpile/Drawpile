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
#include <dptest_engine.h>


static DP_Image *generate_base_image(void)
{
    DP_Image *img = DP_image_new(WIDTH, HEIGHT);
    DP_Pixel8 *pixels = DP_image_pixels(img);
    double wf = DP_int_to_double(WIDTH);
    double hf = DP_int_to_double(HEIGHT);
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            pixels[y * WIDTH + x] = (DP_Pixel8){
                .b = DP_double_to_uint8(DP_int_to_double(y) / hf * 255.0),
                .g = DP_double_to_uint8(DP_int_to_double(x) / wf * 255.0),
                .r = 255,
                .a = 255,
            };
        }
    }

    return img;
}

static void resize_image(TEST_PARAMS)
{
    ResizeTestCase *rtc = T->test->user;
    char *out_path = DP_format("test/tmp/resize_image_%s.png", rtc->name);
    char *expected_path = DP_format("test/data/image_resize/%s.png", rtc->name);

    DP_Image *base = generate_base_image();
    DP_Image *resized = DP_image_new_subimage(
        base, rtc->x, rtc->y, WIDTH + rtc->dw, HEIGHT + rtc->dh);
    DP_image_free(base);

    DP_Output *output = DP_file_output_new_from_path(out_path);
    FATAL(NOT_NULL_OK(output, "got output for %s", out_path));

    if (OK(DP_image_write_png(resized, output), "write png to %s", out_path)) {
        IMAGE_FILE_EQ_OK(out_path, expected_path, "resized image matches");
    }

    DP_output_free(output);
    DP_image_free(resized);
    DP_free(expected_path);
    DP_free(out_path);
}


static void register_tests(REGISTER_PARAMS)
{
    int test_count = DP_ARRAY_LENGTH(resize_test_cases);
    for (int i = 0; i < test_count; ++i) {
        ResizeTestCase *rtc = &resize_test_cases[i];
        DP_test_register(REGISTER_ARGS, rtc->name, resize_image, rtc);
    }
}

int main(int argc, char **argv)
{
    DP_test_main(argc, argv, register_tests, NULL);
}
