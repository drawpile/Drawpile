// SPDX-License-Identifier: GPL-3.0-or-later
#include "image_png.h"
#include "image.h"
#include "pixels.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <png.h>
#include <setjmp.h>
// IWYU pragma: no_include <pngconf.h>


static void error_png(png_structp png_ptr, png_const_charp error_msg)
{
    DP_error_set("PNG error: %s", error_msg);
    png_longjmp(png_ptr, 1);
}

static void warn_png(DP_UNUSED png_structp png_ptr, png_const_charp warning_msg)
{
    DP_warn("PNG warning: %s", warning_msg);
}

static png_voidp malloc_png(DP_UNUSED png_structp png_ptr,
                            png_alloc_size_t size)
{
    return DP_malloc(size);
}

static void free_png(DP_UNUSED png_structp png_ptr, png_voidp ptr)
{
    DP_free(ptr);
}

static void read_png(png_structp png_ptr, png_bytep data, size_t length)
{
    DP_Input *input = png_get_io_ptr(png_ptr);
    bool error;
    size_t read = DP_input_read(input, data, length, &error);
    if (error) {
        png_longjmp(png_ptr, 1);
    }
    else if (read != length) {
        DP_error_set("PNG wanted %zu bytes, but got %zu", length, read);
        png_longjmp(png_ptr, 1);
    }
}

static void write_png(png_structp png_ptr, png_bytep data, size_t length)
{
    DP_Output *output = png_get_io_ptr(png_ptr);
    if (!DP_output_write(output, data, length)) {
        png_longjmp(png_ptr, 1);
    }
}

static void flush_png(png_structp png_ptr)
{
    DP_Output *output = png_get_io_ptr(png_ptr);
    if (!DP_output_flush(output)) {
        png_longjmp(png_ptr, 1);
    }
}


// When compiling with optimizations, gcc produces warnings about potential
// variable clobbering by longjmp, so we have to force this to be non-inlined.
static void push_pixels(png_uint_32 channels, png_uint_32 width,
                        png_uint_32 height, png_bytepp row_pointers,
                        DP_Pixel8 *pixels) DP_NOINLINE;

static void push_pixels(png_uint_32 channels, png_uint_32 width,
                        png_uint_32 height, png_bytepp row_pointers,
                        DP_Pixel8 *pixels)
{
    for (png_uint_32 y = 0; y < height; ++y) {
        png_bytep row = row_pointers[y];
        for (png_uint_32 x = 0; x < width; ++x) {
            png_uint_32 offset = x * channels;
            DP_UPixel8 pixel = {
                .b = row[offset],
                .g = row[offset + 1],
                .r = row[offset + 2],
                .a = channels == 3 ? 0xff : row[offset + 3],
            };
            // PNG stores pixels unpremultiplied, fix them up.
            pixels[y * width + x] = DP_pixel8_premultiply(pixel);
        }
    }
}

DP_Image *DP_image_png_read(DP_Input *input)
{
    png_structp png_ptr =
        png_create_read_struct_2(PNG_LIBPNG_VER_STRING, NULL, error_png,
                                 warn_png, NULL, malloc_png, free_png);
    if (!png_ptr) {
        DP_error_set("Can't create PNG read struct");
        return NULL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        DP_error_set("Can't create PNG read info struct");
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;
    }

    png_set_read_fn(png_ptr, input, read_png);
    png_set_user_limits(png_ptr, INT16_MAX, INT16_MAX);

    int transforms = PNG_TRANSFORM_SCALE_16 | PNG_TRANSFORM_BGR
                   | PNG_TRANSFORM_GRAY_TO_RGB | PNG_TRANSFORM_EXPAND;
    png_read_png(png_ptr, info_ptr, transforms, NULL);

    png_uint_32 channels = png_get_channels(png_ptr, info_ptr);
    if (channels != 3u && channels != 4u) {
        DP_error_set("Can't handle a PNG with %u color channels",
                     (unsigned int)channels);
        png_longjmp(png_ptr, 1);
    }

    png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    if (width == 0 || width > INT16_MAX || height == 0 || height > INT16_MAX) {
        DP_error_set("Invalid PNG dimensions %ux%u", (unsigned int)width,
                     (unsigned int)height);
        png_longjmp(png_ptr, 1);
    }

    size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    size_t expected_rowbytes = width * channels;
    if (rowbytes != expected_rowbytes) {
        DP_error_set("Expected PNG row length of %zu, but got %zu",
                     expected_rowbytes, rowbytes);
        png_longjmp(png_ptr, 1);
    }

    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);
    DP_Image *img = DP_image_new((int)width, (int)height);
    DP_Pixel8 *pixels = DP_image_pixels(img);
    push_pixels(channels, width, height, row_pointers, pixels);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return img;
}


static bool write_png_with(DP_Output *output, int width, int height,
                           DP_UPixel8 (*get_pixel)(void *, size_t), void *user)
{
    png_structp png_ptr =
        png_create_write_struct_2(PNG_LIBPNG_VER_STRING, NULL, error_png,
                                  warn_png, NULL, malloc_png, free_png);
    if (!png_ptr) {
        DP_error_set("Can't create PNG write struct");
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        DP_error_set("Can't create PNG write info struct");
        png_destroy_write_struct(&png_ptr, NULL);
        return false;
    }

    size_t stride = DP_int_to_size(width);
    size_t row_count = DP_int_to_size(height);
    size_t pixel_count = stride * row_count;
    png_bytep bytes = png_malloc(png_ptr, pixel_count * 4);
    for (size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        DP_UPixel8 pixel = get_pixel(user, pixel_index);
        size_t byte_index = pixel_index * 4u;
        bytes[byte_index + 0u] = pixel.r;
        bytes[byte_index + 1u] = pixel.g;
        bytes[byte_index + 2u] = pixel.b;
        bytes[byte_index + 3u] = pixel.a;
    }

    png_bytepp row_pointers =
        png_malloc(png_ptr, row_count * sizeof(*row_pointers));
    for (size_t i = 0; i < row_count; ++i) {
        row_pointers[i] = bytes + i * stride * sizeof(DP_UPixel8);
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_free(png_ptr, bytes);
        png_free(png_ptr, row_pointers);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_set_write_fn(png_ptr, output, write_png, flush_png);

    png_set_IHDR(png_ptr, info_ptr, DP_int_to_uint32(width),
                 DP_int_to_uint32(height), 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    png_free(png_ptr, bytes);
    png_free(png_ptr, row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return DP_output_flush(output);
}

static DP_UPixel8 get_premultiplied_pixel(void *user, size_t pixel_index)
{
    DP_Pixel8 *pixels = user;
    return DP_pixel8_unpremultiply(pixels[pixel_index]);
}

bool DP_image_png_write(DP_Output *output, int width, int height,
                        DP_Pixel8 *pixels)
{
    return write_png_with(output, width, height, get_premultiplied_pixel,
                          pixels);
}

static DP_UPixel8 get_unpremultiplied_pixel(void *user, size_t pixel_index)
{
    DP_UPixel8 *pixels = user;
    return pixels[pixel_index];
}

bool DP_image_png_write_unpremultiplied(DP_Output *output, int width,
                                        int height, DP_UPixel8 *pixels)
{
    return write_png_with(output, width, height, get_unpremultiplied_pixel,
                          pixels);
}
