/*
 * Copyright (C) 2022 - 2023 askmeaboutloom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#ifndef DPENGINE_PIXELS_H
#define DPENGINE_PIXELS_H
#include <dpcommon/common.h>
#include <dpcommon/endianness.h>


#define DP_BIT15 (1 << 15)

// Premultiplied 8 bit pixel
typedef union DP_Pixel8 {
    uint32_t color;
    struct {
#if defined(DP_BYTE_ORDER_LITTLE_ENDIAN)
        uint8_t b, g, r, a;
#elif defined(DP_BYTE_ORDER_BIG_ENDIAN)
        uint8_t a, r, g, b;
#else
#    error "Unknown byte order"
#endif
    };
} DP_Pixel8;

// Unpremultiplied 8 bit pixel
typedef union DP_UPixel8 {
    uint32_t color;
    struct {
#if defined(DP_BYTE_ORDER_LITTLE_ENDIAN)
        uint8_t b, g, r, a;
#elif defined(DP_BYTE_ORDER_BIG_ENDIAN)
        uint8_t a, r, g, b;
#else
#    error "Unknown byte order"
#endif
    };
} DP_UPixel8;

// Premultiplied 15 bit pixel
typedef struct DP_Pixel15 {
    uint16_t b, g, r, a;
} DP_Pixel15;

// Unpremultiplied 15 bit pixel
typedef struct DP_UPixel15 {
    uint16_t b, g, r, a;
} DP_UPixel15;

// Unpremultiplied float pixel
typedef struct DP_UPixelFloat {
    float b, g, r, a;
} DP_UPixelFloat;


uint16_t DP_fix15_mul(uint16_t a, uint16_t b);

uint16_t DP_channel8_to_15(uint8_t c);
uint8_t DP_channel15_to_8(uint16_t c);
float DP_channel8_to_float(uint8_t c);
float DP_channel15_to_float(uint16_t c);
uint8_t DP_channel_float_to_8(float c);
uint16_t DP_channel_float_to_15(float c);

DP_Pixel15 DP_pixel8_to_15(DP_Pixel8 pixel);
DP_Pixel8 DP_pixel15_to_8(DP_Pixel15 pixel);
DP_UPixel15 DP_upixel8_to_15(DP_UPixel8 pixel);
DP_UPixelFloat DP_upixel8_to_float(DP_UPixel8 pixel);
DP_UPixelFloat DP_upixel15_to_float(DP_UPixel15 pixel);
DP_UPixel8 DP_upixel_float_to_8(DP_UPixelFloat pixel);

void DP_pixels8_to_15(DP_Pixel15 *dst, const DP_Pixel8 *src, int count);
void DP_pixels15_to_8(DP_Pixel8 *dst, const DP_Pixel15 *src, int count);

// Checks the color channel of each source pixel if it's less than or equal to
// the alpha channel and clamps them if necessary.
void DP_pixels8_to_15_checked(DP_Pixel15 *dst, const DP_Pixel8 *src, int count);

DP_UPixel8 DP_pixel8_unpremultiply(DP_Pixel8 pixel);
DP_UPixel15 DP_pixel15_unpremultiply(DP_Pixel15 pixel);

DP_Pixel8 DP_pixel8_premultiply(DP_UPixel8 pixel);
DP_Pixel15 DP_pixel15_premultiply(DP_UPixel15 pixel);


DP_INLINE DP_Pixel15 DP_pixel15_zero(void)
{
    DP_Pixel15 pixel = {0, 0, 0, 0};
    return pixel;
}

DP_INLINE DP_UPixel15 DP_upixel15_zero(void)
{
    DP_UPixel15 pixel = {0, 0, 0, 0};
    return pixel;
}

DP_INLINE DP_UPixel15 DP_upixel15_from_color(uint32_t color)
{
    DP_UPixel8 pixel = {color};
    return DP_upixel8_to_15(pixel);
}

DP_INLINE DP_UPixelFloat DP_upixel_float_from_color(uint32_t color)
{
    DP_UPixel8 pixel = {color};
    return DP_upixel8_to_float(pixel);
}


void DP_blend_mask(DP_Pixel15 *dst, DP_UPixel15 src, int blend_mode,
                   const uint16_t *mask, uint16_t opacity, int w, int h,
                   int mask_skip, int base_skip);


void DP_blend_pixels(DP_Pixel15 *dst, DP_Pixel15 *src, int pixel_count,
                     uint16_t opacity, int blend_mode);


void DP_posterize_mask(DP_Pixel15 *dst, int posterize_num, const uint16_t *mask,
                       uint16_t opacity, int w, int h, int mask_skip,
                       int base_skip);


#endif
