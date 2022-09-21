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
 * Parts of this code are based on Drawpile, using it under the GNU General
 * Public License, version 3. See 3rdparty/licenses/drawpile/COPYING for
 * details.
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on the Qt framework's raster paint engine
 * implementation, using it under the GNU General Public License, version 3.
 * See 3rdparty/licenses/qt/license.GPL3 for details.
 */
#ifndef DPENGINE_DRAW_CONTEXT_H
#define DPENGINE_DRAW_CONTEXT_H
#include <dpcommon/common.h>

typedef struct DP_LayerContent DP_LayerContent;
typedef struct DP_LayerProps DP_LayerProps;
typedef union DP_Pixel8 DP_Pixel8;


#define DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER 260
#define DP_DRAW_CONTEXT_STAMP_BUFFER_SIZE \
    (DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER * DP_DRAW_CONTEXT_STAMP_MAX_DIAMETER)

#define DP_DRAW_CONTEXT_TRANSFORM_BUFFER_SIZE 204
#define DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE  8192
#define DP_DRAW_CONTEXT_RASTER_POOL_MAX_SIZE  (1024 * 1024)

struct DP_LayerContentPropsPair {
    DP_LayerContent *lc;
    DP_LayerProps *lp;
};

typedef uint16_t DP_BrushStampBuffer[DP_DRAW_CONTEXT_STAMP_BUFFER_SIZE];

typedef struct DP_DrawContext DP_DrawContext;

DP_DrawContext *DP_draw_context_new(void);

void DP_draw_context_free(DP_DrawContext *dc);

uint16_t *DP_draw_context_stamp_buffer1(DP_DrawContext *dc);
uint16_t *DP_draw_context_stamp_buffer2(DP_DrawContext *dc);

DP_Pixel8 *DP_draw_context_transform_buffer(DP_DrawContext *dc);

DP_Pixel8 *DP_draw_context_tile_decompression_buffer(DP_DrawContext *dc);

unsigned char *DP_draw_context_raster_pool(DP_DrawContext *dc,
                                           size_t *out_size);

unsigned char *DP_draw_context_raster_pool_resize(DP_DrawContext *dc,
                                                  size_t new_size);

struct DP_LayerContentPropsPair *DP_draw_context_layer_pool(DP_DrawContext *dc,
                                                            int *out_capacity);

struct DP_LayerContentPropsPair *
DP_draw_context_layer_pool_resize(DP_DrawContext *dc, int new_capacity);


#endif
