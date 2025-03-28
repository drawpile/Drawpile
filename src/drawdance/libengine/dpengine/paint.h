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
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#ifndef DPENGINE_PAINT_H
#define DPENGINE_PAINT_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_ClassicDab DP_ClassicDab;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_MyPaintDab DP_MyPaintDab;
typedef struct DP_PixelDab DP_PixelDab;
typedef struct DP_UserCursors DP_UserCursors;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
#else
typedef struct DP_LayerContent DP_TransientLayerContent;
#endif


typedef struct DP_BrushStamp {
    int top;
    int left;
    int diameter;
    uint16_t *data;
} DP_BrushStamp;

typedef struct DP_PaintDrawDabsParams {
    int type;
    unsigned int context_id;
    int layer_id;
    int origin_x;
    int origin_y;
    uint32_t color;
    int blend_mode;
    bool indirect;
    int dab_count;
    union {
        struct {
            const DP_ClassicDab *dabs;
        } classic;
        struct {
            const DP_PixelDab *dabs;
        } pixel;
        struct {
            const DP_MyPaintDab *dabs;
            uint8_t lock_alpha;
            uint8_t colorize;
            uint8_t posterize;
            uint8_t posterize_num;
        } mypaint;
    };
} DP_PaintDrawDabsParams;


void DP_paint_draw_dabs(DP_DrawContext *dc, DP_UserCursors *ucs_or_null,
                        DP_PaintDrawDabsParams *params,
                        DP_TransientLayerContent *tlc);

DP_BrushStamp DP_paint_color_sampling_stamp_make(uint16_t *data, int diameter,
                                                 int left, int top,
                                                 int last_diameter);

DP_UPixelFloat DP_paint_sample_to_upixel(int diameter, float weight, float red,
                                         float green, float blue, float alpha);

#endif
