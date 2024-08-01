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
#include "image.h"
#include "compress.h"
#include "image_jpeg.h"
#include "image_png.h"
#include "image_transform.h"
#include "image_webp.h"
#include "paint.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpmsg/messages.h>


struct DP_Image {
    int width, height;
    DP_Pixel8 pixels[];
};


DP_Image *DP_image_new(int width, int height)
{
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);
    size_t count = DP_int_to_size(width) * DP_int_to_size(height);
    DP_Image *img = DP_malloc_zeroed(DP_FLEX_SIZEOF(DP_Image, pixels, count));
    img->width = width;
    img->height = height;
    return img;
}

static void assign_type(DP_ImageFileType *out_type, DP_ImageFileType type)
{
    if (out_type) {
        *out_type = type;
    }
}

static bool guess_png(const unsigned char *buf, size_t size)
{
    unsigned char sig[] = {0x89, 0x50, 0x4e, 0x47, 0xd, 0xa, 0x1a, 0xa};
    return size >= sizeof(sig) && memcmp(buf, sig, sizeof(sig)) == 0;
}

static bool guess_jpeg(const unsigned char *buf, size_t size)
{
    return size >= 4 && buf[0] == 0xff && buf[1] == 0xd8 && buf[2] == 0xff
        && ((buf[3] >= 0xe0 && buf[3] <= 0xef) || buf[3] == 0xdb);
}

static bool guess_webp(const unsigned char *buf, size_t size)
{
    unsigned char riff_sig[] = {0x52, 0x49, 0x46, 0x46};
    unsigned char webp_sig[] = {0x57, 0x45, 0x42, 0x50};
    return size >= 12 && memcmp(buf, riff_sig, sizeof(riff_sig)) == 0
        && memcmp(buf + 8, webp_sig, sizeof(webp_sig)) == 0;
}

