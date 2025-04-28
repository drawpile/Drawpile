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
 * Most of this code is based on Drawpile, using it under the GNU General
 * Public License, version 3. See 3rdparty/licenses/drawpile/COPYING for
 * details.
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on MyPaint, using it under the GNU General
 * Public License, version 3. See 3rdparty/licenses/mypaint/COPYING for
 * details.
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on libmypaint, using it under the MIT license.
 * See 3rdparty/libmypaint/COPYING for details.
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on GIMP, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/gimp/COPYING for details.
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on the Qt framework's raster paint engine
 * implementation, using it under the GNU General Public License, version 3.
 * See 3rdparty/licenses/qt/license.GPL3 for details.
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on Krita, using it under the GNU General
 * Public License, version 3. See 3rdparty/licenses/krita/COPYING.txt for
 * details.
 */
#include "pixels.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/cpu.h>
#include <dpmsg/blend_mode.h>
#include <fastapprox/fastpow.h>
#include <math.h>

static_assert(sizeof(DP_Pixel8) == sizeof(uint32_t), "DP_Pixel8 is 32 bits");
static_assert(sizeof(DP_UPixel8) == sizeof(uint32_t), "DP_UPixel8 is 32 bits");
static_assert(sizeof(uint32_t) == 4, "uint32_t is 4 bytes long");


typedef uint_fast32_t Fix15;
typedef int_fast32_t IFix15;

#define BIT15_U16    ((uint16_t)DP_BIT15)
#define BIT15_FLOAT  ((float)DP_BIT15)
#define BIT15_DOUBLE ((double)DP_BIT15)
#define BIT15_FIX    ((Fix15)DP_BIT15)
#define BIT15_IFIX   ((IFix15)DP_BIT15)

#define FIX_1         ((Fix15)1)
#define FIX_2         ((Fix15)2)
#define FIX_4         ((Fix15)4)
#define FIX_12        ((Fix15)12)
#define FIX_15        ((Fix15)15)
#define FIX_16        ((Fix15)16)
#define FIX_255       ((Fix15)255)
#define BIT15_INC_FIX ((Fix15)(DP_BIT15 + 1))

// Fudge factor of 64 * 255 + 64 added to 15 bit channels when converting them
// to 8 bit channels to round instead of flooring them.
#define FUDGE15_TO_8 16384

typedef struct BGR15 {
    Fix15 b, g, r;
} BGR15;

typedef struct IBGR15 {
    IFix15 b, g, r;
} IBGR15;

typedef struct BGRA15 {
    union {
        BGR15 bgr;
        struct {
            Fix15 b, g, r;
        };
    };
    Fix15 a;
} BGRA15;

typedef struct BGRf {
    float b, g, r;
} BGRf;

typedef struct BGRAf {
    union {
        BGRf bgr;
        struct {
            float b, g, r;
        };
    };
    float a;
} BGRAf;


static Fix15 to_fix(uint16_t x)
{
    return (Fix15)x;
}

static uint16_t from_fix(Fix15 x)
{
    DP_ASSERT(x <= BIT15_FIX);
    return (uint16_t)x;
}

static Fix15 fix15_min(Fix15 a, Fix15 b)
{
    return a < b ? a : b;
}

static IFix15 ifix15_min(IFix15 a, IFix15 b)
{
    return a < b ? a : b;
}

static Fix15 fix15_max(Fix15 a, Fix15 b)
{
    return a > b ? a : b;
}

static IFix15 ifix15_max(IFix15 a, IFix15 b)
{
    return a > b ? a : b;
}

static Fix15 fix15_clamp(Fix15 x)
{
    return x > BIT15_FIX ? BIT15_FIX : x;
}

static Fix15 ifix15_clamp(IFix15 x)
{
    return x < 0 ? 0 : x > BIT15_IFIX ? BIT15_FIX : (Fix15)x;
}

// Adapted from MyPaint, see license above.
static Fix15 fix15_mul(Fix15 a, Fix15 b)
{
    return (a * b) >> FIX_15;
}

// Adapted from MyPaint, see license above.
static Fix15 fix15_div(Fix15 a, Fix15 b)
{
    return (a << FIX_15) / b;
}

static Fix15 fix15_clamp_div(Fix15 a, Fix15 b)
{
    return fix15_clamp(fix15_div(a, b));
}

static Fix15 fix15_lerp(Fix15 a, Fix15 b, Fix15 k)
{
    if (a < b) {
        return a + fix15_mul(b - a, k);
    }
    else {
        return b + fix15_mul(a - b, BIT15_FIX - k);
    }
}

// Adapted from MyPaint, see license above.
static Fix15 fix15_sumprods(Fix15 a1, Fix15 a2, Fix15 b1, Fix15 b2)
{
    return ((a1 * a2) + (b1 * b2)) >> FIX_15;
}

// Adapted from MyPaint, see license above.
static Fix15 fix15_sqrt(Fix15 x)
{
    // Babylonian method of calculating a square root. The lookup table is
    // calculated via floor(((i / 16.0) ** 0.5) * (1<<16)) - 1 for i in 1..=17.
    static const Fix15 lookup[] = {
        16383, 23169, 28376, 32767, 36634, 40131, 43346, 46339,
        49151, 51809, 54338, 56754, 59072, 61302, 63453, 65535,
    };
    if (x == 0 || x == BIT15_FIX) {
        return x;
    }
    else {
        Fix15 s = x << FIX_1;
        Fix15 n = lookup[s >> FIX_12];
        Fix15 n_old;
        for (int i = 0; i < 15; ++i) {
            n_old = n;
            n += (s << FIX_16) / n;
            n >>= FIX_1;
            if (n == n_old || ((n > n_old) && (n - 1 == n_old))
                || ((n < n_old) && (n + 1 == n_old))) {
                break;
            }
        }
        return n >> FIX_1;
    }
}

// Adapted from MyPaint, see license above.
#define LUM_R (0.3 * BIT15_DOUBLE)
#define LUM_G (0.59 * BIT15_DOUBLE)
#define LUM_B (0.11 * BIT15_DOUBLE)

#define LUM_T(T, BGR)                                                     \
    ((((T)LUM_B) * (BGR).b + ((T)LUM_G) * (BGR).g + ((T)LUM_R) * (BGR.r)) \
     / (T)DP_BIT15)

static Fix15 lum(BGR15 bgr)
{
    return LUM_T(Fix15, bgr);
}

// Adapted from MyPaint, see license above.
static BGR15 clip_color(IBGR15 bgr)
{
    IFix15 l = LUM_T(IFix15, bgr);
    IFix15 n = ifix15_min(bgr.b, ifix15_min(bgr.g, bgr.r));
    IFix15 x = ifix15_max(bgr.b, ifix15_max(bgr.g, bgr.r));
    if (n < 0) {
        bgr.b = l + (((bgr.b - l) * l) / (l - n));
        bgr.g = l + (((bgr.g - l) * l) / (l - n));
        bgr.r = l + (((bgr.r - l) * l) / (l - n));
    }
    if (x > BIT15_IFIX) {
        bgr.b = l + (((bgr.b - l) * (BIT15_IFIX - l)) / (x - l));
        bgr.g = l + (((bgr.g - l) * (BIT15_IFIX - l)) / (x - l));
        bgr.r = l + (((bgr.r - l) * (BIT15_IFIX - l)) / (x - l));
    }
    return (BGR15){
        .b = (Fix15)bgr.b,
        .g = (Fix15)bgr.g,
        .r = (Fix15)bgr.r,
    };
}

// Adapted from MyPaint, see license above.
static BGR15 set_lum(BGR15 bgr, Fix15 l)
{
    IFix15 d = ((IFix15)l) - ((IFix15)lum(bgr));
    return clip_color((IBGR15){
        .b = ((IFix15)bgr.b) + d,
        .g = ((IFix15)bgr.g) + d,
        .r = ((IFix15)bgr.r) + d,
    });
}

// Adapted from MyPaint, see license above.
static Fix15 sat(BGR15 bgr)
{
    return fix15_max(bgr.b, fix15_max(bgr.g, bgr.r))
         - fix15_min(bgr.b, fix15_min(bgr.g, bgr.r));
}

// Adapted from MyPaint, see license above.
static BGR15 set_sat(BGR15 bgr, Fix15 s)
{
    Fix15 *max = &bgr.b;
    Fix15 *mid = &bgr.g;
    Fix15 *min = &bgr.r;
    if (*max < *mid) {
        Fix15 *tmp = max;
        max = mid;
        mid = tmp;
    }
    if (*max < *min) {
        Fix15 *tmp = max;
        max = min;
        min = tmp;
    }
    if (*mid < *min) {
        Fix15 *tmp = mid;
        mid = min;
        min = tmp;
    }

    if (*max > *min) {
        *mid = ((*mid - *min) * s) / (*max - *min);
        *max = s;
    }
    else {
        *mid = 0;
        *max = 0;
    }
    *min = 0;
    return bgr;
}

// Adapted from MyPaint, see license above.
// Composites an unpremultiplied source over a premultiplied destination.
static DP_Pixel15 source_over_premultiplied(DP_Pixel15 b, BGRA15 s)
{
    Fix15 as1 = BIT15_FIX - s.a;
    return (DP_Pixel15){
        .b = from_fix(fix15_clamp(fix15_sumprods(s.a, s.b, as1, b.b))),
        .g = from_fix(fix15_clamp(fix15_sumprods(s.a, s.g, as1, b.g))),
        .r = from_fix(fix15_clamp(fix15_sumprods(s.a, s.r, as1, b.r))),
        .a = from_fix(fix15_clamp(s.a + fix15_mul(b.a, as1))),
    };
}


static BGR15 to_bgr(DP_Pixel15 pixel)
{
    return (BGR15){
        .b = to_fix(pixel.b),
        .g = to_fix(pixel.g),
        .r = to_fix(pixel.r),
    };
}

static BGR15 to_bgr_opacity(DP_Pixel15 pixel, Fix15 opacity)
{
    if (opacity == DP_BIT15) {
        return to_bgr(pixel);
    }
    else {
        return (BGR15){
            .b = fix15_mul(to_fix(pixel.b), opacity),
            .g = fix15_mul(to_fix(pixel.g), opacity),
            .r = fix15_mul(to_fix(pixel.r), opacity),
        };
    }
}

static BGR15 to_ubgr(DP_UPixel15 pixel)
{
    return (BGR15){
        .b = to_fix(pixel.b),
        .g = to_fix(pixel.g),
        .r = to_fix(pixel.r),
    };
}

static BGR15 to_ubgr_opacity(DP_UPixel15 pixel, Fix15 opacity)
{
    if (opacity == DP_BIT15) {
        return to_ubgr(pixel);
    }
    else {
        return (BGR15){
            .b = fix15_mul(to_fix(pixel.b), opacity),
            .g = fix15_mul(to_fix(pixel.g), opacity),
            .r = fix15_mul(to_fix(pixel.r), opacity),
        };
    }
}

static BGRA15 to_bgra(DP_Pixel15 pixel)
{
    return (BGRA15){
        .bgr = to_bgr(pixel),
        .a = to_fix(pixel.a),
    };
}

static BGRA15 to_ubgra(DP_UPixel15 pixel)
{
    return (BGRA15){
        .bgr = to_ubgr(pixel),
        .a = to_fix(pixel.a),
    };
}

static DP_Pixel15 from_bgra(BGRA15 bgra)
{
    return (DP_Pixel15){
        .b = from_fix(bgra.b),
        .g = from_fix(bgra.g),
        .r = from_fix(bgra.r),
        .a = from_fix(bgra.a),
    };
}

static DP_Pixel15 from_ubgra(BGRA15 bgra)
{
    return DP_pixel15_premultiply((DP_UPixel15){
        .b = from_fix(bgra.b),
        .g = from_fix(bgra.g),
        .r = from_fix(bgra.r),
        .a = from_fix(bgra.a),
    });
}

static BGR15 bgr_mul(BGR15 bgr, Fix15 a)
{
    if (a == DP_BIT15) {
        return bgr;
    }
    else if (a == 0) {
        return (BGR15){0, 0, 0};
    }
    else {
        return (BGR15){
            .b = fix15_mul(bgr.b, a),
            .g = fix15_mul(bgr.g, a),
            .r = fix15_mul(bgr.r, a),
        };
    }
}

static BGRA15 bgra_mul(BGRA15 bgra, Fix15 a)
{
    if (a == DP_BIT15) {
        return bgra;
    }
    else if (a == 0) {
        return (BGRA15){.b = 0, .g = 0, .r = 0, .a = 0};
    }
    else {
        return (BGRA15){
            .b = fix15_mul(bgra.b, a),
            .g = fix15_mul(bgra.g, a),
            .r = fix15_mul(bgra.r, a),
            .a = fix15_mul(bgra.a, a),
        };
    }
}


uint16_t DP_fix15_mul(uint16_t a, uint16_t b)
{
    return from_fix(fix15_mul(to_fix(a), to_fix(b)));
}

uint16_t DP_channel8_to_15(uint8_t c)
{
    return (uint16_t)(((Fix15)c << FIX_15) / FIX_255);
}

uint8_t DP_channel15_to_8(uint16_t c)
{
    return (uint8_t)(((Fix15)c * FIX_255 + (Fix15)FUDGE15_TO_8) >> FIX_15);
}

float DP_channel8_to_float(uint8_t c)
{
    return DP_uint8_to_float(c) / 255.0f;
}

float DP_channel15_to_float(uint16_t c)
{
    return DP_uint16_to_float(c) / BIT15_FLOAT;
}

float DP_channel15_to_float_round8(uint16_t c)
{
    return floorf(DP_uint16_to_float(c) / BIT15_FLOAT * 255.0f + 0.5f) / 255.0f;
}

uint8_t DP_channel_float_to_8(float c)
{
    return DP_float_to_uint8(c * 255.0f + 0.5f);
}

uint16_t DP_channel_float_to_15(float c)
{
    return DP_float_to_uint16(c * BIT15_FLOAT + 0.5f);
}

DP_Pixel15 DP_pixel8_to_15(DP_Pixel8 pixel)
{
    return (DP_Pixel15){
        .b = DP_channel8_to_15(pixel.b),
        .g = DP_channel8_to_15(pixel.g),
        .r = DP_channel8_to_15(pixel.r),
        .a = DP_channel8_to_15(pixel.a),
    };
}

DP_Pixel8 DP_pixel15_to_8(DP_Pixel15 pixel)
{
    return (DP_Pixel8){
        .b = DP_channel15_to_8(pixel.b),
        .g = DP_channel15_to_8(pixel.g),
        .r = DP_channel15_to_8(pixel.r),
        .a = DP_channel15_to_8(pixel.a),
    };
}

DP_UPixel15 DP_upixel8_to_15(DP_UPixel8 pixel)
{
    return (DP_UPixel15){
        .b = DP_channel8_to_15(pixel.b),
        .g = DP_channel8_to_15(pixel.g),
        .r = DP_channel8_to_15(pixel.r),
        .a = DP_channel8_to_15(pixel.a),
    };
}

DP_UPixel8 DP_upixel15_to_8(DP_UPixel15 pixel)
{
    return (DP_UPixel8){
        .b = DP_channel15_to_8(pixel.b),
        .g = DP_channel15_to_8(pixel.g),
        .r = DP_channel15_to_8(pixel.r),
        .a = DP_channel15_to_8(pixel.a),
    };
}

DP_UPixelFloat DP_upixel8_to_float(DP_UPixel8 pixel)
{
    return (DP_UPixelFloat){
        .b = DP_channel8_to_float(pixel.b),
        .g = DP_channel8_to_float(pixel.g),
        .r = DP_channel8_to_float(pixel.r),
        .a = DP_channel8_to_float(pixel.a),
    };
}

DP_UPixelFloat DP_upixel15_to_float(DP_UPixel15 pixel)
{
    return (DP_UPixelFloat){
        .b = DP_channel15_to_float(pixel.b),
        .g = DP_channel15_to_float(pixel.g),
        .r = DP_channel15_to_float(pixel.r),
        .a = DP_channel15_to_float(pixel.a),
    };
}

DP_UPixelFloat DP_upixel15_to_float_round8(DP_UPixel15 pixel)
{
    return (DP_UPixelFloat){
        .b = DP_channel15_to_float_round8(pixel.b),
        .g = DP_channel15_to_float_round8(pixel.g),
        .r = DP_channel15_to_float_round8(pixel.r),
        .a = DP_channel15_to_float_round8(pixel.a),
    };
}

DP_UPixel8 DP_upixel_float_to_8(DP_UPixelFloat pixel)
{
    return (DP_UPixel8){
        .b = DP_channel_float_to_8(pixel.b),
        .g = DP_channel_float_to_8(pixel.g),
        .r = DP_channel_float_to_8(pixel.r),
        .a = DP_channel_float_to_8(pixel.a),
    };
}

void DP_pixels8_to_15(DP_Pixel15 *dst, const DP_Pixel8 *src, int count)
{
    DP_ASSERT(count <= 0 || dst);
    DP_ASSERT(count <= 0 || src);
    for (int i = 0; i < count; ++i) {
        dst[i] = DP_pixel8_to_15(src[i]);
    }
}

void DP_pixels8_to_15_checked(DP_Pixel15 *dst, const DP_Pixel8 *src, int count)
{
    DP_ASSERT(count <= 0 || dst);
    DP_ASSERT(count <= 0 || src);
    for (int i = 0; i < count; ++i) {
        DP_Pixel8 pixel = src[i];
        uint8_t a = pixel.a;
        if (pixel.b > a) {
            pixel.b = a;
        }
        if (pixel.g > a) {
            pixel.g = a;
        }
        if (pixel.r > a) {
            pixel.r = a;
        }
        dst[i] = DP_pixel8_to_15(pixel);
    }
}

void DP_pixels15_to_8(DP_Pixel8 *dst, const DP_Pixel15 *src, int count)
{
    DP_ASSERT(count <= 0 || dst);
    DP_ASSERT(count <= 0 || src);
    for (int i = 0; i < count; ++i) {
        dst[i] = DP_pixel15_to_8(src[i]);
    }
}

void DP_pixels15_to_8_unpremultiply(DP_UPixel8 *dst, const DP_Pixel15 *src,
                                    int count)
{
    DP_ASSERT(count <= 0 || dst);
    DP_ASSERT(count <= 0 || src);
    for (int i = 0; i < count; ++i) {
        dst[i] = DP_upixel15_to_8(DP_pixel15_unpremultiply(src[i]));
    }
}


#ifdef DP_CPU_X64
DP_TARGET_BEGIN("sse4.2")
static void pixels15_to_8_sse42(DP_Pixel8 *dst, const DP_Pixel15 *src)
{
    for (int i = 0; i < DP_TILE_LENGTH; i += 4) {
        __m128i source1 = _mm_load_si128((void *)&src[i]);
        __m128i source2 = _mm_load_si128((void *)&src[i + 2]);

        // Convert 2x(8x16bit) to 4x(4x32bit)
        __m128i p1 = _mm_cvtepu16_epi32(source1);
        __m128i p2 = _mm_cvtepu16_epi32(_mm_srli_si128(source1, 8));
        __m128i p3 = _mm_cvtepu16_epi32(source2);
        __m128i p4 = _mm_cvtepu16_epi32(_mm_srli_si128(source2, 8));

        // Convert 15bit pixels to 8bit pixels. (p * 255 + 16384) >> 15
        __m128i _255 = _mm_set1_epi32(255);
        __m128i fudge = _mm_set1_epi32(FUDGE15_TO_8);
        p1 =
            _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(p1, _255), fudge), 15);
        p2 =
            _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(p2, _255), fudge), 15);
        p3 =
            _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(p3, _255), fudge), 15);
        p4 =
            _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(p4, _255), fudge), 15);

        // Shift every first byte of every 32bit spot to an empty spot.
        p2 = _mm_slli_si128(p2, 1);
        p3 = _mm_slli_si128(p3, 2);
        p4 = _mm_slli_si128(p4, 3);

        // OR all of the bytes into a single register.
        __m128i combined =
            _mm_or_si128(p1, _mm_or_si128(p2, _mm_or_si128(p3, p4)));

        // Shuffle the bytes into the right order.
        __m128i out = _mm_shuffle_epi8(
            combined, _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3,
                                    7, 11, 15));

        _mm_store_si128((void *)&dst[i], out);
    }
}
DP_TARGET_END

