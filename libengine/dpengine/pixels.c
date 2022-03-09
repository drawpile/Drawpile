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
 * Parts of this code are based on Krita, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/krita/COPYING.txt for details.
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

static_assert(sizeof(DP_Pixel) == sizeof(uint32_t), "DP_Pixel is 32 bits");
static_assert(sizeof(uint32_t) == 4, "uint32_t is 4 bytes long");


typedef void (*DP_CompositeBrushFn)(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                    int w, int h, int mask_skip, int dst_skip);

typedef void (*DP_CompositeLayerFn)(DP_Pixel *DP_RESTRICT dst,
                                    DP_Pixel *DP_RESTRICT src, int pixel_count,
                                    uint8_t opacity);


// Adapted from Krita, see license above.
static uint8_t blend(int a, int b, int alpha)
{
    unsigned int c = DP_int_to_uint(((a - b) * alpha) + (b << 8) - b) + 0x80u;
    return DP_uint_to_uint8(((c >> 8) + c) >> 8);
}

// Multiplying two bytes as if they were floats between 0 and 1.
static uint8_t mul(unsigned int a, unsigned int b)
{
    unsigned int c = a * b + 0x80u;
    return DP_uint_to_uint8(((c >> 8u) + c) >> 8u);
}


static uint8_t blend_multiply(uint8_t base, uint8_t blend)
{
    return DP_min_uint8(255u, mul(base, blend));
}

static uint8_t blend_divide(uint8_t base, uint8_t blend)
{
    return DP_uint_to_uint8(
        DP_min_uint(255u, (base * 256u + blend / 2u) / (1u + blend)));
}

static uint8_t blend_darken(uint8_t base, uint8_t blend)
{
    return DP_min_uint8(base, blend);
}

static uint8_t blend_lighten(uint8_t base, uint8_t blend)
{
    return DP_max_uint8(base, blend);
}

static uint8_t blend_dodge(uint8_t base, uint8_t blend)
{
    return DP_uint_to_uint8(DP_min_uint(255u, base * 256u / (256u - blend)));
}

static uint8_t blend_burn(uint8_t base, uint8_t blend)
{
    return DP_int_to_uint8(DP_max_int(
        0, DP_min_int((255 - ((255 - base) * 256 / (blend + 1))), 255)));
}

static uint8_t blend_add(uint8_t base, uint8_t blend)
{
    return DP_uint_to_uint8(DP_min_uint(base + blend, 255u));
}

static uint8_t blend_subtract(uint8_t base, uint8_t blend)
{
    return DP_int_to_uint8(
        DP_max_int(DP_uint8_to_int(base) - DP_uint8_to_int(blend), 0));
}

