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
#ifdef DP_BUNDLED_LIBMYPAINT
#    include "libmypaint/mypaint-brush-settings-gen.h"
#else
#    include <mypaint-brush-settings-gen.h>
#endif

#define DP_CLASSIC_BRUSH_CURVE_VALUE_COUNT 256
#define DP_MYPAINT_CONTROL_POINTS_COUNT    64

#define DP_MYPAINT_BRUSH_MODE_FLAG        0x80
#define DP_MYPAINT_BRUSH_MODE_INCREMENTAL 0x0
#define DP_MYPAINT_BRUSH_MODE_NORMAL      0x1
#define DP_MYPAINT_BRUSH_MODE_RECOLOR     0x2
#define DP_MYPAINT_BRUSH_MODE_ERASE       0x3
#define DP_MYPAINT_BRUSH_MODE_MASK        0x3


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

typedef struct DP_ClassicBrush {
    DP_ClassicBrushRange size;
    DP_ClassicBrushRange hardness;
    DP_ClassicBrushRange opacity;
    DP_ClassicBrushRange smudge;
    float spacing;
    int resmudge;
    DP_UPixelFloat color;
    DP_BrushShape shape;
    DP_BlendMode brush_mode;
    DP_BlendMode erase_mode;
    bool erase;
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
    bool incremental;
} DP_MyPaintBrush;


float DP_classic_brush_spacing_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_size_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_hardness_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_opacity_at(const DP_ClassicBrush *cb, float pressure);

float DP_classic_brush_smudge_at(const DP_ClassicBrush *cb, float pressure);

DP_BlendMode DP_classic_brush_blend_mode(const DP_ClassicBrush *cb);


uint16_t DP_classic_brush_soft_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure);

uint8_t DP_classic_brush_pixel_dab_size_at(const DP_ClassicBrush *cb,
                                           float pressure);

uint8_t DP_classic_brush_dab_opacity_at(const DP_ClassicBrush *cb,
                                        float pressure);

uint8_t DP_classic_brush_dab_hardness_at(const DP_ClassicBrush *cb,
                                         float pressure);


void DP_mypaint_brush_mode_extract(uint8_t mode, int *out_blend_mode,
                                   bool *out_indirect,
                                   uint8_t *out_posterize_num);


#endif