DP_TARGET_BEGIN("avx2")
static void pixels15_to_8_avx2(DP_Pixel8 *dst, const DP_Pixel15 *src)
{
    for (int i = 0; i < DP_TILE_LENGTH; i += 8) {
        __m256i source1 = _mm256_load_si256((void *)&src[i]);
        __m256i source2 = _mm256_load_si256((void *)&src[i + 4]);

        // Convert 2x(16x16bit) to 4x(8x32bit)
        __m256i p1 = _mm256_and_si256(source1, _mm256_set1_epi32(0xffff));
        __m256i p2 = _mm256_srli_epi32(
            _mm256_and_si256(source1, _mm256_set1_epi32((int)0xffff0000)), 16);
        __m256i p3 = _mm256_and_si256(source2, _mm256_set1_epi32(0xffff));
        __m256i p4 = _mm256_srli_epi32(
            _mm256_and_si256(source2, _mm256_set1_epi32((int)0xffff0000)), 16);

        // Convert 15bit pixels to 8bit pixels. (p * 255 + 16384) >> 15
        __m256i _255 = _mm256_set1_epi32(255);
        __m256i fudge = _mm256_set1_epi32(FUDGE15_TO_8);
        p1 = _mm256_srli_epi32(
            _mm256_add_epi32(_mm256_mullo_epi32(p1, _255), fudge), 15);
        p2 = _mm256_srli_epi32(
            _mm256_add_epi32(_mm256_mullo_epi32(p2, _255), fudge), 15);
        p3 = _mm256_srli_epi32(
            _mm256_add_epi32(_mm256_mullo_epi32(p3, _255), fudge), 15);
        p4 = _mm256_srli_epi32(
            _mm256_add_epi32(_mm256_mullo_epi32(p4, _255), fudge), 15);

        // Shuffle the first of every 32bit spot to form an 4x8bit RGBA.
        // Every register shuffles to a empty spot so they can be ORed together.
        char bit8 = (char)-0x80;
        p1 = _mm256_shuffle_epi8(
            p1, _mm256_setr_epi8(0, bit8, 4, bit8, bit8, bit8, bit8, bit8, 8,
                                 bit8, 12, bit8, bit8, bit8, bit8, bit8, 0,
                                 bit8, 4, bit8, bit8, bit8, bit8, bit8, 8, bit8,
                                 12, bit8, bit8, bit8, bit8, bit8));
        p2 = _mm256_shuffle_epi8(
            p2, _mm256_setr_epi8(bit8, 0, bit8, 4, bit8, bit8, bit8, bit8, bit8,
                                 8, bit8, 12, bit8, bit8, bit8, bit8, bit8, 0,
                                 bit8, 4, bit8, bit8, bit8, bit8, bit8, 8, bit8,
                                 12, bit8, bit8, bit8, bit8));
        p3 = _mm256_shuffle_epi8(
            p3, _mm256_setr_epi8(bit8, bit8, bit8, bit8, 0, bit8, 4, bit8, bit8,
                                 bit8, bit8, bit8, 8, bit8, 12, bit8, bit8,
                                 bit8, bit8, bit8, 0, bit8, 4, bit8, bit8, bit8,
                                 bit8, bit8, 8, bit8, 12, bit8));
        p4 = _mm256_shuffle_epi8(
            p4, _mm256_setr_epi8(bit8, bit8, bit8, bit8, bit8, 0, bit8, 4, bit8,
                                 bit8, bit8, bit8, bit8, 8, bit8, 12, bit8,
                                 bit8, bit8, bit8, bit8, 0, bit8, 4, bit8, bit8,
                                 bit8, bit8, bit8, 8, bit8, 12));

        // Combine all the bytes into a single register.
        __m256i combined =
            _mm256_or_si256(p1, _mm256_or_si256(p2, _mm256_or_si256(p3, p4)));

        // Final permute of 4x8bit RGBA to get them in the right order.
        __m256i out = _mm256_permutevar8x32_epi32(
            combined, _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));

        _mm256_store_si256((void *)&dst[i], out);
    }
    _mm256_zeroupper();
}
DP_TARGET_END
#endif

void DP_pixels15_to_8_tile(DP_Pixel8 *dst, const DP_Pixel15 *src)
{
    DP_Pixel8 *aligned_dst = DP_ASSUME_SIMD_ALIGNED(dst);
    const DP_Pixel15 *aligned_src = DP_ASSUME_SIMD_ALIGNED(src);
    switch (DP_cpu_support) {
#ifdef DP_CPU_X64
    case DP_CPU_SUPPORT_AVX2:
        pixels15_to_8_avx2(aligned_dst, aligned_src);
        break;
    case DP_CPU_SUPPORT_SSE42:
        pixels15_to_8_sse42(aligned_dst, aligned_src);
        break;
#endif
    default:
        DP_pixels15_to_8(aligned_dst, aligned_src, DP_TILE_LENGTH);
        break;
    }
}


// Adapted from the Qt framework, see license above.
static const unsigned int unpremultiply_factors[] = {
    0u,       16711935u, 8355967u, 5570645u, 4177983u, 3342387u, 2785322u,
    2387419u, 2088991u,  1856881u, 1671193u, 1519266u, 1392661u, 1285533u,
    1193709u, 1114129u,  1044495u, 983055u,  928440u,  879575u,  835596u,
    795806u,  759633u,   726605u,  696330u,  668477u,  642766u,  618960u,
    596854u,  576273u,   557064u,  539094u,  522247u,  506422u,  491527u,
    477483u,  464220u,   451673u,  439787u,  428511u,  417798u,  407608u,
    397903u,  388649u,   379816u,  371376u,  363302u,  355573u,  348165u,
    341059u,  334238u,   327685u,  321383u,  315319u,  309480u,  303853u,
    298427u,  293191u,   288136u,  283253u,  278532u,  273966u,  269547u,
    265268u,  261123u,   257106u,  253211u,  249431u,  245763u,  242201u,
    238741u,  235379u,   232110u,  228930u,  225836u,  222825u,  219893u,
    217038u,  214255u,   211543u,  208899u,  206320u,  203804u,  201348u,
    198951u,  196611u,   194324u,  192091u,  189908u,  187774u,  185688u,
    183647u,  181651u,   179698u,  177786u,  175915u,  174082u,  172287u,
    170529u,  168807u,   167119u,  165464u,  163842u,  162251u,  160691u,
    159161u,  157659u,   156186u,  154740u,  153320u,  151926u,  150557u,
    149213u,  147893u,   146595u,  145321u,  144068u,  142837u,  141626u,
    140436u,  139266u,   138115u,  136983u,  135869u,  134773u,  133695u,
    132634u,  131590u,   130561u,  129549u,  128553u,  127572u,  126605u,
    125653u,  124715u,   123792u,  122881u,  121984u,  121100u,  120229u,
    119370u,  118524u,   117689u,  116866u,  116055u,  115254u,  114465u,
    113686u,  112918u,   112160u,  111412u,  110675u,  109946u,  109228u,
    108519u,  107818u,   107127u,  106445u,  105771u,  105106u,  104449u,
    103800u,  103160u,   102527u,  101902u,  101284u,  100674u,  100071u,
    99475u,   98887u,    98305u,   97730u,   97162u,   96600u,   96045u,
    95496u,   94954u,    94417u,   93887u,   93362u,   92844u,   92331u,
    91823u,   91322u,    90825u,   90334u,   89849u,   89368u,   88893u,
    88422u,   87957u,    87497u,   87041u,   86590u,   86143u,   85702u,
    85264u,   84832u,    84403u,   83979u,   83559u,   83143u,   82732u,
    82324u,   81921u,    81521u,   81125u,   80733u,   80345u,   79961u,
    79580u,   79203u,    78829u,   78459u,   78093u,   77729u,   77370u,
    77013u,   76660u,    76310u,   75963u,   75619u,   75278u,   74941u,
    74606u,   74275u,    73946u,   73620u,   73297u,   72977u,   72660u,
    72346u,   72034u,    71725u,   71418u,   71114u,   70813u,   70514u,
    70218u,   69924u,    69633u,   69344u,   69057u,   68773u,   68491u,
    68211u,   67934u,    67659u,   67386u,   67116u,   66847u,   66581u,
    66317u,   66055u,    65795u,   65537u,
};

// Adapted from the Qt framework, see license above.
DP_UPixel8 DP_pixel8_unpremultiply(DP_Pixel8 pixel)
{
    if (pixel.a == 255) {
        return (DP_UPixel8){
            .b = pixel.b,
            .g = pixel.g,
            .r = pixel.r,
            .a = pixel.a,
        };
    }
    else if (pixel.a == 0) {
        return (DP_UPixel8){0};
    }
    else {
        unsigned int a1 = unpremultiply_factors[pixel.a];
        return (DP_UPixel8){
            .b = DP_uint_to_uint8((pixel.b * a1 + 0x8000) >> 16),
            .g = DP_uint_to_uint8((pixel.g * a1 + 0x8000) >> 16),
            .r = DP_uint_to_uint8((pixel.r * a1 + 0x8000) >> 16),
            .a = pixel.a,
        };
    }
}

DP_UPixel15 DP_pixel15_unpremultiply(DP_Pixel15 pixel)
{
    if (pixel.a == BIT15_U16) {
        return (DP_UPixel15){
            .b = pixel.b,
            .g = pixel.g,
            .r = pixel.r,
            .a = pixel.a,
        };
    }
    else if (pixel.a == 0) {
        return DP_upixel15_zero();
    }
    else {
        Fix15 a = to_fix(pixel.a);
        return (DP_UPixel15){
            .b = from_fix(to_fix(pixel.b) * BIT15_FIX / a),
            .g = from_fix(to_fix(pixel.g) * BIT15_FIX / a),
            .r = from_fix(to_fix(pixel.r) * BIT15_FIX / a),
            .a = pixel.a,
        };
    }
}

// Adapted from the Qt framework, see license above.
DP_Pixel8 DP_pixel8_premultiply(DP_UPixel8 pixel)
{
    unsigned int a = pixel.a;
    unsigned int t = (pixel.color & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;
    uint32_t x = ((pixel.color >> 8) & 0xff) * a;
    x = (x + ((x >> 8) & 0xff) + 0x80);
    x &= 0xff00;
    return (DP_Pixel8){DP_uint_to_uint32(x | t | (a << 24u))};
}

DP_Pixel15 DP_pixel15_premultiply(DP_UPixel15 pixel)
{
    if (pixel.a == BIT15_U16) {
        return (DP_Pixel15){
            .b = pixel.b,
            .g = pixel.g,
            .r = pixel.r,
            .a = pixel.a,
        };
    }
    else if (pixel.a == 0) {
        return DP_pixel15_zero();
    }
    else {
        Fix15 a = to_fix(pixel.a);
        return (DP_Pixel15){
            .b = from_fix(to_fix(pixel.b) * a / BIT15_FIX),
            .g = from_fix(to_fix(pixel.g) * a / BIT15_FIX),
            .r = from_fix(to_fix(pixel.r) * a / BIT15_FIX),
            .a = pixel.a,
        };
    }
}

#ifdef DP_CPU_X64
DP_TARGET_BEGIN("sse4.2")
static void shuffle_load_sse42(__m128i source1, __m128i source2,
                               __m128i *out_blue, __m128i *out_green,
                               __m128i *out_red, __m128i *out_alpha)
{
    // clang-format off
    // source1 |B1|G1|R1|A1|B2|G2|R2|A2|
    // source2 |B3|G3|R3|A3|B4|G4|R4|A4|

    __m128i shuffled1 = _mm_shuffle_epi32(source1, _MM_SHUFFLE(3, 1, 2, 0)); // |B1|G1|B2|G2|R1|A1|R2|A2|
    __m128i shuffled2 = _mm_shuffle_epi32(source2, _MM_SHUFFLE(3, 1, 2, 0)); // |B3|G3|B4|G4|R3|A3|R3|A3|

    __m128i blue_green = _mm_unpacklo_epi64(shuffled1, shuffled2); // |B1|G1|B2|G2|B3|G3|B4|G4|
    __m128i red_alpha = _mm_unpackhi_epi64(shuffled1, shuffled2);  // |R1|A1|R2|A2|R3|A3|R4|A4|

    *out_blue = _mm_blend_epi16(blue_green, _mm_set1_epi32(0), 170); // Zero out upper bits (16 -> 32)
    *out_green = _mm_srli_epi32(blue_green, 16);                     // Move and zero out upper bits (16 -> 32)
    *out_red = _mm_blend_epi16(red_alpha, _mm_set1_epi32(0), 170);   // Zero out upper bits (16 -> 32)
    *out_alpha = _mm_srli_epi32(red_alpha, 16);                      // Move and zero out upper bits (16 -> 32)
    // clang-format on
}
// Load 4 16bit pixels and split them into 4x32 bit registers.
static void load_aligned_sse42(const DP_Pixel15 src[4], __m128i *out_blue,
                               __m128i *out_green, __m128i *out_red,
                               __m128i *out_alpha)
{
    DP_ASSERT(((intptr_t)src) % 16 == 0);
    const __m128i *src128 = (const void *)src;
    __m128i source1 = _mm_load_si128(src128);
    __m128i source2 = _mm_load_si128(src128 + 1);

    shuffle_load_sse42(source1, source2, out_blue, out_green, out_red,
                       out_alpha);
}
// Load 4 16bit pixels and split them into 4x32 bit registers.
static void load_unaligned_sse42(const DP_Pixel15 src[4], __m128i *out_blue,
                                 __m128i *out_green, __m128i *out_red,
                                 __m128i *out_alpha)
{
    const __m128i *src128 = (const void *)src;
    __m128i source1 = _mm_loadu_si128(src128);
    __m128i source2 = _mm_loadu_si128(src128 + 1);
    shuffle_load_sse42(source1, source2, out_blue, out_green, out_red,
                       out_alpha);
}

static void shuffle_store_sse42(__m128i blue, __m128i green, __m128i red,
                                __m128i alpha, __m128i *out1, __m128i *out2)
{
    // clang-format off
    __m128i blue_green = _mm_blend_epi16(blue, _mm_slli_si128(green, 2), 170); // |B1|G1|B2|G2|B3|G3|B4|G4|
    __m128i red_alpha = _mm_blend_epi16(red, _mm_slli_si128(alpha, 2), 170);   // |R1|A1|R2|A2|R3|A3|R4|A4|

    *out1 = _mm_unpacklo_epi32(blue_green, red_alpha); // |B1|G1|R1|A1|B2|G2|R2|A2|
    *out2 = _mm_unpackhi_epi32(blue_green, red_alpha); // |B3|G3|R3|A3|B4|G4|R4|A4|
    // clang-format on
}
// Store 4x32 bit registers into 4 16bit pixels.
static void store_aligned_sse42(__m128i blue, __m128i green, __m128i red,
                                __m128i alpha, DP_Pixel15 dest[4])
{

    DP_ASSERT(((intptr_t)dest) % 16 == 0);

    __m128i out1, out2;
    shuffle_store_sse42(blue, green, red, alpha, &out1, &out2);

    __m128i *dest128 = (void *)dest;
    _mm_store_si128(dest128, out1);
    _mm_store_si128(dest128 + 1, out2);
}
// Store 4x32 bit registers into 4 16bit pixels.
static void store_unaligned_sse42(__m128i blue, __m128i green, __m128i red,
                                  __m128i alpha, DP_Pixel15 dest[4])
{
    __m128i out1, out2;
    shuffle_store_sse42(blue, green, red, alpha, &out1, &out2);

    __m128i *dest128 = (void *)dest;
    _mm_storeu_si128(dest128, out1);
    _mm_storeu_si128(dest128 + 1, out2);
}

static __m128i mul_sse42(__m128i a, __m128i b)
{
    return _mm_srli_epi32(_mm_mullo_epi32(a, b), 15);
}

static __m128i sumprods_sse42(__m128i a1, __m128i a2, __m128i b1, __m128i b2)
{
    return _mm_srli_epi32(
        _mm_add_epi32(_mm_mullo_epi32(a1, a2), _mm_mullo_epi32(b1, b2)), 15);
}

static void blend_tile_normal_sse42(DP_Pixel15 *DP_RESTRICT dst,
                                    const DP_Pixel15 *DP_RESTRICT src,
                                    uint16_t opacity)
{
    // clang-format off
    __m128i o = _mm_set1_epi32(opacity); // o = opacity

    // 4 pixels are loaded at a time
    for (int i = 0; i < DP_TILE_LENGTH; i += 4) {
        // load
        __m128i srcB, srcG, srcR, srcA;
        load_aligned_sse42(&src[i], &srcB, &srcG, &srcR, &srcA);

        __m128i dstB, dstG, dstR, dstA;
        load_aligned_sse42(&dst[i], &dstB, &dstG, &dstR, &dstA);

        // Normal blend
        __m128i srcAO = mul_sse42(srcA, o);
        __m128i as1 = _mm_sub_epi32(_mm_set1_epi32(DP_BIT15), srcAO); // as1 = DP_BIT15 - srcA * o

        dstB = _mm_add_epi32(mul_sse42(dstB, as1), mul_sse42(srcB, o)); // dstB = (dstB * as1) + (srcB * o)
        dstG = _mm_add_epi32(mul_sse42(dstG, as1), mul_sse42(srcG, o)); // dstG = (dstG * as1) + (srcG * o)
        dstR = _mm_add_epi32(mul_sse42(dstR, as1), mul_sse42(srcR, o)); // dstR = (dstR * as1) + (srcR * o)
        dstA = _mm_add_epi32(mul_sse42(dstA, as1), srcAO);              // dstA = (dstA * as1) + (srcA * o)

        // store
        store_aligned_sse42(dstB, dstG, dstR, dstA, &dst[i]);
    }
    // clang-format on
}

static void blend_tile_behind_sse42(DP_Pixel15 *DP_RESTRICT dst,
                                    const DP_Pixel15 *DP_RESTRICT src,
                                    uint16_t opacity)
{
    // clang-format off
    __m128i o = _mm_set1_epi32(opacity); // o = opacity

    // 4 pixels are loaded at a time
    for (int i = 0; i < DP_TILE_LENGTH; i += 4) {
        // load
        __m128i srcB, srcG, srcR, srcA;
        load_aligned_sse42(&src[i], &srcB, &srcG, &srcR, &srcA);

        __m128i dstB, dstG, dstR, dstA;
        load_aligned_sse42(&dst[i], &dstB, &dstG, &dstR, &dstA);

        // Behind blend
        __m128i a1 = mul_sse42(_mm_sub_epi32(_mm_set1_epi32(DP_BIT15), dstA), o);

        dstB = _mm_add_epi32(dstB, mul_sse42(srcB, a1));
        dstG = _mm_add_epi32(dstG, mul_sse42(srcG, a1));
        dstR = _mm_add_epi32(dstR, mul_sse42(srcR, a1));
        dstA = _mm_add_epi32(dstA, mul_sse42(srcA, a1));

        // store
        store_aligned_sse42(dstB, dstG, dstR, dstA, &dst[i]);
    }
    // clang-format on
}

static void blend_mask_pixels_normal_sse42(DP_Pixel15 *dst, DP_UPixel15 src,
                                           const uint16_t *mask_int,
                                           Fix15 opacity_int, int count)
{
    // clang-format off
    DP_ASSERT(count % 4 == 0);

    // Do in parent function ?
    __m128i srcB = _mm_set1_epi32(src.b);
    __m128i srcG = _mm_set1_epi32(src.g);
    __m128i srcR = _mm_set1_epi32(src.r);
    __m128i srcA = _mm_set1_epi32(DP_BIT15);

    __m128i opacity = _mm_set1_epi32((int)opacity_int);

    for (int x = 0; x < count; x += 4, dst += 4, mask_int += 4) {
        // load mask
        __m128i mask = _mm_cvtepu16_epi32(_mm_loadl_epi64((void *)mask_int));

        // Load dest
        __m128i dstB, dstG, dstR, dstA;
        load_unaligned_sse42(dst, &dstB, &dstG, &dstR, &dstA);

        __m128i o = mul_sse42(mask, opacity);

        // Normal blend
        __m128i srcAO = mul_sse42(srcA, o);
        __m128i as1 = _mm_sub_epi32(_mm_set1_epi32(DP_BIT15), srcAO); // as1 = DP_BIT15 - srcA * o

        dstB = _mm_add_epi32(mul_sse42(dstB, as1), mul_sse42(srcB, o)); // dstB = (dstB * as1) + (srcB * o)
        dstG = _mm_add_epi32(mul_sse42(dstG, as1), mul_sse42(srcG, o)); // dstG = (dstG * as1) + (srcG * o)
        dstR = _mm_add_epi32(mul_sse42(dstR, as1), mul_sse42(srcR, o)); // dstR = (dstR * as1) + (srcR * o)
        dstA = _mm_add_epi32(mul_sse42(dstA, as1), srcAO);              // dstA = (dstA * as1) + (srcA * o)

        store_unaligned_sse42(dstB, dstG, dstR, dstA, dst);
    }
    // clang-format on
}

static void blend_mask_pixels_normal_and_eraser_sse42(DP_Pixel15 *dst,
                                                      DP_UPixel15 src,
                                                      const uint16_t *mask_int,
                                                      Fix15 opacity_int,
                                                      int count)
{
    DP_ASSERT(count % 4 == 0);

    // Do in parent function ?
    __m128i srcB = _mm_set1_epi32(src.b);
    __m128i srcG = _mm_set1_epi32(src.g);
    __m128i srcR = _mm_set1_epi32(src.r);
    __m128i srcA = _mm_set1_epi32(src.a);

    __m128i opacity = _mm_set1_epi32((int)opacity_int);
    __m128i bit15 = _mm_set1_epi32(DP_BIT15);

    for (int x = 0; x < count; x += 4, dst += 4, mask_int += 4) {
        // load mask
        __m128i mask = _mm_cvtepu16_epi32(_mm_loadl_epi64((void *)mask_int));

        // Load dest
        __m128i dstB, dstG, dstR, dstA;
        load_unaligned_sse42(dst, &dstB, &dstG, &dstR, &dstA);

        __m128i o = mul_sse42(mask, opacity);
        __m128i opa_a = mul_sse42(o, srcA);
        __m128i opa_b = _mm_sub_epi32(bit15, o);

        dstB = sumprods_sse42(opa_a, srcB, opa_b, dstB);
        dstG = sumprods_sse42(opa_a, srcG, opa_b, dstG);
        dstR = sumprods_sse42(opa_a, srcR, opa_b, dstR);
        dstA = _mm_add_epi32(opa_a,
                             _mm_srli_epi32(_mm_mullo_epi32(opa_b, dstA), 15));

        store_unaligned_sse42(dstB, dstG, dstR, dstA, dst);
    }
}

static void blend_mask_pixels_recolor_sse42(DP_Pixel15 *dst, DP_UPixel15 src,
                                            const uint16_t *mask_int,
                                            Fix15 opacity_int, int count)
{
    // clang-format off
    DP_ASSERT(count % 4 == 0);

    // Do in parent function ?
    __m128i srcB = _mm_set1_epi32(src.b);
    __m128i srcG = _mm_set1_epi32(src.g);
    __m128i srcR = _mm_set1_epi32(src.r);
    __m128i srcA = _mm_set1_epi32(DP_BIT15);

    __m128i opacity = _mm_set1_epi32((int)opacity_int);

    for (int x = 0; x < count; x += 4, dst += 4, mask_int += 4) {
        // load mask
        __m128i mask = _mm_cvtepu16_epi32(_mm_loadl_epi64((void *)mask_int));

        // Load dest
        __m128i dstB, dstG, dstR, dstA;
        load_unaligned_sse42(dst, &dstB, &dstG, &dstR, &dstA);

        __m128i o = mul_sse42(mask, opacity);

        // Normal blend
        __m128i srcAO = mul_sse42(srcA, o);
        __m128i as = mul_sse42(dstA, srcAO); // as = dstA * srcA * o
        __m128i as1 = _mm_sub_epi32(_mm_set1_epi32(DP_BIT15), srcAO); // as1 = DP_BIT15 - srcA * o

        dstB = _mm_add_epi32(mul_sse42(dstB, as1), mul_sse42(srcB, as)); // dstB = (dstB * as1) + (srcB * as)
        dstG = _mm_add_epi32(mul_sse42(dstG, as1), mul_sse42(srcG, as)); // dstG = (dstG * as1) + (srcG * as)
        dstR = _mm_add_epi32(mul_sse42(dstR, as1), mul_sse42(srcR, as)); // dstR = (dstR * as1) + (srcR * as)

        store_unaligned_sse42(dstB, dstG, dstR, dstA, dst);
    }
    // clang-format on
}
DP_TARGET_END

DP_TARGET_BEGIN("avx2")
static void shuffle_load_avx2(__m256i source1, __m256i source2,
                              __m256i *out_blue, __m256i *out_green,
                              __m256i *out_red, __m256i *out_alpha)
{
    // clang-format off

    // source1 |B1|G1|R1|A1|B2|G2|R2|A2|B3|G3|R3|A3|B4|G4|R4|A4|
    // source2 |B5|G5|R5|A5|B6|G6|R6|A6|B7|G7|R7|A7|B8|G8|R8|A8|

    __m256i lo = _mm256_unpacklo_epi32(source1, source2);     // |B1|G1|B5|G5|R1|A1|R5|A5|B3|G3|B7|G7|R3|A3|R7|A7|
    __m256i hi = _mm256_unpackhi_epi32(source1, source2);     // |B2|G2|B6|G6|R2|A2|R6|A6|B4|G4|B8|G8|R4|A4|R8|A8|

    __m256i blue_green = _mm256_unpacklo_epi64(lo, hi);       // |B1|G1|B5|G5|B2|G2|B6|G6|B3|G3|B7|G7|B4|G4|B8|G8|
    __m256i red_alpha = _mm256_unpackhi_epi64(lo, hi);        // |R1|A1|R5|A5|R2|A2|R6|A6|R3|A3|R7|A7|R4|A4|R8|A8|

    // Blend with 0 to zero upper bits of 32 spot. Shifting conveniently also zeros out upper bits.
    *out_blue = _mm256_blend_epi16(blue_green, _mm256_set1_epi32(0), 170); // |B1|0|B5|0|B2|0|B6|0|B3|0|B7|0|B4|0|B8|0|
    *out_green = _mm256_srli_epi32(blue_green, 16);                        // |G1|0|G5|0|G2|0|G6|0|G3|0|G7|0|G4|0|G8|0|
    *out_red = _mm256_blend_epi16(red_alpha, _mm256_set1_epi32(0), 170);   // |R1|0|R5|0|R2|0|R6|0|R3|0|R7|0|R4|0|R8|0|
    *out_alpha = _mm256_srli_epi32(red_alpha, 16);                         // |A1|0|A5|0|A2|0|A6|0|A3|0|A7|0|A4|0|A8|0|
    // clang-format on
}
// Load 8 16bit pixels and split them into 8x32 bit registers.
static void load_aligned_avx2(const DP_Pixel15 src[8], __m256i *out_blue,
                              __m256i *out_green, __m256i *out_red,
                              __m256i *out_alpha)
{
    DP_ASSERT(((intptr_t)src) % 32 == 0);

    const __m256i *src256 = (const void *)src;
    __m256i source1 = _mm256_load_si256(src256);
    __m256i source2 = _mm256_load_si256(src256 + 1);

    shuffle_load_avx2(source1, source2, out_blue, out_green, out_red,
                      out_alpha);
}
// Load 8 16bit pixels and split them into 8x32 bit registers.
static void load_unaligned_avx2(const DP_Pixel15 src[8], __m256i *out_blue,
                                __m256i *out_green, __m256i *out_red,
                                __m256i *out_alpha)
{
    const __m256i *src256 = (const void *)src;
    __m256i source1 = _mm256_loadu_si256(src256);
    __m256i source2 = _mm256_loadu_si256(src256 + 1);

    shuffle_load_avx2(source1, source2, out_blue, out_green, out_red,
                      out_alpha);
}

static void shuffle_store_avx2(__m256i blue, __m256i green, __m256i red,
                               __m256i alpha, __m256i *out1, __m256i *out2)
{
    // clang-format off
    __m256i blue_green = _mm256_blend_epi16(blue, _mm256_slli_si256(green, 2), 170); // |B1|G1|B5|G5|B2|G2|B6|G6|B3|G3|B7|G7|B4|G4|B8|G8|
    __m256i red_alpha = _mm256_blend_epi16(red, _mm256_slli_si256(alpha, 2), 170);   // |R1|A1|R5|A5|R2|A2|R6|A6|R3|A3|R7|A7|R4|A4|R8|A8|

    __m256i lo = _mm256_unpacklo_epi32(blue_green, red_alpha);                       // |B1|G1|R1|A1|B5|G5|R5|A5|B3|G3|R3|A3|B7|G7|R7|A7|
    __m256i hi = _mm256_unpackhi_epi32(blue_green, red_alpha);                       // |B2|G2|R2|A2|B6|G6|R6|A6|B4|G4|R4|A4|B8|G8|R8|A8|

    *out1 = _mm256_unpacklo_epi64(lo, hi);                                           // |B1|G1|R1|A1|B2|G2|R2|A2|B3|G3|R3|A3|B4|G4|R4|A4|
    *out2 = _mm256_unpackhi_epi64(lo, hi);                                           // |B5|G5|R5|A5|B6|G6|R6|A6|B7|G7|R7|A7|B8|G8|R8|A8|
    // clang-format on
}
// Store 8x32 bit registers into 8 16bit pixels.
static void store_aligned_avx2(__m256i blue, __m256i green, __m256i red,
                               __m256i alpha, DP_Pixel15 dest[8])
{
    DP_ASSERT(((intptr_t)dest) % 32 == 0);

    __m256i out1, out2;
    shuffle_store_avx2(blue, green, red, alpha, &out1, &out2);

    __m256i *dest256 = (void *)dest;
    _mm256_store_si256(dest256, out1);
    _mm256_store_si256(dest256 + 1, out2);
}
// Store 8x32 bit registers into 8 16bit pixels.
static void store_unaligned_avx2(__m256i blue, __m256i green, __m256i red,
                                 __m256i alpha, DP_Pixel15 dest[8])
{
    __m256i out1, out2;
    shuffle_store_avx2(blue, green, red, alpha, &out1, &out2);

    __m256i *dest256 = (void *)dest;
    _mm256_storeu_si256(dest256, out1);
    _mm256_storeu_si256(dest256 + 1, out2);
}

static __m256i mul_avx2(__m256i a, __m256i b)
{
    return _mm256_srli_epi32(_mm256_mullo_epi32(a, b), 15);
}

static __m256i sumprods_avx2(__m256i a1, __m256i a2, __m256i b1, __m256i b2)
{
    return _mm256_srli_epi32(_mm256_add_epi32(_mm256_mullo_epi32(a1, a2),
                                              _mm256_mullo_epi32(b1, b2)),
                             15);
}

static void blend_tile_normal_avx2(DP_Pixel15 *DP_RESTRICT dst,
                                   const DP_Pixel15 *DP_RESTRICT src,
                                   uint16_t opacity)
{
    // clang-format off
    __m256i o = _mm256_set1_epi32(opacity); // o = opacity

    // 8 pixels are loaded at a time
    for (int i = 0; i < DP_TILE_LENGTH; i += 8) {
        // load
        __m256i srcB, srcG, srcR, srcA;
        load_aligned_avx2(&src[i], &srcB, &srcG, &srcR, &srcA);

        __m256i dstB, dstG, dstR, dstA;
        load_aligned_avx2(&dst[i], &dstB, &dstG, &dstR, &dstA);

        // Normal blend
        __m256i srcAO = mul_avx2(srcA, o);
        __m256i as1 = _mm256_sub_epi32(_mm256_set1_epi32(1 << 15), srcAO); // as1 = 1 - srcA * o

        dstB = _mm256_add_epi32(mul_avx2(dstB, as1), mul_avx2(srcB, o)); // dstB = (dstB * as1) + (srcB * o)
        dstG = _mm256_add_epi32(mul_avx2(dstG, as1), mul_avx2(srcG, o)); // dstG = (dstG * as1) + (srcG * o)
        dstR = _mm256_add_epi32(mul_avx2(dstR, as1), mul_avx2(srcR, o)); // dstR = (dstR * as1) + (srcR * o)
        dstA = _mm256_add_epi32(mul_avx2(dstA, as1), srcAO);             // dstA = (dstA * as1) + (srcA * o)

        // store
        store_aligned_avx2(dstB, dstG, dstR, dstA, &dst[i]);
    }
    _mm256_zeroupper();
    // clang-format on
}

static void blend_tile_behind_avx2(DP_Pixel15 *DP_RESTRICT dst,
                                   const DP_Pixel15 *DP_RESTRICT src,
                                   uint16_t opacity)
{
    // clang-format off
    __m256i o = _mm256_set1_epi32(opacity); // o = opacity

    // 4 pixels are loaded at a time
    for (int i = 0; i < DP_TILE_LENGTH; i += 8) {
        // load
        __m256i srcB, srcG, srcR, srcA;
        load_aligned_avx2(&src[i], &srcB, &srcG, &srcR, &srcA);

        __m256i dstB, dstG, dstR, dstA;
        load_aligned_avx2(&dst[i], &dstB, &dstG, &dstR, &dstA);

        // Behind blend
        __m256i a1 = mul_avx2(_mm256_sub_epi32(_mm256_set1_epi32(DP_BIT15), dstA), o);

        dstB = _mm256_add_epi32(dstB, mul_avx2(srcB, a1));
        dstG = _mm256_add_epi32(dstG, mul_avx2(srcG, a1));
        dstR = _mm256_add_epi32(dstR, mul_avx2(srcR, a1));
        dstA = _mm256_add_epi32(dstA, mul_avx2(srcA, a1));

        // store
        store_aligned_avx2(dstB, dstG, dstR, dstA, &dst[i]);
    }
    _mm256_zeroupper();
    // clang-format on
}

static void blend_mask_pixels_normal_avx2(DP_Pixel15 *dst, DP_UPixel15 src,
                                          const uint16_t *mask_int,
                                          Fix15 opacity_int, int count)
{
    // clang-format off
    DP_ASSERT(count % 8 == 0);

    // Do in parent function ?
    __m256i srcB = _mm256_set1_epi32(src.b);
    __m256i srcG = _mm256_set1_epi32(src.g);
    __m256i srcR = _mm256_set1_epi32(src.r);
    __m256i srcA = _mm256_set1_epi32(DP_BIT15);

    __m256i opacity = _mm256_set1_epi32((int)opacity_int);

    for (int x = 0; x < count; x += 8, dst += 8, mask_int += 8) {
        // load mask
        __m256i mask = _mm256_cvtepu16_epi32(_mm_loadu_si128((void *)mask_int));
        // Permute mask to fit pixel load order (15263748)
        mask = _mm256_permutevar8x32_epi32(mask, _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7));

        // Load dst
        __m256i dstB, dstG, dstR, dstA;
        load_unaligned_avx2(dst, &dstB, &dstG, &dstR, &dstA);

        __m256i o = mul_avx2(mask, opacity);

        // Normal blend
        __m256i srcAO = mul_avx2(srcA, o);
        __m256i as1 = _mm256_sub_epi32(_mm256_set1_epi32(1 << 15), srcAO); // as1 = 1 - srcA * o

        dstB = _mm256_add_epi32(mul_avx2(dstB, as1), mul_avx2(srcB, o)); // dstB = (dstB * as1) + (srcB * o)
        dstG = _mm256_add_epi32(mul_avx2(dstG, as1), mul_avx2(srcG, o)); // dstG = (dstG * as1) + (srcG * o)
        dstR = _mm256_add_epi32(mul_avx2(dstR, as1), mul_avx2(srcR, o)); // dstR = (dstR * as1) + (srcR * o)
        dstA = _mm256_add_epi32(mul_avx2(dstA, as1), srcAO);             // dstA = (dstA * as1) + (srcA * o)

        store_unaligned_avx2(dstB, dstG, dstR, dstA, dst);
    }
    _mm256_zeroupper();
    // clang-format on
}

