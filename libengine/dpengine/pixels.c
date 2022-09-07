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
#include "blend_mode.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>

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
#define BIT15_INC_FIX ((Fix15)(DP_BIT15 + 1))

static Fix15 to_fix(uint16_t x)
{
    return (Fix15)x;
}

static uint16_t from_fix(Fix15 x)
{
    DP_ASSERT(x <= BIT15_FIX);
    return (uint16_t)x;
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
    return (a * b) >> (Fix15)15;
}

// Adapted from MyPaint, see license above.
static Fix15 fix15_sumprods(Fix15 a1, Fix15 a2, Fix15 b1, Fix15 b2)
{
    return ((a1 * a2) + (b1 * b2)) >> (Fix15)15;
}


typedef struct BGR15 {
    Fix15 b, g, r;
} BGR15;

typedef struct BGRA15 {
    union {
        BGR15 bgr;
        struct {
            Fix15 b, g, r;
        };
    };
    Fix15 a;
} BGRA15;

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


uint16_t DP_channel8_to_15(uint8_t c)
{
    return DP_float_to_uint16(DP_uint8_to_float(c) / 255.0f * BIT15_FLOAT);
}

uint8_t DP_channel15_to_8(uint16_t c)
{
    return DP_float_to_uint8(DP_uint16_to_float(c) / BIT15_FLOAT * 255.0f);
}

float DP_channel15_to_float(uint16_t c)
{
    return DP_uint16_to_float(c) / BIT15_FLOAT;
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

DP_UPixelFloat DP_upixel15_to_float(DP_UPixel15 pixel)
{
    return (DP_UPixelFloat){
        .b = DP_channel15_to_float(pixel.b),
        .g = DP_channel15_to_float(pixel.g),
        .r = DP_channel15_to_float(pixel.r),
        .a = DP_channel15_to_float(pixel.a),
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

static void blend_mask_alpha_op(DP_Pixel15 *dst, DP_Pixel15 src,
                                const uint16_t *mask, Fix15 opacity, int w,
                                int h, int mask_skip, int base_skip,
                                BGRA15 (*op)(BGR15, BGR15, Fix15, Fix15, Fix15))
{
    BGR15 cs = to_bgr(src);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, o, {
        BGRA15 b = to_bgra(*dst);
        *dst = from_bgra(op(b.bgr, cs, b.a, BIT15_FIX, o));
    });
}

static void blend_mask_color_erase(DP_Pixel15 *dst, DP_Pixel15 src,
                                   const uint16_t *mask, Fix15 opacity, int w,
                                   int h, int mask_skip, int base_skip)
{
    double fsrc_b = src.b / BIT15_DOUBLE;
    double fsrc_g = src.g / BIT15_DOUBLE;
    double fsrc_r = src.r / BIT15_DOUBLE;
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        if (as != 0u) {
            double fas = (double)as;
            DP_UPixel15 ub = DP_pixel15_unpremultiply(*dst);
            *dst = blend_color_erase(fsrc_b, fsrc_g, fsrc_r, fas / BIT15_DOUBLE,
                                     DP_uint16_to_double(ub.b) / BIT15_DOUBLE,
                                     DP_uint16_to_double(ub.g) / BIT15_DOUBLE,
                                     DP_uint16_to_double(ub.r) / BIT15_DOUBLE,
                                     DP_uint16_to_double(ub.a) / BIT15_DOUBLE);
        }
    });
}

static void blend_mask_composite_separable(DP_Pixel15 *dst, DP_Pixel15 src,
                                           const uint16_t *mask, Fix15 opacity,
                                           int w, int h, int mask_skip,
                                           int base_skip,
                                           Fix15 (*comp_op)(Fix15, Fix15))
{
    BGR15 s = to_bgr(src);
    FOR_MASK_PIXEL(dst, mask, opacity, w, h, mask_skip, base_skip, x, y, as, {
        DP_Pixel15 bp = *dst;
        BGR15 b = to_ubgr(DP_pixel15_unpremultiply(bp));
        Fix15 as1 = BIT15_FIX - as;
        *dst = DP_pixel15_premultiply((DP_UPixel15){
            from_fix(fix15_sumprods(as1, b.b, as, comp_op(b.b, s.b))),
            from_fix(fix15_sumprods(as1, b.g, as, comp_op(b.g, s.g))),
            from_fix(fix15_sumprods(as1, b.r, as, comp_op(b.r, s.r))),
            bp.a,
        });
    });
}

void DP_blend_mask(DP_Pixel15 *dst, DP_Pixel15 src, int blend_mode,
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
        BGR15 b = to_ubgr(DP_pixel15_unpremultiply(bp));
        BGR15 s = to_ubgr(DP_pixel15_unpremultiply(sp));
        Fix15 as = fix15_mul(to_fix(sp.a), opacity);
        Fix15 as1 = BIT15_FIX - as;
        *dst = DP_pixel15_premultiply((DP_UPixel15){
            from_fix(fix15_sumprods(as1, b.b, as, comp_op(b.b, s.b))),
            from_fix(fix15_sumprods(as1, b.g, as, comp_op(b.g, s.g))),
            from_fix(fix15_sumprods(as1, b.r, as, comp_op(b.r, s.r))),
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
    default:
        DP_debug("Unknown mask blend mode %d (%s)", blend_mode,
                 DP_blend_mode_enum_name(blend_mode));
        break;
    }
}


void DP_sample_mask(DP_Pixel15 *src, const uint16_t *mask, int w, int h,
                    int mask_skip, int base_skip, float *out_weight,
                    float *out_red, float *out_green, float *out_blue,
                    float *out_alpha)
{
    DP_ASSERT(out_weight);
    DP_ASSERT(out_red);
    DP_ASSERT(out_green);
    DP_ASSERT(out_blue);
    DP_ASSERT(out_alpha);
    float weight = 0.0f;
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    float alpha = 0.0f;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x, ++mask) {
            float m = DP_uint16_to_float(*mask) / BIT15_FLOAT;
            DP_UPixelFloat p =
                DP_upixel15_to_float(DP_pixel15_unpremultiply(*src));
            weight += m;
            red += p.r * m;
            green += p.g * m;
            blue += p.b * m;
            alpha += p.a * m;
            ++src;
        }
        src += base_skip;
        mask += mask_skip;
    }

    *out_weight = weight;
    *out_red = red;
    *out_green = green;
    *out_blue = blue;
    *out_alpha = alpha;
}
