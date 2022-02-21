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
 * Parts of this code are based on GIMP, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/gimp/COPYING for details.
 *
 */
#include "paint.h"
#include "draw_context.h"
#include "layer_content.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpmsg/message.h>
#include <dpmsg/messages/draw_dabs.h>
#include <SDL_atomic.h>
#include <math.h>


// These "classic" brush stamps are based on GIMP, see license above.

#define CLASSIC_LUT_RADIUS       128
#define CLASSIC_LUT_SIZE         (CLASSIC_LUT_RADIUS * CLASSIC_LUT_RADIUS)
#define CLASSIC_LUT_MIN_HARDNESS 0
#define CLASSIC_LUT_MAX_HARDNESS 100
#define CLASSIC_LUT_COUNT \
    (CLASSIC_LUT_MAX_HARDNESS - CLASSIC_LUT_MIN_HARDNESS + 1)

static float *generate_classic_lut(int index)
{
    DP_debug("Generating classic dab lookup table for index %d", index);
    float *cl = DP_malloc(sizeof(*cl) * CLASSIC_LUT_SIZE);
    double h = 1.0 - (index / 100.0);
    double exponent = h < 0.0000004 ? 1000000.0 : 0.4 / h;
    double radius = CLASSIC_LUT_RADIUS;
    for (int i = 0; i < CLASSIC_LUT_SIZE; ++i) {
        double d = 1.0 - pow(pow(sqrt(i) / radius, exponent), 2.0);
        cl[i] = DP_double_to_float(d);
    }
    return cl;
}

static const float *get_classic_lut(double hardness)
{
    static SDL_SpinLock lock;
    static float *classic_luts[CLASSIC_LUT_COUNT];
    int index = DP_double_to_int(hardness * 100.0);
    DP_ASSERT(index >= CLASSIC_LUT_MIN_HARDNESS);
    DP_ASSERT(index <= CLASSIC_LUT_MAX_HARDNESS);
    float *cl = classic_luts[index];
    if (cl) {
        return cl;
    }
    else {
        SDL_AtomicLock(&lock);
        cl = classic_luts[index];
        if (!cl) {
            cl = generate_classic_lut(index);
            classic_luts[index] = cl;
        }
        SDL_AtomicUnlock(&lock);
        return cl;
    }
}


struct DP_BrushStamp {
    int top;
    int left;
    int diameter;
    uint8_t *data;
};

static DP_BrushStamp make_brush_stamp1(DP_DrawContext *dc)
{
    return (DP_BrushStamp){0, 0, 0, DP_draw_context_stamp_buffer1(dc)};
}

static DP_BrushStamp make_brush_stamp2(DP_DrawContext *dc)
{
    return (DP_BrushStamp){0, 0, 0, DP_draw_context_stamp_buffer2(dc)};
}

int DP_brush_stamp_top(DP_BrushStamp *stamp)
{
    DP_ASSERT(stamp);
    return stamp->top;
}

int DP_brush_stamp_left(DP_BrushStamp *stamp)
{
    DP_ASSERT(stamp);
    return stamp->left;
}

int DP_brush_stamp_diameter(DP_BrushStamp *stamp)
{
    DP_ASSERT(stamp);
    return stamp->diameter;
}

uint8_t *DP_brush_stamp_data(DP_BrushStamp *stamp)
{
    DP_ASSERT(stamp);
    return stamp->data;
}


static void prepare_stamp(DP_BrushStamp *stamp, double hardness, double radius,
                          int diameter, const float **out_lut,
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

static void get_mask(DP_BrushStamp *stamp, double radius, double hardness,
                     double opacity)
{
    double r = radius / 2.0;
    uint8_t o = DP_double_to_uint8(opacity * 255.0);

    if (r < 1.0) {
        // Special case for single-pixel brush.
        stamp->left = -1;
        stamp->top = -1;
        stamp->diameter = 3;
        memset(stamp->data, 0, 9);
        stamp->data[4] = o;
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

        const float *lut;
        float lut_scale;
        prepare_stamp(stamp, hardness, r, diameter, &lut, &lut_scale);
        uint8_t *d = stamp->data;

        for (int y = 0; y < diameter; ++y) {
            double yy = DP_square_double(y - r + offset);
            for (int x = 0; x < diameter; ++x) {
                double dist =
                    (DP_square_double(x - r + offset) + yy) * fudge * lut_scale;
                int i = DP_double_to_int(dist);
                *d = i < CLASSIC_LUT_SIZE ? DP_double_to_uint8(lut[i] * o) : 0;
                ++d;
            }
        }
    }
}

static void get_high_res_mask(DP_BrushStamp *stamp, double radius,
                              double hardness, double opacity)
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

    const float *lut;
    float lut_scale;
    prepare_stamp(stamp, hardness, radius, diameter, &lut, &lut_scale);
    uint8_t *ptr = stamp->data;
    double o = opacity * (255 / 4);

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

            double d = (dist00 < CLASSIC_LUT_SIZE ? lut[dist00] : 0.0)
                     + (dist01 < CLASSIC_LUT_SIZE ? lut[dist01] : 0.0)
                     + (dist10 < CLASSIC_LUT_SIZE ? lut[dist10] : 0.0)
                     + (dist11 < CLASSIC_LUT_SIZE ? lut[dist11] : 0.0);
            *(ptr++) = DP_double_to_uint8(d * o);
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
    uint8_t *src = src_stamp->data;
    uint8_t *dst = dst_stamp->data;

    *(dst++) = DP_double_to_uint8(src[0] * k3);
    for (int x = 0; x < diameter - 1; ++x) {
        *(dst++) = DP_double_to_uint8(src[x] * k2 + src[x + 1] * k3);
    }
    for (int y = 0; y < diameter - 1; ++y) {
        int yd = y * diameter;
        *(dst++) = DP_double_to_uint8(src[yd] * k1 + src[yd + diameter] * k3);
        for (int x = 0; x < diameter - 1; ++x) {
            *(dst++) =
                DP_double_to_uint8(src[yd + x] * k0 + src[yd + x + 1] * k1
                                   + src[yd + diameter + x] * k2
                                   + src[yd + diameter + x + 1] * k3);
        }
    }
}

