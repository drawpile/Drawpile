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
#include "compress.h"
#include "pixels.h"
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#ifdef DP_LIBSWSCALE
#    include <libswscale/swscale.h>
#endif


struct DP_DrawContext {
    // Transformations, decompression and layer id generation are used by
    // distinct operations, so their buffers can share memory.
    union {
        // Pixel buffer for image transformation. Used by region move transform.
        DP_Pixel8 transform_buffer[DP_DRAW_CONTEXT_TRANSFORM_BUFFER_SIZE];
        // Buffer to hold 8 bit tiles, used e.g. for inflate/deflate.
        DP_Pixel8 tile8_buffer[DP_TILE_LENGTH];
        // Split channel buffer, same as above.
        DP_SplitTile8 split_tile8_buffer;
        // Layer id generation, masking off already used ids.
        struct {
            bool ids_used[DP_DRAW_CONTEXT_ID_COUNT];
            int last_used_id;
        };
    };
    // Resizable memory pool used by qgrayraster during region move transform,
    // to collect layers into when reordering them, to hold the brush dab masks
    // when making brush strokes and other such temporary need. The pool is
    // allocated through DP_malloc_simd, so it's always going to be aligned.
    size_t pool_size;
    void *pool;
    ZSTD_DCtx *zstd_dctx;
#ifdef DP_LIBSWSCALE
    struct SwsContext *sws_context;
#endif
};


DP_DrawContext *DP_draw_context_new(void)
{
    DP_DrawContext *dc = DP_malloc(sizeof(*dc));
    dc->pool_size = 0;
    dc->pool = NULL;
    dc->zstd_dctx = NULL;
#ifdef DP_LIBSWSCALE
    dc->sws_context = NULL;
#endif
    return dc;
}

void DP_draw_context_free(DP_DrawContext *dc)
{
    if (dc) {
#ifdef DP_LIBSWSCALE
        sws_freeContext(dc->sws_context);
#endif
        DP_decompress_zstd_free(&dc->zstd_dctx);
        DP_free_simd(dc->pool);
        DP_free(dc);
    }
}


DP_DrawContextStatistics DP_draw_context_statistics(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return (DP_DrawContextStatistics){sizeof(*dc), dc->pool_size};
}


DP_Pixel8 *DP_draw_context_transform_buffer(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return dc->transform_buffer;
}

DP_Pixel8 *DP_draw_context_tile8_buffer(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return dc->tile8_buffer;
}

DP_SplitTile8 *DP_draw_context_split_tile8_buffer(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return &dc->split_tile8_buffer;
}

void DP_draw_context_id_generator_reset(DP_DrawContext *dc, int last_used_id)
{
    DP_ASSERT(dc);
    DP_ASSERT(last_used_id >= 0);
    DP_ASSERT(last_used_id < DP_DRAW_CONTEXT_ID_COUNT);
    memset(dc->ids_used, 0, sizeof(dc->ids_used));
    dc->ids_used[last_used_id] = true;
    dc->last_used_id = last_used_id;
}

void DP_draw_context_id_generator_mark_used(DP_DrawContext *dc, int id)
{
    DP_ASSERT(dc);
    DP_ASSERT(id >= 0);
    DP_ASSERT(id < DP_DRAW_CONTEXT_ID_COUNT);
    DP_ASSERT(!dc->ids_used[id]);
    dc->ids_used[id] = true;
}

int DP_draw_context_id_generator_next(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    int last_used_id = dc->last_used_id;
    for (int i = 1; i < DP_DRAW_CONTEXT_ID_COUNT; ++i) {
        int candidate_id = (last_used_id + i) % DP_DRAW_CONTEXT_ID_COUNT;
        if (!dc->ids_used[candidate_id]) {
            dc->ids_used[candidate_id] = true;
            dc->last_used_id = candidate_id;
            return candidate_id;
        }
    }
    return -1;
}


void *DP_draw_context_pool(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return dc->pool;
}