static uint8_t blend_blend(DP_UNUSED uint8_t base, uint8_t blend)
{
    return blend;
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
DP_Pixel DP_pixel_unpremultiply(DP_Pixel pixel)
{
    if (pixel.a == 255) {
        return pixel;
    }
    else if (pixel.a == 0) {
        return (DP_Pixel){0};
    }
    else {
        unsigned int a1 = unpremultiply_factors[pixel.a];
        return (DP_Pixel){
            .b = DP_uint_to_uint8((pixel.b * a1 + 0x8000) >> 16),
            .g = DP_uint_to_uint8((pixel.g * a1 + 0x8000) >> 16),
            .r = DP_uint_to_uint8((pixel.r * a1 + 0x8000) >> 16),
            .a = pixel.a,
        };
    }
}

// Adapted from the Qt framework, see license above.
DP_Pixel DP_pixel_premultiply(DP_Pixel pixel)
{
    unsigned int a = pixel.a;
    unsigned int t = (pixel.color & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;
    uint32_t x = ((pixel.color >> 8) & 0xff) * a;
    x = (x + ((x >> 8) & 0xff) + 0x80);
    x &= 0xff00;
    return (DP_Pixel){DP_uint_to_uint32(x | t | (a << 24u))};
}


// Adapted from GIMP, see license above.
static DP_Pixel color_erase(double fsrc_b, double fsrc_g, double fsrc_r,
                            double fsrc_a, double ufdst_b, double ufdst_g,
                            double ufdst_r, double ufdst_a)
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

    return DP_pixel_premultiply((DP_Pixel){
        .b = DP_double_to_uint8(ufdst_b * 255.0),
        .g = DP_double_to_uint8(ufdst_g * 255.0),
        .r = DP_double_to_uint8(ufdst_r * 255.0),
        .a = DP_double_to_uint8(ufdst_a * 255.0),
    });
}


#define FOR_MASK_PIXEL(DST, MASK, W, H, MASK_SKIP, DST_SKIP, X, Y, A, BLOCK) \
    do {                                                                     \
        for (int Y = 0; Y < H; ++Y) {                                        \
            for (int X = 0; X < W; ++X, ++DST, ++MASK) {                     \
                uint8_t A = *MASK;                                           \
                BLOCK                                                        \
            }                                                                \
            DST += DST_SKIP;                                                 \
            MASK += MASK_SKIP;                                               \
        }                                                                    \
    } while (0)

static void composite_mask_unknown(DP_UNUSED DP_Pixel *dst,
                                   DP_UNUSED DP_Pixel src,
                                   DP_UNUSED uint8_t *mask, DP_UNUSED int w,
                                   DP_UNUSED int h, DP_UNUSED int mask_skip,
                                   DP_UNUSED int dst_skip)
{
    // Nothing, unknown blend mode.
}

static void composite_mask_copy(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                int w, int h, int mask_skip, int dst_skip)
{
    FOR_MASK_PIXEL(dst, mask, w, h, mask_skip, dst_skip, x, y, a, {
        dst->b = mul(src.b, a);
        dst->g = mul(src.g, a);
        dst->r = mul(src.r, a);
        dst->a = mul(src.a, a);
    });
}

static void composite_mask_erase(DP_Pixel *dst, DP_UNUSED DP_Pixel src,
                                 uint8_t *mask, int w, int h, int mask_skip,
                                 int dst_skip)
{
    FOR_MASK_PIXEL(dst, mask, w, h, mask_skip, dst_skip, x, y, a, {
        if (a != 0u && dst->a != 0u) {
            unsigned int a1 = 255u - a;
            dst->b = mul(dst->b, a1);
            dst->g = mul(dst->g, a1);
            dst->r = mul(dst->r, a1);
            dst->a = mul(dst->a, a1);
        }
    });
}

static void composite_mask_color_erase(DP_Pixel *dst, DP_Pixel src,
                                       uint8_t *mask, int w, int h,
                                       int mask_skip, int dst_skip)
{
    double fsrc_b = src.b / 255.0;
    double fsrc_g = src.g / 255.0;
    double fsrc_r = src.r / 255.0;
    FOR_MASK_PIXEL(dst, mask, w, h, mask_skip, dst_skip, x, y, a, {
        if (a != 0u) {
            DP_Pixel udst = DP_pixel_unpremultiply(*dst);
            *dst =
                color_erase(fsrc_b, fsrc_g, fsrc_r, a / 255.0, udst.b / 255.0,
                            udst.g / 255.0, udst.r / 255.0, udst.a / 255.0);
        }
    });
}

static void composite_mask_alpha_blend(DP_Pixel *dst, DP_Pixel src,
                                       uint8_t *mask, int w, int h,
                                       int mask_skip, int dst_skip)
{
    FOR_MASK_PIXEL(dst, mask, w, h, mask_skip, dst_skip, x, y, a, {
        if (a == 255u) { // Pixel is opaque, just replace the color.
            dst->color = src.color | 0xff000000u;
        }
        else if (a != 0u) { // Pixel isn't transparent, blend normally.
            unsigned int a1 = 255u - a;
            dst->b = mul(src.b, a) + mul(dst->b, a1);
            dst->g = mul(src.g, a) + mul(dst->g, a1);
            dst->r = mul(src.r, a) + mul(dst->r, a1);
            dst->a = a + mul(dst->a, a1);
        }
    });
}

static void composite_mask_alpha_under(DP_Pixel *dst, DP_Pixel src,
                                       uint8_t *mask, int w, int h,
                                       int mask_skip, int dst_skip)
{
    FOR_MASK_PIXEL(dst, mask, w, h, mask_skip, dst_skip, x, y, a, {
        if (a != 0u && dst->a != 255u) {
            unsigned int a1 = mul(255u - dst->a, a);
            dst->b = mul(src.b, a1) + dst->b;
            dst->g = mul(src.g, a1) + dst->g;
            dst->r = mul(src.r, a1) + dst->r;
            dst->a += DP_uint_to_uint8(a1);
        }
    });
}

static void mask_composite_with(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                int w, int h, int mask_skip, int dst_skip,
                                uint8_t (*blend_op)(uint8_t, uint8_t))
{
    FOR_MASK_PIXEL(dst, mask, w, h, mask_skip, dst_skip, x, y, a, {
        if (a != 0u && dst->color != 0u) {
            DP_Pixel d = DP_pixel_unpremultiply(*dst);
            d.b = blend(blend_op(d.b, src.b), d.b, a);
            d.g = blend(blend_op(d.g, src.g), d.g, a);
            d.r = blend(blend_op(d.r, src.r), d.r, a);
            *dst = DP_pixel_premultiply(d);
        }
    });
}

static void composite_mask_multiply(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                    int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip,
                        blend_multiply);
}

