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
#include "draw_context.h"
#include "pixels.h"
#include "tile.h"
#include <dpcommon/common.h>


struct DP_DrawContext {
    // Brush stamps, transformations and decompression are used by distinct
    // operations, so their buffers can share the same memory.
    union {
        // Brush stamp masks. Pixel brushes need one, classic brush needs two.
        struct {
            DP_BrushStampBuffer stamp_buffer1;
            DP_BrushStampBuffer stamp_buffer2;
        };
        // Pixel buffer for image transformation. Used by region move transform.
        DP_Pixel8 transform_buffer[DP_DRAW_CONTEXT_TRANSFORM_BUFFER_SIZE];
        // Buffer to inflate compressed 8 bit tiles into.
        DP_Pixel8 tile_decompression_buffer[DP_TILE_LENGTH];
    };
    // Memory pool used by qgrayraster during region move transform.
    size_t raster_pool_size;
    unsigned char *raster_pool;
};


DP_DrawContext *DP_draw_context_new(void)
{
    DP_DrawContext *dc = DP_malloc(sizeof(*dc));
    dc->raster_pool_size = DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE;
    dc->raster_pool = DP_malloc(DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE);
    return dc;
}

void DP_draw_context_free(DP_DrawContext *dc)
{
    if (dc) {
        DP_free(dc->raster_pool);
        DP_free(dc);
    }
}

uint16_t *DP_draw_context_stamp_buffer1(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return dc->stamp_buffer1;
}

uint16_t *DP_draw_context_stamp_buffer2(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return dc->stamp_buffer2;
}

DP_Pixel8 *DP_draw_context_transform_buffer(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return dc->transform_buffer;
}

DP_Pixel8 *DP_draw_context_tile_decompression_buffer(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return dc->tile_decompression_buffer;
}

unsigned char *DP_draw_context_raster_pool(DP_DrawContext *dc, size_t *out_size)
{
    DP_ASSERT(dc);
    DP_ASSERT(out_size);
    *out_size = dc->raster_pool_size;
    return dc->raster_pool;
}

unsigned char *DP_draw_context_raster_pool_resize(DP_DrawContext *dc,
                                                  size_t new_size)
{
    DP_ASSERT(dc);
    DP_ASSERT(new_size < DP_DRAW_CONTEXT_RASTER_POOL_MAX_SIZE);
    DP_free(dc->raster_pool);
    unsigned char *new_raster_pool = DP_malloc(new_size);
    dc->raster_pool = new_raster_pool;
    dc->raster_pool_size = new_size;
    return new_raster_pool;
}
