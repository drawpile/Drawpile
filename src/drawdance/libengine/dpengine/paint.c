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
 * Parts of this code are based on libmypaint, using it under the MIT license.
 * See 3rdparty/libmypaint/COPYING for details.
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on GIMP, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/gimp/COPYING for details.
 *
 */
#include "paint.h"
#include "draw_context.h"
#include "layer_content.h"
#include "pixels.h"
#include "user_cursors.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/cpu.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>
#include <math.h>
#include <helpers.h> // CLAMP


// These "classic" brush stamps are based on GIMP, see license above.

#define CLASSIC_LUT_RADIUS       128
#define CLASSIC_LUT_SIZE         (CLASSIC_LUT_RADIUS * CLASSIC_LUT_RADIUS)
#define CLASSIC_LUT_MIN_HARDNESS 0
#define CLASSIC_LUT_MAX_HARDNESS 100
#define CLASSIC_LUT_COUNT \
    (CLASSIC_LUT_MAX_HARDNESS - CLASSIC_LUT_MIN_HARDNESS + 1)

static uint16_t *generate_classic_lut(int index)
{
    DP_debug("Generating classic dab lookup table for index %d", index);
    uint16_t *cl = DP_malloc(sizeof(*cl) * CLASSIC_LUT_SIZE);
    double h = 1.0 - (index / 100.0);
    double exponent = h < 0.0000004 ? 1000000.0 : 0.4 / h;
    double radius = CLASSIC_LUT_RADIUS;
    for (int i = 0; i < CLASSIC_LUT_SIZE; ++i) {
        double d = 1.0 - pow(pow(sqrt(i) / radius, exponent), 2.0);
        cl[i] = DP_double_to_uint16(d * DP_BIT15);
    }
    return cl;
}

static const uint16_t *get_classic_lut(double hardness)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(lock);
    static uint16_t *classic_luts[CLASSIC_LUT_COUNT];
    int index = DP_double_to_int(hardness * 100.0);
    DP_ASSERT(index >= CLASSIC_LUT_MIN_HARDNESS);
    DP_ASSERT(index <= CLASSIC_LUT_MAX_HARDNESS);
    uint16_t *cl = classic_luts[index];
    if (cl) {
        return cl;
    }
    else {
        DP_atomic_lock(&lock);
        cl = classic_luts[index];
        if (!cl) {
            cl = generate_classic_lut(index);
            classic_luts[index] = cl;
        }
        DP_atomic_unlock(&lock);
        return cl;
    }
}


static DP_BrushStamp make_brush_stamp1(DP_DrawContext *dc)
{
    return (DP_BrushStamp){0, 0, 0, DP_draw_context_stamp_buffer1(dc)};
}

static DP_BrushStamp make_brush_stamp2(DP_DrawContext *dc)
{
    return (DP_BrushStamp){0, 0, 0, DP_draw_context_stamp_buffer2(dc)};
}


static void prepare_stamp(DP_BrushStamp *stamp, double hardness, double radius,
                          int diameter, const uint16_t **out_lut,
                          float *out_lut_scale)
{
    DP_ASSERT(diameter <= DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER);
    DP_ASSERT(DP_square_int(diameter) <= DP_DRAW_CONTEXT_STAMP_BUFFER_SIZE);
    stamp->diameter = diameter;

    int stamp_offsets = -diameter / 2;
    stamp->top = stamp_offsets;
    stamp->left = stamp_offsets;

    *out_lut = get_classic_lut(hardness);
    *out_lut_scale = DP_double_to_float(
        DP_square_double((CLASSIC_LUT_RADIUS - 1.0) / radius));
}

static void get_mask(DP_BrushStamp *stamp, double radius, double hardness)
{
    double r = radius / 2.0;

    if (r < 1.0) {
        // Special case for single-pixel brush.
        stamp->left = -1;
        stamp->top = -1;
        stamp->diameter = 3;
        memset(stamp->data, 0, 9);
        stamp->data[4] = DP_BIT15;
    }
    else {
        float offset;
        int diameter;
        int raw_diameter = DP_double_to_int(ceil(radius) + 2.0);
        bool raw_diameter_even = raw_diameter % 2 == 0;
        if (raw_diameter_even) {
            diameter = raw_diameter + 1;
            offset = -1.0f;
        }
        else {
            diameter = raw_diameter;
            offset = -0.5f;
        }

        // Empirically determined fudge factors to make small brushes look nice.
        float fudge = r < 4.0                      ? 0.8f
                    : raw_diameter_even && r < 8.0 ? 0.9f
                                                   : 1.0f;

        const uint16_t *lut;
        float lut_scale;
        prepare_stamp(stamp, hardness, r, diameter, &lut, &lut_scale);
        uint16_t *d = stamp->data;

        for (int y = 0; y < diameter; ++y) {
            double yy = DP_square_double(y - r + offset);
            for (int x = 0; x < diameter; ++x) {
                double dist =
                    (DP_square_double(x - r + offset) + yy) * fudge * lut_scale;
                int i = DP_double_to_int(dist);
                *d = i < CLASSIC_LUT_SIZE ? lut[i] : 0;
                ++d;
            }
        }
    }
}