static void blend_mask_pixels_normal_and_eraser_avx2(DP_Pixel15 *dst,
                                                     DP_UPixel15 src,
                                                     const uint16_t *mask_int,
                                                     Fix15 opacity_int,
                                                     int count)
{
    DP_ASSERT(count % 8 == 0);

    // Do in parent function ?
    __m256i srcB = _mm256_set1_epi32(src.b);
    __m256i srcG = _mm256_set1_epi32(src.g);
    __m256i srcR = _mm256_set1_epi32(src.r);
    __m256i srcA = _mm256_set1_epi32(src.a);

    __m256i opacity = _mm256_set1_epi32((int)opacity_int);
    __m256i bit15 = _mm256_set1_epi32(DP_BIT15);

    for (int x = 0; x < count; x += 8, dst += 8, mask_int += 8) {
        // load mask
        __m256i mask = _mm256_cvtepu16_epi32(_mm_loadu_si128((void *)mask_int));
        // Permute mask to fit pixel load order (15263748)
        mask = _mm256_permutevar8x32_epi32(
            mask, _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7));

        // Load dst
        __m256i dstB, dstG, dstR, dstA;
        load_unaligned_avx2(dst, &dstB, &dstG, &dstR, &dstA);

        __m256i o = mul_avx2(mask, opacity);
        __m256i opa_a = mul_avx2(o, srcA);
        __m256i opa_b = _mm256_sub_epi32(bit15, o);

        dstB = sumprods_avx2(opa_a, srcB, opa_b, dstB);
        dstG = sumprods_avx2(opa_a, srcG, opa_b, dstG);
        dstR = sumprods_avx2(opa_a, srcR, opa_b, dstR);
        dstA = _mm256_add_epi32(
            opa_a, _mm256_srli_epi32(_mm256_mullo_epi32(opa_b, dstA), 15));

        store_unaligned_avx2(dstB, dstG, dstR, dstA, dst);
    }
    _mm256_zeroupper();
}

static void blend_mask_pixels_recolor_avx2(DP_Pixel15 *dst, DP_UPixel15 src,
                                           const uint16_t *mask_int,
                                           Fix15 opacity_int, int count)
{
    // clang-format off
    DP_ASSERT(count % 8 == 0);

    // Do in parent function ?
    __m256i srcB = _mm256_set1_epi32(src.b);
    __m256i srcG = _mm256_set1_epi32(src.g);
    __m256i srcR = _mm256_set1_epi32(src.r);
    __m256i srcA = _mm256_set1_epi32(DP_BIT15);

    __m256i opacity = _mm256_set1_epi32((int)opacity_int);

    for (int x = 0; x < count; x += 8, dst += 8, mask_int += 8) {
        // load mask
        __m256i mask = _mm256_cvtepu16_epi32(_mm_loadu_si128((void *)mask_int));
        // Permute mask to fit pixel load order (15263748)
        mask = _mm256_permutevar8x32_epi32(mask, _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7));

        // Load dst
        __m256i dstB, dstG, dstR, dstA;
        load_unaligned_avx2(dst, &dstB, &dstG, &dstR, &dstA);

        __m256i o = mul_avx2(mask, opacity);

        // Recolor blend
        __m256i srcAO = mul_avx2(srcA, o);
        __m256i as = mul_avx2(dstA, srcAO); // as = dstA * srcA * o
        __m256i as1 = _mm256_sub_epi32(_mm256_set1_epi32(1 << 15), srcAO); // as1 = 1 - srcA * o

        dstB = _mm256_add_epi32(mul_avx2(dstB, as1), mul_avx2(srcB, as)); // dstB = (dstB * as1) + (srcB * as)
        dstG = _mm256_add_epi32(mul_avx2(dstG, as1), mul_avx2(srcG, as)); // dstG = (dstG * as1) + (srcG * as)
        dstR = _mm256_add_epi32(mul_avx2(dstR, as1), mul_avx2(srcR, as)); // dstR = (dstR * as1) + (srcR * as)

        store_unaligned_avx2(dstB, dstG, dstR, dstA, dst);
    }
    _mm256_zeroupper();
    // clang-format on
}
DP_TARGET_END
#endif

static BGRA15 blend_normal(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as, Fix15 o)
{
    Fix15 as1 = BIT15_FIX - fix15_mul(as, o);
    return (BGRA15){
        .b = fix15_sumprods(cb.b, as1, cs.b, o),
        .g = fix15_sumprods(cb.g, as1, cs.g, o),
        .r = fix15_sumprods(cb.r, as1, cs.r, o),
        .a = fix15_sumprods(ab, as1, as, o),
    };
}

static BGRA15 blend_behind(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as, Fix15 o)
{
    Fix15 a1 = fix15_mul(BIT15_FIX - ab, o);
    return (BGRA15){
        .b = cb.b + fix15_mul(cs.b, a1),
        .g = cb.g + fix15_mul(cs.g, a1),
        .r = cb.r + fix15_mul(cs.r, a1),
        .a = ab + fix15_mul(as, a1),
    };
}

static BGRA15 blend_erase(BGR15 cb, DP_UNUSED BGR15 cs, Fix15 ab, Fix15 as,
                          Fix15 o)
{
    Fix15 as1 = BIT15_FIX - fix15_mul(as, o);
    return (BGRA15){
        .b = fix15_mul(cb.b, as1),
        .g = fix15_mul(cb.g, as1),
        .r = fix15_mul(cb.r, as1),
        .a = fix15_mul(ab, as1),
    };
}

static BGRA15 blend_erase_light(BGR15 cb, BGR15 cs, Fix15 ab,
                                DP_UNUSED Fix15 as, Fix15 o)
{
    Fix15 as1 = BIT15_FIX - fix15_mul(lum(cs), o);
    return (BGRA15){
        .b = fix15_mul(cb.b, as1),
        .g = fix15_mul(cb.g, as1),
        .r = fix15_mul(cb.r, as1),
        .a = fix15_mul(ab, as1),
    };
}

static BGRA15 blend_erase_dark(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as, Fix15 o)
{
    return blend_erase_light(cb, (BGR15){as - cs.b, as - cs.g, as - cs.r}, ab,
                             as, o);
}

static BGRA15 blend_replace(DP_UNUSED BGR15 cb, DP_UNUSED BGR15 cs,
                            DP_UNUSED Fix15 ab, Fix15 as, Fix15 o)
{
    return (BGRA15){
        .b = fix15_mul(cs.b, o),
        .g = fix15_mul(cs.g, o),
        .r = fix15_mul(cs.r, o),
        .a = fix15_mul(as, o),
    };
}

static BGRA15 blend_recolor(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as, Fix15 o)
{
    Fix15 abo = fix15_mul(ab, o);
    Fix15 as1 = BIT15_FIX - fix15_mul(as, o);
    return (BGRA15){
        .b = fix15_sumprods(cb.b, as1, cs.b, abo),
        .g = fix15_sumprods(cb.g, as1, cs.g, abo),
        .r = fix15_sumprods(cb.r, as1, cs.r, abo),
        .a = ab,
    };
}

// Adapted from GIMP, see license above.
static DP_Pixel15 blend_color_erase(double fsrc_b, double fsrc_g, double fsrc_r,
                                    double fsrc_a, double ufdst_b,
                                    double ufdst_g, double ufdst_r,
                                    double ufdst_a)
{
    double ufalpha_a = ufdst_a;

    double ufalpha_r;
    if (fsrc_r < 0.0001) {
        ufalpha_r = ufdst_r;
    }
    else if (ufdst_r > fsrc_r) {
        ufalpha_r = (ufdst_r - fsrc_r) / (1.0 - fsrc_r);
    }
    else if (ufdst_r < fsrc_r) {
        ufalpha_r = (fsrc_r - ufdst_r) / fsrc_r;
    }
    else {
        ufalpha_r = 0.0;
    }

    double ufalpha_g;
    if (fsrc_g < 0.0001) {
        ufalpha_g = ufdst_g;
    }
    else if (ufdst_g > fsrc_g) {
        ufalpha_g = (ufdst_g - fsrc_g) / (1.0 - fsrc_g);
    }
    else if (ufdst_g < fsrc_g) {
        ufalpha_g = (fsrc_g - ufdst_g) / (fsrc_g);
    }
    else {
        ufalpha_g = 0.0;
    }

    double ufalpha_b;
    if (fsrc_b < 0.0001) {
        ufalpha_b = ufdst_b;
    }
    else if (ufdst_b > fsrc_b) {
        ufalpha_b = (ufdst_b - fsrc_b) / (1.0 - fsrc_b);
    }
    else if (ufdst_b < fsrc_b) {
        ufalpha_b = (fsrc_b - ufdst_b) / (fsrc_b);
    }
    else {
        ufalpha_b = 0.0;
    }

    if (ufalpha_r > ufalpha_g) {
        if (ufalpha_r > ufalpha_b) {
            ufdst_a = ufalpha_r;
        }
        else {
            ufdst_a = ufalpha_b;
        }
    }
    else if (ufalpha_g > ufalpha_b) {
        ufdst_a = ufalpha_g;
    }
    else {
        ufdst_a = ufalpha_b;
    }

    ufdst_a = (1.0 - fsrc_a) + (ufdst_a * fsrc_a);

    if (ufdst_a >= 0.0001) {
        ufdst_r = (ufdst_r - fsrc_r) / ufdst_a + fsrc_r;
        ufdst_g = (ufdst_g - fsrc_g) / ufdst_a + fsrc_g;
        ufdst_b = (ufdst_b - fsrc_b) / ufdst_a + fsrc_b;
        ufdst_a *= ufalpha_a;
    }

    return DP_pixel15_premultiply((DP_UPixel15){
        .b = DP_double_to_uint16(ufdst_b * BIT15_DOUBLE),
        .g = DP_double_to_uint16(ufdst_g * BIT15_DOUBLE),
        .r = DP_double_to_uint16(ufdst_r * BIT15_DOUBLE),
        .a = DP_double_to_uint16(ufdst_a * BIT15_DOUBLE),
    });
}

