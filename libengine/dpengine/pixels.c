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
 */
#include "pixels.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpmsg/blend_mode.h>
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
#define BIT15_INC_FIX ((Fix15)(DP_BIT15 + 1))

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


static BGR15 to_bgr(DP_Pixel15 pixel)
{
    return (BGR15){
        .b = to_fix(pixel.b),
        .g = to_fix(pixel.g),
        .r = to_fix(pixel.r),
    };
}

static BGR15 to_ubgr(DP_UPixel15 pixel)
{
    return (BGR15){
        .b = to_fix(pixel.b),
        .g = to_fix(pixel.g),
        .r = to_fix(pixel.r),
    };
}

static BGRA15 to_bgra(DP_Pixel15 pixel)
{
    return (BGRA15){
        .bgr = to_bgr(pixel),
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


uint16_t DP_fix15_mul(uint16_t a, uint16_t b)
{
    return from_fix(fix15_mul(to_fix(a), to_fix(b)));
}

uint16_t DP_channel8_to_15(uint8_t c)
{
    return DP_float_to_uint16(DP_uint8_to_float(c) / 255.0f * BIT15_FLOAT);
}

uint8_t DP_channel15_to_8(uint16_t c)
{
    return DP_float_to_uint8(DP_uint16_to_float(c) / BIT15_FLOAT * 255.0f);
}

float DP_channel8_to_float(uint8_t c)
{
    return DP_uint8_to_float(c) / 255.0f;
}

float DP_channel15_to_float(uint16_t c)
{
    return DP_uint16_to_float(c) / BIT15_FLOAT;
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


static BGRA15 blend_normal(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as, Fix15 o)
{
    Fix15 as1 = BIT15_FIX - fix15_mul(as, o);
    return (BGRA15){
        .b = fix15_mul(cb.b, as1) + fix15_mul(cs.b, o),
        .g = fix15_mul(cb.g, as1) + fix15_mul(cs.g, o),
        .r = fix15_mul(cb.r, as1) + fix15_mul(cs.r, o),
        .a = fix15_mul(ab, as1) + fix15_mul(as, o),
    };
}

static BGRA15 blend_behind(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as, Fix15 o)
{
    Fix15 a1 = fix15_mul(BIT15_FIX - ab, fix15_mul(as, o));
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

// Adapted from libmypaint, see license above.
static BGRA15 blend_normal_and_eraser(BGR15 cb, BGR15 cs, Fix15 ab, Fix15 as,
                                      Fix15 o)
{
    Fix15 opa_a = fix15_mul(o, as);
    Fix15 opa_b = BIT15_FIX - o;
    return (BGRA15){
        .b = fix15_sumprods(opa_a, cs.b, opa_b, cb.b),
        .g = fix15_sumprods(opa_a, cs.g, opa_b, cb.g),
        .r = fix15_sumprods(opa_a, cs.r, opa_b, cb.r),
        .a = opa_a + opa_b * ab / BIT15_FIX,
    };
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

static Fix15 comp_recolor(DP_UNUSED Fix15 a, Fix15 b)
{
    return b;
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

// This blend mode is from Paint Tool SAI, version 1 calls it "Luminosity",
// version 2 calls it "Shine". Krita calls it "Luminosity/Shine (SAI)", so we
// do too. It works by Normal blending the new pixel with a fully opaque black
// pixel and then compositing the result via Addition.
static Fix15 comp_luminosity_shine_sai(Fix15 a, Fix15 b, Fix15 o)
{
    return comp_add(a, fix15_mul(b, o));
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


#define FOR_MASK_PIXEL(DST, MASK, OPACITY, W, H, MASK_SKIP, DST_SKIP, X, Y, A, \
                       BLOCK)                                                  \
    do {                                                                       \
        for (int Y = 0; Y < H; ++Y) {                                          \
            for (int X = 0; X < W; ++X, ++DST, ++MASK) {                       \
                Fix15 A = fix15_mul(*MASK, OPACITY);                           \
                BLOCK                                                          \
            }                                                                  \
            DST += DST_SKIP;                                                   \
            MASK += MASK_SKIP;                                                 \
        }                                                                      \
    } while (0)

static void blend_mask_alpha_op(DP_Pixel15 *dst, DP_UPixel15 src,
                                const uint16_t *mask, Fix15 opacity, int w,
                                int h, int mask_skip, int base_skip,
                                BGRA15 (*op)(BGR15, BGR15, Fix15, Fix15, Fix15))
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        BGRA15 b = to_bgra(*dst);
        *dst = from_bgra(op(b.bgr, cs, b.a, BIT15_FIX, o));
    });
}

// Adapted from libmypaint, see license above.
static void blend_mask_normal_and_eraser(DP_Pixel15 *dst, DP_UPixel15 src,
                                         const uint16_t *mask, Fix15 opacity,
                                         int w, int h, int mask_skip,
                                         int base_skip)
{
    BGR15 cs = to_ubgr(src);
    Fix15 as = to_fix(src.a);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        BGRA15 b = to_bgra(*dst);
        *dst = from_bgra(blend_normal_and_eraser(b.bgr, cs, b.a, as, o));
    });
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

static void blend_mask_composite_separable(DP_Pixel15 *dst, DP_UPixel15 src,
                                           const uint16_t *mask, Fix15 opacity,
                                           int w, int h, int mask_skip,
                                           int base_skip,
                                           Fix15 (*comp_op)(Fix15, Fix15))
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        DP_Pixel15 bp = *dst;
        BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
        Fix15 o1 = BIT15_FIX - o;
        *dst = DP_pixel15_premultiply((DP_UPixel15){
            from_fix(fix15_sumprods(o1, cb.b, o, comp_op(cb.b, cs.b))),
            from_fix(fix15_sumprods(o1, cb.g, o, comp_op(cb.g, cs.g))),
            from_fix(fix15_sumprods(o1, cb.r, o, comp_op(cb.r, cs.r))),
            bp.a,
        });
    });
}