static void get_high_res_mask(DP_BrushStamp *stamp, double radius,
                              double hardness)
{

    int raw_diameter = DP_double_to_int(ceil(radius) + 2.0);
    float raw_offset = DP_double_to_float((ceil(radius) - radius) / -2.0);
    int diameter;
    float offset;
    if (raw_diameter % 2 == 0) {
        diameter = raw_diameter + 1;
        offset = raw_offset - 2.5f;
    }
    else {
        diameter = raw_diameter;
        offset = raw_offset - 1.5f;
    }

    const uint16_t *lut;
    float lut_scale;
    prepare_stamp(stamp, hardness, radius, diameter, &lut, &lut_scale);
    uint16_t *ptr = stamp->data;

    for (int y = 0; y < diameter; ++y) {
        double yy0 = DP_square_double(y * 2.0 - radius + offset);
        double yy1 = DP_square_double(y * 2.0 + 1.0 - radius + offset);

        for (int x = 0; x < diameter; ++x) {
            double xx0 = DP_square_double(x * 2.0 - radius + offset);
            double xx1 = DP_square_double(x * 2.0 + 1.0 - radius + offset);

            int dist00 = DP_double_to_int((xx0 + yy0) * lut_scale);
            int dist01 = DP_double_to_int((xx0 + yy1) * lut_scale);
            int dist10 = DP_double_to_int((xx1 + yy0) * lut_scale);
            int dist11 = DP_double_to_int((xx1 + yy1) * lut_scale);

            uint32_t acc = (dist00 < CLASSIC_LUT_SIZE ? (uint32_t)lut[dist00] : 0)
                + (dist01 < CLASSIC_LUT_SIZE ? (uint32_t)lut[dist01] : 0)
                + (dist10 < CLASSIC_LUT_SIZE ? (uint32_t)lut[dist10] : 0)
                + (dist11 < CLASSIC_LUT_SIZE ? (uint32_t)lut[dist11] : 0);
            *(ptr++) = DP_uint32_to_uint16(acc / 4);
        }
    }
}

static void offset_mask(DP_BrushStamp *dst_stamp, DP_BrushStamp *src_stamp,
                        float xfrac, float yfrac)
{
    DP_ASSERT(dst_stamp->diameter == src_stamp->diameter);
    int diameter = src_stamp->diameter;
    double k0 = xfrac * yfrac;
    double k1 = (1.0 - xfrac) * yfrac;
    double k2 = xfrac * (1.0 - yfrac);
    double k3 = (1.0 - xfrac) * (1.0 - yfrac);
    uint16_t *src = src_stamp->data;
    uint16_t *dst = dst_stamp->data;

    *(dst++) = DP_double_to_uint16(src[0] * k3);
    for (int x = 0; x < diameter - 1; ++x) {
        *(dst++) = DP_double_to_uint16(src[x] * k2 + src[x + 1] * k3);
    }
    for (int y = 0; y < diameter - 1; ++y) {
        int yd = y * diameter;
        *(dst++) = DP_double_to_uint16(src[yd] * k1 + src[yd + diameter] * k3);
        for (int x = 0; x < diameter - 1; ++x) {
            *(dst++) =
                DP_double_to_uint16(src[yd + x] * k0 + src[yd + x + 1] * k1
                                    + src[yd + diameter + x] * k2
                                    + src[yd + diameter + x + 1] * k3);
        }
    }
}

static void get_classic_mask_stamp(DP_BrushStamp *stamp, double radius,
                                   double hardness)
{
    // Don't bother with a high-resolution mask for large brushes.
    if (radius < 8.0) {
        get_high_res_mask(stamp, radius, hardness);
    }
    else {
        get_mask(stamp, radius, hardness);
    }
}

static void get_classic_offset_stamp(DP_BrushStamp *offset_stamp,
                                     DP_BrushStamp *mask_stamp, double x,
                                     double y)
{
    offset_stamp->diameter = mask_stamp->diameter;

    float fx = DP_double_to_float(floor(x));
    float fy = DP_double_to_float(floor(y));
    offset_stamp->left = mask_stamp->left + DP_float_to_int(fx);
    offset_stamp->top = mask_stamp->top + DP_float_to_int(fy);

    float xfrac = DP_double_to_float(x - fx);
    float yfrac = DP_double_to_float(y - fy);

    if (xfrac < 0.5f) {
        xfrac += 0.5f;
        --offset_stamp->left;
    }
    else {
        xfrac -= 0.5f;
    }

    if (yfrac < 0.5) {
        yfrac += 0.5f;
        --offset_stamp->top;
    }
    else {
        yfrac -= 0.5f;
    }

    offset_mask(offset_stamp, mask_stamp, xfrac, yfrac);
}

static void draw_dabs_classic(DP_DrawContext *dc, DP_UserCursors *ucs_or_null,
                              DP_PaintDrawDabsParams *params,
                              DP_TransientLayerContent *tlc)
{
    unsigned int context_id = params->context_id;
    DP_UPixel15 pixel = DP_upixel15_from_color(params->color);
    int blend_mode = params->blend_mode;
    int dab_count = params->dab_count;
    const DP_ClassicDab *dabs = params->classic.dabs;

    int last_x = params->origin_x;
    int last_y = params->origin_y;
    DP_BrushStamp mask_stamp = make_brush_stamp1(dc);
    DP_BrushStamp offset_stamp = make_brush_stamp2(dc);
    for (int i = 0; i < dab_count; ++i) {
        const DP_ClassicDab *dab = DP_classic_dab_at(dabs, i);

        int x = last_x + DP_classic_dab_x(dab);
        int y = last_y + DP_classic_dab_y(dab);
        double radius = DP_classic_dab_size(dab) / 256.0;
        uint8_t opacity = DP_classic_dab_opacity(dab);

        // Don't try to draw infinitesimal or fully opaque dabs.
        if (radius >= 0.1 && opacity != 0) {
            get_classic_mask_stamp(&mask_stamp, radius,
                                   DP_classic_dab_hardness(dab) / 255.0);

            get_classic_offset_stamp(&offset_stamp, &mask_stamp, x / 4.0,
                                     y / 4.0);

            DP_transient_layer_content_brush_stamp_apply(
                tlc, context_id, pixel, DP_channel8_to_15(opacity), blend_mode,
                &offset_stamp);
        }

        last_x = x;
        last_y = y;
    }

    if (ucs_or_null) {
        DP_user_cursors_activate(ucs_or_null, context_id);
        DP_user_cursors_move(ucs_or_null, context_id, params->layer_id,
                             last_x / 4, last_y / 4);
    }
}


