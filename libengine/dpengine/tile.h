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
#ifndef DPENGINE_TILE_H
#define DPENGINE_TILE_H
#include <dpcommon/common.h>

typedef struct DP_Image DP_Image;
typedef union DP_Pixel DP_Pixel;


#define DP_TILE_SIZE   64
#define DP_TILE_LENGTH (DP_TILE_SIZE * DP_TILE_SIZE)
#define DP_TILE_BYTES  (DP_TILE_LENGTH * sizeof(uint32_t))

typedef struct DP_TileCounts {
    int x, y;
} DP_TileCounts;

typedef struct DP_TileWeightedAverage {
    uint32_t weight;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t alpha;
} DP_TileWeightedAverage;

#ifdef DP_NO_STRICT_ALIASING

typedef struct DP_Tile DP_Tile;
typedef struct DP_TransientTile DP_TransientTile;

#else

typedef struct DP_Tile DP_Tile;
typedef struct DP_Tile DP_TransientTile;

#endif


DP_INLINE int DP_tile_size_round_up(int i)
{
    return (i + DP_TILE_SIZE - 1) / DP_TILE_SIZE;
}

DP_INLINE int DP_tile_size_round_down(int i)
{
    return (i / DP_TILE_SIZE) * DP_TILE_SIZE;
}

DP_INLINE int DP_tile_count_round(int i)
{
    return i < 0 ? (i - DP_TILE_SIZE + 1) / DP_TILE_SIZE
                 : (i + DP_TILE_SIZE - 1) / DP_TILE_SIZE;
}

DP_INLINE DP_TileCounts DP_tile_counts_round(int width, int height)
{
    return (DP_TileCounts){DP_tile_count_round(width),
                           DP_tile_count_round(height)};
}

DP_INLINE int DP_tile_total_round(int width, int height)
{
    DP_TileCounts tile_counts = DP_tile_counts_round(width, height);
    return tile_counts.x * tile_counts.y;
}


DP_Tile *DP_tile_new(unsigned int context_id);

DP_Tile *DP_tile_new_from_bgra(unsigned int context_id, uint32_t bgra);

DP_Tile *DP_tile_new_from_compressed(unsigned int context_id,
                                     const unsigned char *image,
                                     size_t image_size);

DP_Tile *DP_tile_incref(DP_Tile *tile);

DP_Tile *DP_tile_incref_nullable(DP_Tile *tile_or_null);

DP_Tile *DP_tile_incref_by(DP_Tile *tile, int refcount);

DP_Tile *DP_tile_incref_by_nullable(DP_Tile *tile_or_null, int refcount);

void DP_tile_decref(DP_Tile *tile);

void DP_tile_decref_nullable(DP_Tile *tile_or_null);

bool DP_tile_transient(DP_Tile *tile);


DP_Pixel *DP_tile_pixels(DP_Tile *tile);

DP_Pixel DP_tile_pixel_at(DP_Tile *tile, int x, int y);

bool DP_tile_blank(DP_Tile *tile);


void DP_tile_copy_to_image(DP_Tile *tile_or_null, DP_Image *img, int x, int y);


DP_TileWeightedAverage DP_tile_weighted_average(DP_Tile *tile_or_null,
                                                uint8_t *mask, int x, int y,
                                                int width, int height,
                                                int skip);


DP_TransientTile *DP_transient_tile_new(DP_Tile *tile, unsigned int context_id);

DP_TransientTile *DP_transient_tile_new_blank(unsigned int context_id);

DP_TransientTile *DP_transient_tile_new_nullable(DP_Tile *tile_or_null,
                                                 unsigned int context_id);

DP_Tile *DP_transient_tile_persist(DP_TransientTile *tt);


DP_Pixel DP_transient_tile_pixel_at(DP_TransientTile *tt, int x, int y);

void DP_transient_tile_pixel_at_set(DP_TransientTile *tt, int x, int y,
                                    DP_Pixel pixel);

void DP_transient_tile_pixel_at_put(DP_TransientTile *tt, int blend_mode, int x,
                                    int y, DP_Pixel pixel);

void DP_transient_tile_merge(DP_TransientTile *DP_RESTRICT tt,
                             DP_Tile *DP_RESTRICT t, uint8_t opacity,
                             int blend_mode);

void DP_transient_tile_brush_apply(DP_TransientTile *tt, DP_Pixel src,
                                   int blend_mode, uint8_t *mask, int x, int y,
                                   int w, int h, int skip);


#endif