static void blend_mask_composite_separable_with_opacity(
    DP_Pixel15 *dst, DP_UPixel15 src, const uint16_t *mask, Fix15 opacity,
    int w, int h, int mask_skip, int base_skip,
    Fix15 (*comp_op)(Fix15, Fix15, Fix15))
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        DP_Pixel15 bp = *dst;
        BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
        Fix15 o1 = BIT15_FIX - o;
        *dst = DP_pixel15_premultiply((DP_UPixel15){
            from_fix(fix15_sumprods(o1, cb.b, o, comp_op(cb.b, cs.b, o))),
            from_fix(fix15_sumprods(o1, cb.g, o, comp_op(cb.g, cs.g, o))),
            from_fix(fix15_sumprods(o1, cb.r, o, comp_op(cb.r, cs.r, o))),
            bp.a,
        });
    });
}

static void blend_mask_composite_nonseparable(DP_Pixel15 *dst, DP_UPixel15 src,
                                              const uint16_t *mask,
                                              Fix15 opacity, int w, int h,
                                              int mask_skip, int base_skip,
                                              BGR15 (*comp_op)(BGR15, BGR15))
{
    BGR15 cs = to_ubgr(src);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        DP_Pixel15 bp = *dst;
        BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
        BGR15 cr = comp_op(cb, cs);
        Fix15 o1 = BIT15_FIX - o;
        *dst = DP_pixel15_premultiply((DP_UPixel15){
            from_fix(fix15_sumprods(o1, cb.b, o, cr.b)),
            from_fix(fix15_sumprods(o1, cb.g, o, cr.g)),
            from_fix(fix15_sumprods(o1, cb.r, o, cr.r)),
            bp.a,
        });
    });
}

void DP_blend_mask(DP_Pixel15 *dst, DP_UPixel15 src, int blend_mode,
                   const uint16_t *mask, uint16_t opacity, int w, int h,
                   int mask_skip, int base_skip)
{
    switch (blend_mode) {
    // Alpha-affecting blend modes.
    case DP_BLEND_MODE_NORMAL:
        blend_mask_alpha_op(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                            base_skip, blend_normal);
        break;
    case DP_BLEND_MODE_BEHIND:
        blend_mask_alpha_op(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                            base_skip, blend_behind);
        break;
    case DP_BLEND_MODE_ERASE:
        blend_mask_alpha_op(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                            base_skip, blend_erase);
        break;
    case DP_BLEND_MODE_NORMAL_AND_ERASER:
        blend_mask_normal_and_eraser(dst, src, mask, to_fix(opacity), w, h,
                                     mask_skip, base_skip);
        break;
    case DP_BLEND_MODE_REPLACE:
        blend_mask_alpha_op(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                            base_skip, blend_replace);
        break;
    case DP_BLEND_MODE_COLOR_ERASE:
        blend_mask_color_erase(dst, src, mask, to_fix(opacity), w, h, mask_skip,
                               base_skip);
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
    case DP_BLEND_MODE_RECOLOR:
        blend_mask_composite_separable(dst, src, mask, opacity, w, h, mask_skip,
                                       base_skip, comp_recolor);
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
    // Alpha-preserving separable blend modes where the opacity affects blending
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
        blend_mask_composite_separable_with_opacity(dst, src, mask, opacity, w,
                                                    h, mask_skip, base_skip,
                                                    comp_luminosity_shine_sai);
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
    default:
        DP_debug("Unknown mask blend mode %d (%s)", blend_mode,
                 DP_blend_mode_enum_name(blend_mode));
        break;
    }
}


#define FOR_PIXEL(DST, SRC, PIXEL_COUNT, I, BLOCK)            \
    do {                                                      \
        for (int I = 0; I < PIXEL_COUNT; ++I, ++DST, ++SRC) { \
            BLOCK                                             \
        }                                                     \
    } while (0)