static void get_round_pixel_mask_stamp(DP_BrushStamp *stamp, int diameter)
{
    DP_ASSERT(diameter <= DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER);
    DP_ASSERT(DP_square_int(diameter) <= DP_DRAW_CONTEXT_STAMP_BUFFER_SIZE);
    stamp->diameter = diameter;

    uint16_t *data = stamp->data;
    double r = diameter / 2.0;
    double rr = DP_square_double(r);
    for (int y = 0; y < diameter; ++y) {
        double yy = DP_square_double(y - r + 0.5);
        for (int x = 0; x < diameter; ++x) {
            double xx = DP_square_double(x - r + 0.5);
            uint16_t value = xx + yy <= rr ? DP_BIT15 : 0;
            data[y * diameter + x] = value;
        }
    }
}

static void get_square_pixel_mask_stamp(DP_BrushStamp *stamp, int diameter)
{
    DP_ASSERT(diameter <= DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER);
    DP_ASSERT(DP_square_int(diameter) <= DP_DRAW_CONTEXT_STAMP_BUFFER_SIZE);
    stamp->diameter = diameter;

    uint16_t *data = stamp->data;
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            data[y * diameter + x] = DP_BIT15;
        }
    }
}

static void draw_dabs_pixel(DP_DrawContext *dc, DP_UserCursors *ucs_or_null,
                            DP_PaintDrawDabsParams *params,
                            DP_TransientLayerContent *tlc,
                            void (*get_stamp)(DP_BrushStamp *, int))
{
    unsigned int context_id = params->context_id;
    DP_UPixel15 pixel = DP_upixel15_from_color(params->color);
    int blend_mode = params->blend_mode;
    int dab_count = params->dab_count;
    const DP_PixelDab *dabs = params->pixel.dabs;

    int last_x = params->origin_x;
    int last_y = params->origin_y;
    DP_BrushStamp stamp = make_brush_stamp1(dc);

    int last_size = -1;
    for (int i = 0; i < dab_count; ++i) {
        const DP_PixelDab *dab = DP_pixel_dab_at(dabs, i);
        int x = last_x + DP_pixel_dab_x(dab);
        int y = last_y + DP_pixel_dab_y(dab);
        int size = DP_pixel_dab_size(dab);
        uint8_t opacity = DP_pixel_dab_opacity(dab);

        // Don't try to draw infinitesimal or fully opaque dabs.
        if (size != 0 && opacity != 0) {
            if (size != last_size) {
                get_stamp(&stamp, size);
                last_size = size;
            }

            int offset = size / 2;
            stamp.left = x - offset;
            stamp.top = y - offset;

            DP_transient_layer_content_brush_stamp_apply(
                tlc, context_id, pixel, DP_channel8_to_15(opacity), blend_mode,
                &stamp);
        }

        last_x = x;
        last_y = y;
    }

    if (ucs_or_null) {
        DP_user_cursors_activate(ucs_or_null, context_id);
        DP_user_cursors_move(ucs_or_null, context_id, params->layer_id, last_x,
                             last_y);
    }
}


static float aspect_ratio_from_uint8(uint8_t aspect_ratio)
{
    if (aspect_ratio == 0) {
        return 1.0f; // Fudged to be a perfectly round dab.
    }
    else {
        return DP_uint8_to_float(aspect_ratio) / 28.333f + 1.0f;
    }
}

// The following code is based on libmypaint, see license above.

#define AA_BORDER                    1.0f
#define TWO_PI                       6.283185307179586f
#define RADIUS_OF_CIRCLE_WITH_AREA_1 0.5641895835477563f /* sqrt(1.0 / pi) */

static void calculate_rr_mask_row(float *rr_mask_row, int start_x, int yp,
                                  int count, float radius, float aspect_ratio,
                                  float sn, float cs, float one_over_radius2)
{
    for (int xp = start_x; xp < start_x + count; ++xp) {
        float yy = DP_int_to_float(yp) + 0.5f - radius;
        float xx = DP_int_to_float(xp) + 0.5f - radius;
        float yyr = (yy * cs - xx * sn) * aspect_ratio;
        float xxr = yy * sn + xx * cs;
        float rr = (yyr * yyr + xxr * xxr) * one_over_radius2;

        rr_mask_row[xp] = rr;
    }
}