static Fix15 comp_multiply(Fix15 a, Fix15 b)
{
    return fix15_mul(a, b);
}

static Fix15 comp_divide(Fix15 a, Fix15 b)
{
    return fix15_clamp((a * BIT15_INC_FIX + b / FIX_2) / (b + FIX_1));
}

static Fix15 comp_burn(Fix15 a, Fix15 b)
{
    return ifix15_clamp(
        BIT15_IFIX - (IFix15)((BIT15_FIX - a) * BIT15_INC_FIX / (b + FIX_1)));
}

static Fix15 comp_dodge(Fix15 a, Fix15 b)
{
    return fix15_clamp(a * BIT15_INC_FIX / (BIT15_INC_FIX - b));
}

static Fix15 comp_vivid_light(Fix15 a, Fix15 b)
{
    Fix15 b2 = b * FIX_2;
    return b2 <= BIT15_FIX ? comp_burn(a, b2) : comp_dodge(a, b2 - BIT15_FIX);
}

static Fix15 comp_pin_light(Fix15 a, Fix15 b)
{
    Fix15 b2 = b * FIX_2;
    return b2 <= BIT15_FIX ? fix15_min(a, b2) : fix15_max(a, b2 - BIT15_FIX);
}

static Fix15 comp_lighten(Fix15 a, Fix15 b)
{
    return a > b ? a : b;
}

static Fix15 comp_darken(Fix15 a, Fix15 b)
{
    return a < b ? a : b;
}

static Fix15 comp_subtract(Fix15 a, Fix15 b)
{
    return b < a ? a - b : 0;
}

static Fix15 comp_add(Fix15 a, Fix15 b)
{
    return fix15_clamp(a + b);
}

static Fix15 comp_difference(Fix15 a, Fix15 b)
{
    return a < b ? b - a : a - b;
}

static Fix15 comp_screen(Fix15 a, Fix15 b)
{
    return BIT15_FIX - fix15_mul(BIT15_FIX - a, BIT15_FIX - b);
}

static Fix15 comp_hard_light(Fix15 a, Fix15 b)
{
    Fix15 b2 = b * FIX_2;
    return b2 <= BIT15_FIX ? comp_multiply(a, b2)
                           : comp_screen(a, b2 - BIT15_FIX);
}

static Fix15 comp_overlay(Fix15 a, Fix15 b)
{
    return comp_hard_light(b, a);
}

static Fix15 comp_soft_light(Fix15 a, Fix15 b)
{
    Fix15 b2 = b * FIX_2;
    if (b2 <= BIT15_FIX) {
        return a - fix15_mul(fix15_mul(BIT15_FIX - b2, a), BIT15_FIX - a);
    }
    else {
        Fix15 a4 = a * FIX_4;
        Fix15 d;
        if (a4 <= BIT15_FIX) {
            Fix15 squared = fix15_mul(a, a);
            d = a4 + FIX_16 * fix15_mul(squared, a) - FIX_12 * squared;
        }
        else {
            d = fix15_sqrt(a);
        }
        return a + fix15_mul(b2 - BIT15_FIX, d - a);
    }
}

static Fix15 comp_linear_burn(Fix15 a, Fix15 b)
{
    Fix15 c = a + b;
    return c <= BIT15_FIX ? 0 : c - BIT15_FIX;
}

static Fix15 comp_linear_light(Fix15 a, Fix15 b)
{
    Fix15 c = (a + b * FIX_2);
    return c <= BIT15_FIX ? 0 : fix15_clamp(c - BIT15_FIX);
}

static BGR15 comp_hue(BGR15 a, BGR15 b)
{
    return set_lum(set_sat(b, sat(a)), lum(a));
}

static BGR15 comp_saturation(BGR15 a, BGR15 b)
{
    return set_lum(set_sat(a, sat(b)), lum(a));
}

static BGR15 comp_luminosity(BGR15 a, BGR15 b)
{
    return set_lum(a, lum(b));
}

static BGR15 comp_color(BGR15 a, BGR15 b)
{
    return set_lum(b, lum(a));
}

static BGR15 comp_darker_color(BGR15 a, BGR15 b)
{
    return lum(b) < lum(a) ? b : a;
}

static BGR15 comp_lighter_color(BGR15 a, BGR15 b)
{
    return lum(b) > lum(a) ? b : a;
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: MIT
// SDPX—SnippetName: SAI blending logic based on psd-export by cromachina
static Fix15 comp_linear_burn_sai(Fix15 a, Fix15 b, Fix15 o)
{
    Fix15 c = a + b;
    return c <= o ? 0 : fix15_clamp(c - o);
}

static Fix15 comp_linear_light_sai(Fix15 a, Fix15 b, Fix15 o)
{
    return comp_linear_burn_sai(a, b * FIX_2, o);
}

static Fix15 comp_burn_sai(Fix15 a, Fix15 b, Fix15 o)
{
    if (o == BIT15_FIX && b == 0) {
        return 0;
    }
    else {
        return BIT15_FIX - fix15_clamp_div(BIT15_FIX - a, BIT15_FIX - o + b);
    }
}

static Fix15 comp_vivid_light_sai(Fix15 a, Fix15 b, Fix15 o)
{
    if (b == BIT15_FIX) {
        return BIT15_FIX;
    }
    else {
        Fix15 b2 = b * FIX_2;
        if (b2 <= o) {
            Fix15 c = o - b2;
            if (c < BIT15_FIX) {
                return BIT15_FIX
                     - fix15_clamp_div(BIT15_FIX - a, BIT15_FIX - c);
            }
            else {
                return 0;
            }
        }
        else {
            return fix15_clamp_div(a, BIT15_FIX + o - b2);
        }
    }
}

static Fix15 comp_hard_mix_sai(Fix15 a, Fix15 b, Fix15 o)
{
    if (o == BIT15_FIX) {
        return BIT15_FIX;
    }
    else {
        Fix15 c = a + b;
        return c < o ? 0 : fix15_clamp_div(c - o, BIT15_FIX - o);
    }
}
// SPDX-SnippetEnd


#define FOR_MASK_PIXEL_WITH(CALC_A, DST, MASK, OPACITY, W, H, MASK_SKIP, \
                            DST_SKIP, X, Y, A, BLOCK)                    \
    do {                                                                 \
        for (int Y = 0; Y < H; ++Y) {                                    \
            for (int X = 0; X < W; ++X, ++DST, ++MASK) {                 \
                Fix15 A = CALC_A;                                        \
                BLOCK                                                    \
            }                                                            \
            DST += DST_SKIP;                                             \
            MASK += MASK_SKIP;                                           \
        }                                                                \
    } while (0)

#define FOR_MASK_PIXEL(DST, MASK, OPACITY, W, H, MASK_SKIP, DST_SKIP, X, Y, A, \
                       BLOCK)                                                  \
    FOR_MASK_PIXEL_WITH(fix15_mul(*MASK, OPACITY), DST, MASK, OPACITY, W, H,   \
                        MASK_SKIP, DST_SKIP, X, Y, A, BLOCK)

#define FOR_MASK_PIXEL_M(DST, MASK, OPACITY, W, H, MASK_SKIP, DST_SKIP, X, Y, \
                         A, BLOCK)                                            \
    FOR_MASK_PIXEL_WITH(*MASK, DST, MASK, OPACITY, W, H, MASK_SKIP, DST_SKIP, \
                        X, Y, A, BLOCK)

static void blend_mask_alpha_op(DP_Pixel15 *dst, DP_UPixel15 src,
                                const uint16_t *mask, Fix15 opacity, int w,
                                int h, int mask_skip, int base_skip,
                                BGRA15 (*op)(BGR15, BGR15, Fix15, Fix15, Fix15))
{
    BGR15 cs = to_ubgr(src);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x, ++dst, ++mask) {
            Fix15 o = fix15_mul(*mask, opacity);

            BGRA15 b = to_bgra(*dst);
            *dst = from_bgra(op(b.bgr, cs, b.a, ((Fix15)(1 << 15)), o));
        }
        dst += base_skip;
        mask += mask_skip;
    }
}

static void blend_mask_color_erase(DP_Pixel15 *dst, DP_UPixel15 src,
                                   const uint16_t *mask, Fix15 opacity, int w,
                                   int h, int mask_skip, int base_skip)
{
    double fsrc_b = src.b / BIT15_DOUBLE;
    double fsrc_g = src.g / BIT15_DOUBLE;
    double fsrc_r = src.r / BIT15_DOUBLE;
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        if (o != 0u) {
            double fo = (double)o;
            DP_UPixel15 ub = DP_pixel15_unpremultiply(*dst);
            *dst = blend_color_erase(fsrc_b, fsrc_g, fsrc_r, fo / BIT15_DOUBLE,
                                     DP_uint16_to_double(ub.b) / BIT15_DOUBLE,
                                     DP_uint16_to_double(ub.g) / BIT15_DOUBLE,
                                     DP_uint16_to_double(ub.r) / BIT15_DOUBLE,
                                     DP_uint16_to_double(ub.a) / BIT15_DOUBLE);
        }
    });
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-2.0-or-later
// SDPX—SnippetName: Spectral blending based on MyPaint's blending.hpp
#define WGM_EPSILON 0.001f
#define WGM_OFFSET  (1.0f - WGM_EPSILON)

#define SPECTRAL_B0 (0.537052150373386f)
#define SPECTRAL_B1 (0.546646402401469f)
#define SPECTRAL_B2 (0.575501819073983f)
#define SPECTRAL_B3 (0.258778829633924f)
#define SPECTRAL_B4 (0.041709923751716f)
#define SPECTRAL_B5 (0.012662638828324f)
#define SPECTRAL_B6 (0.007485593127390f)
#define SPECTRAL_B7 (0.006766900622462f)
#define SPECTRAL_B8 (0.006699764779016f)
#define SPECTRAL_B9 (0.006676219883241f)

#define SPECTRAL_G0 (0.002854127435775f)
#define SPECTRAL_G1 (0.003917589679914f)
#define SPECTRAL_G2 (0.012132151699187f)
#define SPECTRAL_G3 (0.748259205918013f)
#define SPECTRAL_G4 (1.000000000000000f)
#define SPECTRAL_G5 (0.865695937531795f)
#define SPECTRAL_G6 (0.037477469241101f)
#define SPECTRAL_G7 (0.022816789725717f)
#define SPECTRAL_G8 (0.021747419446456f)
#define SPECTRAL_G9 (0.021384940572308f)

#define SPECTRAL_R0 (0.009281362787953f)
#define SPECTRAL_R1 (0.009732627042016f)
#define SPECTRAL_R2 (0.011254252737167f)
#define SPECTRAL_R3 (0.015105578649573f)
#define SPECTRAL_R4 (0.024797924177217f)
#define SPECTRAL_R5 (0.083622585502406f)
#define SPECTRAL_R6 (0.977865045723212f)
#define SPECTRAL_R7 (1.000000000000000f)
#define SPECTRAL_R8 (0.999961046144372f)
#define SPECTRAL_R9 (0.999999992756822f)

#define T_MATRIX_B0 (0.339475473216284f)
#define T_MATRIX_B1 (0.635401374177222f)
#define T_MATRIX_B2 (0.771520797089589f)
#define T_MATRIX_B3 (0.113222640692379f)
#define T_MATRIX_B4 (-0.055251113343776f)
#define T_MATRIX_B5 (-0.048222578468680f)
#define T_MATRIX_B6 (-0.012966666339586f)
#define T_MATRIX_B7 (-0.001523814504223f)
#define T_MATRIX_B8 (-0.000094718948810f)
#define T_MATRIX_B9 (-0.000051604594741f)

#define T_MATRIX_G0 (-0.032601672674412f)
#define T_MATRIX_G1 (-0.061021043498478f)
#define T_MATRIX_G2 (-0.052490001018404f)
#define T_MATRIX_G3 (0.206659098273522f)
#define T_MATRIX_G4 (0.572496335158169f)
#define T_MATRIX_G5 (0.317837248815438f)
#define T_MATRIX_G6 (-0.021216624031211f)
#define T_MATRIX_G7 (-0.019387668756117f)
#define T_MATRIX_G8 (-0.001521339050858f)
#define T_MATRIX_G9 (-0.000835181622534f)

#define T_MATRIX_R0 (0.026595621243689f)
#define T_MATRIX_R1 (0.049779426257903f)
#define T_MATRIX_R2 (0.022449850859496f)
#define T_MATRIX_R3 (-0.218453689278271f)
#define T_MATRIX_R4 (-0.256894883201278f)
#define T_MATRIX_R5 (0.445881722194840f)
#define T_MATRIX_R6 (0.772365886289756f)
#define T_MATRIX_R7 (0.194498761382537f)
#define T_MATRIX_R8 (0.014038157587820f)
#define T_MATRIX_R9 (0.007687264480513f)

static float srgb_to_linear(float x)
{
    return x < 0.04045f ? x / 12.92f : fastpow((x + 0.055f) / 1.055f, 2.4f);
}

static float linear_to_srgb(float x)
{
    return x < 0.0031308f ? x * 12.92f
                          : fastpow(x, 1.0f / 2.4f) * 1.055f - 0.055f;
}

#ifdef DP_CPU_X64
DP_TARGET_BEGIN("avx2")
static void rgb_to_spectral_avx2(float b, float g, float r, float *out)
{
    // First eight elements per channel. This is one big dot product.
    _mm256_store_ps(
        out,
        // B' + (G' + R')
        _mm256_add_ps(
            // B' = SPECTRAL_B * b
            _mm256_mul_ps(_mm256_set_ps(SPECTRAL_B7, SPECTRAL_B6, SPECTRAL_B5,
                                        SPECTRAL_B4, SPECTRAL_B3, SPECTRAL_B2,
                                        SPECTRAL_B1, SPECTRAL_B0),
                          _mm256_set1_ps(b)),
            // G' + R'
            _mm256_add_ps(
                // G' = SPECTRAL_G * g
                _mm256_mul_ps(_mm256_set_ps(SPECTRAL_G7, SPECTRAL_G6,
                                            SPECTRAL_G5, SPECTRAL_G4,
                                            SPECTRAL_G3, SPECTRAL_G2,
                                            SPECTRAL_G1, SPECTRAL_G0),
                              _mm256_set1_ps(g)),
                // R' = SPECTRAL_R * r
                _mm256_mul_ps(_mm256_set_ps(SPECTRAL_R7, SPECTRAL_R6,
                                            SPECTRAL_R5, SPECTRAL_R4,
                                            SPECTRAL_R3, SPECTRAL_R2,
                                            SPECTRAL_R1, SPECTRAL_R0),
                              _mm256_set1_ps(r)))));
    // Last two elements in each channel, two zeroes for scalar calculation.
    _mm256_store_ps(
        out + 8,
        _mm256_mul_ps(_mm256_set_ps(SPECTRAL_R9, SPECTRAL_G9, SPECTRAL_B9,
                                    SPECTRAL_R8, SPECTRAL_G8, SPECTRAL_B8, 0.0f,
                                    0.0f),
                      _mm256_set_ps(r, g, b, r, g, b, 0.0f, 0.0f)));
    // Sum up the last two channels as a scalar operation.
    out[8] = out[10] + out[11] + out[12];
    out[9] = out[13] + out[14] + out[15];
    _mm256_zeroupper();
}
DP_TARGET_END

DP_TARGET_BEGIN("sse4.2")
static void rgb_to_spectral_sse42(float b, float g, float r, float *out)
{
    {
        __m128 vb = _mm_set1_ps(b);
        __m128 vg = _mm_set1_ps(g);
        __m128 vr = _mm_set1_ps(r);
        // First four elements.
        _mm_store_ps(out,
                     // B' + (G' + R')
                     _mm_add_ps(
                         // B' = SPECTRAL_B * b
                         _mm_mul_ps(_mm_set_ps(SPECTRAL_B3, SPECTRAL_B2,
                                               SPECTRAL_B1, SPECTRAL_B0),
                                    vb),
                         // G' + R'
                         _mm_add_ps(
                             // G' = SPECTRAL_G * g
                             _mm_mul_ps(_mm_set_ps(SPECTRAL_G3, SPECTRAL_G2,
                                                   SPECTRAL_G1, SPECTRAL_G0),
                                        vg),
                             // R' = SPECTRAL_R * r
                             _mm_mul_ps(_mm_set_ps(SPECTRAL_R3, SPECTRAL_R2,
                                                   SPECTRAL_R1, SPECTRAL_R0),
                                        vr))));
        // Next four elements.
        _mm_store_ps(out + 4,
                     // B' + (G' + R')
                     _mm_add_ps(
                         // B' = SPECTRAL_B * b
                         _mm_mul_ps(_mm_set_ps(SPECTRAL_B7, SPECTRAL_B6,
                                               SPECTRAL_B5, SPECTRAL_B4),
                                    vb),
                         // G' + R'
                         _mm_add_ps(
                             // G' = SPECTRAL_G * g
                             _mm_mul_ps(_mm_set_ps(SPECTRAL_G7, SPECTRAL_G6,
                                                   SPECTRAL_G5, SPECTRAL_G4),
                                        vg),
                             // R' = SPECTRAL_R * r
                             _mm_mul_ps(_mm_set_ps(SPECTRAL_R7, SPECTRAL_R6,
                                                   SPECTRAL_R5, SPECTRAL_R4),
                                        vr))));
    }
    // Last two elements in each channel, two zeroes for scalar calculation.
    _mm_store_ps(out + 8,
                 _mm_mul_ps(_mm_set_ps(SPECTRAL_G8, SPECTRAL_B8, 0.0f, 0.0f),
                            _mm_set_ps(g, b, 0.0f, 0.0f)));
    _mm_store_ps(out + 12, _mm_mul_ps(_mm_set_ps(SPECTRAL_R9, SPECTRAL_G9,
                                                 SPECTRAL_B9, SPECTRAL_R8),
                                      _mm_set_ps(r, g, b, r)));
    // Sum up the last two channels as a scalar operation.
    out[8] = out[10] + out[11] + out[12];
    out[9] = out[13] + out[14] + out[15];
}
DP_TARGET_END
#endif

static void rgb_to_spectral_scalar(float b, float g, float r, float *out)
{
    float *aligned_out = DP_ASSUME_SIMD_ALIGNED(out);
    aligned_out[0] = SPECTRAL_B0 * b + SPECTRAL_G0 * g + SPECTRAL_R0 * r;
    aligned_out[1] = SPECTRAL_B1 * b + SPECTRAL_G1 * g + SPECTRAL_R1 * r;
    aligned_out[2] = SPECTRAL_B2 * b + SPECTRAL_G2 * g + SPECTRAL_R2 * r;
    aligned_out[3] = SPECTRAL_B3 * b + SPECTRAL_G3 * g + SPECTRAL_R3 * r;
    aligned_out[4] = SPECTRAL_B4 * b + SPECTRAL_G4 * g + SPECTRAL_R4 * r;
    aligned_out[5] = SPECTRAL_B5 * b + SPECTRAL_G5 * g + SPECTRAL_R5 * r;
    aligned_out[6] = SPECTRAL_B6 * b + SPECTRAL_G6 * g + SPECTRAL_R6 * r;
    aligned_out[7] = SPECTRAL_B7 * b + SPECTRAL_G7 * g + SPECTRAL_R7 * r;
    aligned_out[8] = SPECTRAL_B8 * b + SPECTRAL_G8 * g + SPECTRAL_R8 * r;
    aligned_out[9] = SPECTRAL_B9 * b + SPECTRAL_G9 * g + SPECTRAL_R9 * r;
}

static DP_Spectral rgb_to_spectral(BGRf bgr)
{
    float b = srgb_to_linear(bgr.b) * WGM_OFFSET + WGM_EPSILON;
    float g = srgb_to_linear(bgr.g) * WGM_OFFSET + WGM_EPSILON;
    float r = srgb_to_linear(bgr.r) * WGM_OFFSET + WGM_EPSILON;
    DP_Spectral out;
    switch (DP_cpu_support) {
#ifdef DP_CPU_X64
    case DP_CPU_SUPPORT_AVX2:
        rgb_to_spectral_avx2(b, g, r, out.channels);
        break;
    case DP_CPU_SUPPORT_SSE42:
        rgb_to_spectral_sse42(b, g, r, out.channels);
        break;
#endif
    default:
        rgb_to_spectral_scalar(b, g, r, out.channels);
        break;
    }
    return out;
}

