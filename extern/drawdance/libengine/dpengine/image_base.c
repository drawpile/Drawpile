/*
 * Copyright (c) 2022 - 2023 askmeaboutloom
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
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


// This file is necessary as a circular chainsaw. The Qt image implementation
// needs to construct images, but images are part of dpengine, which requires
// the Qt image implementation. So the base functions to deal with images are in
// this file, which is included as part of the Qt implementation.


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