#ifdef DP_CPU_X64
static void calculate_rr_mask_row_sse(float *rr_mask_row, int start_x,
                                      int yp_int, int count, float radius,
                                      float aspect_ratio_float, float sn_float,
                                      float cs_float,
                                      float one_over_radius2_float)
{
    DP_ASSERT(count % 4 == 0);

    // Refer to calculate_rr_mask_row for the formulas

    __m128 half_minus_radius =
        _mm_sub_ps(_mm_set1_ps(0.5f), _mm_set1_ps((float)radius));

    __m128 aspect_ratio = _mm_set1_ps(aspect_ratio_float);
    __m128 sn = _mm_set1_ps(sn_float);
    __m128 cs = _mm_set1_ps(cs_float);
    __m128 one_over_radius2 = _mm_set1_ps(one_over_radius2_float);

    __m128 yp = _mm_set1_ps((float)yp_int);
    __m128 yy = _mm_add_ps(yp, half_minus_radius);

    __m128 xp = _mm_add_ps(_mm_setr_ps(0.0f, 1.0f, 2.0f, 3.0f),
                           _mm_set1_ps((float)start_x));

    for (int i = start_x; i < start_x + count; i += 4) {
        __m128 xx = _mm_add_ps(xp, half_minus_radius);
        __m128 yyr = _mm_mul_ps(
            _mm_sub_ps(_mm_mul_ps(yy, cs), _mm_mul_ps(xx, sn)), aspect_ratio);

        __m128 xxr = _mm_add_ps(_mm_mul_ps(yy, sn), _mm_mul_ps(xx, cs));

        __m128 rr =
            _mm_mul_ps(_mm_add_ps(_mm_mul_ps(yyr, yyr), _mm_mul_ps(xxr, xxr)),
                       one_over_radius2);

        _mm_storeu_ps(&rr_mask_row[i], rr);

        xp = _mm_add_ps(xp, _mm_set1_ps(4.0f));
    }
}

DP_TARGET_BEGIN("avx")
static void calculate_rr_mask_row_avx(float *rr_mask_row, int start_x,
                                      int yp_int, int count, float radius,
                                      float aspect_ratio_float, float sn_float,
                                      float cs_float,
                                      float one_over_radius2_float)
{
    DP_ASSERT(count % 8 == 0);

    // Refer to calculate_rr_mask_row for the formulas

    __m256 half_minus_radius =
        _mm256_sub_ps(_mm256_set1_ps(0.5f), _mm256_set1_ps((float)radius));

    __m256 aspect_ratio = _mm256_set1_ps(aspect_ratio_float);
    __m256 sn = _mm256_set1_ps(sn_float);
    __m256 cs = _mm256_set1_ps(cs_float);
    __m256 one_over_radius2 = _mm256_set1_ps(one_over_radius2_float);

    __m256 yp = _mm256_set1_ps((float)yp_int);
    __m256 yy = _mm256_add_ps(yp, half_minus_radius);

    __m256 xp = _mm256_add_ps(
        _mm256_setr_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f),
        _mm256_set1_ps((float)start_x));

    for (int i = start_x; i < start_x + count; i += 8) {
        __m256 xx = _mm256_add_ps(xp, half_minus_radius);
        __m256 yyr = _mm256_mul_ps(
            _mm256_sub_ps(_mm256_mul_ps(yy, cs), _mm256_mul_ps(xx, sn)),
            aspect_ratio);

        __m256 xxr =
            _mm256_add_ps(_mm256_mul_ps(yy, sn), _mm256_mul_ps(xx, cs));

        __m256 rr = _mm256_mul_ps(
            _mm256_add_ps(_mm256_mul_ps(yyr, yyr), _mm256_mul_ps(xxr, xxr)),
            one_over_radius2);

        _mm256_storeu_ps(&rr_mask_row[i], rr);

        xp = _mm256_add_ps(xp, _mm256_set1_ps(8.0f));
    }
    _mm256_zeroupper();
}
DP_TARGET_END
#endif

static void calculate_rr_mask(float *rr_mask, int idia, float radius,
                              float aspect_ratio, float sn, float cs,
                              float one_over_radius2)
{
#ifdef DP_CPU_X64
    for (int yp = 0; yp < idia; ++yp) {
        int xp = 0;
        int remaining = idia;

        if (DP_cpu_support >= DP_CPU_SUPPORT_AVX) {
            int remaining_after_avx_width = remaining % 8;
            int avx_width = remaining - remaining_after_avx_width;

            calculate_rr_mask_row_avx(&rr_mask[yp * idia], xp, yp, avx_width,
                                      radius, aspect_ratio, sn, cs,
                                      one_over_radius2);

            remaining -= avx_width;
            xp += avx_width;
        }

        int remaining_after_sse_width = remaining % 4;
        int sse_width = remaining - remaining_after_sse_width;

        calculate_rr_mask_row_sse(&rr_mask[yp * idia], xp, yp, sse_width,
                                  radius, aspect_ratio, sn, cs,
                                  one_over_radius2);

        remaining -= sse_width;
        xp += sse_width;


        calculate_rr_mask_row(&rr_mask[yp * idia], xp, yp, remaining, radius,
                              aspect_ratio, sn, cs, one_over_radius2);
    }
#else
    for (int yp = 0; yp < idia; ++yp) {
        calculate_rr_mask_row(&rr_mask[yp * idia], 0, yp, idia, radius,
                              aspect_ratio, sn, cs, one_over_radius2);
    }
#endif
}

static float calculate_r_sample(float x, float y, float aspect_ratio, float sn,
                                float cs)
{
    float yyr = (y * cs - x * sn) * aspect_ratio;
    float xxr = y * sn + x * cs;
    return yyr * yyr + xxr * xxr;
}