static DP_Spectral pixel15_to_spectral(DP_Pixel15 p)
{
    float a = (float)p.a;
    BGRf bgr = {(float)p.b / a, (float)p.g / a, (float)p.r / a};
    return rgb_to_spectral(bgr);
}

static float clampf(float x)
{
    return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
}

static BGRf spectral_to_rgb(const float *spectral)
{
    const float *aligned = DP_ASSUME_SIMD_ALIGNED(spectral);
    float b = T_MATRIX_B0 * aligned[0] + T_MATRIX_B1 * aligned[1]
            + T_MATRIX_B2 * aligned[2] + T_MATRIX_B3 * aligned[3]
            + T_MATRIX_B4 * aligned[4] + T_MATRIX_B5 * aligned[5]
            + T_MATRIX_B6 * aligned[6] + T_MATRIX_B7 * aligned[7]
            + T_MATRIX_B8 * aligned[8] + T_MATRIX_B9 * aligned[9];
    float g = T_MATRIX_G0 * aligned[0] + T_MATRIX_G1 * aligned[1]
            + T_MATRIX_G2 * aligned[2] + T_MATRIX_G3 * aligned[3]
            + T_MATRIX_G4 * aligned[4] + T_MATRIX_G5 * aligned[5]
            + T_MATRIX_G6 * aligned[6] + T_MATRIX_G7 * aligned[7]
            + T_MATRIX_G8 * aligned[8] + T_MATRIX_G9 * aligned[9];
    float r = T_MATRIX_R0 * aligned[0] + T_MATRIX_R1 * aligned[1]
            + T_MATRIX_R2 * aligned[2] + T_MATRIX_R3 * aligned[3]
            + T_MATRIX_R4 * aligned[4] + T_MATRIX_R5 * aligned[5]
            + T_MATRIX_R6 * aligned[6] + T_MATRIX_R7 * aligned[7]
            + T_MATRIX_R8 * aligned[8] + T_MATRIX_R9 * aligned[9];
    return (BGRf){
        clampf(linear_to_srgb((b - WGM_EPSILON) / WGM_OFFSET)),
        clampf(linear_to_srgb((g - WGM_EPSILON) / WGM_OFFSET)),
        clampf(linear_to_srgb((r - WGM_EPSILON) / WGM_OFFSET)),
    };
}

static void mix_spectral_channels(const float *a, float *b, float fac_a,
                                  float fac_b)
{
    const float *aligned_a = DP_ASSUME_SIMD_ALIGNED(a);
    float *aligned_b = DP_ASSUME_SIMD_ALIGNED(b);
    aligned_b[0] = fastpow(aligned_a[0], fac_a) * fastpow(aligned_b[0], fac_b);
    aligned_b[1] = fastpow(aligned_a[1], fac_a) * fastpow(aligned_b[1], fac_b);
    aligned_b[2] = fastpow(aligned_a[2], fac_a) * fastpow(aligned_b[2], fac_b);
    aligned_b[3] = fastpow(aligned_a[3], fac_a) * fastpow(aligned_b[3], fac_b);
    aligned_b[4] = fastpow(aligned_a[4], fac_a) * fastpow(aligned_b[4], fac_b);
    aligned_b[5] = fastpow(aligned_a[5], fac_a) * fastpow(aligned_b[5], fac_b);
    aligned_b[6] = fastpow(aligned_a[6], fac_a) * fastpow(aligned_b[6], fac_b);
    aligned_b[7] = fastpow(aligned_a[7], fac_a) * fastpow(aligned_b[7], fac_b);
    aligned_b[8] = fastpow(aligned_a[8], fac_a) * fastpow(aligned_b[8], fac_b);
    aligned_b[9] = fastpow(aligned_a[9], fac_a) * fastpow(aligned_b[9], fac_b);
}

static DP_Spectral mix_spectral(DP_Pixel15 dp, const float *channels_a,
                                Fix15 ab, Fix15 aso)
{
    float fac_a = (float)aso / (float)(aso + (DP_BIT15 - aso) * ab / BIT15_FIX);
    float fac_b = 1.0f - fac_a;
    DP_Spectral spectral_b = pixel15_to_spectral(dp);
    mix_spectral_channels(channels_a, spectral_b.channels, fac_a, fac_b);
    return spectral_b;
}

static BGRf blend_pigment_mask(DP_Pixel15 dp, const float *spectral_b_channels,
                               Fix15 ab, Fix15 aso)
{
    DP_Spectral spectral = mix_spectral(dp, spectral_b_channels, ab, aso);
    return spectral_to_rgb(spectral.channels);
}

static void assign_pigment(DP_Pixel15 *dst, uint16_t ab, BGRf cr_f)
{
    float ar_roundf = 0.5f + (float)ab;
    dst->b = (uint16_t)(cr_f.b * ar_roundf);
    dst->g = (uint16_t)(cr_f.g * ar_roundf);
    dst->r = (uint16_t)(cr_f.r * ar_roundf);
}

static void assign_pigment_alpha(DP_Pixel15 *dst, Fix15 ab, Fix15 aso,
                                 BGRf cr_f)
{
    float ar_f = (float)fix15_clamp(aso + fix15_mul(ab, (DP_BIT15 - aso)));
    float ar_roundf = 0.5f + ar_f;
    dst->b = (uint16_t)(cr_f.b * ar_roundf);
    dst->g = (uint16_t)(cr_f.g * ar_roundf);
    dst->r = (uint16_t)(cr_f.r * ar_roundf);
    dst->a = (uint16_t)ar_f;
}

static void blend_mask_pigment(DP_Pixel15 *dst, DP_UPixel15 src,
                               const uint16_t *mask, Fix15 opacity, int w,
                               int h, int mask_skip, int base_skip)
{
    BGR15 cs = to_ubgr(src);
    DP_Spectral spectral_b = rgb_to_spectral(
        (BGRf){(float)src.b / BIT15_FLOAT, (float)src.g / BIT15_FLOAT,
               (float)src.r / BIT15_FLOAT});
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        DP_Pixel15 dp = *dst;
        if (dp.a != 0 && as != 0) {
            if (as == BIT15_FIX && opacity == BIT15_FIX) {
                Fix15 ab = to_fix(dp.a);
                dst->b = from_fix(fix15_mul(cs.b, ab));
                dst->g = from_fix(fix15_mul(cs.g, ab));
                dst->r = from_fix(fix15_mul(cs.r, ab));
            }
            else {
                Fix15 ab = from_fix(dp.a);
                Fix15 aso = fix15_mul(as, opacity);
                BGRf cr_f =
                    blend_pigment_mask(dp, spectral_b.channels, ab, aso);
                assign_pigment(dst, dp.a, cr_f);
            }
        }
    });
}

static void blend_mask_pigment_alpha(DP_Pixel15 *dst, DP_UPixel15 src,
                                     const uint16_t *mask, Fix15 opacity, int w,
                                     int h, int mask_skip, int base_skip)
{
    BGR15 cs = to_ubgr(src);
    DP_Spectral spectral_b = rgb_to_spectral(
        (BGRf){(float)src.b / BIT15_FLOAT, (float)src.g / BIT15_FLOAT,
               (float)src.r / BIT15_FLOAT});
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        if (as != 0) {
            DP_Pixel15 dp = *dst;
            if (dp.a == 0) {
                Fix15 aso = fix15_mul(as, opacity);
                *dst = ((DP_Pixel15){
                    .b = from_fix(fix15_mul(cs.b, aso)),
                    .g = from_fix(fix15_mul(cs.g, aso)),
                    .r = from_fix(fix15_mul(cs.r, aso)),
                    .a = from_fix(aso),
                });
            }
            else if (as == BIT15_FIX && opacity == BIT15_FIX) {
                *dst = ((DP_Pixel15){
                    .b = src.b,
                    .g = src.g,
                    .r = src.r,
                    .a = DP_BIT15,
                });
            }
            else {
                Fix15 ab = from_fix(dp.a);
                Fix15 aso = fix15_mul(as, opacity);
                BGRf cr_f =
                    blend_pigment_mask(dp, spectral_b.channels, ab, aso);
                assign_pigment_alpha(dst, ab, aso, cr_f);
            }
        }
    });
}
// SPDX-SnippetEnd

static void blend_mask_composite_separable(DP_Pixel15 *dst, DP_UPixel15 src,
                                           const uint16_t *mask, Fix15 opacity,
                                           int w, int h, int mask_skip,
                                           int base_skip,
                                           Fix15 (*comp_op)(Fix15, Fix15))
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        DP_Pixel15 bp = *dst;
        if (bp.a != 0) {
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
            Fix15 o1 = BIT15_FIX - o;
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(fix15_sumprods(o1, cb.b, o, comp_op(cb.b, cs.b))),
                from_fix(fix15_sumprods(o1, cb.g, o, comp_op(cb.g, cs.g))),
                from_fix(fix15_sumprods(o1, cb.r, o, comp_op(cb.r, cs.r))),
                bp.a,
            });
        }
    });
}

static void composite_separable_alpha(DP_Pixel15 *dst, BGR15 cs, Fix15 as,
                                      Fix15 opacity,
                                      Fix15 (*comp_op)(Fix15, Fix15))
{
    DP_Pixel15 bp = *dst;
    BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
    Fix15 ab = bp.a;
    Fix15 ab1 = BIT15_FIX - ab;
    *dst = source_over_premultiplied(
        bp, (BGRA15){
                .b = fix15_sumprods(ab1, cs.b, ab, comp_op(cb.b, cs.b)),
                .g = fix15_sumprods(ab1, cs.g, ab, comp_op(cb.g, cs.g)),
                .r = fix15_sumprods(ab1, cs.r, ab, comp_op(cb.r, cs.r)),
                .a = fix15_mul(as, opacity),
            });
}

static void blend_mask_composite_separable_alpha(
    DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask, Fix15 opacity,
    int w, int h, int mask_skip, int base_skip, Fix15 (*comp_op)(Fix15, Fix15))
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, m, {
        if (m != 0) {
            composite_separable_alpha(dst, cs, m, opacity, comp_op);
        }
    });
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: MIT
// SDPX—SnippetName: SAI blending logic based on psd-export by cromachina
static void blend_mask_composite_separable_sai(DP_Pixel15 *dst, DP_UPixel15 src,
                                               const uint16_t *mask,
                                               Fix15 opacity, int w, int h,
                                               int mask_skip, int base_skip,
                                               Fix15 (*comp_op)(Fix15, Fix15))
{
    BGR15 ucs = to_ubgr_opacity(src, opacity);
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        DP_Pixel15 bp = *dst;
        if (bp.a != 0 && as != 0) {
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
            BGR15 cs = bgr_mul(ucs, as);
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(comp_op(cb.b, cs.b)),
                from_fix(comp_op(cb.g, cs.g)),
                from_fix(comp_op(cb.r, cs.r)),
                bp.a,
            });
        }
    });
}

static void blend_mask_composite_separable_sai_opacity(
    DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask, Fix15 opacity,
    int w, int h, int mask_skip, int base_skip,
    Fix15 (*comp_op)(Fix15, Fix15, Fix15))
{
    BGR15 ucs = to_ubgr_opacity(src, opacity);
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        DP_Pixel15 bp = *dst;
        if (bp.a != 0 && as != 0) {
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
            BGR15 cs = bgr_mul(ucs, as);
            Fix15 aso = fix15_mul(as, opacity);
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(comp_op(cb.b, cs.b, aso)),
                from_fix(comp_op(cb.g, cs.g, aso)),
                from_fix(comp_op(cb.r, cs.r, aso)),
                bp.a,
            });
        }
    });
}

static DP_Pixel15
blend_composite_separable_sai_alpha(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as,
                                    Fix15 opacity,
                                    Fix15 (*comp_op)(Fix15, Fix15))
{
    Fix15 aso = fix15_mul(as, opacity);
    return (DP_Pixel15){
        from_fix(fix15_lerp(cs.b, comp_op(cb.b, cs.b), ab)),
        from_fix(fix15_lerp(cs.g, comp_op(cb.g, cs.g), ab)),
        from_fix(fix15_lerp(cs.r, comp_op(cb.r, cs.r), ab)),
        from_fix(aso + fix15_mul(ab, BIT15_FIX - aso)),
    };
}

static DP_Pixel15 blend_composite_separable_sai_opacity_alpha(
    BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as, Fix15 opacity,
    Fix15 (*comp_op)(Fix15, Fix15, Fix15))
{
    Fix15 aso = fix15_mul(as, opacity);
    return (DP_Pixel15){
        from_fix(fix15_lerp(cs.b, comp_op(cb.b, cs.b, aso), ab)),
        from_fix(fix15_lerp(cs.g, comp_op(cb.g, cs.g, aso), ab)),
        from_fix(fix15_lerp(cs.r, comp_op(cb.r, cs.r, aso), ab)),
        from_fix(aso + fix15_mul(ab, BIT15_FIX - aso)),
    };
}

static void blend_mask_composite_separable_sai_alpha(
    DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask, Fix15 opacity,
    int w, int h, int mask_skip, int base_skip, Fix15 (*comp_op)(Fix15, Fix15))
{
    BGRA15 uc = {.bgr = to_ubgr_opacity(src, opacity), .a = opacity};
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        if (as != 0) {
            DP_Pixel15 bp = *dst;
            if (bp.a == 0) {
                *dst = from_bgra(bgra_mul(uc, as));
            }
            else {
                BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
                BGR15 cs = bgr_mul(uc.bgr, as);
                Fix15 ab = to_fix(bp.a);
                *dst = blend_composite_separable_sai_alpha(cb, cs, ab, as,
                                                           opacity, comp_op);
            }
        }
    });
}

static void blend_mask_composite_separable_sai_opacity_alpha(
    DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask, Fix15 opacity,
    int w, int h, int mask_skip, int base_skip,
    Fix15 (*comp_op)(Fix15, Fix15, Fix15))
{
    BGRA15 uc = {.bgr = to_ubgr_opacity(src, opacity), .a = opacity};
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        if (as != 0) {
            DP_Pixel15 bp = *dst;
            if (bp.a == 0) {
                *dst = from_bgra(bgra_mul(uc, as));
            }
            else {
                BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
                BGR15 cs = bgr_mul(uc.bgr, as);
                Fix15 ab = to_fix(bp.a);
                *dst = blend_composite_separable_sai_opacity_alpha(
                    cb, cs, ab, as, opacity, comp_op);
            }
        }
    });
}
// SPDX-SnippetEnd

static void blend_mask_composite_nonseparable(DP_Pixel15 *dst, DP_UPixel15 src,
                                              const uint16_t *mask,
                                              Fix15 opacity, int w, int h,
                                              int mask_skip, int base_skip,
                                              BGR15 (*comp_op)(BGR15, BGR15))
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        DP_Pixel15 bp = *dst;
        if (bp.a != 0) {
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
            BGR15 cr = comp_op(cb, cs);
            Fix15 o1 = BIT15_FIX - o;
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(fix15_sumprods(o1, cb.b, o, cr.b)),
                from_fix(fix15_sumprods(o1, cb.g, o, cr.g)),
                from_fix(fix15_sumprods(o1, cb.r, o, cr.r)),
                bp.a,
            });
        }
    });
}

static void composite_nonseparable_alpha(DP_Pixel15 *dst, BGR15 cs, Fix15 as,
                                         Fix15 opacity,
                                         BGR15 (*comp_op)(BGR15, BGR15))
{
    DP_Pixel15 bp = *dst;
    BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
    BGR15 cr = comp_op(cb, cs);
    Fix15 ab = bp.a;
    Fix15 ab1 = BIT15_FIX - ab;
    *dst = source_over_premultiplied(
        bp, (BGRA15){
                .b = fix15_sumprods(ab1, cs.b, ab, cr.b),
                .g = fix15_sumprods(ab1, cs.g, ab, cr.g),
                .r = fix15_sumprods(ab1, cs.r, ab, cr.r),
                .a = fix15_mul(as, opacity),
            });
}

static void blend_mask_composite_nonseparable_alpha(
    DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask, Fix15 opacity,
    int w, int h, int mask_skip, int base_skip, BGR15 (*comp_op)(BGR15, BGR15))
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, m, {
        if (m != 0) {
            composite_nonseparable_alpha(dst, cs, m, opacity, comp_op);
        }
    });
}

static void blend_mask_pixels_normal(DP_Pixel15 *dst, DP_UPixel15 src,
                                     const uint16_t *mask, Fix15 opacity,
                                     int count)
{
    for (int x = 0; x < count; ++x, ++dst, ++mask) {
        Fix15 o = fix15_mul(*mask, opacity);
        Fix15 as1 = BIT15_FIX - fix15_mul(BIT15_FIX, o);
        dst->b =
            from_fix(fix15_sumprods(to_fix(dst->b), as1, to_fix(src.b), o));
        dst->g =
            from_fix(fix15_sumprods(to_fix(dst->g), as1, to_fix(src.g), o));
        dst->r =
            from_fix(fix15_sumprods(to_fix(dst->r), as1, to_fix(src.r), o));
        dst->a = from_fix(fix15_sumprods(to_fix(dst->a), as1, BIT15_FIX, o));
    }
}

// Adapted from libmypaint, see license above.
static void blend_mask_pixels_normal_and_eraser(DP_Pixel15 *dst,
                                                DP_UPixel15 src,
                                                const uint16_t *mask,
                                                Fix15 opacity, int count)
{
    for (int x = 0; x < count; ++x, ++dst, ++mask) {
        Fix15 o = fix15_mul(*mask, opacity);

        Fix15 opa_a = fix15_mul(o, src.a);
        Fix15 opa_b = DP_BIT15 - o;

        // clang-format off
        dst->b = from_fix(fix15_sumprods(opa_a, to_fix(src.b), opa_b, to_fix(dst->b)));
        dst->g = from_fix(fix15_sumprods(opa_a, to_fix(src.g), opa_b, to_fix(dst->g)));
        dst->r = from_fix(fix15_sumprods(opa_a, to_fix(src.r), opa_b, to_fix(dst->r)));
        dst->a = from_fix(opa_a + ((opa_b * to_fix(dst->a)) / DP_BIT15));
        // clang-format on
    }
}

static void blend_mask_pixels_recolor(DP_Pixel15 *dst, DP_UPixel15 src,
                                      const uint16_t *mask, Fix15 opacity,
                                      int count)
{
    for (int x = 0; x < count; ++x, ++dst, ++mask) {
        Fix15 o = fix15_mul(*mask, opacity);
        Fix15 as = fix15_mul(dst->a, o);
        Fix15 as1 = BIT15_FIX - fix15_mul(BIT15_FIX, o);
        dst->b =
            from_fix(fix15_sumprods(to_fix(dst->b), as1, to_fix(src.b), as));
        dst->g =
            from_fix(fix15_sumprods(to_fix(dst->g), as1, to_fix(src.g), as));
        dst->r =
            from_fix(fix15_sumprods(to_fix(dst->r), as1, to_fix(src.r), as));
    }
}

static void blend_mask_normal(DP_Pixel15 *dst, DP_UPixel15 src,
                              const uint16_t *mask, Fix15 opacity, int w, int h,
                              int mask_skip, int base_skip)
{
#ifdef DP_CPU_X64
    for (int y = 0; y < h; ++y) {
        int remaining = w;

        if (DP_cpu_support >= DP_CPU_SUPPORT_AVX2) {
            int remaining_after_avx_width = remaining % 8;
            int avx_width = remaining - remaining_after_avx_width;

            blend_mask_pixels_normal_avx2(dst, src, mask, opacity, avx_width);

            remaining -= avx_width;
            dst += avx_width;
            mask += avx_width;
        }

        if (DP_cpu_support >= DP_CPU_SUPPORT_SSE42) {
            int remaining_after_sse_width = remaining % 4;
            int sse_width = remaining - remaining_after_sse_width;

            blend_mask_pixels_normal_sse42(dst, src, mask, opacity, sse_width);

            remaining -= sse_width;
            dst += sse_width;
            mask += sse_width;
        }

        blend_mask_pixels_normal(dst, src, mask, opacity, remaining);
        dst += remaining;
        mask += remaining;

        dst += base_skip;
        mask += mask_skip;
    }
#else
    for (int y = 0; y < h; ++y) {
        blend_mask_pixels_normal(dst, src, mask, opacity, w);

        dst += w + base_skip;
        mask += w + mask_skip;
    }
#endif
}