DP_ImageFileType DP_image_guess(const unsigned char *buf, size_t size)
{
    if (guess_png(buf, size)) {
        return DP_IMAGE_FILE_TYPE_PNG;
    }
    else if (guess_jpeg(buf, size)) {
        return DP_IMAGE_FILE_TYPE_JPEG;
    }
    else if (guess_webp(buf, size)) {
        return DP_IMAGE_FILE_TYPE_WEBP;
    }
    else {
        return DP_IMAGE_FILE_TYPE_UNKNOWN;
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


struct DP_ImageInflateArgs {
    int width, height;
    size_t element_size;
    DP_Image *img;
};

static unsigned char *get_output_buffer(size_t out_size, void *user)
{
    struct DP_ImageInflateArgs *args = user;
    int width = args->width;
    int height = args->height;
    size_t expected_size =
        DP_int_to_size(width) * DP_int_to_size(height) * args->element_size;
    if (out_size == expected_size) {
        DP_Image *img = DP_image_new(width, height);
        args->img = img;
        return (unsigned char *)DP_image_pixels(img);
    }
    else {
        DP_error_set("Image decompression needs size %zu, but got %zu",
                     expected_size, out_size);
        return NULL;
    }
}

DP_Image *DP_image_new_from_compressed(int width, int height,
                                       const unsigned char *in, size_t in_size)
{
    struct DP_ImageInflateArgs args = {width, height, 4, NULL};
    if (DP_compress_inflate(in, in_size, get_output_buffer, &args)) {
#if defined(DP_BYTE_ORDER_LITTLE_ENDIAN)
        // Nothing else to do here.
#elif defined(DP_BYTE_ORDER_BIG_ENDIAN)
        // Gotta byte-swap the pixels.
        DP_Pixel8 *pixels = DP_image_pixels(args.img);
        for (int i = 0; i < width * height; ++i) {
            pixels[i].color = DP_swap_uint32(pixels[i].color);
        }
#else
#    error "Unknown byte order"
#endif
        return args.img;
    }
    else {
        DP_image_free(args.img);
        return NULL;
    }
}

DP_Image *DP_image_new_from_compressed_alpha_mask(int width, int height,
                                                  const unsigned char *in,
                                                  size_t in_size)
{
    struct DP_ImageInflateArgs args = {width, height, 1, NULL};
    if (DP_compress_inflate(in, in_size, get_output_buffer, &args)) {
        DP_Pixel8 *pixels = DP_image_pixels(args.img);
        for (int i = width * height - 1; i >= 0; --i) {
            pixels[i] = (DP_Pixel8){
                .b = 0,
                .g = 0,
                .r = 0,
                .a = ((unsigned char *)pixels)[i],
            };
        }
        return args.img;
    }
    else {
        DP_image_free(args.img);
        return NULL;
    }
}

// Monochrome MSB format: 1 bit per pixel, bytes packed with the most
// significant bit first, lines padded to 32 bit boundaries.

struct DP_MonochromeInflateArgs {
    int line_width;
    int line_count;
    uint8_t *buffer;
};

static unsigned char *get_monochrome_buffer(size_t out_size, void *user)
{
    struct DP_MonochromeInflateArgs *args = user;
    size_t expected_size =
        DP_int_to_size(args->line_width) * DP_int_to_size(args->line_count);
    if (out_size == expected_size) {
        uint8_t *buffer = DP_malloc(out_size);
        args->buffer = buffer;
        return buffer;
    }
    else {
        DP_error_set("Monochrome decompression needs size %zu, but got %zu",
                     expected_size, out_size);
        return NULL;
    }
}

static DP_Image *extract_monochrome(int width, int height, int line_width,
                                    const unsigned char *buffer)
{
    DP_Image *img = DP_image_new(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int byte_index = y * line_width + x / 8;
            int bit_mask = 1 << (7 - (x % 8)); // most significant bit first
            bool white = buffer[byte_index] & bit_mask;
            DP_Pixel8 pixel = {white ? 0xffffffff : 0x00000000};
            DP_image_pixel_at_set(img, x, y, pixel);
        }
    }
    return img;
}

DP_Image *DP_image_new_from_compressed_monochrome(int width, int height,
                                                  const unsigned char *in,
                                                  size_t in_size)
{
    int line_width = (width + 31) / 32 * 4;
    struct DP_MonochromeInflateArgs args = {line_width, height, NULL};
    DP_Image *img;
    if (DP_compress_inflate(in, in_size, get_monochrome_buffer, &args)) {
        img = extract_monochrome(width, height, line_width, args.buffer);
    }
    else {
        img = NULL;
    }
    DP_free(args.buffer);
    return img;
}


void DP_image_free(DP_Image *img)
{
    DP_free(img);
}


int DP_image_width(DP_Image *img)
{
    DP_ASSERT(img);
    return img->width;
}

int DP_image_height(DP_Image *img)
{
    DP_ASSERT(img);
    return img->height;
}

DP_Pixel8 *DP_image_pixels(DP_Image *img)
{
    DP_ASSERT(img);
    return img->pixels;
}

DP_Pixel8 DP_image_pixel_at(DP_Image *img, int x, int y)
{
    DP_ASSERT(img);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < img->width);
    DP_ASSERT(y < img->height);
    return img->pixels[y * img->width + x];
}

void DP_image_pixel_at_set(DP_Image *img, int x, int y, DP_Pixel8 pixel)
{
    DP_ASSERT(img);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < img->width);
    DP_ASSERT(y < img->height);
    img->pixels[y * img->width + x] = pixel;
}


static void copy_pixels(DP_Image *DP_RESTRICT dst, DP_Image *DP_RESTRICT src,
                        int dst_x, int dst_y, int src_x, int src_y,
                        int copy_width, int copy_height)
{
    DP_ASSERT(dst);
    DP_ASSERT(src);
    DP_ASSERT(dst_x >= 0);
    DP_ASSERT(dst_y >= 0);
    DP_ASSERT(src_x >= 0);
    DP_ASSERT(src_y >= 0);
    DP_ASSERT(copy_width >= 0);
    DP_ASSERT(copy_height >= 0);
    DP_ASSERT(dst_x + copy_width <= DP_image_width(dst));
    DP_ASSERT(src_x + copy_width <= DP_image_width(src));
    DP_ASSERT(dst_y + copy_height <= DP_image_height(dst));
    DP_ASSERT(src_y + copy_height <= DP_image_height(src));
    int dst_width = DP_image_width(dst);
    int src_width = DP_image_width(src);
    DP_Pixel8 *DP_RESTRICT dst_pixels = DP_image_pixels(dst);
    DP_Pixel8 *DP_RESTRICT src_pixels = DP_image_pixels(src);
    size_t row_size = sizeof(uint32_t) * DP_int_to_size(copy_width);
    for (int y = 0; y < copy_height; ++y) {
        int d = (y + dst_y) * dst_width + dst_x;
        int s = (y + src_y) * src_width + src_x;
        memcpy(dst_pixels + d, src_pixels + s, row_size);
    }
}