static float sign_point_in_line(float px, float py, float vx, float vy)
{
    return (px - vx) * -vy - vx * (py - vy);
}

static float calculate_rr_antialiased(int xp, int yp, float radius,
                                      float aspect_ratio, float sn, float cs,
                                      float one_over_radius2, float r_aa_start)
{
    float pixel_right = radius - DP_int_to_float(xp);
    float pixel_bottom = radius - DP_int_to_float(yp);
    float pixel_center_x = pixel_right - 0.5f;
    float pixel_center_y = pixel_bottom - 0.5f;
    float pixel_left = pixel_right - 1.0f;
    float pixel_top = pixel_bottom - 1.0f;

    float nearest_x, nearest_y, rr_near;
    if (pixel_left < 0.0f && pixel_right > 0.0f && pixel_top < 0.0f
        && pixel_bottom > 0.0f) {
        nearest_x = 0.0f;
        nearest_y = 0.0f;
        rr_near = 0.0f;
    }
    else {
        float l2 = cs * cs + sn * sn;
        float ltp_dot = pixel_center_x * cs + pixel_center_y * sn;
        float t = ltp_dot / l2;
        nearest_x = DP_min_float(pixel_right, DP_max_float(pixel_left, cs * t));
        nearest_y = DP_min_float(pixel_top, DP_max_float(pixel_bottom, sn * t));
        float r_near =
            calculate_r_sample(nearest_x, nearest_y, aspect_ratio, sn, cs);
        rr_near = r_near * one_over_radius2;
    }

    if (rr_near > 1.0f) {
        return rr_near;
    }
    else {
        float center_sign =
            sign_point_in_line(pixel_center_x, pixel_center_y, cs, -sn);

        float farthest_x, farthest_y;
        if (center_sign < 0.0f) {
            farthest_x = nearest_x - sn * RADIUS_OF_CIRCLE_WITH_AREA_1;
            farthest_y = nearest_y + cs * RADIUS_OF_CIRCLE_WITH_AREA_1;
        }
        else {
            farthest_x = nearest_x + sn * RADIUS_OF_CIRCLE_WITH_AREA_1;
            farthest_y = nearest_y - cs * RADIUS_OF_CIRCLE_WITH_AREA_1;
        }

        float r_far =
            calculate_r_sample(farthest_x, farthest_y, aspect_ratio, sn, cs);
        float rr_far = r_far * one_over_radius2;

        if (r_far < r_aa_start) {
            return (rr_far + rr_near) * 0.5f;
        }
        else {
            float delta = rr_far - rr_near;
            float delta2 = 1.0f + delta;
            float visibility_near = (1.0f - rr_near) / delta2;
            return 1.0f - visibility_near;
        }
    }
}

static void calculate_opa_mask(uint16_t *mask, float *rr_mask, int count,
                               float hardness, float segment1_offset,
                               float segment1_slope, float segment2_offset,
                               float segment2_slope)
{

    for (int i = 0; i < count; ++i) {
        float rr = rr_mask[i];

        float opa = 0.0f;
        if (rr > 1.0f) {
            opa = 0.0f;
        }
        else {
            if (rr <= hardness) {
                opa = segment1_offset + rr * segment1_slope;
            }
            else {
                opa = segment2_offset + rr * segment2_slope;
            }
        }


        DP_ASSERT(opa >= 0.0f);
        DP_ASSERT(opa <= 1.0f);
        mask[i] = DP_float_to_uint16(opa * (float)DP_BIT15);
    }
}
#ifdef DP_CPU_X64
DP_TARGET_BEGIN("sse4.2")
static void calculate_opa_mask_sse42(uint16_t *mask, float *rr_mask, int count,
                                     float hardness, float segment1_offset,
                                     float segment1_slope,
                                     float segment2_offset,
                                     float segment2_slope)
{

    DP_ASSERT(count % 4 == 0);
    DP_ASSERT((intptr_t)rr_mask % 16 == 0);

    // Refer to calculate_opa_mask for conditions

    for (int i = 0; i < count; i += 4) {
        __m128 rr = _mm_load_ps(&rr_mask[i]);

        __m128 if_gt_1 = _mm_set1_ps(0.0f);

        __m128 if_le_hardness =
            _mm_add_ps(_mm_set1_ps(segment1_offset),
                       _mm_mul_ps(rr, _mm_set1_ps(segment1_slope)));
        __m128 else_le_hardness =
            _mm_add_ps(_mm_set1_ps(segment2_offset),
                       _mm_mul_ps(rr, _mm_set1_ps(segment2_slope)));

        __m128 if_gt_1_mask = _mm_cmpgt_ps(rr, _mm_set1_ps(1.0f));
        __m128 le_hardness_mask = _mm_cmple_ps(rr, _mm_set1_ps(hardness));

        __m128 opa = _mm_blendv_ps(
            _mm_blendv_ps(else_le_hardness, if_le_hardness, le_hardness_mask),
            if_gt_1, if_gt_1_mask);

        // Convert to 32 bit and shuffle to 16
        __m128i _32 =
            _mm_cvtps_epi32(_mm_mul_ps(opa, _mm_set1_ps((float)DP_BIT15)));
        __m128i _16 =
            _mm_shuffle_epi8(_32, _mm_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, -1,
                                                -1, -1, -1, -1, -1, -1, -1));

        _mm_storel_epi64((void *)&mask[i], _16);
    }
}
DP_TARGET_END