static void composite_mask_divide(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                  int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip,
                        blend_divide);
}

static void composite_mask_burn(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip, blend_burn);
}

static void composite_mask_dodge(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                 int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip, blend_dodge);
}

static void composite_mask_darken(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                  int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip,
                        blend_darken);
}

static void composite_mask_lighten(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                   int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip,
                        blend_lighten);
}

static void composite_mask_subtract(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                    int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip,
                        blend_subtract);
}

static void composite_mask_add(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                               int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip, blend_add);
}

static void composite_mask_blend(DP_Pixel *dst, DP_Pixel src, uint8_t *mask,
                                 int w, int h, int mask_skip, int dst_skip)
{
    mask_composite_with(dst, src, mask, w, h, mask_skip, dst_skip, blend_blend);
}

static DP_CompositeBrushFn
get_composite_composite_mask_operation(int blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_ERASE:
        return composite_mask_erase;
    case DP_BLEND_MODE_NORMAL:
        return composite_mask_alpha_blend;
    case DP_BLEND_MODE_MULTIPLY:
        return composite_mask_multiply;
    case DP_BLEND_MODE_DIVIDE:
        return composite_mask_divide;
    case DP_BLEND_MODE_BURN:
        return composite_mask_burn;
    case DP_BLEND_MODE_DODGE:
        return composite_mask_dodge;
    case DP_BLEND_MODE_DARKEN:
        return composite_mask_darken;
    case DP_BLEND_MODE_LIGHTEN:
        return composite_mask_lighten;
    case DP_BLEND_MODE_SUBTRACT:
        return composite_mask_subtract;
    case DP_BLEND_MODE_ADD:
        return composite_mask_add;
    case DP_BLEND_MODE_RECOLOR:
        return composite_mask_blend;
    case DP_BLEND_MODE_BEHIND:
        return composite_mask_alpha_under;
    case DP_BLEND_MODE_COLOR_ERASE:
        return composite_mask_color_erase;
    case DP_BLEND_MODE_REPLACE:
        return composite_mask_copy;
    default:
        DP_debug("Unknown brush composite blend mode %d (%s)", blend_mode,
                 DP_blend_mode_enum_name(blend_mode));
        return composite_mask_unknown;
    }
}