static void blend_mask_normal_and_eraser(DP_Pixel15 *dst, DP_UPixel15 src,
                                         const uint16_t *mask, Fix15 opacity,
                                         int w, int h, int mask_skip,
                                         int base_skip)
{
#ifdef DP_CPU_X64
    for (int y = 0; y < h; ++y) {
        int remaining = w;

        if (DP_cpu_support >= DP_CPU_SUPPORT_AVX2) {
            int remaining_after_avx_width = remaining % 8;
            int avx_width = remaining - remaining_after_avx_width;

            blend_mask_pixels_normal_and_eraser_avx2(dst, src, mask, opacity,
                                                     avx_width);

            remaining -= avx_width;
            dst += avx_width;
            mask += avx_width;
        }

        if (DP_cpu_support >= DP_CPU_SUPPORT_SSE42) {
            int remaining_after_sse_width = remaining % 4;
            int sse_width = remaining - remaining_after_sse_width;

            blend_mask_pixels_normal_and_eraser_sse42(dst, src, mask, opacity,
                                                      sse_width);

            remaining -= sse_width;
            dst += sse_width;
            mask += sse_width;
        }

        blend_mask_pixels_normal_and_eraser(dst, src, mask, opacity, remaining);
        dst += remaining;
        mask += remaining;

        dst += base_skip;
        mask += mask_skip;
    }
#else
    for (int y = 0; y < h; ++y) {
        blend_mask_pixels_normal_and_eraser(dst, src, mask, opacity, w);

        dst += w + base_skip;
        mask += w + mask_skip;
    }
#endif
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: MIT
// SDPX—SnippetName: Based on libmypaint's brushmodes.c

// Fast sigmoid-like function with constant offsets, used to get a
// fairly smooth transition between additive and spectral blending.
static float spectral_blend_factor(float x)
{
    float ver_fac = 1.65f; // vertical compression factor
    float hor_fac = 8.0f;  // horizontal compression factor
    float hor_offs = 3.0f; // horizontal offset (slightly left of center)
    float b = x * hor_fac - hor_offs;
    return 0.5f + b / (1.0f + fabsf(b) * ver_fac);
}

static void blend_mask_pigment_and_eraser(DP_Pixel15 *dst, DP_UPixel15 src,
                                          const uint16_t *mask, Fix15 opacity,
                                          int w, int h, int mask_skip,
                                          int base_skip)
{
    Fix15 erase_alpha = from_fix(src.a);
    BGR15 cs = to_ubgr(src);
    DP_Spectral spectral_a = rgb_to_spectral(
        (BGRf){(float)src.b / BIT15_FLOAT, (float)src.g / BIT15_FLOAT,
               (float)src.r / BIT15_FLOAT});

    // pigment-mode does not like very low opacity, probably due to rounding
    // errors with int->float->int round-trip.
    if (opacity < 150) {
        opacity = 150;
    }

    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        BGRA15 cb = to_bgra(*dst);
        Fix15 opa_a = fix15_mul(as, opacity);
        Fix15 opa_b = BIT15_FIX - opa_a;
        Fix15 opa_a2 = fix15_mul(opa_a, erase_alpha);
        Fix15 opa_out = opa_a2 + fix15_mul(opa_b, cb.a);

        // Spectral blending does not handle low transparency well, so we try to
        // patch that up by using mostly additive mixing for lower canvas
        // alphas, gradually moving to full spectral blending at mostly opaque
        // pixels.
        float ab_f = (float)cb.a;
        float spectral_factor =
            clampf(spectral_blend_factor(ab_f / BIT15_FLOAT));
        float additive_factor = 1.0f - spectral_factor;

        BGR15 cr;
        if (additive_factor == 0.0f) {
            cr.b = 0;
            cr.g = 0;
            cr.r = 0;
        }
        else {
            cr.b = fix15_sumprods(opa_a2, cs.b, opa_b, cb.b);
            cr.g = fix15_sumprods(opa_a2, cs.g, opa_b, cb.g);
            cr.r = fix15_sumprods(opa_a2, cs.r, opa_b, cb.r);
        }

        if (spectral_factor != 0.0f && cb.a != 0) {
            // Convert straightened tile pixel color to a spectral
            BGRf ucb_f = ((BGRf){(float)cb.b / ab_f, (float)cb.g / ab_f,
                                 (float)cb.r / ab_f});
            DP_Spectral spectral_b = rgb_to_spectral(ucb_f);

            float fac_a =
                ((float)opa_a / (float)(opa_a + fix15_mul(opa_b, cb.a)))
                * ((float)erase_alpha / BIT15_FLOAT);
            float fac_b = 1.0f - fac_a;

            // Mix input and tile pixel colors using WGM (into spectral_b)
            mix_spectral_channels(spectral_a.channels, spectral_b.channels,
                                  fac_a, fac_b);

            // Convert back to RGB
            BGRf s = spectral_to_rgb(spectral_b.channels);
            float opa_out_f = (float)opa_out;
            cr.b = (Fix15)((additive_factor * (float)cr.b)
                           + (spectral_factor * s.b * opa_out_f));
            cr.g = (Fix15)((additive_factor * (float)cr.g)
                           + (spectral_factor * s.g * opa_out_f));
            cr.r = (Fix15)((additive_factor * (float)cr.r)
                           + (spectral_factor * s.r * opa_out_f));
        }

        *dst = ((DP_Pixel15){
            .b = from_fix(cr.b),
            .g = from_fix(cr.g),
            .r = from_fix(cr.r),
            .a = from_fix(opa_out),
        });
    });
}
// SPDX-SnippetEnd

static void blend_mask_compare_density_soft(DP_Pixel15 *dst, DP_UPixel15 src,
                                            const uint16_t *mask, Fix15 opacity,
                                            int w, int h, int mask_skip,
                                            int base_skip)
{
    // Replaces pixels if the alpha value is greater.
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, aso, {
        if (dst->a < aso) {
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                src.b,
                src.g,
                src.r,
                from_fix(aso),
            });
        }
    });
}

static void blend_mask_compare_density(DP_Pixel15 *dst, DP_UPixel15 src,
                                       const uint16_t *mask, Fix15 opacity,
                                       int w, int h, int mask_skip,
                                       int base_skip)
{
    // Blends alpha and replaces color if the dab opacity is greater.
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        Fix15 ab = dst->a;
        if (as != 0 && ab < opacity) {
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                src.b,
                src.g,
                src.r,
                from_fix(ab + fix15_mul(as, opacity - ab)),
            });
        }
    });
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: Based on Krita's KoCompositeOpAlphaDarken.h
static DP_Pixel15 blend_marker(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as, Fix15 o)
{
    Fix15 k = fix15_clamp_div(fix15_mul(as, o), ab);
    return DP_pixel15_premultiply((DP_UPixel15){
        from_fix(fix15_lerp(cb.b, cs.b, k)),
        from_fix(fix15_lerp(cb.g, cs.g, k)),
        from_fix(fix15_lerp(cb.r, cs.r, k)),
        from_fix(ab),
    });
}

static void blend_mask_marker(DP_Pixel15 *dst, DP_UPixel15 src,
                              const uint16_t *mask, Fix15 opacity, int w, int h,
                              int mask_skip, int base_skip)
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        Fix15 ab = dst->a;
        if (as != 0 && ab != 0) {
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(*dst));
            *dst = blend_marker(cb, cs, ab, as, opacity);
        }
    });
}

static void blend_mask_marker_alpha(DP_Pixel15 *dst, DP_UPixel15 src,
                                    const uint16_t *mask, Fix15 opacity, int w,
                                    int h, int mask_skip, int base_skip)
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        if (as != 0) {
            Fix15 ab = dst->a;
            Fix15 aso = fix15_mul(as, opacity);
            if (ab == 0) {
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    src.b,
                    src.g,
                    src.r,
                    from_fix(aso),
                });
            }
            else {
                Fix15 ar = ab < aso ? aso : ab;
                BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(*dst));
                Fix15 k = fix15_clamp_div(aso, ab);
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    from_fix(fix15_lerp(cb.b, cs.b, k)),
                    from_fix(fix15_lerp(cb.g, cs.g, k)),
                    from_fix(fix15_lerp(cb.r, cs.r, k)),
                    from_fix(ar),
                });
            }
        }
    });
}

static DP_Pixel15 blend_marker_alpha_wash(BGR15 cb, BGR15 cs, Fix15 ab,
                                          Fix15 as, Fix15 o)
{
    Fix15 ar = ab < o ? ab + fix15_mul(as, o - ab) : ab;
    Fix15 k = fix15_clamp_div(fix15_mul(as, o), ab);
    return DP_pixel15_premultiply((DP_UPixel15){
        from_fix(fix15_lerp(cb.b, cs.b, k)),
        from_fix(fix15_lerp(cb.g, cs.g, k)),
        from_fix(fix15_lerp(cb.r, cs.r, k)),
        from_fix(ar),
    });
}

static void blend_mask_marker_alpha_wash(DP_Pixel15 *dst, DP_UPixel15 src,
                                         const uint16_t *mask, Fix15 opacity,
                                         int w, int h, int mask_skip,
                                         int base_skip)
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        if (as != 0) {
            Fix15 ab = dst->a;
            if (ab == 0) {
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    src.b,
                    src.g,
                    src.r,
                    from_fix(fix15_mul(as, opacity)),
                });
            }
            else {
                BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(*dst));
                *dst = blend_marker_alpha_wash(cb, cs, ab, as, opacity);
            }
        }
    });
}
// SPDX-SnippetEnd

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: Based on Krita's KoCompositeOpGreater.h
static void blend_mask_greater(DP_Pixel15 *dst, DP_UPixel15 src,
                               const uint16_t *mask, Fix15 opacity, int w,
                               int h, int mask_skip, int base_skip)
{
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, aso, {
        if (aso != 0) {
            Fix15 ab = dst->a;
            if (ab != 0 && ab < aso) {
                float ad = (float)(aso - ab) / BIT15_FLOAT;
                Fix15 wa = to_fix(
                    DP_channel_float_to_15(1.0f / (1.0f + expf(40.0f * ad))));
                Fix15 a = fix15_sumprods(ab, wa, aso, DP_BIT15 - wa);
                Fix15 fake_opacity =
                    BIT15_FIX
                    - fix15_clamp_div((BIT15_FIX - a), (BIT15_FIX - ab));
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->b), src.b, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->g), src.g, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->r), src.r, fake_opacity), a)),
                    from_fix(ab),
                });
            }
        }
    });
}

static void blend_mask_greater_wash(DP_Pixel15 *dst, DP_UPixel15 src,
                                    const uint16_t *mask, Fix15 opacity, int w,
                                    int h, int mask_skip, int base_skip)
{
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        if (as != 0) {
            Fix15 ab = dst->a;
            if (ab != 0 && ab < opacity) {
                Fix15 ar = ab + fix15_mul(as, opacity - ab);
                float ad = (float)(ar - ab) / BIT15_FLOAT;
                Fix15 wa = to_fix(
                    DP_channel_float_to_15(1.0f / (1.0f + expf(40.0f * ad))));
                Fix15 a = fix15_sumprods(ab, wa, ar, DP_BIT15 - wa);
                Fix15 fake_opacity =
                    BIT15_FIX
                    - fix15_clamp_div((BIT15_FIX - a), (BIT15_FIX - ab));
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->b), src.b, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->g), src.g, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->r), src.r, fake_opacity), a)),
                    from_fix(ab),
                });
            }
        }
    });
}

static void blend_mask_greater_alpha(DP_Pixel15 *dst, DP_UPixel15 src,
                                     const uint16_t *mask, Fix15 opacity, int w,
                                     int h, int mask_skip, int base_skip)
{
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, aso, {
        if (aso != 0) {
            Fix15 ab = dst->a;
            if (ab == 0) {
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    src.b,
                    src.g,
                    src.r,
                    from_fix(aso),
                });
            }
            else if (ab < aso) {
                float ad = (float)(aso - ab) / BIT15_FLOAT;
                Fix15 wa = to_fix(
                    DP_channel_float_to_15(1.0f / (1.0f + expf(40.0f * ad))));
                Fix15 a = fix15_sumprods(ab, wa, aso, DP_BIT15 - wa);
                Fix15 fake_opacity =
                    BIT15_FIX
                    - fix15_clamp_div((BIT15_FIX - a), (BIT15_FIX - ab));
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->b), src.b, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->g), src.g, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->r), src.r, fake_opacity), a)),
                    from_fix(aso),
                });
            }
        }
    });
}

static void blend_mask_greater_alpha_wash(DP_Pixel15 *dst, DP_UPixel15 src,
                                          const uint16_t *mask, Fix15 opacity,
                                          int w, int h, int mask_skip,
                                          int base_skip)
{
    FOR_MASK_PIXEL_M(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        if (as != 0) {
            Fix15 ab = dst->a;
            if (ab == 0) {
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    src.b,
                    src.g,
                    src.r,
                    from_fix(fix15_mul(as, opacity)),
                });
            }
            else if (ab < opacity) {
                Fix15 ar = ab + fix15_mul(as, opacity - ab);
                float ad = (float)(ar - ab) / BIT15_FLOAT;
                Fix15 wa = to_fix(
                    DP_channel_float_to_15(1.0f / (1.0f + expf(40.0f * ad))));
                Fix15 a = fix15_sumprods(ab, wa, ar, DP_BIT15 - wa);
                Fix15 fake_opacity =
                    BIT15_FIX
                    - fix15_clamp_div((BIT15_FIX - a), (BIT15_FIX - ab));
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->b), src.b, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->g), src.g, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->r), src.r, fake_opacity), a)),
                    from_fix(ar),
                });
            }
        }
    });
}
// SPDX-SnippetEnd

static void blend_mask_recolor(DP_Pixel15 *dst, DP_UPixel15 src,
                               const uint16_t *mask, Fix15 opacity, int w,
                               int h, int mask_skip, int base_skip)
{
#ifdef DP_CPU_X64
    for (int y = 0; y < h; ++y) {
        int remaining = w;

        if (DP_cpu_support >= DP_CPU_SUPPORT_AVX2) {
            int remaining_after_avx_width = remaining % 8;
            int avx_width = remaining - remaining_after_avx_width;

            blend_mask_pixels_recolor_avx2(dst, src, mask, opacity, avx_width);

            remaining -= avx_width;
            dst += avx_width;
            mask += avx_width;
        }

        if (DP_cpu_support >= DP_CPU_SUPPORT_SSE42) {
            int remaining_after_sse_width = remaining % 4;
            int sse_width = remaining - remaining_after_sse_width;

            blend_mask_pixels_recolor_sse42(dst, src, mask, opacity, sse_width);

            remaining -= sse_width;
            dst += sse_width;
            mask += sse_width;
        }

        blend_mask_pixels_recolor(dst, src, mask, opacity, remaining);
        dst += remaining;
        mask += remaining;

        dst += base_skip;
        mask += mask_skip;
    }
#else
    for (int y = 0; y < h; ++y) {
        blend_mask_pixels_recolor(dst, src, mask, opacity, w);

        dst += w + base_skip;
        mask += w + mask_skip;
    }
#endif
}

static void
blend_mask_to_alpha_op(DP_Pixel15 *dst, const uint16_t *mask, Fix15 opacity,
                       int w, int h, int mask_skip, int base_skip,
                       BGRA15 (*op)(BGR15, BGR15, Fix15, Fix15, Fix15))
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x, ++dst, ++mask) {
            Fix15 o = fix15_mul(*mask, opacity);
            DP_Pixel15 dp = *dst;
            BGRA15 b = to_bgra(dp);
            BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(dp));
            *dst = from_bgra(op(b.bgr, cs, b.a, ((Fix15)(1 << 15)), o));
        }
        dst += base_skip;
        mask += mask_skip;
    }
}

void DP_blend_mask(DP_Pixel15 *dst, DP_UPixel15 src, int blend_mode,
                   const uint16_t *mask, uint16_t opacity, int w, int h,
                   int mask_skip, int base_skip)
{
    switch (blend_mode) {
    // Alpha-affecting blend modes.
    case DP_BLEND_MODE_NORMAL:
        blend_mask_normal(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                          base_skip);
        break;
    case DP_BLEND_MODE_BEHIND:
    case DP_BLEND_MODE_BEHIND_PRESERVE:
        blend_mask_alpha_op(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                            base_skip, blend_behind);
        break;
    case DP_BLEND_MODE_ERASE:
    case DP_BLEND_MODE_ERASE_PRESERVE:
        blend_mask_alpha_op(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                            base_skip, blend_erase);
        break;
    case DP_BLEND_MODE_NORMAL_AND_ERASER:
        blend_mask_normal_and_eraser(dst, src, mask, to_fix(opacity), w, h,
                                     mask_skip, base_skip);
        break;
    case DP_BLEND_MODE_COMPARE_DENSITY_SOFT:
        blend_mask_compare_density_soft(dst, src, mask, to_fix(opacity), w, h,
                                        mask_skip, base_skip);
        break;
    case DP_BLEND_MODE_COMPARE_DENSITY:
        blend_mask_compare_density(dst, src, mask, to_fix(opacity), w, h,
                                   mask_skip, base_skip);
        break;
    case DP_BLEND_MODE_REPLACE:
        blend_mask_alpha_op(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                            base_skip, blend_replace);
        break;
    case DP_BLEND_MODE_COLOR_ERASE:
    case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
        blend_mask_color_erase(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                               base_skip);
        break;
    case DP_BLEND_MODE_PIGMENT_ALPHA:
        blend_mask_pigment_alpha(dst, src, mask, to_fix(opacity), w, h,
                                 mask_skip, base_skip);
        break;
    case DP_BLEND_MODE_PIGMENT_AND_ERASER:
        blend_mask_pigment_and_eraser(dst, src, mask, to_fix(opacity), w, h,
                                      mask_skip, base_skip);
        break;
    case DP_BLEND_MODE_LIGHT_TO_ALPHA:
    case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
        blend_mask_to_alpha_op(dst, mask, to_fix(opacity), w, h, mask_skip,
                               base_skip, blend_erase_light);
        break;
    case DP_BLEND_MODE_DARK_TO_ALPHA:
    case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
        blend_mask_to_alpha_op(dst, mask, to_fix(opacity), w, h, mask_skip,
                               base_skip, blend_erase_dark);
        break;
    // Alpha-preserving separable blend modes (each channel handled separately)
    case DP_BLEND_MODE_MULTIPLY:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_multiply);
        break;
    case DP_BLEND_MODE_DIVIDE:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_divide);
        break;
    case DP_BLEND_MODE_BURN:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_burn);
        break;
    case DP_BLEND_MODE_DODGE:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_dodge);
        break;
    case DP_BLEND_MODE_VIVID_LIGHT:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_vivid_light);
        break;
    case DP_BLEND_MODE_PIN_LIGHT:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_pin_light);
        break;
    case DP_BLEND_MODE_DARKEN:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_darken);
        break;
    case DP_BLEND_MODE_LIGHTEN:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_lighten);
        break;
    case DP_BLEND_MODE_SUBTRACT:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_subtract);
        break;
    case DP_BLEND_MODE_ADD:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_add);
        break;
    case DP_BLEND_MODE_DIFFERENCE:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_difference);
        break;
    case DP_BLEND_MODE_RECOLOR:
        blend_mask_recolor(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                           base_skip);
        break;
    case DP_BLEND_MODE_SCREEN:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_screen);
        break;
    case DP_BLEND_MODE_OVERLAY:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_overlay);
        break;
    case DP_BLEND_MODE_HARD_LIGHT:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_hard_light);
        break;
    case DP_BLEND_MODE_SOFT_LIGHT:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_soft_light);
        break;
    case DP_BLEND_MODE_LINEAR_BURN:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_linear_burn);
        break;
    case DP_BLEND_MODE_LINEAR_LIGHT:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_linear_light);
        break;
    case DP_BLEND_MODE_MARKER:
    case DP_BLEND_MODE_MARKER_WASH:
        blend_mask_marker(dst, src, mask, opacity, w, h, mask_skip, base_skip);
        break;
    case DP_BLEND_MODE_GREATER:
        blend_mask_greater(dst, src, mask, opacity, w, h, mask_skip, base_skip);
        break;
    case DP_BLEND_MODE_GREATER_WASH:
        blend_mask_greater_wash(dst, src, mask, opacity, w, h, mask_skip,
                                base_skip);
        break;
    // Alpha-preserving separable blend modes where the opacity affects blending
    case DP_BLEND_MODE_SHADE_SAI:
        blend_mask_composite_separable_sai_opacity(dst, src, mask, opacity, w,
                                                   h, mask_skip, base_skip,
                                                   comp_linear_burn_sai);
        break;
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
        blend_mask_composite_separable_sai(dst, src, mask, opacity, w, h,
                                           mask_skip, base_skip, comp_add);
        break;
    case DP_BLEND_MODE_SHADE_SHINE_SAI:
        blend_mask_composite_separable_sai_opacity(dst, src, mask, opacity, w,
                                                   h, mask_skip, base_skip,
                                                   comp_linear_light_sai);
        break;
    case DP_BLEND_MODE_BURN_SAI:
        blend_mask_composite_separable_sai_opacity(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_burn_sai);
        break;
    case DP_BLEND_MODE_DODGE_SAI:
        blend_mask_composite_separable_sai(dst, src, mask, opacity, w, h,
                                           mask_skip, base_skip, comp_dodge);
        break;
    case DP_BLEND_MODE_BURN_DODGE_SAI:
        blend_mask_composite_separable_sai_opacity(dst, src, mask, opacity, w,
                                                   h, mask_skip, base_skip,
                                                   comp_vivid_light_sai);
        break;
    case DP_BLEND_MODE_HARD_MIX_SAI:
        blend_mask_composite_separable_sai_opacity(dst, src, mask, opacity, w,
                                                   h, mask_skip, base_skip,
                                                   comp_hard_mix_sai);
        break;
    case DP_BLEND_MODE_DIFFERENCE_SAI:
        blend_mask_composite_separable_sai(dst, src, mask, opacity, w, h,
                                           mask_skip, base_skip,
                                           comp_difference);
        break;
    // Alpha-preserving non-separable blend modes (channels interact)
    case DP_BLEND_MODE_HUE:
        blend_mask_composite_nonseparable(dst, src, mask, opacity, w, h,
                                          mask_skip, base_skip, comp_hue);
        break;
    case DP_BLEND_MODE_SATURATION:
        blend_mask_composite_nonseparable(dst, src, mask, opacity, w, h,
                                          mask_skip, base_skip,
                                          comp_saturation);
        break;
    case DP_BLEND_MODE_LUMINOSITY:
        blend_mask_composite_nonseparable(dst, src, mask, opacity, w, h,
                                          mask_skip, base_skip,
                                          comp_luminosity);
        break;
    case DP_BLEND_MODE_COLOR:
        blend_mask_composite_nonseparable(dst, src, mask, opacity, w, h,
                                          mask_skip, base_skip, comp_color);
        break;
    case DP_BLEND_MODE_DARKER_COLOR:
        blend_mask_composite_nonseparable(dst, src, mask, opacity, w, h,
                                          mask_skip, base_skip,
                                          comp_darker_color);
        break;
    case DP_BLEND_MODE_LIGHTER_COLOR:
        blend_mask_composite_nonseparable(dst, src, mask, opacity, w, h,
                                          mask_skip, base_skip,
                                          comp_lighter_color);
        break;
    case DP_BLEND_MODE_PIGMENT:
        blend_mask_pigment(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                           base_skip);
        break;
    // Alpha-affecting separable blend modes (each channel handled separately)
    case DP_BLEND_MODE_MULTIPLY_ALPHA:
        blend_mask_composite_separable_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_multiply);
        break;
    case DP_BLEND_MODE_DIVIDE_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip, comp_divide);
        break;
    case DP_BLEND_MODE_BURN_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip, comp_burn);
        break;
    case DP_BLEND_MODE_DODGE_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip, comp_dodge);
        break;
    case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip,
                                             comp_vivid_light);
        break;
    case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip,
                                             comp_pin_light);
        break;
    case DP_BLEND_MODE_DARKEN_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip, comp_darken);
        break;
    case DP_BLEND_MODE_LIGHTEN_ALPHA:
        blend_mask_composite_separable_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_lighten);
        break;
    case DP_BLEND_MODE_SUBTRACT_ALPHA:
        blend_mask_composite_separable_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_subtract);
        break;
    case DP_BLEND_MODE_ADD_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip, comp_add);
        break;
    case DP_BLEND_MODE_DIFFERENCE_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip,
                                             comp_difference);
        break;
    case DP_BLEND_MODE_SCREEN_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip, comp_screen);
        break;
    case DP_BLEND_MODE_OVERLAY_ALPHA:
        blend_mask_composite_separable_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_overlay);
        break;
    case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip,
                                             comp_hard_light);
        break;
    case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip,
                                             comp_soft_light);
        break;
    case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip,
                                             comp_linear_burn);
        break;
    case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
        blend_mask_composite_separable_alpha(dst, src, mask, opacity, w, h,
                                             mask_skip, base_skip,
                                             comp_linear_light);
        break;
    case DP_BLEND_MODE_MARKER_ALPHA:
        blend_mask_marker_alpha(dst, src, mask, opacity, w, h, mask_skip,
                                base_skip);
        break;
    case DP_BLEND_MODE_MARKER_ALPHA_WASH:
        blend_mask_marker_alpha_wash(dst, src, mask, opacity, w, h, mask_skip,
                                     base_skip);
        break;
    case DP_BLEND_MODE_GREATER_ALPHA:
        blend_mask_greater_alpha(dst, src, mask, opacity, w, h, mask_skip,
                                 base_skip);
        break;
    case DP_BLEND_MODE_GREATER_ALPHA_WASH:
        blend_mask_greater_alpha_wash(dst, src, mask, opacity, w, h, mask_skip,
                                      base_skip);
        break;
    // Alpha-preserving separable blend modes where the opacity affects blending
    case DP_BLEND_MODE_SHADE_SAI_ALPHA:
        blend_mask_composite_separable_sai_opacity_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip,
            comp_linear_burn_sai);
        break;
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA:
        blend_mask_composite_separable_sai_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_add);
        break;
    case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
        blend_mask_composite_separable_sai_opacity_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip,
            comp_linear_light_sai);
        break;
    case DP_BLEND_MODE_BURN_SAI_ALPHA:
        blend_mask_composite_separable_sai_opacity_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_burn_sai);
        break;
    case DP_BLEND_MODE_DODGE_SAI_ALPHA:
        blend_mask_composite_separable_sai_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_dodge);
        break;
    case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
        blend_mask_composite_separable_sai_opacity_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip,
            comp_vivid_light_sai);
        break;
    case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
        blend_mask_composite_separable_sai_opacity_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip,
            comp_hard_mix_sai);
        break;
    case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
        blend_mask_composite_separable_sai_alpha(dst, src, mask, opacity, w, h,
                                                 mask_skip, base_skip,
                                                 comp_difference);
        break;
    // Alpha-affecting non-separable blend modes (channels interact)
    case DP_BLEND_MODE_HUE_ALPHA:
        blend_mask_composite_nonseparable_alpha(dst, src, mask, opacity, w, h,
                                                mask_skip, base_skip, comp_hue);
        break;
    case DP_BLEND_MODE_SATURATION_ALPHA:
        blend_mask_composite_nonseparable_alpha(dst, src, mask, opacity, w, h,
                                                mask_skip, base_skip,
                                                comp_saturation);
        break;
    case DP_BLEND_MODE_LUMINOSITY_ALPHA:
        blend_mask_composite_nonseparable_alpha(dst, src, mask, opacity, w, h,
                                                mask_skip, base_skip,
                                                comp_luminosity);
        break;
    case DP_BLEND_MODE_COLOR_ALPHA:
        blend_mask_composite_nonseparable_alpha(
            dst, src, mask, opacity, w, h, mask_skip, base_skip, comp_color);
        break;
    case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
        blend_mask_composite_nonseparable_alpha(dst, src, mask, opacity, w, h,
                                                mask_skip, base_skip,
                                                comp_darker_color);
        break;
    case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
        blend_mask_composite_nonseparable_alpha(dst, src, mask, opacity, w, h,
                                                mask_skip, base_skip,
                                                comp_lighter_color);
        break;
    default:
        DP_debug("Unknown mask blend mode %d (%s)", blend_mode,
                 DP_blend_mode_enum_name(blend_mode));
        break;
    }
}