DP_Image *DP_image_new_subimage(DP_Image *img, int x, int y, int width,
                                int height)
{
    DP_ASSERT(img);
    DP_ASSERT(width >= 0);
    DP_ASSERT(height >= 0);
    DP_Image *sub = DP_image_new(width, height);
    int dst_x = x < 0 ? -x : 0;
    int dst_y = y < 0 ? -y : 0;
    int src_x = x > 0 ? x : 0;
    int src_y = y > 0 ? y : 0;
    int copy_width = DP_min_int(width - dst_x, DP_image_width(img) - src_x);
    int copy_height = DP_min_int(height - dst_y, DP_image_height(img) - src_y);
    copy_pixels(sub, img, dst_x, dst_y, src_x, src_y, copy_width, copy_height);
    return sub;
}


DP_Image *DP_image_transform_pixels(int src_width, int src_height,
                                    const DP_Pixel8 *src_pixels,
                                    DP_DrawContext *dc, const DP_Quad *dst_quad,
                                    int interpolation, int *out_offset_x,
                                    int *out_offset_y)
{
    DP_ASSERT(src_pixels);
    DP_ASSERT(dst_quad);
    DP_Quad src_quad =
        DP_quad_make(0, 0, src_width, 0, src_width, src_height, 0, src_height);

    DP_Rect dst_bounds = DP_quad_bounds(*dst_quad);
    int dst_bounds_x = DP_rect_x(dst_bounds);
    int dst_bounds_y = DP_rect_y(dst_bounds);
    DP_Quad translated_dst_quad =
        DP_quad_translate(*dst_quad, -dst_bounds_x, -dst_bounds_y);

    DP_MaybeTransform mtf =
        DP_transform_quad_to_quad(src_quad, translated_dst_quad);
    if (!mtf.valid) {
        DP_error_set("Image transform failed");
        return NULL;
    }

    int dst_width = DP_rect_width(dst_bounds);
    int dst_height = DP_rect_height(dst_bounds);
    // Weird distortions can cause the transform to be way oversized. It's not
    // going to fit into a message anyway, so we refuse to work with it.
    long long max_area = 35000000LL; // A bit larger than 8K, that's plenty.
    if (DP_int_to_llong(dst_width) * DP_int_to_llong(dst_height) > max_area) {
        DP_error_set("Image transform size out of bounds");
        return NULL;
    }

    DP_Image *dst_img = DP_image_new(dst_width, dst_height);
    if (!DP_image_transform_draw(src_width, src_height, src_pixels, dc, dst_img,
                                 mtf.tf, interpolation)) {
        DP_image_free(dst_img);
        return NULL;
    }

    if (out_offset_x) {
        *out_offset_x = dst_bounds_x;
    }
    if (out_offset_y) {
        *out_offset_y = dst_bounds_y;
    }
    return dst_img;
}

DP_Image *DP_image_transform(DP_Image *img, DP_DrawContext *dc,
                             const DP_Quad *dst_quad, int interpolation,
                             int *out_offset_x, int *out_offset_y)
{
    DP_ASSERT(img);
    DP_ASSERT(dst_quad);
    return DP_image_transform_pixels(DP_image_width(img), DP_image_height(img),
                                     DP_image_pixels(img), dc, dst_quad,
                                     interpolation, out_offset_x, out_offset_y);
}

static void thumbnail_scale(int width, int height, int max_width,
                            int max_height, int *out_width, int *out_height)
{
    int w = max_height * width / height;
    if (w <= max_width) {
        *out_width = DP_max_int(w, 1);
        *out_height = DP_max_int(max_height, 1);
    }
    else {
        *out_width = DP_max_int(max_width, 1);
        *out_height = DP_max_int(max_width * height / width, 1);
    }
}

