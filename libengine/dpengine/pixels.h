/*
 * Copyright (C) 2022 askmeaboutloom
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


typedef union DP_Pixel {
    uint32_t color;
    struct {
#if DP_BYTE_ORDER == DP_LITTLE_ENDIAN
        uint8_t b, g, r, a;
#elif DP_BYTE_ORDER == DP_BIG_ENDIAN
        uint8_t a, r, g, b;
#else
#    error "Unknown byte order"
#endif
    };
} DP_Pixel;


DP_Pixel DP_pixel_unpremultiply(DP_Pixel pixel);

DP_Pixel DP_pixel_premultiply(DP_Pixel pixel);


void DP_pixels_composite_mask(DP_Pixel *dst, DP_Pixel src, int blend_mode,
                              uint8_t *mask, int w, int h, int mask_skip,
                              int base_skip);


void DP_pixels_composite(DP_Pixel *dst, DP_Pixel *src, int pixel_count,
                         uint8_t opacity, int blend_mode);


#endif