DP_TARGET_BEGIN("avx2")
static __m256i calculate_opa_mask_load_and_calculate_avx2(
    float *rr_mask, __m256 hardness, __m256 segment1_offset,
    __m256 segment1_slope, __m256 segment2_offset, __m256 segment2_slope)
{
    DP_ASSERT((intptr_t)rr_mask % 32 == 0);

    __m256 rr = _mm256_load_ps(rr_mask);

    __m256 if_gt_1 = _mm256_set1_ps(0.0f);

    __m256 if_le_hardness =
        _mm256_add_ps(segment1_offset, _mm256_mul_ps(rr, segment1_slope));
    __m256 else_le_hardness =
        _mm256_add_ps(segment2_offset, _mm256_mul_ps(rr, segment2_slope));

    __m256 if_gt_1_mask = _mm256_cmp_ps(rr, _mm256_set1_ps(1.0f), _CMP_GT_OS);
    __m256 le_hardness_mask = _mm256_cmp_ps(rr, hardness, _CMP_LE_OS);

    __m256 opa = _mm256_blendv_ps(
        _mm256_blendv_ps(else_le_hardness, if_le_hardness, le_hardness_mask),
        if_gt_1, if_gt_1_mask);

    return _mm256_cvtps_epi32(
        _mm256_mul_ps(opa, _mm256_set1_ps((float)DP_BIT15)));
}

static void calculate_opa_mask_avx2(uint16_t *mask, float *rr_mask, int count,
                                    float hardness_f, float segment1_offset_f,
                                    float segment1_slope_f,
                                    float segment2_offset_f,
                                    float segment2_slope_f)
{
    DP_ASSERT(count % 16 == 0);
    DP_ASSERT((intptr_t)rr_mask % 32 == 0);

    // Refer to calculate_opa_mask for conditions
    __m256 hardness = _mm256_set1_ps(hardness_f);
    __m256 segment1_offset = _mm256_set1_ps(segment1_offset_f);
    __m256 segment1_slope = _mm256_set1_ps(segment1_slope_f);
    __m256 segment2_offset = _mm256_set1_ps(segment2_offset_f);
    __m256 segment2_slope = _mm256_set1_ps(segment2_slope_f);

    for (int i = 0; i < count; i += 16) {
        __m256i _32_1 = calculate_opa_mask_load_and_calculate_avx2(
            &rr_mask[i], hardness, segment1_offset, segment1_slope,
            segment2_offset, segment2_slope);
        __m256i _32_2 = calculate_opa_mask_load_and_calculate_avx2(
            &rr_mask[i + 8], hardness, segment1_offset, segment1_slope,
            segment2_offset, segment2_slope);

        __m256i combined = _mm256_packus_epi32(_32_1, _32_2);
        __m256i _16 = _mm256_permute4x64_epi64(combined, 0xD8);

        _mm256_store_si256((void *)&mask[i], _16);
    }
}
DP_TARGET_END
#endif

static void calculate_opa(uint16_t *mask, float *rr_mask, int count,
                          float hardness, float segment1_offset,
                          float segment1_slope, float segment2_offset,
                          float segment2_slope)
{
#ifdef DP_CPU_X64
    if (DP_cpu_support >= DP_CPU_SUPPORT_AVX2) {
        int remaining_after_avx_width = count % 16;
        int avx_width = count - remaining_after_avx_width;

        calculate_opa_mask_avx2(mask, rr_mask, avx_width, hardness,
                                segment1_offset, segment1_slope,
                                segment2_offset, segment2_slope);

        count -= avx_width;
        mask += avx_width;
        rr_mask += avx_width;
    }

    if (DP_cpu_support >= DP_CPU_SUPPORT_SSE42) {
        int remaining_after_sse_width = count % 4;
        int sse_width = count - remaining_after_sse_width;

        calculate_opa_mask_sse42(mask, rr_mask, sse_width, hardness,
                                 segment1_offset, segment1_slope,
                                 segment2_offset, segment2_slope);

        count -= sse_width;
        mask += sse_width;
        rr_mask += sse_width;
    }
#endif

    calculate_opa_mask(mask, rr_mask, count, hardness, segment1_offset,
                       segment1_slope, segment2_offset, segment2_slope);
}

static void get_mypaint_brush_stamp_offsets(DP_BrushStamp *stamp, float x,
                                            float y, float radius)
{
    stamp->top = DP_float_to_int(y - radius + 0.5f);
    stamp->left = DP_float_to_int(x - radius + 0.5f);
}

