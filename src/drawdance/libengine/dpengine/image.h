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
#ifndef DPENGINE_IMAGE_H
#define DPENGINE_IMAGE_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Input DP_Input;
typedef struct DP_Output DP_Output;
typedef struct DP_Quad DP_Quad;

// A bit larger than 8K. TODO: make this limit configurable in sessions.
#define DP_IMAGE_TRANSFORM_MAX_AREA 35000000LL

typedef enum DP_ImageFileType {
    DP_IMAGE_FILE_TYPE_GUESS,
    DP_IMAGE_FILE_TYPE_PNG,
    DP_IMAGE_FILE_TYPE_JPEG,
    DP_IMAGE_FILE_TYPE_WEBP,
    DP_IMAGE_FILE_TYPE_UNKNOWN = DP_IMAGE_FILE_TYPE_GUESS,
} DP_ImageFileType;

typedef enum DP_ImageScaleInterpolation {
    DP_IMAGE_SCALE_INTERPOLATION_FAST_BILINEAR = -1,
    DP_IMAGE_SCALE_INTERPOLATION_BILINEAR = -2,
    DP_IMAGE_SCALE_INTERPOLATION_BICUBIC = -3,
    DP_IMAGE_SCALE_INTERPOLATION_EXPERIMENTAL = -4,
    DP_IMAGE_SCALE_INTERPOLATION_NEAREST = -5,
    DP_IMAGE_SCALE_INTERPOLATION_AREA = -6,
    DP_IMAGE_SCALE_INTERPOLATION_BICUBLIN = -7,
    DP_IMAGE_SCALE_INTERPOLATION_GAUSS = -8,
    DP_IMAGE_SCALE_INTERPOLATION_SINC = -9,
    DP_IMAGE_SCALE_INTERPOLATION_LANCZOS = -10,
    DP_IMAGE_SCALE_INTERPOLATION_SPLINE = -11,
} DP_ImageScaleInterpolation;

typedef struct DP_Image DP_Image;

typedef DP_Pixel8 (*DP_ImageGetPixelFn)(void *user, int x, int y);

DP_Image *DP_image_new(int width, int height);

DP_ImageFileType DP_image_guess(const unsigned char *buf, size_t size);

DP_Image *DP_image_new_from_compressed(int width, int height,
                                       const unsigned char *in, size_t in_size);

DP_Image *DP_image_new_from_compressed_alpha_mask(int width, int height,
                                                  const unsigned char *in,
                                                  size_t in_size);

DP_Image *DP_image_new_from_compressed_monochrome(int width, int height,
                                                  const unsigned char *in,
                                                  size_t in_size);

DP_Image *DP_image_new_subimage(DP_Image *img, int x, int y, int width,
                                int height);

void DP_image_free(DP_Image *img);


int DP_image_width(DP_Image *img);

int DP_image_height(DP_Image *img);

DP_Pixel8 *DP_image_pixels(DP_Image *img);

DP_Pixel8 DP_image_pixel_at(DP_Image *img, int x, int y);

void DP_image_pixel_at_set(DP_Image *img, int x, int y, DP_Pixel8 pixel);


DP_Image *DP_image_transform_pixels(int src_width, int src_height,
                                    const DP_Pixel8 *src_pixels,
                                    DP_DrawContext *dc, const DP_Quad *dst_quad,
                                    int interpolation, bool check_bounds,
                                    int *out_offset_x, int *out_offset_y);

DP_Image *DP_image_transform(DP_Image *img, DP_DrawContext *dc,
                             const DP_Quad *dst_quad, int interpolation,
                             int *out_offset_x, int *out_offset_y);

void DP_image_thumbnail_dimensions(int width, int height, int max_width,
                                   int max_height, int *out_width,
                                   int *out_height);

// Creates a scaled-down thumbnail of the given `img` if it doesn't fit into the
// given maximum dimensions. Return value and the value filled into `out_thumb`
// will be as follows:
// | return value | *out_thumb | meaning                          |
// |--------------|------------|----------------------------------|
// | true         | NULL       | img is already thumbnail-sized   |
// | true         | non-NULL   | thumbnail generated successfully |
// | false        | NULL       | thumbnail generation failed      |
bool DP_image_thumbnail(DP_Image *img, DP_DrawContext *dc, int max_width,
                        int max_height, int interpolation,
                        DP_Image **out_thumb) DP_MUST_CHECK;

DP_Image *DP_image_scale_pixels(int src_width, int src_height,
                                const DP_Pixel8 *src_pixels, DP_DrawContext *dc,
                                int width, int height, int interpolation);

DP_Image *DP_image_scale(DP_Image *img, DP_DrawContext *dc, int width,
                         int height, int interpolation);

bool DP_image_same_pixel(DP_Image *img, DP_Pixel8 *out_pixel);

DP_UPixelFloat DP_image_sample_color_at_with(int width, int height,
                                             DP_ImageGetPixelFn get_pixel,
                                             void *user, uint16_t *stamp_buffer,
                                             int x, int y, int diameter,
                                             bool opaque,
                                             int *in_out_last_diameter);


#endif
