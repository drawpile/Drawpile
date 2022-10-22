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
#include <dpcommon/conversions.h>


// We know how many index lists we need at most, since our operations never
// need to dynamically modify an arbitrary amount of layer ids. Currently this
// is 3: one list as a running buffer, two for layers to be modified. Most layer
// operations only need one list of indexes, merging layers needs two.
#define LAYER_INDEXES_COUNT 3

struct DP_DrawContext {
    // Brush stamps, transformations, decompression and layer id generation
    // are used by distinct operations, so their buffers can share memory.
    union {
        // Brush stamp masks. Pixel brushes use only stamp buffer 1, classic
        // brushes use 1 and 2, MyPaint brushes use 1 and the RR mask buffer.
        struct {
            DP_BrushStampBuffer stamp_buffer1;
            union {
                DP_BrushStampBuffer stamp_buffer2;
                DP_RrMaskBuffer rr_mask_buffer;
            };
        };
        // Pixel buffer for image transformation. Used by region move transform.
        DP_Pixel8 transform_buffer[DP_DRAW_CONTEXT_TRANSFORM_BUFFER_SIZE];
        // Buffer to inflate compressed 8 bit tiles into.
        DP_Pixel8 tile_decompression_buffer[DP_TILE_LENGTH];
        // Layer id generation, masking off already used ids.
        struct {
            bool ids_used[DP_DRAW_CONTEXT_ID_COUNT];
            int last_used_id;
        };
    };
    // Resizable memory pool used by qgrayraster during region move transform
    // and to collect layers into when reordering them. The pool is allocated
    // through malloc, so it's always going to be maximally aligned.
    size_t pool_size;
    void *pool;
    // Lists of indexes, used when searching for layers in the stack.
    // The capacity for all lists is the same so that we can use offsets
    // into a single array of integers that gets resized as needed.
    int layer_indexes_capacity;
    int layer_indexes_used[LAYER_INDEXES_COUNT];
    int *layer_indexes;
};


DP_DrawContext *DP_draw_context_new(void)
{
    DP_DrawContext *dc = DP_malloc(sizeof(*dc));
    dc->pool_size = DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE;
    dc->pool = DP_malloc(DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE);
    dc->layer_indexes_capacity = 0;
    for (int i = 0; i < LAYER_INDEXES_COUNT; ++i) {
        dc->layer_indexes_used[i] = 0;
    }
    dc->layer_indexes = NULL;
    return dc;
}

void DP_draw_context_free(DP_DrawContext *dc)
{
    if (dc) {
        DP_free(dc->layer_indexes);
        DP_free(dc->pool);
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

float *DP_draw_context_rr_mask_buffer(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    return dc->rr_mask_buffer;
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


unsigned char *DP_draw_context_raster_pool(DP_DrawContext *dc, size_t *out_size)
{
    DP_ASSERT(dc);
    DP_ASSERT(out_size);
    *out_size = dc->pool_size;
    return dc->pool;
}

unsigned char *DP_draw_context_raster_pool_resize(DP_DrawContext *dc,
                                                  size_t new_size)
{
    DP_ASSERT(dc);
    DP_ASSERT(new_size > dc->pool_size);
    DP_ASSERT(new_size < DP_DRAW_CONTEXT_RASTER_POOL_MAX_SIZE);
    DP_free(dc->pool);
    void *new_raster_pool = DP_malloc(new_size);
    dc->pool = new_raster_pool;
    dc->pool_size = new_size;
    return new_raster_pool;
}

struct DP_LayerPoolEntry *DP_draw_context_layer_pool(DP_DrawContext *dc,
                                                     int *out_capacity)
{
    DP_ASSERT(dc);
    struct DP_LayerPoolEntry *layer_pool = dc->pool;
    if (out_capacity) {
        *out_capacity = DP_size_to_int(dc->pool_size / sizeof(*layer_pool));
    }
    return layer_pool;
}

struct DP_LayerPoolEntry *DP_draw_context_layer_pool_resize(DP_DrawContext *dc,
                                                            int new_capacity)
{
    DP_ASSERT(dc);
    DP_ASSERT(new_capacity > 0);

    size_t new_size =
        DP_int_to_size(new_capacity) * sizeof(struct DP_LayerPoolEntry);
    DP_ASSERT(new_size > dc->pool_size);

    dc->pool = DP_realloc(dc->pool, new_size);
    dc->pool_size = new_size;
    return dc->pool;
}


void DP_draw_context_layer_indexes_clear(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    dc->layer_indexes_used[0] = 0;
}

void DP_draw_context_layer_indexes_push(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    int used = ++dc->layer_indexes_used[0];
    int old_capacity = dc->layer_indexes_capacity;
    if (used > old_capacity) {
        int new_capacity = DP_max_int(8, old_capacity * 2);
        size_t old_size =
            DP_int_to_size(old_capacity) * sizeof(*dc->layer_indexes);
        size_t new_size =
            DP_int_to_size(new_capacity) * sizeof(*dc->layer_indexes);
        dc->layer_indexes =
            DP_realloc(dc->layer_indexes, new_size * LAYER_INDEXES_COUNT);
        dc->layer_indexes_capacity = new_capacity;
        for (int i = LAYER_INDEXES_COUNT - 1; i > 0; --i) {
            memmove(dc->layer_indexes + i * new_capacity,
                    dc->layer_indexes + i * old_capacity, old_size);
        }
    }
}

void DP_draw_context_layer_indexes_set(DP_DrawContext *dc, int layer_index)
{
    DP_ASSERT(dc);
    DP_ASSERT(layer_index >= 0);
    dc->layer_indexes[dc->layer_indexes_used[0] - 1] = layer_index;
}

void DP_draw_context_layer_indexes_pop(DP_DrawContext *dc)
{
    DP_ASSERT(dc);
    --dc->layer_indexes_used[0];
}

void DP_draw_context_layer_indexes_flush(DP_DrawContext *dc, int list_index)
{
    DP_ASSERT(dc);
    DP_ASSERT(list_index > 0);
    DP_ASSERT(list_index < LAYER_INDEXES_COUNT);
    int used = dc->layer_indexes_used[0];
    memcpy(dc->layer_indexes + dc->layer_indexes_capacity * list_index,
           dc->layer_indexes,
           DP_int_to_size(used) * sizeof(*dc->layer_indexes));
    dc->layer_indexes_used[list_index] = used;
}

int *DP_draw_context_layer_indexes_at(DP_DrawContext *dc, int list_index,
                                      int *out_count)
{
    DP_ASSERT(dc);
    DP_ASSERT(list_index >= 0);
    DP_ASSERT(list_index < LAYER_INDEXES_COUNT);
    if (out_count) {
        *out_count = dc->layer_indexes_used[list_index];
    }
    return dc->layer_indexes + dc->layer_indexes_capacity * list_index;
}