void *DP_draw_context_pool_require(DP_DrawContext *dc, size_t required_capacity)
{
    if (dc->pool) {
        if (dc->pool_size < required_capacity) {
            // If we got here, there was an allocation before us, which
            // must have been at least enough to hold the minimum size.
            DP_ASSERT(required_capacity
                      >= DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE);
            DP_free_simd(dc->pool);
            dc->pool_size = required_capacity;
            dc->pool = DP_malloc_simd(required_capacity);
        }
    }
    else {
        size_t size = DP_max_size(DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE,
                                  required_capacity);
        dc->pool_size = size;
        dc->pool = DP_malloc_simd(size);
    }
    return dc->pool;
}

static void init_pool(DP_DrawContext *dc)
{
    if (!dc->pool) {
        dc->pool_size = DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE;
        dc->pool = DP_malloc_simd(DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE);
    }
}

unsigned char *DP_draw_context_raster_pool(DP_DrawContext *dc, size_t *out_size)
{
    DP_ASSERT(dc);
    DP_ASSERT(out_size);
    init_pool(dc);
    *out_size = dc->pool_size;
    return dc->pool;
}

unsigned char *DP_draw_context_raster_pool_resize(DP_DrawContext *dc,
                                                  size_t new_size)
{
    DP_ASSERT(dc);
    DP_ASSERT(new_size > dc->pool_size);
    DP_ASSERT(new_size < DP_DRAW_CONTEXT_RASTER_POOL_MAX_SIZE);
    DP_ASSERT(dc->pool);
    DP_free_simd(dc->pool);
    void *new_raster_pool = DP_malloc_simd(new_size);
    dc->pool = new_raster_pool;
    dc->pool_size = new_size;
    return new_raster_pool;
}

// For layer indexes, the memory pool is just used as an array of integers. The
// first element is the count, subsequent elements are the list of indexes.

void DP_draw_context_layer_indexes_clear(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    init_pool(dc);
    int *indexes = dc->pool;
    indexes[0] = 0;
}

void DP_draw_context_layer_indexes_push(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    DP_ASSERT(dc->pool);
    int *indexes = dc->pool;
    size_t used = DP_int_to_size(++indexes[0]);
    size_t capacity = dc->pool_size / sizeof(*indexes);
    if (used == capacity) {
        size_t new_size = capacity * sizeof(*indexes) * 2;
        dc->pool = DP_realloc(dc->pool, new_size);
        dc->pool_size = new_size;
    }
}

int *DP_draw_context_layer_indexes_push_get(DP_DrawContext *dc, int *out_count)
{
    DP_draw_context_layer_indexes_push(dc);
    return DP_draw_context_layer_indexes(dc, out_count);
}

void DP_draw_context_layer_indexes_set(DP_DrawContext *dc, int layer_index)
{
    DP_ASSERT(dc);
    DP_ASSERT(dc->pool);
    DP_ASSERT(layer_index >= 0);
    int *indexes = dc->pool;
    indexes[indexes[0]] = layer_index;
}

void DP_draw_context_layer_indexes_pop(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    DP_ASSERT(dc->pool);
    int *indexes = dc->pool;
    --indexes[0];
}

int *DP_draw_context_layer_indexes(DP_DrawContext *dc, int *out_count)
{
    DP_ASSERT(dc);
    DP_ASSERT(dc->pool);
    int *indexes = dc->pool;
    if (out_count) {
        *out_count = indexes[0];
    }
    return indexes + 1;
}

ZSTD_DCtx **DP_draw_context_zstd_dctx(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return &dc->zstd_dctx;
}

#ifdef DP_LIBSWSCALE
struct SwsContext *DP_draw_context_sws_context(DP_DrawContext *dc,
                                               int src_width, int src_height,
                                               int dst_width, int dst_height,
                                               int flags)
{
    DP_ASSERT(dc);
    return dc->sws_context = sws_getCachedContext(
               dc->sws_context, src_width, src_height, AV_PIX_FMT_BGRA,
               dst_width, dst_height, AV_PIX_FMT_BGRA, flags, NULL, NULL, NULL);
}
#endif