static float
get_mypaint_brush_stamp(DP_BrushStamp *stamp, DP_DrawContext *dc, int raw_x,
                        int raw_y, uint16_t raw_diameter, uint8_t raw_hardness,
                        uint8_t raw_aspect_ratio, uint8_t raw_angle)
{
    float x = DP_int_to_float(raw_x) / 4.0f;
    float y = DP_int_to_float(raw_y) / 4.0f;
    float diameter = DP_uint16_to_float(raw_diameter) / 256.0f;
    float hardness = DP_uint8_to_float(raw_hardness) / 255.0f;
    float aspect_ratio = aspect_ratio_from_uint8(raw_aspect_ratio);
    float angle = DP_uint8_to_float(raw_angle) / 255.0f;

    float segment1_offset = 1.0f;
    float segment1_slope = -(1.0f / hardness - 1.0f);
    float segment2_offset = hardness / (1.0f - hardness);
    float segment2_slope = -hardness / (1.0f - hardness);

    float angle_rad = angle * TWO_PI; // angle is between 0 and 1, scale it.
    float cs = cosf(angle_rad);
    float sn = sinf(angle_rad);

    float radius = diameter / 2.0f;
    int idia = DP_float_to_int(floorf(diameter + 2.0f));
    DP_ASSERT(idia < DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER);
    float one_over_radius2 = 1.0f / (radius * radius);

    // Like MyPaint, we precalculate the RR mask because that's way faster than
    // doing each value inline, about twice as fast by my measurements.
    float *rr_mask = DP_draw_context_rr_mask_buffer(dc);
    if (radius < 3.0f) {
        float aa_start = DP_max_float(0.0f, radius - AA_BORDER);
        float r_aa_start = aa_start * (aa_start / aspect_ratio);
        for (int yp = 0; yp < idia; ++yp) {
            for (int xp = 0; xp < idia; ++xp) {
                rr_mask[yp * idia + xp] =
                    calculate_rr_antialiased(xp, yp, radius, aspect_ratio, sn,
                                             cs, one_over_radius2, r_aa_start);
            }
        }
    }
    else {
        calculate_rr_mask(rr_mask, idia, radius, aspect_ratio, sn, cs,
                          one_over_radius2);
    }

    uint16_t *mask = DP_draw_context_stamp_buffer1(dc);
    calculate_opa(mask, rr_mask, idia * idia, hardness, segment1_offset,
                  segment1_slope, segment2_offset, segment2_slope);

    get_mypaint_brush_stamp_offsets(stamp, x, y, radius);
    stamp->diameter = idia;
    stamp->data = mask;
    return radius;
}

// End of libmypaint-based code.

static uint16_t scale_opacity(float ratio, float opacity)
{
    return DP_float_to_uint16(ratio * opacity * (float)DP_BIT15);
}

static void apply_mypaint_dab(DP_TransientLayerContent *tlc,
                              unsigned int context_id, bool indirect,
                              DP_UPixel15 pixel, float normal, float lock_alpha,
                              float colorize, float posterize,
                              int posterize_num, DP_BrushStamp *stamp,
                              uint8_t dab_opacity)
{
    if (indirect) {
        DP_transient_layer_content_brush_stamp_apply(
            tlc, context_id, pixel, DP_channel8_to_15(dab_opacity),
            DP_BLEND_MODE_ALPHA_DARKEN, stamp);
    }
    else {
        float opacity = DP_uint8_to_float(dab_opacity) / 255.0f;

        if (normal > 0.0f) {
            DP_transient_layer_content_brush_stamp_apply(
                tlc, context_id, pixel, scale_opacity(normal, opacity),
                pixel.a == DP_BIT15 ? DP_BLEND_MODE_NORMAL
                                    : DP_BLEND_MODE_NORMAL_AND_ERASER,
                stamp);
        }

        if (lock_alpha > 0.0f && pixel.a != 0) {
            DP_transient_layer_content_brush_stamp_apply(
                tlc, context_id, pixel, scale_opacity(lock_alpha, opacity),
                DP_BLEND_MODE_RECOLOR, stamp);
        }

        if (colorize > 0.0f) {
            DP_transient_layer_content_brush_stamp_apply(
                tlc, context_id, pixel, scale_opacity(colorize, opacity),
                DP_BLEND_MODE_COLOR, stamp);
        }

        if (posterize > 0.0f) {
            DP_transient_layer_content_brush_stamp_apply_posterize(
                tlc, context_id, scale_opacity(posterize, opacity),
                posterize_num, stamp);
        }
    }
}