static void blend_pixels_alpha_op(DP_Pixel15 *DP_RESTRICT dst,
                                  const DP_Pixel15 *DP_RESTRICT src,
                                  int pixel_count, Fix15 opacity,
                                  BGRA15 (*op)(BGR15, BGR15, Fix15, Fix15,
                                               Fix15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        BGRA15 b = to_bgra(*dst);
        BGRA15 s = to_bgra(*src);
        *dst = from_bgra(op(b.bgr, s.bgr, b.a, s.a, opacity));
    }
}

static void blend_pixels_to_alpha_op(DP_Pixel15 *DP_RESTRICT dst,
                                     int pixel_count, Fix15 opacity,
                                     BGRA15 (*op)(BGR15, BGR15, Fix15, Fix15,
                                                  Fix15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst) {
        DP_Pixel15 dp = *dst;
        BGRA15 b = to_bgra(*dst);
        BGR15 s = to_ubgr(DP_pixel15_unpremultiply(dp));
        *dst = from_bgra(op(b.bgr, s, b.a, ((Fix15)(1 << 15)), opacity));
    }
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: Based on Krita's KoCompositeOpAlphaDarken.h
static void blend_pixels_marker(DP_Pixel15 *DP_RESTRICT dst,
                                const DP_Pixel15 *DP_RESTRICT src,
                                int pixel_count, Fix15 opacity)
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        Fix15 as = to_fix(src->a);
        Fix15 ab = to_fix(dst->a);
        if (as != 0 && ab != 0) {
            Fix15 aso = fix15_mul(as, opacity);
            BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(*src));
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(*dst));
            Fix15 k = fix15_clamp_div(aso, ab);
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(fix15_lerp(cb.b, cs.b, k)),
                from_fix(fix15_lerp(cb.g, cs.g, k)),
                from_fix(fix15_lerp(cb.r, cs.r, k)),
                from_fix(ab),
            });
        }
    }
}

static void blend_pixels_marker_alpha(DP_Pixel15 *DP_RESTRICT dst,
                                      const DP_Pixel15 *DP_RESTRICT src,
                                      int pixel_count, Fix15 opacity)
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        Fix15 as = to_fix(src->a);
        if (as != 0) {
            Fix15 ab = to_fix(dst->a);
            if (ab == 0) {
                if (opacity == DP_BIT15) {
                    *dst = *src;
                }
                else {
                    BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(*src));
                    *dst = DP_pixel15_premultiply((DP_UPixel15){
                        from_fix(cs.b),
                        from_fix(cs.g),
                        from_fix(cs.r),
                        from_fix(fix15_mul(as, opacity)),
                    });
                }
            }
            else {
                BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(*src));
                BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(*dst));
                *dst = blend_marker_alpha_wash(cb, cs, ab, as, opacity);
            }
        }
    }
}
// SPDX-SnippetEnd

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: Based on Krita's KoCompositeOpGreater.h
static void blend_pixels_greater(DP_Pixel15 *DP_RESTRICT dst,
                                 const DP_Pixel15 *DP_RESTRICT src,
                                 int pixel_count, Fix15 opacity)
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        Fix15 as = to_fix(src->a);
        Fix15 ab = to_fix(dst->a);
        if (as != 0 && ab != 0) {
            Fix15 aso = fix15_mul(as, opacity);
            if (ab < aso) {
                BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(*src));
                float ad = (float)(aso - ab) / BIT15_FLOAT;
                Fix15 wa = to_fix(
                    DP_channel_float_to_15(1.0f / (1.0f + expf(40.0f * ad))));
                Fix15 a = fix15_sumprods(ab, wa, aso, DP_BIT15 - wa);
                Fix15 fake_opacity =
                    BIT15_FIX
                    - fix15_clamp_div((BIT15_FIX - a), (BIT15_FIX - ab));
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->b), cs.b, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->g), cs.g, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->r), cs.r, fake_opacity), a)),
                    from_fix(ab),
                });
            }
        }
    }
}

static void blend_pixels_greater_alpha(DP_Pixel15 *DP_RESTRICT dst,
                                       const DP_Pixel15 *DP_RESTRICT src,
                                       int pixel_count, Fix15 opacity)
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        Fix15 as = to_fix(src->a);
        if (as != 0) {
            Fix15 ab = to_fix(dst->a);
            Fix15 aso = fix15_mul(as, opacity);
            if (ab == 0) {
                if (opacity == DP_BIT15) {
                    *dst = *src;
                }
                else {
                    BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(*src));
                    *dst = DP_pixel15_premultiply((DP_UPixel15){
                        from_fix(cs.b),
                        from_fix(cs.g),
                        from_fix(cs.r),
                        from_fix(aso),
                    });
                }
            }
            else if (ab < aso) {
                BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(*src));
                float ad = (float)(aso - ab) / BIT15_FLOAT;
                Fix15 wa = to_fix(
                    DP_channel_float_to_15(1.0f / (1.0f + expf(40.0f * ad))));
                Fix15 a = fix15_sumprods(ab, wa, aso, DP_BIT15 - wa);
                Fix15 fake_opacity =
                    BIT15_FIX
                    - fix15_clamp_div((BIT15_FIX - a), (BIT15_FIX - ab));
                *dst = DP_pixel15_premultiply((DP_UPixel15){
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->b), cs.b, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->g), cs.g, fake_opacity), a)),
                    from_fix(fix15_clamp_div(
                        fix15_lerp(to_fix(dst->r), cs.r, fake_opacity), a)),
                    from_fix(aso),
                });
            }
        }
    }
}
// SPDX-SnippetEnd

static void blend_pixels_color_erase(DP_Pixel15 *DP_RESTRICT dst,
                                     const DP_Pixel15 *DP_RESTRICT src,
                                     int pixel_count, uint16_t opacity)
{
    double o = DP_uint16_to_double(opacity) / BIT15_DOUBLE;
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_UPixel15 udst = DP_pixel15_unpremultiply(*dst);
        DP_UPixel15 usrc = DP_pixel15_unpremultiply(*src);
        *dst =
            blend_color_erase(usrc.b / BIT15_DOUBLE, usrc.g / BIT15_DOUBLE,
                              usrc.r / BIT15_DOUBLE, usrc.a / BIT15_DOUBLE * o,
                              udst.b / BIT15_DOUBLE, udst.g / BIT15_DOUBLE,
                              udst.r / BIT15_DOUBLE, udst.a / BIT15_DOUBLE);
    }
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-2.0-or-later
// SDPX—SnippetName: Spectral blending based on MyPaint's blending.hpp
static BGRf blend_pigment_pixel(DP_Pixel15 dp, DP_Pixel15 sp, Fix15 ab,
                                Fix15 aso)
{
    DP_Spectral spectral_a = pixel15_to_spectral(sp);
    DP_Spectral spectral_b = mix_spectral(dp, spectral_a.channels, ab, aso);
    return spectral_to_rgb(spectral_b.channels);
}

static void blend_pixels_pigment(DP_Pixel15 *DP_RESTRICT dst,
                                 const DP_Pixel15 *DP_RESTRICT src,
                                 int pixel_count, Fix15 opacity)
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 dp = *dst;
        DP_Pixel15 sp = *src;
        if (dp.a != 0 && sp.a != 0) {
            if (sp.a == DP_BIT15 && opacity == BIT15_FIX) {
                if (dp.a == DP_BIT15) {
                    dst->b = sp.b;
                    dst->g = sp.g;
                    dst->r = sp.r;
                }
                else {
                    BGR15 cs = to_bgr(sp);
                    Fix15 ab = to_fix(dp.a);
                    dst->b = from_fix(fix15_mul(cs.b, ab));
                    dst->g = from_fix(fix15_mul(cs.g, ab));
                    dst->r = from_fix(fix15_mul(cs.r, ab));
                }
            }
            else {
                Fix15 ab = from_fix(dp.a);
                Fix15 aso = fix15_mul(from_fix(sp.a), opacity);
                BGRf cr_f = blend_pigment_pixel(dp, sp, ab, aso);
                assign_pigment(dst, dp.a, cr_f);
            }
        }
    }
}

static void blend_pixels_pigment_alpha(DP_Pixel15 *DP_RESTRICT dst,
                                       const DP_Pixel15 *DP_RESTRICT src,
                                       int pixel_count, Fix15 opacity)
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 sp = *src;
        if (sp.a != 0) {
            DP_Pixel15 dp = *dst;
            if (dp.a == 0) {
                if (opacity == DP_BIT15) {
                    *dst = sp;
                }
                else {
                    BGRA15 s = to_bgra(sp);
                    *dst = (DP_Pixel15){
                        .b = from_fix(fix15_mul(s.b, opacity)),
                        .g = from_fix(fix15_mul(s.g, opacity)),
                        .r = from_fix(fix15_mul(s.r, opacity)),
                        .a = from_fix(fix15_mul(s.a, opacity)),
                    };
                }
            }
            else if (sp.a == DP_BIT15 && opacity == BIT15_FIX) {
                *dst = sp;
            }
            else {
                Fix15 ab = from_fix(dp.a);
                Fix15 aso = fix15_mul(from_fix(sp.a), opacity);
                BGRf cr_f = blend_pigment_pixel(dp, sp, ab, aso);
                assign_pigment_alpha(dst, ab, aso, cr_f);
            }
        }
    }
}
// SPDX-SnippetEnd

static void blend_pixels_composite_separable(DP_Pixel15 *DP_RESTRICT dst,
                                             const DP_Pixel15 *DP_RESTRICT src,
                                             int pixel_count, Fix15 opacity,
                                             Fix15 (*comp_op)(Fix15, Fix15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 bp = *dst;
        if (bp.a != 0) {
            DP_Pixel15 sp = *src;
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
            BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(sp));
            Fix15 o = fix15_mul(to_fix(sp.a), opacity);
            Fix15 o1 = BIT15_FIX - o;
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(fix15_sumprods(o1, cb.b, o, comp_op(cb.b, cs.b))),
                from_fix(fix15_sumprods(o1, cb.g, o, comp_op(cb.g, cs.g))),
                from_fix(fix15_sumprods(o1, cb.r, o, comp_op(cb.r, cs.r))),
                bp.a,
            });
        }
    }
}

static void blend_pixels_composite_separable_alpha(
    DP_Pixel15 *DP_RESTRICT dst, const DP_Pixel15 *DP_RESTRICT src,
    int pixel_count, Fix15 opacity, Fix15 (*comp_op)(Fix15, Fix15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 sp = *src;
        if (sp.a != 0) {
            BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(sp));
            composite_separable_alpha(dst, cs, sp.a, opacity, comp_op);
        }
    }
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: MIT
// SDPX—SnippetName: SAI blending logic based on psd-export by cromachina
static void blend_pixels_composite_separable_sai(
    DP_Pixel15 *DP_RESTRICT dst, const DP_Pixel15 *DP_RESTRICT src,
    int pixel_count, Fix15 opacity, Fix15 (*comp_op)(Fix15, Fix15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 bp = *dst;
        DP_Pixel15 sp = *src;
        if (bp.a != 0 && sp.a != 0) {
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
            BGR15 cs = to_bgr_opacity(sp, opacity);
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(comp_op(cb.b, cs.b)),
                from_fix(comp_op(cb.g, cs.g)),
                from_fix(comp_op(cb.r, cs.r)),
                bp.a,
            });
        }
    }
}

static void blend_pixels_composite_separable_sai_opacity(
    DP_Pixel15 *DP_RESTRICT dst, const DP_Pixel15 *DP_RESTRICT src,
    int pixel_count, Fix15 opacity, Fix15 (*comp_op)(Fix15, Fix15, Fix15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 bp = *dst;
        DP_Pixel15 sp = *src;
        if (bp.a != 0 && sp.a != 0) {
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
            BGR15 cs = to_bgr_opacity(sp, opacity);
            Fix15 as = to_fix(sp.a);
            Fix15 aso = fix15_mul(as, opacity);
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(comp_op(cb.b, cs.b, aso)),
                from_fix(comp_op(cb.g, cs.g, aso)),
                from_fix(comp_op(cb.r, cs.r, aso)),
                bp.a,
            });
        }
    }
}

static void blend_pixels_composite_separable_sai_alpha(
    DP_Pixel15 *DP_RESTRICT dst, const DP_Pixel15 *DP_RESTRICT src,
    int pixel_count, Fix15 opacity, Fix15 (*comp_op)(Fix15, Fix15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 sp = *src;
        if (sp.a != 0) {
            DP_Pixel15 bp = *dst;
            if (bp.a == 0) {
                if (opacity == DP_BIT15) {
                    *dst = sp;
                }
                else {
                    Fix15 as = to_fix(sp.a);
                    BGR15 cs = to_bgr(sp);
                    *dst = (DP_Pixel15){
                        from_fix(fix15_mul(cs.b, opacity)),
                        from_fix(fix15_mul(cs.g, opacity)),
                        from_fix(fix15_mul(cs.r, opacity)),
                        from_fix(fix15_mul(as, opacity)),
                    };
                }
            }
            else {
                BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
                BGR15 cs = to_bgr_opacity(sp, opacity);
                Fix15 ab = to_fix(bp.a);
                Fix15 as = to_fix(sp.a);
                *dst = blend_composite_separable_sai_alpha(cb, cs, ab, as,
                                                           opacity, comp_op);
            }
        }
    }
}

static void blend_pixels_composite_separable_sai_opacity_alpha(
    DP_Pixel15 *DP_RESTRICT dst, const DP_Pixel15 *DP_RESTRICT src,
    int pixel_count, Fix15 opacity, Fix15 (*comp_op)(Fix15, Fix15, Fix15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 sp = *src;
        if (sp.a != 0) {
            DP_Pixel15 bp = *dst;
            if (bp.a == 0) {
                if (opacity == DP_BIT15) {
                    *dst = sp;
                }
                else {
                    Fix15 as = to_fix(sp.a);
                    BGR15 cs = to_bgr(sp);
                    *dst = (DP_Pixel15){
                        from_fix(fix15_mul(cs.b, opacity)),
                        from_fix(fix15_mul(cs.g, opacity)),
                        from_fix(fix15_mul(cs.r, opacity)),
                        from_fix(fix15_mul(as, opacity)),
                    };
                }
            }
            else {
                BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
                BGR15 cs = to_bgr_opacity(sp, opacity);
                Fix15 ab = to_fix(bp.a);
                Fix15 as = to_fix(sp.a);
                *dst = blend_composite_separable_sai_opacity_alpha(
                    cb, cs, ab, as, opacity, comp_op);
            }
        }
    }
}
// SPDX-SnippetEnd

static void blend_pixels_composite_nonseparable(
    DP_Pixel15 *DP_RESTRICT dst, const DP_Pixel15 *DP_RESTRICT src,
    int pixel_count, Fix15 opacity, BGR15 (*comp_op)(BGR15, BGR15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 bp = *dst;
        if (bp.a != 0) {
            DP_Pixel15 sp = *src;
            BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
            BGR15 cr = comp_op(cb, to_ubgr(DP_pixel15_unpremultiply(sp)));
            Fix15 o = fix15_mul(to_fix(sp.a), opacity);
            Fix15 o1 = BIT15_FIX - o;
            *dst = DP_pixel15_premultiply((DP_UPixel15){
                from_fix(fix15_sumprods(o1, cb.b, o, cr.b)),
                from_fix(fix15_sumprods(o1, cb.g, o, cr.g)),
                from_fix(fix15_sumprods(o1, cb.r, o, cr.r)),
                bp.a,
            });
        }
    }
}

static void blend_pixels_composite_nonseparable_alpha(
    DP_Pixel15 *DP_RESTRICT dst, const DP_Pixel15 *DP_RESTRICT src,
    int pixel_count, Fix15 opacity, BGR15 (*comp_op)(BGR15, BGR15))
{
    for (int i = 0; i < pixel_count; ++i, ++dst, ++src) {
        DP_Pixel15 sp = *src;
        if (sp.a != 0) {
            BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(sp));
            composite_nonseparable_alpha(dst, cs, sp.a, opacity, comp_op);
        }
    }
}

