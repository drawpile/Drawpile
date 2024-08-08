// SPDX-License-Identifier: GPL-3.0-or-later
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/draw_context.h>
#include <dpengine/image.h>
#include <dpimpex/image_impex.h>
#include <dptest_impex.h>


static DP_Image *read_image(TEST_PARAMS, const char *path)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    DP_Image *img = DP_image_read_png(input);
    FATAL(NOT_NULL_OK(img, "got image from %s", path));
    DP_input_free(input);
    return img;
}

static bool make_thumbnail(TEST_PARAMS, const char *path, int max_width,
                           int max_height, DP_Image **out_thumb)
{
    DP_Image *img = read_image(TEST_ARGS, path);
    DP_DrawContext *dc = DP_draw_context_new();
    DP_Image *thumb;
    bool ok = DP_image_thumbnail(img, dc, max_width, max_height, &thumb);
    DP_image_free(img);
    DP_draw_context_free(dc);
    *out_thumb = thumb;
    return ok;
}

static void assert_thumb(TEST_PARAMS, DP_Image *thumb, const char *name)
{
    char *actual_path = DP_format("test/tmp/image_thumbnail_%s.png", name);
    DP_Output *output = DP_file_output_new_from_path(actual_path);
    FATAL(NOT_NULL_OK(output, "got output for %s", actual_path));
    OK(DP_image_write_png(thumb, output), "write thumb to %s", actual_path);
    DP_output_free(output);

    char *expected_path = DP_format("test/data/image_thumbnail/%s.png", name);
    IMAGE_FILE_EQ_OK(actual_path, expected_path, "thumbnail matches expected");
    DP_free(expected_path);
    DP_free(actual_path);
}


static void image_already_thumbnail_sized(TEST_PARAMS)
{
    DP_Image *thumb;
    OK(make_thumbnail(TEST_ARGS, "test/data/logo256x256.png", 256, 256, &thumb),
       "make thumbnail from already thumbnail-sized image");
    NULL_OK(thumb, "thumbnail-sized image does not generate a new image");
    DP_image_free(thumb);
}

static void image_thumbnail_same_aspect_ratio(TEST_PARAMS)
{
    DP_Image *thumb;
    OK(make_thumbnail(TEST_ARGS, "test/data/logo256x256.png", 128, 128, &thumb),
       "make thumbnail with same aspect ratio");
    if (NOT_NULL_OK(thumb, "same aspect ratio thumbnail generated")) {
        assert_thumb(TEST_ARGS, thumb, "same_aspect_ratio");
    }
    DP_image_free(thumb);
}

static void image_thumbnail_width_limit(TEST_PARAMS)
{
    DP_Image *thumb;
    OK(make_thumbnail(TEST_ARGS, "test/data/logo256x256.png", 100, 1024,
                      &thumb),
       "make width-limited thumbnail");
    if (NOT_NULL_OK(thumb, "width-limited thumbnail generated")) {
        assert_thumb(TEST_ARGS, thumb, "width_limit");
    }
    DP_image_free(thumb);
}

static void image_thumbnail_height_limit(TEST_PARAMS)
{
    DP_Image *thumb;
    OK(make_thumbnail(TEST_ARGS, "test/data/logo256x256.png", 999, 200, &thumb),
       "make height-limited thumbnail");
    if (NOT_NULL_OK(thumb, "height-limited thumbnail generated")) {
        assert_thumb(TEST_ARGS, thumb, "height_limit");
    }
    DP_image_free(thumb);
}

static void image_thumbnail_zero_dimension_becomes_one(TEST_PARAMS)
{
    DP_Image *thumb;
    OK(make_thumbnail(TEST_ARGS, "test/data/logo1160x263.png", 4, 4, &thumb),
       "make single-pixel dimension thumbnail");
    if (NOT_NULL_OK(thumb, "single-pixel dimension thumbnail generated")) {
        assert_thumb(TEST_ARGS, thumb, "zero_dimension_becomes_one");
    }
    DP_image_free(thumb);
}

static void image_thumbnail_ratio_kept(TEST_PARAMS)
{
    DP_Image *thumb;
    OK(make_thumbnail(TEST_ARGS, "test/data/logo1160x263.png", 256, 256,
                      &thumb),
       "make thumbnail from different aspect ratio");
    if (NOT_NULL_OK(thumb, "aspect ratio thumbnail generated")) {
        assert_thumb(TEST_ARGS, thumb, "ratio_kept");
    }
    DP_image_free(thumb);
}


static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(image_already_thumbnail_sized);
    REGISTER_TEST(image_thumbnail_same_aspect_ratio);
    REGISTER_TEST(image_thumbnail_width_limit);
    REGISTER_TEST(image_thumbnail_height_limit);
    REGISTER_TEST(image_thumbnail_zero_dimension_becomes_one);
    REGISTER_TEST(image_thumbnail_ratio_kept);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