static void draw_dabs_mypaint(DP_DrawContext *dc, DP_UserCursors *ucs_or_null,
                              DP_PaintDrawDabsParams *params,
                              DP_TransientLayerContent *tlc)
{
    unsigned int context_id = params->context_id;
    bool indirect = params->indirect;
    DP_UPixel15 pixel = DP_upixel15_from_color(params->color);
    int dab_count = params->dab_count;
    const DP_MyPaintDab *dabs = params->mypaint.dabs;

    float lock_alpha = DP_uint8_to_float(params->mypaint.lock_alpha) / 255.0f;
    float colorize = DP_uint8_to_float(params->mypaint.colorize) / 255.0f;
    float posterize = DP_uint8_to_float(params->mypaint.posterize) / 255.0f;
    int posterize_num =
        DP_max_int(0, DP_min_int(127, params->mypaint.posterize_num)) + 1;
    float normal =
        1.0f * (1.0f - lock_alpha) * (1.0f - colorize) * (1.0f - posterize);

    // If two subsequent dabs use the same parameters, we can avoid the effort
    // of generating a new mask. The first dab is guaranteed to exist, a draw
    // dabs command with 0 dabs is rejected before we get here.
    const DP_MyPaintDab *first_dab = DP_mypaint_dab_at(dabs, 0);
    int last_x = params->origin_x + DP_mypaint_dab_x(first_dab);
    int last_y = params->origin_y + DP_mypaint_dab_y(first_dab);
    // FIXME: size is supposed to be the radius, not the diameter. I think.
    uint16_t last_size = DP_mypaint_dab_size(first_dab);
    uint8_t last_hardness = DP_mypaint_dab_hardness(first_dab);
    uint8_t last_aspect_ratio = DP_mypaint_dab_aspect_ratio(first_dab);
    uint8_t last_angle = DP_mypaint_dab_angle(first_dab);

    DP_BrushStamp stamp;
    float radius =
        get_mypaint_brush_stamp(&stamp, dc, last_x, last_y, last_size,
                                last_hardness, last_aspect_ratio, last_angle);
    apply_mypaint_dab(tlc, context_id, indirect, pixel, normal, lock_alpha,
                      colorize, posterize, posterize_num, &stamp,
                      DP_mypaint_dab_opacity(first_dab));
    if (ucs_or_null) {
        DP_user_cursors_activate(ucs_or_null, context_id);
        DP_user_cursors_move_smooth(ucs_or_null, context_id, params->layer_id,
                                    last_x / 4, last_y / 4, radius);
    }

    for (int i = 1; i < dab_count; ++i) {
        const DP_MyPaintDab *dab = DP_mypaint_dab_at(dabs, i);
        int x = last_x + DP_mypaint_dab_x(dab);
        int y = last_y + DP_mypaint_dab_y(dab);

        uint16_t size = DP_mypaint_dab_size(dab);
        uint8_t hardness = DP_mypaint_dab_hardness(dab);
        uint8_t aspect_ratio = DP_mypaint_dab_aspect_ratio(dab);
        uint8_t angle = DP_mypaint_dab_angle(dab);
        bool needs_new_mask = hardness != last_hardness || size != last_size
                           || aspect_ratio != last_aspect_ratio
                           || angle != last_angle;

        if (needs_new_mask) {
            radius = get_mypaint_brush_stamp(&stamp, dc, x, y, size, hardness,
                                             aspect_ratio, angle);
            last_hardness = hardness;
            last_size = size;
            last_aspect_ratio = aspect_ratio;
            last_angle = angle;
        }
        else {
            float xf = DP_int_to_float(x) / 4.0f;
            float yf = DP_int_to_float(y) / 4.0f;
            radius = DP_int_to_float(size) / 256.0f / 2.0f;
            get_mypaint_brush_stamp_offsets(&stamp, xf, yf, radius);
        }

        apply_mypaint_dab(tlc, context_id, indirect, pixel, normal, lock_alpha,
                          colorize, posterize, posterize_num, &stamp,
                          DP_mypaint_dab_opacity(dab));

        if (ucs_or_null) {
            DP_user_cursors_move_smooth(ucs_or_null, context_id,
                                        params->layer_id, x / 4, y / 4, radius);
        }

        last_x = x;
        last_y = y;
    }
}


void DP_paint_draw_dabs(DP_DrawContext *dc, DP_UserCursors *ucs_or_null,
                        DP_PaintDrawDabsParams *params,
                        DP_TransientLayerContent *tlc)
{
    DP_ASSERT(dc);
    DP_ASSERT(params);
    DP_ASSERT(tlc);
    DP_ASSERT(params->dab_count > 0); // This should be checked beforehand.
    int type = params->type;
    switch (type) {
    case DP_MSG_DRAW_DABS_CLASSIC:
        draw_dabs_classic(dc, ucs_or_null, params, tlc);
        break;
    case DP_MSG_DRAW_DABS_PIXEL:
        draw_dabs_pixel(dc, ucs_or_null, params, tlc,
                        get_round_pixel_mask_stamp);
        break;
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
        draw_dabs_pixel(dc, ucs_or_null, params, tlc,
                        get_square_pixel_mask_stamp);
        break;
    case DP_MSG_DRAW_DABS_MYPAINT:
        draw_dabs_mypaint(dc, ucs_or_null, params, tlc);
        break;
    default:
        DP_UNREACHABLE();
    }
}


DP_BrushStamp DP_paint_color_sampling_stamp_make(uint16_t *data, int diameter,
                                                 int left, int top,
                                                 int last_diameter)
{
    DP_ASSERT(data);
    DP_ASSERT(diameter > 0);
    DP_ASSERT(diameter <= DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER);

    const float *lut = get_classic_lut(0.5);
    int radius = diameter / 2;

    // Optimization, no need to recalculate the mask for the same diameter.
    if (diameter != last_diameter) {
        float lut_scale = DP_double_to_float(
            DP_square_double((CLASSIC_LUT_RADIUS - 1.0) / radius));
        uint16_t *d = data;

        for (int y = 0; y < diameter; ++y) {
            int yy = DP_double_to_int(DP_square_double(y - radius));
            for (int x = 0; x < diameter; ++x) {
                double dist = (DP_square_double(x - radius) + yy) * lut_scale;
                int i = DP_double_to_int(dist);
                *d = i < CLASSIC_LUT_SIZE
                       ? DP_float_to_uint16(DP_BIT15 * lut[i])
                       : 0;
                ++d;
            }
        }
    }

    return (DP_BrushStamp){top - radius, left - radius, diameter, data};
}

DP_UPixelFloat DP_paint_sample_to_upixel(int diameter, float weight, float red,
                                         float green, float blue, float alpha)
{
    // There must be at least some alpha for the results to make sense
    float required_alpha =
        DP_int_to_float(DP_square_int(diameter) * 30) / (float)DP_BIT15;
    if (alpha < required_alpha || weight < required_alpha) {
        return DP_upixel_float_zero();
    }

    // Calculate final average
    red /= weight;
    green /= weight;
    blue /= weight;
    alpha /= weight;

    // Unpremultiply, clamp against rounding error.
    red = CLAMP(red / alpha, 0.0f, 1.0f);
    green = CLAMP(green / alpha, 0.0f, 1.0f);
    blue = CLAMP(blue / alpha, 0.0f, 1.0f);

    return (DP_UPixelFloat){
        .b = blue,
        .g = green,
        .r = red,
        .a = alpha,
    };
}
