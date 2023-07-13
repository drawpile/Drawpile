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
#include "dptest_engine.h"
#include <dpcommon/input.h>
#include <dpengine/image.h>
#include <dptest.h>


static bool image_vok(DP_TestContext *T, const char *file, int line,
                      DP_Image *a, DP_Image *b, const char *fmt, va_list ap)
    DP_VFORMAT(6);

static bool image_vok(DP_TestContext *T, const char *file, int line,
                      DP_Image *a, DP_Image *b, const char *fmt, va_list ap)
{
    int width_a = DP_image_width(a);
    int width_b = DP_image_width(b);
    int height_a = DP_image_height(a);
    int height_b = DP_image_height(b);
    if (width_a != width_b || height_a != height_b) {
        DP_test_vok(T, file, line, false, fmt, ap);
        DIAG("image dimensions differ\n"
             "actual dimensions:   %dx%d\n"
             "expected dimensions: %dx%d",
             width_a, height_a, width_b, height_b);
        return false;
    }

    DP_Pixel8 *pixels_a = DP_image_pixels(a);
    DP_Pixel8 *pixels_b = DP_image_pixels(b);
    for (int y = 0; y < height_a; ++y) {
        for (int x = 0; x < width_a; ++x) {
            int i = y * width_a + x;
            unsigned int color_a = pixels_a[i].color;
            unsigned int color_b = pixels_b[i].color;
            if (color_a != color_b) {
                DP_test_vok(T, file, line, false, fmt, ap);
                DIAG("image pixels differ at (%d, %d)\n"
                     "actual color:   %08x\n"
                     "expected color: %08x",
                     x, y, color_a, color_b);
                return false;
            }
        }
    }

    DP_test_vok(T, file, line, true, fmt, ap);
    return true;
}


bool DP_test_image_eq_ok(DP_TestContext *T, const char *file, int line,
                         const char *sa, const char *sb, DP_Image *a,
                         DP_Image *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool result = image_vok(TEST_ARGS, file, line, a, b, fmt, ap);
    va_end(ap);

    if (!result) {
        DIAG_VALUE("actual image", "%p", sa, (void *)a);
        DIAG_VALUE("expected image", "%p", sb, (void *)b);
    }
    return result;
}


static DP_Image *read_image(const char *path)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    if (input) {
        DP_Image *img =
            DP_image_new_from_file(input, DP_IMAGE_FILE_TYPE_GUESS, NULL);
        DP_input_free(input);
        return img;
    }
    else {
        return NULL;
    }
}

bool DP_test_image_file_eq_ok(DP_TestContext *T, const char *file, int line,
                              const char *sa, const char *sb, const char *a,
                              const char *b, const char *fmt, ...)
{
    DP_Image *img_a = read_image(a);
    if (!img_a) {
        va_list ap;
        va_start(ap, fmt);
        DP_test_vok(T, file, line, false, fmt, ap);
        va_end(ap);
        DIAG("reading actual path failed: %s", DP_error());
        DIAG_VALUE("actual path", "%s", sa, a);
        DIAG_VALUE("expected path", "%s", sb, b);
        return false;
    }

    DP_Image *img_b = read_image(b);
    if (!img_b) {
        DP_free(img_b);
        va_list ap;
        va_start(ap, fmt);
        DP_test_vok(T, file, line, false, fmt, ap);
        va_end(ap);
        DIAG("reading expected path failed: %s", DP_error());
        DIAG_VALUE("actual path", "%s", sa, a);
        DIAG_VALUE("expected path", "%s", sb, b);
        return false;
    }

    va_list ap;
    va_start(ap, fmt);
    bool result = image_vok(TEST_ARGS, file, line, img_a, img_b, fmt, ap);
    va_end(ap);

    DP_image_free(img_a);
    DP_image_free(img_b);

    if (!result) {
        DIAG_VALUE("actual path", "%s", sa, a);
        DIAG_VALUE("expected path", "%s", sb, b);
    }
    return result;
}