static void get_classic_mask_stamp(DP_BrushStamp *stamp, double radius,
                                   double hardness, double opacity)
{
    // Don't bother with a high-resolution mask for large brushes.
    if (radius < 8.0) {
        get_high_res_mask(stamp, radius, hardness, opacity);
    }
    else {
        get_mask(stamp, radius, hardness, opacity);
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

static void draw_dabs_classic(DP_PaintDrawDabsParams *params,
                              DP_TransientLayerContent *tlc)
{
    DP_DrawContext *dc = params->draw_context;
    unsigned int context_id = params->context_id;
    uint32_t color = params->color;
    int blend_mode = params->blend_mode;
    int dab_count = params->dab_count;
    DP_ClassicBrushDab *dabs = params->dabs;

    int last_x = params->origin_x;
    int last_y = params->origin_y;
    DP_BrushStamp mask_stamp = make_brush_stamp1(dc);
    DP_BrushStamp offset_stamp = make_brush_stamp2(dc);
    for (int i = 0; i < dab_count; ++i) {
        DP_ClassicBrushDab *dab = DP_classic_brush_dab_at(dabs, i);

        get_classic_mask_stamp(&mask_stamp,
                               DP_classic_brush_dab_size(dab) / 256.0,
                               DP_classic_brush_dab_hardness(dab) / 255.0,
                               DP_classic_brush_dab_opacity(dab) / 255.0);

        int x = last_x + DP_classic_brush_dab_x(dab);
        int y = last_y + DP_classic_brush_dab_y(dab);
        get_classic_offset_stamp(&offset_stamp, &mask_stamp, x / 4.0, y / 4.0);

        DP_transient_layer_content_brush_stamp_apply(tlc, context_id, color,
                                                     blend_mode, &offset_stamp);
        last_x = x;
        last_y = y;
    }
}


static void get_round_pixel_mask_stamp(DP_BrushStamp *stamp, int diameter,
                                       uint8_t opacity)
{
    DP_ASSERT(diameter <= DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER);
    DP_ASSERT(DP_square_int(diameter) <= DP_DRAW_CONTEXT_STAMP_BUFFER_SIZE);
    stamp->diameter = diameter;

    uint8_t *data = stamp->data;
    double r = diameter / 2.0;
    double rr = DP_square_double(r);
    for (int y = 0; y < diameter; ++y) {
        double yy = DP_square_double(y - r + 0.5);
        for (int x = 0; x < diameter; ++x) {
            double xx = DP_square_double(x - r + 0.5);
            uint8_t value = xx + yy <= rr ? opacity : 0;
            data[y * diameter + x] = value;
        }
    }
}

static void get_square_pixel_mask_stamp(DP_BrushStamp *stamp, int diameter,
                                        uint8_t opacity)
{
    size_t size = DP_int_to_size(DP_square_int(diameter));
    DP_ASSERT(diameter <= DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER);
    DP_ASSERT(size <= DP_DRAW_CONTEXT_STAMP_BUFFER_SIZE);
    stamp->diameter = diameter;
    memset(stamp->data, opacity, size);
}

static void draw_dabs_pixel(DP_PaintDrawDabsParams *params,
                            DP_TransientLayerContent *tlc,
                            void (*get_stamp)(DP_BrushStamp *, int, uint8_t))
{
    unsigned int context_id = params->context_id;
    uint32_t color = params->color;
    int blend_mode = params->blend_mode;
    int dab_count = params->dab_count;
    DP_PixelBrushDab *dabs = params->dabs;

    int last_x = params->origin_x;
    int last_y = params->origin_y;
    DP_BrushStamp stamp = make_brush_stamp1(params->draw_context);

    int last_size = -1;
    uint8_t last_opacity = 0;
    for (int i = 0; i < dab_count; ++i) {
        DP_PixelBrushDab *dab = DP_pixel_brush_dab_at(dabs, i);

        int size = DP_pixel_brush_dab_size(dab);
        uint8_t opacity = DP_pixel_brush_dab_opacity(dab);
        if (size != last_size || opacity != last_opacity) {
            get_stamp(&stamp, size, opacity);
            last_size = size;
            last_opacity = opacity;
        }

        int x = last_x + DP_pixel_brush_dab_x(dab);
        int y = last_y + DP_pixel_brush_dab_y(dab);
        int offset = size / 2;
        stamp.left = x - offset;
        stamp.top = y - offset;

        DP_transient_layer_content_brush_stamp_apply(tlc, context_id, color,
                                                     blend_mode, &stamp);
        last_x = x;
        last_y = y;
    }
}


void DP_paint_draw_dabs(DP_PaintDrawDabsParams *params,
                        DP_TransientLayerContent *tlc)
{
    DP_ASSERT(params);
    DP_ASSERT(tlc);
    DP_ASSERT(params->dab_count > 0); // This should be checked beforehand.
    int type = params->type;
    switch (type) {
    case DP_MSG_DRAW_DABS_CLASSIC:
        draw_dabs_classic(params, tlc);
        break;
    case DP_MSG_DRAW_DABS_PIXEL:
        draw_dabs_pixel(params, tlc, get_round_pixel_mask_stamp);
        break;
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
        draw_dabs_pixel(params, tlc, get_square_pixel_mask_stamp);
        break;
    default:
        DP_panic("Unknown paint type %d", type);
    }
}
