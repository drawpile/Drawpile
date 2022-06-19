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
#include <dpcommon/common.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/draw_context.h>
#include <dpengine/image.h>
#include <dpengine_test.h>


static DP_Image *read_image(void **state, const char *path)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    push_input(state, input);
    DP_Image *img = DP_image_read_png(input);
    assert_non_null(img);
    push_image(state, img);
    destructor_run(state, input);
    return img;
}

static bool make_thumbnail(void **state, const char *path, int max_width,
                           int max_height, DP_Image **out_thumb)
{
    DP_Image *img = read_image(state, path);
    DP_DrawContext *dc = DP_draw_context_new();
    push_draw_context(state, dc);
    DP_Image *thumb;
    bool ok = DP_image_thumbnail(img, dc, max_width, max_height, &thumb);
    push_image(state, thumb);
    destructor_run(state, dc);
    *out_thumb = thumb;
    return ok;
}

static void assert_thumb(void **state, DP_Image *thumb, const char *name)
{
    char *actual_path =
        push_format(state, "test/tmp/image_thumbnail_%s.png", name);
    DP_Output *output = DP_file_output_new_from_path(actual_path);
    push_output(state, output);
    assert_true(DP_image_write_png(thumb, output));
    destructor_run(state, output);

    char *expected_path =
        push_format(state, "test/data/image_thumbnail/%s.png", name);
    assert_image_files_equal(state, actual_path, expected_path);
    destructor_run(state, expected_path);
    destructor_run(state, actual_path);
}


static void test_image_already_thumbnail_sized(void **state)
{
    DP_Image *thumb;
    assert_true(
        make_thumbnail(state, "test/data/logo256x256.png", 256, 256, &thumb));
    assert_null(thumb);
}

static void test_image_thumbnail_same_aspect_ratio(void **state)
{
    DP_Image *thumb;
    assert_true(
        make_thumbnail(state, "test/data/logo256x256.png", 128, 128, &thumb));
    assert_non_null(thumb);
    assert_thumb(state, thumb, "same_aspect_ratio");
}

static void test_image_thumbnail_width_limit(void **state)
{
    DP_Image *thumb;
    assert_true(
        make_thumbnail(state, "test/data/logo256x256.png", 100, 1024, &thumb));
    assert_non_null(thumb);
    assert_thumb(state, thumb, "width_limit");
}

static void test_image_thumbnail_height_limit(void **state)
{
    DP_Image *thumb;
    assert_true(
        make_thumbnail(state, "test/data/logo256x256.png", 999, 200, &thumb));
    assert_non_null(thumb);
    assert_thumb(state, thumb, "height_limit");
}

static void test_image_thumbnail_zero_dimension_becomes_one(void **state)
{
    DP_Image *thumb;
    assert_true(
        make_thumbnail(state, "test/data/logo1160x263.png", 4, 4, &thumb));
    assert_non_null(thumb);
    assert_thumb(state, thumb, "zero_dimension_becomes_one");
}

static void test_image_thumbnail_ratio_kept(void **state)
{
    DP_Image *thumb;
    assert_true(
        make_thumbnail(state, "test/data/logo1160x263.png", 256, 256, &thumb));
    assert_non_null(thumb);
    assert_thumb(state, thumb, "ratio_kept");
}


int main(void)
{
    const struct CMUnitTest tests[] = {
        dp_unit_test(test_image_already_thumbnail_sized),
        dp_unit_test(test_image_thumbnail_same_aspect_ratio),
        dp_unit_test(test_image_thumbnail_width_limit),
        dp_unit_test(test_image_thumbnail_height_limit),
        dp_unit_test(test_image_thumbnail_zero_dimension_becomes_one),
        dp_unit_test(test_image_thumbnail_ratio_kept),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
