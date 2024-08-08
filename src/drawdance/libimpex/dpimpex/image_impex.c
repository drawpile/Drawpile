// SPDX-License-Identifier: GPL-3.0-or-later
#include "image_impex.h"
#include "image_jpeg.h"
#include "image_png.h"
#include "image_webp.h"
#include <dpcommon/common.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpengine/image.h>


static void assign_type(DP_ImageFileType *out_type, DP_ImageFileType type)
{
    if (out_type) {
        *out_type = type;
    }
}

static DP_Image *read_image_guess(DP_Input *input, DP_ImageFileType *out_type)
{
    unsigned char buf[12];
    bool error;
    size_t read = DP_input_read(input, buf, sizeof(buf), &error);
    if (error) {
        return NULL;
    }

    DP_ImageFileType type = DP_image_guess(buf, read);
    assign_type(out_type, type);

    DP_Image *(*read_fn)(DP_Input *);
    switch (type) {
    case DP_IMAGE_FILE_TYPE_PNG:
        read_fn = DP_image_png_read;
        break;
    case DP_IMAGE_FILE_TYPE_JPEG:
        read_fn = DP_image_jpeg_read;
        break;
    case DP_IMAGE_FILE_TYPE_WEBP:
        read_fn = DP_image_webp_read;
        break;
    default:
        DP_error_set("Could not guess image format");
        return NULL;
    }

    if (!DP_input_rewind(input)) {
        return NULL;
    }

    return read_fn(input);
}

DP_Image *DP_image_new_from_file(DP_Input *input, DP_ImageFileType type,
                                 DP_ImageFileType *out_type)
{
    DP_ASSERT(input);
    switch (type) {
    case DP_IMAGE_FILE_TYPE_GUESS:
        return read_image_guess(input, out_type);
    case DP_IMAGE_FILE_TYPE_PNG:
        assign_type(out_type, DP_IMAGE_FILE_TYPE_PNG);
        return DP_image_png_read(input);
    case DP_IMAGE_FILE_TYPE_JPEG:
        assign_type(out_type, DP_IMAGE_FILE_TYPE_JPEG);
        return DP_image_jpeg_read(input);
    case DP_IMAGE_FILE_TYPE_WEBP:
        assign_type(out_type, DP_IMAGE_FILE_TYPE_WEBP);
        return DP_image_webp_read(input);
    default:
        assign_type(out_type, DP_IMAGE_FILE_TYPE_UNKNOWN);
        DP_error_set("Unknown image file type %d", (int)type);
        return NULL;
    }
}

DP_Image *DP_image_read_png(DP_Input *input)
{
    DP_ASSERT(input);
    return DP_image_png_read(input);
}

DP_Image *DP_image_read_jpeg(DP_Input *input)
{
    DP_ASSERT(input);
    return DP_image_jpeg_read(input);
}

DP_Image *DP_image_read_webp(DP_Input *input)
{
    DP_ASSERT(input);
    return DP_image_webp_read(input);
}

bool DP_image_write_png(DP_Image *img, DP_Output *output)
{
    DP_ASSERT(img);
    DP_ASSERT(output);
    return DP_image_png_write(output, DP_image_width(img), DP_image_height(img),
                              DP_image_pixels(img));
}

bool DP_image_write_jpeg(DP_Image *img, DP_Output *output)
{
    DP_ASSERT(img);
    DP_ASSERT(output);
    return DP_image_jpeg_write(output, DP_image_width(img),
                               DP_image_height(img), DP_image_pixels(img));
}

bool DP_image_write_webp(DP_Image *img, DP_Output *output)
{
    DP_ASSERT(img);
    DP_ASSERT(output);
    return DP_image_webp_write(output, DP_image_width(img),
                               DP_image_height(img), DP_image_pixels(img));
}