void DP_pixels_composite_mask(DP_Pixel *dst, DP_Pixel src, int blend_mode,
                              uint8_t *mask, int w, int h, int mask_skip,
                              int dst_skip)
{
    DP_CompositeBrushFn op = get_composite_composite_mask_operation(blend_mode);
    op(dst, src, mask, w, h, mask_skip, dst_skip);
}


#define FOR_PIXEL(DST, SRC, PIXEL_COUNT, I, BLOCK)            \
    do {                                                      \
        for (int I = 0; I < PIXEL_COUNT; ++I, ++DST, ++SRC) { \
            BLOCK                                             \
        }                                                     \
    } while (0)

static void composite_unknown(DP_UNUSED DP_Pixel *DP_RESTRICT dst,
                              DP_UNUSED DP_Pixel *DP_RESTRICT src,
                              DP_UNUSED int pixel_count,
                              DP_UNUSED uint8_t opacity)
{
    // Nothing, unknown blend mode.
}

static void composite_copy(DP_Pixel *DP_RESTRICT dst, DP_Pixel *DP_RESTRICT src,
                           int pixel_count, DP_UNUSED uint8_t opacity)
{
    FOR_PIXEL(dst, src, pixel_count, i, { *dst = *src; });
}

static void composite_erase(DP_Pixel *DP_RESTRICT dst,
                            DP_Pixel *DP_RESTRICT src, int pixel_count,
                            uint8_t opacity)
{
    FOR_PIXEL(dst, src, pixel_count, i, {
        unsigned int a1 = 255 - mul(src->a, opacity);
        dst->b = mul(dst->b, a1);
        dst->g = mul(dst->g, a1);
        dst->r = mul(dst->r, a1);
        dst->a = mul(dst->a, a1);
    });
}

static void composite_color_erase(DP_Pixel *DP_RESTRICT dst,
                                  DP_Pixel *DP_RESTRICT src, int pixel_count,
                                  uint8_t opacity)
{
    double o = opacity / 255.0;
    FOR_PIXEL(dst, src, pixel_count, i, {
        DP_Pixel udst = DP_pixel_unpremultiply(*dst);
        DP_Pixel usrc = DP_pixel_unpremultiply(*src);
        *dst = color_erase(usrc.b / 255.0, usrc.g / 255.0, usrc.r / 255.0,
                           usrc.a / 255.0 * o, udst.b / 255.0, udst.g / 255.0,
                           udst.r / 255.0, udst.a / 255.0);
    });
}

static void composite_alpha_blend(DP_Pixel *DP_RESTRICT dst,
                                  DP_Pixel *DP_RESTRICT src, int pixel_count,
                                  uint8_t opacity)
{
    FOR_PIXEL(dst, src, pixel_count, i, {
        DP_Pixel s = *src;
        unsigned int sa1 = 255u - mul(s.a, opacity);
        if (sa1 != 255u) {
            dst->b = mul(s.b, opacity) + mul(dst->b, sa1);
            dst->g = mul(s.g, opacity) + mul(dst->g, sa1);
            dst->r = mul(s.r, opacity) + mul(dst->r, sa1);
            dst->a = mul(s.a, opacity) + mul(dst->a, sa1);
        }
    });
}

static void composite_alpha_under(DP_Pixel *DP_RESTRICT dst,
                                  DP_Pixel *DP_RESTRICT src, int pixel_count,
                                  uint8_t opacity)
{
    FOR_PIXEL(dst, src, pixel_count, i, {
        DP_Pixel d = *dst;
        DP_Pixel s = *src;
        if (d.a != 255u && s.a != 0u) {
            unsigned int sa1 = mul(255u - d.a, mul(s.a, opacity));
            dst->b = mul(s.b, sa1) + d.b;
            dst->g = mul(s.g, sa1) + d.g;
            dst->r = mul(s.r, sa1) + d.r;
            dst->a = mul(s.a, sa1) + d.a;
        }
    });
}