bool DP_image_thumbnail(DP_Image *img, DP_DrawContext *dc, int max_width,
                        int max_height, DP_Image **out_thumb)
{
    DP_ASSERT(img);
    DP_ASSERT(out_thumb);
    DP_ASSERT(dc);
    DP_ASSERT(max_width > 0);
    DP_ASSERT(max_height > 0);
    int width = DP_image_width(img);
    int height = DP_image_height(img);
    if (width > max_width || height > max_height) {
        int thumb_width, thumb_height;
        thumbnail_scale(width, height, max_width, max_height, &thumb_width,
                        &thumb_height);
        DP_Image *thumb = DP_image_new(thumb_width, thumb_height);

        DP_Transform tf = DP_transform_scale(
            DP_transform_identity(),
            DP_int_to_double(thumb_width) / DP_int_to_double(width),
            DP_int_to_double(thumb_height) / DP_int_to_double(height));

        if (DP_image_transform_draw(width, height, DP_image_pixels(img), dc,
                                    thumb, tf,
                                    DP_MSG_TRANSFORM_REGION_MODE_BILINEAR)) {
            *out_thumb = thumb;
            return true;
        }
        else {
            DP_image_free(thumb);
            *out_thumb = NULL;
            return false;
        }
    }
    else {
        *out_thumb = NULL;
        return true;
    }
}

bool DP_image_same_pixel(DP_Image *img, DP_Pixel8 *out_pixel)
{
    DP_Pixel8 *pixels = DP_image_pixels(img);
    DP_Pixel8 pixel = pixels[0];
    size_t count = DP_int_to_size(DP_image_width(img))
                 * DP_int_to_size(DP_image_height(img));
    for (size_t i = 1; i < count; ++i) {
        if (pixels[i].color != pixel.color) {
            return false;
        }
    }
    if (out_pixel) {
        *out_pixel = pixel;
    }
    return true;
}

static DP_UPixelFloat sample_dab_color(int width, int height,
                                       DP_ImageGetPixelFn get_pixel, void *user,
                                       DP_BrushStamp stamp, bool opaque)
{
    int diameter = stamp.diameter;
    int right = DP_min_int(stamp.left + diameter, width);
    int bottom = DP_min_int(stamp.top + diameter, height);

    int y = DP_max_int(0, stamp.top);
    int yb = stamp.top < 0 ? -stamp.top : 0; // y in relation to brush origin
    int x0 = DP_max_int(0, stamp.left);
    int xb0 = stamp.left < 0 ? -stamp.left : 0;

    float weight = 0.0;
    float red = 0.0;
    float green = 0.0;
    float blue = 0.0;
    float alpha = 0.0;

    // collect weighted color sums
    while (y < bottom) {
        int x = x0;
        int xb = xb0; // x in relation to brush origin
        while (x < right) {
            uint16_t m = stamp.data[yb * stamp.diameter + xb];
            DP_Pixel8 p = get_pixel(user, x, y);
            // When working in opaque mode, disregard low alpha values because
            // the resulting unpremultiplied colors are just too inacurrate.
            if (!opaque || (m > 512 && p.a > 3)) {
                float mf = DP_uint16_to_float(m) / (float)DP_BIT15;
                weight += mf;
                red += mf * DP_uint8_to_float(p.r) / 255.0f;
                green += mf * DP_uint8_to_float(p.g) / 255.0f;
                blue += mf * DP_uint8_to_float(p.b) / 255.0f;
                alpha += mf * DP_uint8_to_float(p.a) / 255.0f;
            }
            ++x;
            ++xb;
        }
        ++y;
        ++yb;
    }

    return DP_paint_sample_to_upixel(diameter, weight, red, green, blue, alpha);
}

DP_UPixelFloat DP_image_sample_color_at_with(int width, int height,
                                             DP_ImageGetPixelFn get_pixel,
                                             void *user, uint16_t *stamp_buffer,
                                             int x, int y, int diameter,
                                             bool opaque,
                                             int *in_out_last_diameter)
{
    DP_ASSERT(get_pixel);
    if (diameter < 2) {
        if (x >= 0 && y >= 0 && x < width && y < height) {
            DP_Pixel8 pixel = get_pixel(user, x, y);
            return DP_upixel8_to_float(DP_pixel8_unpremultiply(pixel));
        }
        else {
            return DP_upixel_float_zero();
        }
    }
    else {
        int last_diameter;
        if (in_out_last_diameter) {
            last_diameter = *in_out_last_diameter;
            *in_out_last_diameter = diameter;
        }
        else {
            last_diameter = -1;
        }
        DP_BrushStamp stamp = DP_paint_color_sampling_stamp_make(
            stamp_buffer, diameter, x, y, last_diameter);
        return sample_dab_color(width, height, get_pixel, user, stamp, opaque);
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
