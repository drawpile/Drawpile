/*
 * Copyright (C) 2022 askmeaboufoom
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
#include "brush.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <helpers.h> // CLAMP


static float lerp_range(const DP_ClassicBrushRange *cbr, float input)
{
    float max = cbr->max;
    float min = cbr->min;
    if (min < max) {
        // Find the adjacent points in the curve and linearly interpolate
        // between them. MyPaint does it like this too, rather than using some
        // kind of more complicated smooth curve algorithm.
        float fmax = DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT - 1;
        int index = DP_max_int(0, DP_float_to_int(input * fmax));
        float a = cbr->curve.values[index];
        float p;
        if (index < DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT - 1) {
            float b = cbr->curve.values[index + 1];
            float k = (input - DP_int_to_float(index) / fmax) * fmax;
            p = a * (1.0f - k) + (b * k);
        }
        else {
            p = a;
        }
        return (max - min) * p + min;
    }
    return max;
}

static float lerp_range_dynamic(const DP_ClassicBrushRange *cbr, float pressure,
                                float velocity, float distance,
                                const DP_ClassicBrushDynamic *dynamic)
{
    switch (dynamic->type) {
    case DP_CLASSIC_BRUSH_DYNAMIC_NONE:
        return cbr->max;
    case DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE:
        return lerp_range(cbr, pressure);
    case DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY: {
        DP_ASSERT(velocity >= 0.0f);
        float max_velocity = dynamic->max_velocity;
        float vinput = velocity < max_velocity ? velocity / max_velocity : 1.0f;
        return lerp_range(cbr, vinput);
    }
    case DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE: {
        DP_ASSERT(distance >= 0.0f);
        float max_distance = dynamic->max_distance;
        float dinput = distance < max_distance ? distance / max_distance : 1.0f;
        return lerp_range(cbr, dinput);
    }
    }
    DP_UNREACHABLE();
}

float DP_classic_brush_spacing_at(const DP_ClassicBrush *cb, float pressure,
                                  float velocity, float distance)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return cb->spacing
         * DP_classic_brush_size_at(cb, pressure, velocity, distance);
}

float DP_classic_brush_size_at(const DP_ClassicBrush *cb, float pressure,
                               float velocity, float distance)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return lerp_range_dynamic(&cb->size, pressure, velocity, distance,
                              &cb->size_dynamic);
}

float DP_classic_brush_hardness_at(const DP_ClassicBrush *cb, float pressure,
                                   float velocity, float distance)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return lerp_range_dynamic(&cb->hardness, pressure, velocity, distance,
                              &cb->hardness_dynamic);
}

float DP_classic_brush_opacity_at(const DP_ClassicBrush *cb, float pressure,
                                  float velocity, float distance)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return lerp_range_dynamic(&cb->opacity, pressure, velocity, distance,
                              &cb->opacity_dynamic);
}

float DP_classic_brush_smudge_at(const DP_ClassicBrush *cb, float pressure,
                                 float velocity, float distance)
{
    DP_ASSERT(cb);
    DP_ASSERT(pressure >= 0.0f);
    DP_ASSERT(pressure <= 1.0f);
    return lerp_range_dynamic(&cb->smudge, pressure, velocity, distance,
                              &cb->smudge_dynamic);
}

DP_BlendMode DP_classic_brush_blend_mode(const DP_ClassicBrush *cb)
{
    DP_ASSERT(cb);
    return cb->erase ? cb->erase_mode : cb->brush_mode;
}


uint16_t DP_classic_brush_soft_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure, float velocity,
                                           float distance)
{
    float value =
        DP_classic_brush_size_at(cb, pressure, velocity, distance) * 256.0f
        + 0.5f;
    return DP_float_to_uint16(CLAMP(value, 0, UINT16_MAX));
}

uint8_t DP_classic_brush_pixel_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure, float velocity,
                                           float distance)
{
    float value =
        DP_classic_brush_size_at(cb, pressure, velocity, distance) + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 0, UINT8_MAX));
}

uint8_t DP_classic_brush_dab_opacity_at(const DP_ClassicBrush *cb,
                                        float pressure, float velocity,
                                        float distance)
{
    float value =
        DP_classic_brush_opacity_at(cb, pressure, velocity, distance) * 255.0f
        + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 0, UINT8_MAX));
}

uint8_t DP_classic_brush_dab_hardness_at(const DP_ClassicBrush *cb,
                                         float pressure, float velocity,
                                         float distance)
{
    float value =
        DP_classic_brush_hardness_at(cb, pressure, velocity, distance) * 255.0f
        + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 0, UINT8_MAX));
}


void DP_mypaint_brush_mode_extract(uint8_t mode, int *out_blend_mode,
                                   bool *out_indirect,
                                   uint8_t *out_posterize_num)
{
    int blend_mode;
    bool indirect;
    uint8_t posterize_num;
    if (mode & DP_MYPAINT_BRUSH_MODE_FLAG) {
        posterize_num = 0;
        switch (mode & DP_MYPAINT_BRUSH_MODE_MASK) {
        case DP_MYPAINT_BRUSH_MODE_NORMAL:
            blend_mode = DP_BLEND_MODE_NORMAL;
            indirect = true;
            break;
        case DP_MYPAINT_BRUSH_MODE_RECOLOR:
            blend_mode = DP_BLEND_MODE_RECOLOR;
            indirect = true;
            break;
        case DP_MYPAINT_BRUSH_MODE_ERASE:
            blend_mode = DP_BLEND_MODE_ERASE;
            indirect = true;
            break;
        default:
            blend_mode = DP_BLEND_MODE_NORMAL_AND_ERASER;
            indirect = false;
            break;
        }
    }
    else {
        blend_mode = DP_BLEND_MODE_NORMAL_AND_ERASER;
        indirect = false;
        posterize_num = mode;
    }

    if (out_blend_mode) {
        *out_blend_mode = blend_mode;
    }
    if (out_indirect) {
        *out_indirect = indirect;
    }
    if (out_posterize_num) {
        *out_posterize_num = posterize_num;
    }
}
