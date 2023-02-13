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

#define DP_MYPAINT_CONTROL_POINTS_COUNT 64


typedef enum DP_ClassicBrushShape {
    DP_CLASSIC_BRUSH_SHAPE_PIXEL_ROUND,
    DP_CLASSIC_BRUSH_SHAPE_PIXEL_SQUARE,
    DP_CLASSIC_BRUSH_SHAPE_SOFT_ROUND,
} DP_ClassicBrushShape;

typedef struct DP_ClassicBrushRange {
    float min_ratio;
    float max;
} DP_ClassicBrushRange;

typedef struct DP_ClassicBrush {
    DP_ClassicBrushRange size;
    DP_ClassicBrushRange hardness;
    DP_ClassicBrushRange opacity;
    DP_ClassicBrushRange smudge;
    float spacing;
    int resmudge;
    DP_UPixelFloat color;
    DP_ClassicBrushShape shape;
    DP_BlendMode mode;
    bool incremental;
    bool colorpick;
    bool size_pressure;
    bool hardness_pressure;
    bool opacity_pressure;
    bool smudge_pressure;
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
    bool lock_alpha;
    bool erase;
} DP_MyPaintBrush;


float DP_classic_brush_spacing_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_size_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_hardness_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_opacity_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_smudge_at(const DP_ClassicBrush *cb, float pressure);


uint16_t DP_classic_brush_soft_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure);

uint8_t DP_classic_brush_pixel_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure);

uint8_t DP_classic_brush_dab_opacity_at(const DP_ClassicBrush *cb,
                                        float pressure);

uint8_t DP_classic_brush_dab_hardness_at(const DP_ClassicBrush *cb,
                                         float pressure);


#endif