static void composite_with(DP_Pixel *DP_RESTRICT dst, DP_Pixel *DP_RESTRICT src,
                           int pixel_count, uint8_t opacity,
                           uint8_t (*blend_op)(uint8_t, uint8_t))
{
    FOR_PIXEL(dst, src, pixel_count, i, {
        DP_Pixel d = *dst;
        DP_Pixel s = *src;
        if (d.color && s.color) {
            DP_Pixel ud = DP_pixel_unpremultiply(d);
            DP_Pixel us = DP_pixel_unpremultiply(s);
            uint8_t a = mul(us.a, opacity);
            ud.b = blend(blend_op(ud.b, us.b), ud.b, a);
            ud.g = blend(blend_op(ud.g, us.g), ud.g, a);
            ud.r = blend(blend_op(ud.r, us.r), ud.r, a);
            *dst = DP_pixel_premultiply(ud);
        }
    });
}

static void composite_multiply(DP_Pixel *DP_RESTRICT dst,
                               DP_Pixel *DP_RESTRICT src, int pixel_count,
                               uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_multiply);
}

static void composite_divide(DP_Pixel *DP_RESTRICT dst,
                             DP_Pixel *DP_RESTRICT src, int pixel_count,
                             uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_divide);
}

static void composite_burn(DP_Pixel *DP_RESTRICT dst, DP_Pixel *DP_RESTRICT src,
                           int pixel_count, uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_burn);
}

static void composite_dodge(DP_Pixel *DP_RESTRICT dst,
                            DP_Pixel *DP_RESTRICT src, int pixel_count,
                            uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_dodge);
}

static void composite_darken(DP_Pixel *DP_RESTRICT dst,
                             DP_Pixel *DP_RESTRICT src, int pixel_count,
                             uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_darken);
}

static void composite_lighten(DP_Pixel *DP_RESTRICT dst,
                              DP_Pixel *DP_RESTRICT src, int pixel_count,
                              uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_lighten);
}

static void composite_subtract(DP_Pixel *DP_RESTRICT dst,
                               DP_Pixel *DP_RESTRICT src, int pixel_count,
                               uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_subtract);
}

static void composite_add(DP_Pixel *DP_RESTRICT dst, DP_Pixel *DP_RESTRICT src,
                          int pixel_count, uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_add);
}

static void composite_blend(DP_Pixel *DP_RESTRICT dst,
                            DP_Pixel *DP_RESTRICT src, int pixel_count,
                            uint8_t opacity)
{
    composite_with(dst, src, pixel_count, opacity, blend_blend);
}

static DP_CompositeLayerFn get_composite_operation(int blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_ERASE:
        return composite_erase;
    case DP_BLEND_MODE_NORMAL:
        return composite_alpha_blend;
    case DP_BLEND_MODE_MULTIPLY:
        return composite_multiply;
    case DP_BLEND_MODE_DIVIDE:
        return composite_divide;
    case DP_BLEND_MODE_BURN:
        return composite_burn;
    case DP_BLEND_MODE_DODGE:
        return composite_dodge;
    case DP_BLEND_MODE_DARKEN:
        return composite_darken;
    case DP_BLEND_MODE_LIGHTEN:
        return composite_lighten;
    case DP_BLEND_MODE_SUBTRACT:
        return composite_subtract;
    case DP_BLEND_MODE_ADD:
        return composite_add;
    case DP_BLEND_MODE_RECOLOR:
        return composite_blend;
    case DP_BLEND_MODE_BEHIND:
        return composite_alpha_under;
    case DP_BLEND_MODE_COLOR_ERASE:
        return composite_color_erase;
    case DP_BLEND_MODE_REPLACE:
        return composite_copy;
    default:
        DP_debug("Unknown layer composite blend mode %d (%s)", blend_mode,
                 DP_blend_mode_enum_name(blend_mode));
        return composite_unknown;
    }
}

void DP_pixels_composite(DP_Pixel *dst, DP_Pixel *src, int pixel_count,
                         uint8_t opacity, int blend_mode)
{
    DP_CompositeLayerFn op = get_composite_operation(blend_mode);
    op(dst, src, pixel_count, opacity);
}