void DP_blend_pixels(DP_Pixel15 *DP_RESTRICT dst,
                     const DP_Pixel15 *DP_RESTRICT src, int pixel_count,
                     uint16_t opacity, int blend_mode)
{
    switch (blend_mode) {
    // Alpha-affecting blend modes.
    case DP_BLEND_MODE_NORMAL:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_normal);
        break;
    case DP_BLEND_MODE_BEHIND:
    case DP_BLEND_MODE_BEHIND_PRESERVE:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_behind);
        break;
    case DP_BLEND_MODE_ERASE:
    case DP_BLEND_MODE_ERASE_PRESERVE:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_erase);
        break;
    case DP_BLEND_MODE_LIGHT_TO_ALPHA:
    case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
        blend_pixels_to_alpha_op(dst, pixel_count, to_fix(opacity),
                                 blend_erase_light);
        break;
    case DP_BLEND_MODE_DARK_TO_ALPHA:
    case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
        blend_pixels_to_alpha_op(dst, pixel_count, to_fix(opacity),
                                 blend_erase_dark);
        break;
    case DP_BLEND_MODE_REPLACE:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_replace);
        break;
    case DP_BLEND_MODE_COLOR_ERASE:
    case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
        blend_pixels_color_erase(dst, src, pixel_count, opacity);
        break;
    case DP_BLEND_MODE_PIGMENT_ALPHA:
        blend_pixels_pigment_alpha(dst, src, pixel_count, to_fix(opacity));
        break;
    // Alpha-preserving separable blend modes (each channel handled separately)
    case DP_BLEND_MODE_MULTIPLY:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_multiply);
        break;
    case DP_BLEND_MODE_DIVIDE:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_divide);
        break;
    case DP_BLEND_MODE_BURN:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_burn);
        break;
    case DP_BLEND_MODE_DODGE:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_dodge);
        break;
    case DP_BLEND_MODE_VIVID_LIGHT:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_vivid_light);
        break;
    case DP_BLEND_MODE_PIN_LIGHT:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_pin_light);
        break;
    case DP_BLEND_MODE_DARKEN:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_darken);
        break;
    case DP_BLEND_MODE_LIGHTEN:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_lighten);
        break;
    case DP_BLEND_MODE_SUBTRACT:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_subtract);
        break;
    case DP_BLEND_MODE_DIFFERENCE:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_difference);
        break;
    case DP_BLEND_MODE_ADD:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_add);
        break;
    case DP_BLEND_MODE_RECOLOR:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_recolor);
        break;
    case DP_BLEND_MODE_SCREEN:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_screen);
        break;
    case DP_BLEND_MODE_OVERLAY:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_overlay);
        break;
    case DP_BLEND_MODE_HARD_LIGHT:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_hard_light);
        break;
    case DP_BLEND_MODE_SOFT_LIGHT:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_soft_light);
        break;
    case DP_BLEND_MODE_LINEAR_BURN:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_linear_burn);
        break;
    case DP_BLEND_MODE_LINEAR_LIGHT:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_linear_light);
        break;
    case DP_BLEND_MODE_MARKER:
    case DP_BLEND_MODE_MARKER_WASH:
        blend_pixels_marker(dst, src, pixel_count, to_fix(opacity));
        break;
    case DP_BLEND_MODE_GREATER:
    case DP_BLEND_MODE_GREATER_WASH:
        blend_pixels_greater(dst, src, pixel_count, to_fix(opacity));
        break;
    // Alpha-preserving separable blend modes where the opacity affects blending
    case DP_BLEND_MODE_SHADE_SAI:
        blend_pixels_composite_separable_sai_opacity(
            dst, src, pixel_count, to_fix(opacity), comp_linear_burn_sai);
        break;
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
        blend_pixels_composite_separable_sai(dst, src, pixel_count,
                                             to_fix(opacity), comp_add);
        break;
    case DP_BLEND_MODE_SHADE_SHINE_SAI:
        blend_pixels_composite_separable_sai_opacity(
            dst, src, pixel_count, to_fix(opacity), comp_linear_light_sai);
        break;
    case DP_BLEND_MODE_BURN_SAI:
        blend_pixels_composite_separable_sai_opacity(
            dst, src, pixel_count, to_fix(opacity), comp_burn_sai);
        break;
    case DP_BLEND_MODE_DODGE_SAI:
        blend_pixels_composite_separable_sai(dst, src, pixel_count,
                                             to_fix(opacity), comp_dodge);
        break;
    case DP_BLEND_MODE_BURN_DODGE_SAI:
        blend_pixels_composite_separable_sai_opacity(
            dst, src, pixel_count, to_fix(opacity), comp_vivid_light_sai);
        break;
    case DP_BLEND_MODE_HARD_MIX_SAI:
        blend_pixels_composite_separable_sai_opacity(
            dst, src, pixel_count, to_fix(opacity), comp_hard_mix_sai);
        break;
    case DP_BLEND_MODE_DIFFERENCE_SAI:
        blend_pixels_composite_separable_sai(dst, src, pixel_count,
                                             to_fix(opacity), comp_difference);
        break;
    // Alpha-preserving non-separable blend modes (channels interact)
    case DP_BLEND_MODE_HUE:
        blend_pixels_composite_nonseparable(dst, src, pixel_count,
                                            to_fix(opacity), comp_hue);
        break;
    case DP_BLEND_MODE_SATURATION:
        blend_pixels_composite_nonseparable(dst, src, pixel_count,
                                            to_fix(opacity), comp_saturation);
        break;
    case DP_BLEND_MODE_LUMINOSITY:
        blend_pixels_composite_nonseparable(dst, src, pixel_count,
                                            to_fix(opacity), comp_luminosity);
        break;
    case DP_BLEND_MODE_COLOR:
        blend_pixels_composite_nonseparable(dst, src, pixel_count,
                                            to_fix(opacity), comp_color);
        break;
    case DP_BLEND_MODE_DARKER_COLOR:
        blend_pixels_composite_nonseparable(dst, src, pixel_count,
                                            to_fix(opacity), comp_darker_color);
        break;
    case DP_BLEND_MODE_LIGHTER_COLOR:
        blend_pixels_composite_nonseparable(
            dst, src, pixel_count, to_fix(opacity), comp_lighter_color);
        break;
    case DP_BLEND_MODE_PIGMENT:
        blend_pixels_pigment(dst, src, pixel_count, to_fix(opacity));
        break;
    // Alpha-affecting separable blend modes (each channel handled separately)
    case DP_BLEND_MODE_MULTIPLY_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_multiply);
        break;
    case DP_BLEND_MODE_DIVIDE_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_divide);
        break;
    case DP_BLEND_MODE_BURN_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_burn);
        break;
    case DP_BLEND_MODE_DODGE_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_dodge);
        break;
    case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
        blend_pixels_composite_separable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_vivid_light);
        break;
    case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_pin_light);
        break;
    case DP_BLEND_MODE_DARKEN_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_darken);
        break;
    case DP_BLEND_MODE_LIGHTEN_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_lighten);
        break;
    case DP_BLEND_MODE_SUBTRACT_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_subtract);
        break;
    case DP_BLEND_MODE_ADD_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_add);
        break;
    case DP_BLEND_MODE_DIFFERENCE_ALPHA:
        blend_pixels_composite_separable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_difference);
        break;
    case DP_BLEND_MODE_SCREEN_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_screen);
        break;
    case DP_BLEND_MODE_OVERLAY_ALPHA:
        blend_pixels_composite_separable_alpha(dst, src, pixel_count,
                                               to_fix(opacity), comp_overlay);
        break;
    case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
        blend_pixels_composite_separable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_hard_light);
        break;
    case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
        blend_pixels_composite_separable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_soft_light);
        break;
    case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
        blend_pixels_composite_separable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_linear_burn);
        break;
    case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
        blend_pixels_composite_separable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_linear_light);
        break;
    case DP_BLEND_MODE_MARKER_ALPHA:
    case DP_BLEND_MODE_MARKER_ALPHA_WASH:
        blend_pixels_marker_alpha(dst, src, pixel_count, to_fix(opacity));
        break;
    case DP_BLEND_MODE_GREATER_ALPHA:
    case DP_BLEND_MODE_GREATER_ALPHA_WASH:
        blend_pixels_greater_alpha(dst, src, pixel_count, to_fix(opacity));
        break;
    // Alpha-preserving separable blend modes where the opacity affects blending
    case DP_BLEND_MODE_SHADE_SAI_ALPHA:
        blend_pixels_composite_separable_sai_opacity_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_linear_burn_sai);
        break;
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA:
        blend_pixels_composite_separable_sai_alpha(dst, src, pixel_count,
                                                   to_fix(opacity), comp_add);
        break;
    case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
        blend_pixels_composite_separable_sai_opacity_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_linear_light_sai);
        break;
    case DP_BLEND_MODE_BURN_SAI_ALPHA:
        blend_pixels_composite_separable_sai_opacity_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_burn_sai);
        break;
    case DP_BLEND_MODE_DODGE_SAI_ALPHA:
        blend_pixels_composite_separable_sai_alpha(dst, src, pixel_count,
                                                   to_fix(opacity), comp_dodge);
        break;
    case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
        blend_pixels_composite_separable_sai_opacity_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_vivid_light_sai);
        break;
    case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
        blend_pixels_composite_separable_sai_opacity_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_hard_mix_sai);
        break;
    case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
        blend_pixels_composite_separable_sai_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_difference);
        break;
    // Alpha-affecting non-separable blend modes (channels interact)
    case DP_BLEND_MODE_HUE_ALPHA:
        blend_pixels_composite_nonseparable_alpha(dst, src, pixel_count,
                                                  to_fix(opacity), comp_hue);
        break;
    case DP_BLEND_MODE_SATURATION_ALPHA:
        blend_pixels_composite_nonseparable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_saturation);
        break;
    case DP_BLEND_MODE_LUMINOSITY_ALPHA:
        blend_pixels_composite_nonseparable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_luminosity);
        break;
    case DP_BLEND_MODE_COLOR_ALPHA:
        blend_pixels_composite_nonseparable_alpha(dst, src, pixel_count,
                                                  to_fix(opacity), comp_color);
        break;
    case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
        blend_pixels_composite_nonseparable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_darker_color);
        break;
    case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
        blend_pixels_composite_nonseparable_alpha(
            dst, src, pixel_count, to_fix(opacity), comp_lighter_color);
        break;
    default:
        DP_debug("Unknown pixel blend mode %d (%s)", blend_mode,
                 DP_blend_mode_enum_name(blend_mode));
        break;
    }
}

void DP_tint_pixels(DP_Pixel15 *dst, int pixel_count, DP_UPixel8 tint)
{
    if (tint.a == 255) {
        DP_UPixel15 t = DP_upixel8_to_15(tint);
        for (int i = 0; i < pixel_count; ++i) {
            BGRA15 bgra = to_ubgra(DP_pixel15_unpremultiply(dst[i]));
            dst[i] = from_ubgra((BGRA15){
                .b = t.b,
                .g = t.g,
                .r = t.r,
                .a = fix15_mul(bgra.a, BIT15_FIX - lum(bgra.bgr)),
            });
        }
    }
    else if (tint.a > 0) {
        DP_UPixel15 t = DP_upixel8_to_15(tint);
        Fix15 ta1 = BIT15_FIX - t.a;
        for (int i = 0; i < pixel_count; ++i) {
            BGRA15 bgra = to_ubgra(DP_pixel15_unpremultiply(dst[i]));
            Fix15 l = lum(bgra.bgr);
            Fix15 l1 = BIT15_FIX - l;
            dst[i] = from_ubgra((BGRA15){
                .b = fix15_sumprods(bgra.b, l, t.b, l1),
                .g = fix15_sumprods(bgra.g, l, t.g, l1),
                .r = fix15_sumprods(bgra.r, l, t.r, l1),
                .a = fix15_sumprods(bgra.a, ta1, fix15_mul(bgra.a, l1), t.a),
            });
        }
    }
}

void DP_blend_selection(DP_Pixel15 *DP_RESTRICT dst,
                        const DP_Pixel15 *DP_RESTRICT src, int pixel_count,
                        DP_UPixel15 color)
{
    Fix15 opacity = to_fix(color.a);
    BGR15 cs = to_ubgr(color);
    if (opacity != 0) {
        for (int i = 0; i < pixel_count; ++i) {
            DP_Pixel15 sp = src[i];
            Fix15 as = to_fix(sp.a);
            if (as != 0) {
                DP_Pixel15 dp = dst[i];
                Fix15 ab = to_fix(dp.a);
                Fix15 aso = fix15_mul(as, opacity);
                if (ab == 0) {
                    dst[i] = DP_pixel15_premultiply((DP_UPixel15){
                        .b = color.b,
                        .g = color.g,
                        .r = color.r,
                        .a = from_fix(aso),
                    });
                }
                else {
                    BGR15 cb = to_bgr(dp);
                    Fix15 aso1 = BIT15_FIX - aso;
                    dst[i] = (DP_Pixel15){
                        .b = from_fix(fix15_sumprods(cs.b, aso, cb.b, aso1)),
                        .g = from_fix(fix15_sumprods(cs.g, aso, cb.g, aso1)),
                        .r = from_fix(fix15_sumprods(cs.r, aso, cb.r, aso1)),
                        .a = from_fix(aso + fix15_mul(ab, aso1)),
                    };
                }
            }
        }
    }
}

void DP_blend_tile(DP_Pixel15 *DP_RESTRICT dst,
                   const DP_Pixel15 *DP_RESTRICT src, uint16_t opacity,
                   int blend_mode)
{
    DP_Pixel15 *aligned_dst = DP_ASSUME_SIMD_ALIGNED(dst);
    const DP_Pixel15 *aligned_src = DP_ASSUME_SIMD_ALIGNED(src);
#ifdef DP_CPU_X64
    switch (blend_mode) {
    // Alpha-affecting blend modes.
    case DP_BLEND_MODE_NORMAL:
        switch (DP_cpu_support) {
        case DP_CPU_SUPPORT_AVX2:
            blend_tile_normal_avx2(aligned_dst, aligned_src, opacity);
            return;
        case DP_CPU_SUPPORT_SSE42:
            blend_tile_normal_sse42(aligned_dst, aligned_src, opacity);
            return;
        default:
            break;
        }
        break;
    case DP_BLEND_MODE_BEHIND:
        switch (DP_cpu_support) {
        case DP_CPU_SUPPORT_AVX2:
            blend_tile_behind_avx2(aligned_dst, aligned_src, opacity);
            return;
        case DP_CPU_SUPPORT_SSE42:
            blend_tile_behind_sse42(aligned_dst, aligned_src, opacity);
            return;
        default:
            break;
        }
        break;
    default:
        break;
    }
#endif
    DP_blend_pixels(aligned_dst, aligned_src, DP_TILE_LENGTH, opacity,
                    blend_mode);
}

void DP_mask_tile(DP_Pixel15 *DP_RESTRICT dst,
                  const DP_Pixel15 *DP_RESTRICT src,
                  const DP_Pixel15 *DP_RESTRICT mask)
{
    DP_Pixel15 *aligned_dst = DP_ASSUME_SIMD_ALIGNED(dst);
    const DP_Pixel15 *aligned_src = DP_ASSUME_SIMD_ALIGNED(src);
    const DP_Pixel15 *aligned_mask = DP_ASSUME_SIMD_ALIGNED(mask);
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        DP_Pixel15 sp = aligned_src[i];
        uint16_t mask_a = aligned_mask[i].a;
        if (sp.a == 0 || mask_a == 0) {
            aligned_dst[i] = (DP_Pixel15){0, 0, 0, 0};
        }
        else if (sp.a == DP_BIT15 && mask_a == DP_BIT15) {
            aligned_dst[i] = sp;
        }
        else {
            BGRA15 bgra = to_bgra(sp);
            Fix15 m = to_fix(mask_a);
            aligned_dst[i] = (DP_Pixel15){
                .b = from_fix(fix15_mul(bgra.b, m)),
                .g = from_fix(fix15_mul(bgra.g, m)),
                .r = from_fix(fix15_mul(bgra.r, m)),
                .a = from_fix(fix15_mul(bgra.a, m)),
            };
        }
    }
}

void DP_mask_tile_in_place(DP_Pixel15 *DP_RESTRICT dst,
                           const DP_Pixel15 *DP_RESTRICT mask)
{
    DP_Pixel15 *aligned_dst = DP_ASSUME_SIMD_ALIGNED(dst);
    const DP_Pixel15 *aligned_mask = DP_ASSUME_SIMD_ALIGNED(mask);
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        DP_Pixel15 sp = aligned_dst[i];
        uint16_t mask_a = aligned_mask[i].a;
        if (sp.a == 0 || (sp.a == DP_BIT15 && mask_a == DP_BIT15)) {
            // Nothing to do.
        }
        else if (mask_a == 0) {
            aligned_dst[i] = (DP_Pixel15){0, 0, 0, 0};
        }
        else {
            BGRA15 bgra = to_bgra(sp);
            Fix15 m = to_fix(mask_a);
            aligned_dst[i] = (DP_Pixel15){
                .b = from_fix(fix15_mul(bgra.b, m)),
                .g = from_fix(fix15_mul(bgra.g, m)),
                .r = from_fix(fix15_mul(bgra.r, m)),
                .a = from_fix(fix15_mul(bgra.a, m)),
            };
        }
    }
}


// Posterization adapted from libmypaint, see license above.

static BGRA15 posterize(float p, Fix15 o, DP_UPixel15 ub)
{
    DP_UPixelFloat fb = DP_upixel15_to_float(ub);
    BGR15 b = (BGR15){
        .b = (Fix15)(BIT15_FLOAT * floorf(fb.b * p + 0.5f) / p),
        .g = (Fix15)(BIT15_FLOAT * floorf(fb.g * p + 0.5f) / p),
        .r = (Fix15)(BIT15_FLOAT * floorf(fb.r * p + 0.5f) / p),
    };
    Fix15 o1 = BIT15_FIX - o;
    return (BGRA15){
        .b = (o * b.b + o1 * to_fix(ub.b)) / BIT15_FIX,
        .g = (o * b.g + o1 * to_fix(ub.g)) / BIT15_FIX,
        .r = (o * b.r + o1 * to_fix(ub.r)) / BIT15_FIX,
        .a = ub.a,
    };
}

void DP_posterize_mask(DP_Pixel15 *dst, int posterize_num, const uint16_t *mask,
                       uint16_t opacity, int w, int h, int mask_skip,
                       int base_skip)
{
    Fix15 ofix = to_fix(opacity);
    float p = DP_int_to_float(posterize_num);
    FOR_MASK_PIXEL(dst, mask, ofix, w, h, mask_skip, base_skip, x, y, o, {
        *dst = from_ubgra(posterize(p, o, DP_pixel15_unpremultiply(*dst)));
    });
}


// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: 8 bit multiplication adapted from Krita
uint8_t DP_pixel8_mul(unsigned int a, unsigned int b)
{
    unsigned int c = a * b + 0x80u;
    return DP_uint_to_uint8(((c >> 8u) + c) >> 8u);
}
// SPDX-SnippetEnd

void DP_blend_color8_to(DP_Pixel8 *DP_RESTRICT out,
                        const DP_Pixel8 *DP_RESTRICT dst, DP_UPixel8 color,
                        int pixel_count, uint8_t opacity)
{
    DP_Pixel8 src = DP_pixel8_premultiply(color);
    unsigned int sa1 = 255u - DP_pixel8_mul(src.a, opacity);
    if (sa1 != 255u) {
        unsigned int sb = DP_pixel8_mul(src.b, opacity);
        unsigned int sg = DP_pixel8_mul(src.g, opacity);
        unsigned int sr = DP_pixel8_mul(src.r, opacity);
        unsigned int sa = DP_pixel8_mul(src.a, opacity);
        for (int i = 0; i < pixel_count; ++i) {
            DP_Pixel8 d = dst[i];
            out[i] = (DP_Pixel8){
                .b = (uint8_t)(sb + DP_pixel8_mul(d.b, sa1)),
                .g = (uint8_t)(sg + DP_pixel8_mul(d.g, sa1)),
                .r = (uint8_t)(sr + DP_pixel8_mul(d.r, sa1)),
                .a = (uint8_t)(sa + DP_pixel8_mul(d.a, sa1)),
            };
        }
    }
}

void DP_blend_pixels8(DP_Pixel8 *DP_RESTRICT dst,
                      const DP_Pixel8 *DP_RESTRICT src, int pixel_count,
                      uint8_t opacity)
{
    for (int i = 0; i < pixel_count; ++i) {
        DP_Pixel8 s = src[i];
        DP_Pixel8 d = dst[i];
        unsigned int sa1 = 255u - DP_pixel8_mul(s.a, opacity);
        if (sa1 != 255u) {
            dst[i] = (DP_Pixel8){
                .b = (uint8_t)(DP_pixel8_mul(s.b, opacity)
                               + DP_pixel8_mul(d.b, sa1)),
                .g = (uint8_t)(DP_pixel8_mul(s.g, opacity)
                               + DP_pixel8_mul(d.g, sa1)),
                .r = (uint8_t)(DP_pixel8_mul(s.r, opacity)
                               + DP_pixel8_mul(d.r, sa1)),
                .a = (uint8_t)(DP_pixel8_mul(s.a, opacity)
                               + DP_pixel8_mul(d.a, sa1)),
            };
        }
    }
}


DP_Spectral DP_rgb_to_spectral(float r, float g, float b)
{
    return rgb_to_spectral((BGRf){b, g, r});
}

void DP_spectral_to_rgb(const DP_Spectral *spectral, float *out_r, float *out_g,
                        float *out_b)
{
    BGRf bgr = spectral_to_rgb(spectral->channels);
    *out_r = bgr.r;
    *out_g = bgr.g;
    *out_b = bgr.b;
}
