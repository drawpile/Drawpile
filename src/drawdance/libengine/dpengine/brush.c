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
#include <mypaint-brush-settings.h>
#include <helpers.h> // CLAMP


static bool preset_equal_classic_brush_curve(const DP_ClassicBrushCurve *a,
                                             const DP_ClassicBrushCurve *b)
{
    for (int i = 0; i < DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT; ++i) {
        if (a->values[i] != b->values[i]) {
            return false;
        }
    }
    return true;
}

static bool preset_equal_classic_brush_range(const DP_ClassicBrushRange *a,
                                             const DP_ClassicBrushRange *b)
{
    return a->min == b->min && a->max == b->max
        && preset_equal_classic_brush_curve(&a->curve, &b->curve);
}

static bool preset_equal_classic_brush_dynamic(const DP_ClassicBrushDynamic *a,
                                               const DP_ClassicBrushDynamic *b)
{
    return a->type == b->type && a->max_velocity == b->max_velocity
        && a->max_distance == b->max_distance;
}

static bool preset_equal_classic_brush(const DP_ClassicBrush *a,
                                       const DP_ClassicBrush *b,
                                       bool in_eraser_slot)
{
    return preset_equal_classic_brush_range(&a->size, &b->size)
        && preset_equal_classic_brush_range(&a->hardness, &b->hardness)
        && preset_equal_classic_brush_range(&a->opacity, &b->opacity)
        && preset_equal_classic_brush_range(&a->smudge, &b->smudge)
        && a->spacing == b->spacing && a->resmudge == b->resmudge
        && a->shape == b->shape && a->paint_mode == b->paint_mode
        && a->brush_mode == b->brush_mode && a->erase_mode == b->erase_mode
        && (in_eraser_slot || a->erase == b->erase)
        && a->colorpick == b->colorpick
        && preset_equal_classic_brush_dynamic(&a->size_dynamic,
                                              &b->size_dynamic)
        && preset_equal_classic_brush_dynamic(&a->hardness_dynamic,
                                              &b->hardness_dynamic)
        && preset_equal_classic_brush_dynamic(&a->opacity_dynamic,
                                              &b->opacity_dynamic)
        && preset_equal_classic_brush_dynamic(&a->smudge_dynamic,
                                              &b->smudge_dynamic);
}

bool DP_classic_brush_equal_preset(const DP_ClassicBrush *a,
                                   const DP_ClassicBrush *b,
                                   bool in_eraser_slot)
{
    return a == b
        || (a && b && preset_equal_classic_brush(a, b, in_eraser_slot));
}


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


static bool preset_equal_mypaint_inputs(const DP_MyPaintControlPoints *a,
                                        const DP_MyPaintControlPoints *b)
{
    for (int input_id = 0; input_id < MYPAINT_BRUSH_INPUTS_COUNT; ++input_id) {
        int n = a->n;
        if (n != b->n) {
            return false;
        }
        for (int i = 0; i < n; ++i) {
            if (a->xvalues[i] != b->xvalues[i]
                || a->yvalues[i] != b->yvalues[i]) {
                return false;
            }
        }
    }
    return true;
}

static bool preset_equal_mypaint_mapping(int setting_id,
                                         const DP_MyPaintMapping *a,
                                         const DP_MyPaintMapping *b)
{
    return a->base_value == b->base_value
        && (mypaint_brush_setting_info((MyPaintBrushSetting)setting_id)
                ->constant
            || preset_equal_mypaint_inputs(a->inputs, b->inputs));
}

static bool preset_equal_mypaint_settings(const DP_MyPaintSettings *a,
                                          const DP_MyPaintSettings *b)
{
    for (int setting_id = 0; setting_id < MYPAINT_BRUSH_SETTINGS_COUNT;
         ++setting_id) {
        if (!preset_equal_mypaint_mapping(setting_id, &a->mappings[setting_id],
                                          &b->mappings[setting_id])) {
            return false;
        }
    }
    return true;
}

bool DP_mypaint_settings_equal_preset(const DP_MyPaintSettings *a,
                                      const DP_MyPaintSettings *b)
{
    return a == b || (a && b && preset_equal_mypaint_settings(a, b));
}


static bool preset_equal_mypaint_brush(const DP_MyPaintBrush *a,
                                       const DP_MyPaintBrush *b,
                                       bool in_eraser_slot)
{
    return a->paint_mode == b->paint_mode && a->brush_mode == b->brush_mode
        && a->erase_mode == b->erase_mode
        && (in_eraser_slot || a->erase == b->erase);
}

bool DP_mypaint_brush_equal_preset(const DP_MyPaintBrush *a,
                                   const DP_MyPaintBrush *b,
                                   bool in_eraser_slot)
{
    return a == b
        || (a && b && preset_equal_mypaint_brush(a, b, in_eraser_slot));
}

DP_BlendMode DP_mypaint_brush_blend_mode(const DP_MyPaintBrush *mb)
{
    DP_ASSERT(mb);
    return mb->erase ? mb->erase_mode : mb->brush_mode;
}
