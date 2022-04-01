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
#include <dpcommon/common.h>

typedef struct DP_ClassicBrushDab DP_ClassicBrushDab;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_PixelBrushDab DP_PixelBrushDab;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
#else
typedef struct DP_LayerContent DP_TransientLayerContent;
#endif


typedef struct DP_BrushStamp {
    int top;
    int left;
    int diameter;
    uint8_t *data;
} DP_BrushStamp;

typedef struct DP_PaintDrawDabsParams {
    int type;
    DP_DrawContext *draw_context;
    unsigned int context_id;
    int origin_x;
    int origin_y;
    uint32_t color;
    int blend_mode;
    int dab_count;
    void *dabs;
} DP_PaintDrawDabsParams;


void DP_paint_draw_dabs(DP_PaintDrawDabsParams *params,
                        DP_TransientLayerContent *tlc);


#endif