static void blend_pixels_alpha_op(DP_Pixel15 *dst, DP_Pixel15 *src,
                                  int pixel_count, Fix15 opacity,
                                  BGRA15 (*op)(BGR15, BGR15, Fix15, Fix15,
                                               Fix15))
{
    FOR_PIXEL(dst, src, pixel_count, i, {
        BGRA15 b = to_bgra(*dst);
        BGRA15 s = to_bgra(*src);
        *dst = from_bgra(op(b.bgr, s.bgr, b.a, s.a, opacity));
    });
}

static void blend_pixels_color_erase(DP_Pixel15 *dst, DP_Pixel15 *src,
                                     int pixel_count, uint16_t opacity)
{
    double o = DP_uint16_to_double(opacity) / BIT15_DOUBLE;
    FOR_PIXEL(dst, src, pixel_count, i, {
        DP_UPixel15 udst = DP_pixel15_unpremultiply(*dst);
        DP_UPixel15 usrc = DP_pixel15_unpremultiply(*src);
        *dst =
            blend_color_erase(usrc.b / BIT15_DOUBLE, usrc.g / BIT15_DOUBLE,
                              usrc.r / BIT15_DOUBLE, usrc.a / BIT15_DOUBLE * o,
                              udst.b / BIT15_DOUBLE, udst.g / BIT15_DOUBLE,
                              udst.r / BIT15_DOUBLE, udst.a / BIT15_DOUBLE);
    });
}

static void blend_pixels_composite_separable(DP_Pixel15 *dst, DP_Pixel15 *src,
                                             int pixel_count, Fix15 opacity,
                                             Fix15 (*comp_op)(Fix15, Fix15))
{
    FOR_PIXEL(dst, src, pixel_count, i, {
        DP_Pixel15 bp = *dst;
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
    });
}

static void blend_pixels_composite_separable_with_opacity(
    DP_Pixel15 *dst, DP_Pixel15 *src, int pixel_count, Fix15 opacity,
    Fix15 (*comp_op)(Fix15, Fix15, Fix15))
{
    FOR_PIXEL(dst, src, pixel_count, i, {
        DP_Pixel15 bp = *dst;
        DP_Pixel15 sp = *src;
        BGR15 cb = to_ubgr(DP_pixel15_unpremultiply(bp));
        BGR15 cs = to_ubgr(DP_pixel15_unpremultiply(sp));
        Fix15 o = fix15_mul(to_fix(sp.a), opacity);
        Fix15 o1 = BIT15_FIX - o;
        *dst = DP_pixel15_premultiply((DP_UPixel15){
            from_fix(fix15_sumprods(o1, cb.b, o, comp_op(cb.b, cs.b, o))),
            from_fix(fix15_sumprods(o1, cb.g, o, comp_op(cb.g, cs.g, o))),
            from_fix(fix15_sumprods(o1, cb.r, o, comp_op(cb.r, cs.r, o))),
            bp.a,
        });
    });
}

static void blend_pixels_composite_nonseparable(DP_Pixel15 *dst,
                                                DP_Pixel15 *src,
                                                int pixel_count, Fix15 opacity,
                                                BGR15 (*comp_op)(BGR15, BGR15))
{
    FOR_PIXEL(dst, src, pixel_count, i, {
        DP_Pixel15 bp = *dst;
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
    });
}

void DP_blend_pixels(DP_Pixel15 *dst, DP_Pixel15 *src, int pixel_count,
                     uint16_t opacity, int blend_mode)
{
    switch (blend_mode) {
    // Alpha-affecting blend modes.
    case DP_BLEND_MODE_NORMAL:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_normal);
        break;
    case DP_BLEND_MODE_BEHIND:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_behind);
        break;
    case DP_BLEND_MODE_ERASE:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_erase);
        break;
    case DP_BLEND_MODE_REPLACE:
        blend_pixels_alpha_op(dst, src, pixel_count, to_fix(opacity),
                              blend_replace);
        break;
    case DP_BLEND_MODE_COLOR_ERASE:
        blend_pixels_color_erase(dst, src, pixel_count, opacity);
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
    case DP_BLEND_MODE_ADD:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_add);
        break;
    case DP_BLEND_MODE_RECOLOR:
        blend_pixels_composite_separable(dst, src, pixel_count, to_fix(opacity),
                                         comp_recolor);
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
    // Alpha-preserving separable blend modes where the opacity affects blending
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
        blend_pixels_composite_separable_with_opacity(
            dst, src, pixel_count, to_fix(opacity), comp_luminosity_shine_sai);
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
    default:
        DP_debug("Unknown mask blend mode %d (%s)", blend_mode,
                 DP_blend_mode_enum_name(blend_mode));
        break;
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
        *dst = from_bgra(posterize(p, o, DP_pixel15_unpremultiply(*dst)));
    });
}
