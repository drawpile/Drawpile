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

typedef struct DP_LayerListEntry DP_LayerListEntry;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_SplitTile8 DP_SplitTile8;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef union DP_Pixel8 DP_Pixel8;


#define DP_DRAW_CONTEXT_TRANSFORM_BUFFER_SIZE 204
#define DP_DRAW_CONTEXT_RASTER_POOL_MIN_SIZE  8192
#define DP_DRAW_CONTEXT_RASTER_POOL_MAX_SIZE  (1024 * 1024)

#define DP_DRAW_CONTEXT_ID_COUNT UINT16_MAX

typedef struct DP_DrawContextStatistics {
    size_t static_bytes;
    size_t pool_bytes;
} DP_DrawContextStatistics;

typedef struct DP_DrawContext DP_DrawContext;


DP_DrawContext *DP_draw_context_new(void);

void DP_draw_context_free(DP_DrawContext *dc);


DP_DrawContextStatistics DP_draw_context_statistics(DP_DrawContext *dc);


// All of the following operations share the same memory, their use can't be
// intermixed within the same operation, they must be used in sequence.

DP_Pixel8 *DP_draw_context_transform_buffer(DP_DrawContext *dc);

DP_Pixel8 *DP_draw_context_tile8_buffer(DP_DrawContext *dc);

DP_SplitTile8 *DP_draw_context_split_tile8_buffer(DP_DrawContext *dc);

void DP_draw_context_id_generator_reset(DP_DrawContext *dc, int last_used_id);

void DP_draw_context_id_generator_mark_used(DP_DrawContext *dc, int id);

int DP_draw_context_id_generator_next(DP_DrawContext *dc);


// The following operations also share a memory pool, same restriction applies.
// It's different memory to the set of operations above though, so mixing those
// above with these here works fine (and is done during transform handling.)

void *DP_draw_context_pool(DP_DrawContext *dc);

void *DP_draw_context_pool_require(DP_DrawContext *dc,
                                   size_t required_capacity);

unsigned char *DP_draw_context_raster_pool(DP_DrawContext *dc,
                                           size_t *out_size);

unsigned char *DP_draw_context_raster_pool_resize(DP_DrawContext *dc,
                                                  size_t new_size);

// Set the layer index list count to zero. Must be called to initialize.
void DP_draw_context_layer_indexes_clear(DP_DrawContext *dc);

// Push only increments the count of the layer index list. Set is used to
// assign a value to the pushed element. Pop decrements the count again.
void DP_draw_context_layer_indexes_push(DP_DrawContext *dc);
int *DP_draw_context_layer_indexes_push_get(DP_DrawContext *dc, int *out_count);
void DP_draw_context_layer_indexes_set(DP_DrawContext *dc, int layer_index);
void DP_draw_context_layer_indexes_pop(DP_DrawContext *dc);

// Get the contents of the layer index list.
int *DP_draw_context_layer_indexes(DP_DrawContext *dc, int *out_count);

ZSTD_DCtx **DP_draw_context_zstd_dctx(DP_DrawContext *dc);

#ifdef DP_LIBSWSCALE
struct SwsContext *DP_draw_context_sws_context(DP_DrawContext *dc,
                                               int src_width, int src_height,
                                               int dst_width, int dst_height,
                                               int flags);
#endif


#endif
