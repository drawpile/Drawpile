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
#ifndef DPENGINE_BRUSH_H
#define DPENGINE_BRUSH_H
#include "pixels.h"
#include <dpcommon/common.h>
#include <dpmsg/blend_mode.h>
#include <mypaint-brush-settings-gen.h>

#define DP_BRUSH_SIZE_MAX                  1000
#define DP_BRUSH_SIZE_MAX_COMPAT           255
#define DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT 256
#define DP_MYPAINT_CONTROL_POINTS_COUNT    64


typedef enum DP_BrushShape {
    DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND,
    DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE,
    DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND,
    DP_BRUSH_SHAPE_MYPAINT,
    DP_BRUSH_SHAPE_COUNT,
} DP_BrushShape;

typedef struct DP_ClassicBrushCurve {
    float values[DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT];
} DP_ClassicBrushCurve;

typedef struct DP_ClassicBrushRange {
    float min;
    float max;
    DP_ClassicBrushCurve curve;
} DP_ClassicBrushRange;

typedef enum DP_ClassicBrushDynamicType {
    DP_CLASSIC_BRUSH_DYNAMIC_NONE,
    DP_CLASSIC_BRUSH_DYNAMIC_PRESSURE,
    DP_CLASSIC_BRUSH_DYNAMIC_VELOCITY,
    DP_CLASSIC_BRUSH_DYNAMIC_DISTANCE,
} DP_ClassicBrushDynamicType;

typedef struct DP_ClassicBrushDynamic {
    DP_ClassicBrushDynamicType type;
    float max_velocity;
    float max_distance;
} DP_ClassicBrushDynamic;

typedef struct DP_ClassicBrush {
    DP_ClassicBrushRange size;
    DP_ClassicBrushRange hardness;
    DP_ClassicBrushRange opacity;
    DP_ClassicBrushRange smudge;
    float spacing;
    int resmudge;
    DP_UPixelFloat color;
    DP_BrushShape shape;
    DP_PaintMode paint_mode;
    DP_BlendMode brush_mode;
    DP_BlendMode erase_mode;
    bool erase;
    bool colorpick;
    DP_ClassicBrushDynamic size_dynamic;
    DP_ClassicBrushDynamic hardness_dynamic;
    DP_ClassicBrushDynamic opacity_dynamic;
    DP_ClassicBrushDynamic smudge_dynamic;
} DP_ClassicBrush;


typedef struct DP_MyPaintControlPoints {
    float xvalues[DP_MYPAINT_CONTROL_POINTS_COUNT];
    float yvalues[DP_MYPAINT_CONTROL_POINTS_COUNT];
    int n;
} DP_MyPaintControlPoints;

typedef struct DP_MyPaintMapping {
    float base_value;
    DP_MyPaintControlPoints inputs[MYPAINT_BRUSH_INPUTS_COUNT];
} DP_MyPaintMapping;

typedef struct DP_MyPaintSettings {
    DP_MyPaintMapping mappings[MYPAINT_BRUSH_SETTINGS_COUNT];
} DP_MyPaintSettings;

typedef struct DP_MyPaintBrush {
    DP_UPixelFloat color;
    DP_PaintMode paint_mode;
    DP_BlendMode brush_mode;
    DP_BlendMode erase_mode;
    bool erase;
} DP_MyPaintBrush;


DP_INLINE float DP_brush_size_maxf(bool compatibility_mode)
{
    if (compatibility_mode) {
        return DP_BRUSH_SIZE_MAX_COMPAT;
    }
    else {
        return DP_BRUSH_SIZE_MAX;
    }
}

bool DP_classic_brush_equal_preset(const DP_ClassicBrush *a,
                                   const DP_ClassicBrush *b,
                                   bool in_eraser_slot);

float DP_classic_brush_spacing_at(const DP_ClassicBrush *cb, float pressure,
                                  float velocity, float distance);

float DP_classic_brush_size_at(const DP_ClassicBrush *cb, float pressure,
                               float velocity, float distance);

float DP_classic_brush_hardness_at(const DP_ClassicBrush *cb, float pressure,
                                   float velocity, float distance);

float DP_classic_brush_opacity_at(const DP_ClassicBrush *cb, float pressure,
                                  float velocity, float distance);

float DP_classic_brush_smudge_at(const DP_ClassicBrush *cb, float pressure,
                                 float velocity, float distance);

DP_BlendMode DP_classic_brush_blend_mode(const DP_ClassicBrush *cb);


uint32_t DP_classic_brush_soft_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure, float velocity,
                                           float distance,
                                           bool compatibility_mode);

uint16_t DP_classic_brush_pixel_dab_size_at(const DP_ClassicBrush *cb,
                                            float pressure, float velocity,
                                            float distance,
                                            bool compatibility_mode);

uint8_t DP_classic_brush_dab_opacity_at(const DP_ClassicBrush *cb,
                                        float pressure, float velocity,
                                        float distance);

uint8_t DP_classic_brush_dab_hardness_at(const DP_ClassicBrush *cb,
                                         float pressure, float velocity,
                                         float distance);


float DP_mypaint_settings_max_size_for(const DP_MyPaintSettings *settings,
                                       float base_value);

float DP_mypaint_settings_base_value_for_max_size(
    const DP_MyPaintSettings *settings, float max_size);

bool DP_mypaint_settings_equal_preset(const DP_MyPaintSettings *a,
                                      const DP_MyPaintSettings *b);

bool DP_mypaint_brush_equal_preset(const DP_MyPaintBrush *a,
                                   const DP_MyPaintBrush *b,
                                   bool in_eraser_slot);

DP_BlendMode DP_mypaint_brush_blend_mode(const DP_MyPaintBrush *mb);

#endif
