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
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;


#define DP_TILE_SIZE             64
#define DP_TILE_LENGTH           (DP_TILE_SIZE * DP_TILE_SIZE)
#define DP_TILE_BYTES            (DP_TILE_LENGTH * sizeof(DP_Pixel15))
#define DP_TILE_COMPRESSED_BYTES (DP_TILE_LENGTH * sizeof(DP_Pixel8))

typedef struct DP_TileCounts {
    int x, y;
} DP_TileCounts;

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
#ifdef __cplusplus
    return DP_TileCounts{DP_tile_count_round(width),
                         DP_tile_count_round(height)};
#else
    return (DP_TileCounts){DP_tile_count_round(width),
                           DP_tile_count_round(height)};
#endif
}

DP_INLINE int DP_tile_total_round(int width, int height)
{
    DP_TileCounts tile_counts = DP_tile_counts_round(width, height);
    return tile_counts.x * tile_counts.y;
}


const uint16_t *DP_tile_opaque_mask(void);


DP_Tile *DP_tile_new(unsigned int context_id);

DP_Tile *DP_tile_new_from_pixel15(unsigned int context_id, DP_Pixel15 pixel);

DP_Tile *DP_tile_new_from_upixel15(unsigned int context_id, DP_UPixel15 pixel);

DP_Tile *DP_tile_new_from_pixels8(unsigned int context_id,
                                  const DP_Pixel8 *pixels);

DP_Tile *DP_tile_new_from_bgra(unsigned int context_id, uint32_t bgra);

DP_Tile *DP_tile_new_from_compressed(DP_DrawContext *dc,
                                     unsigned int context_id,
                                     const unsigned char *image,
                                     size_t image_size);

DP_Tile *DP_tile_new_checker(unsigned int context_id, DP_Pixel15 pixel1,
                             DP_Pixel15 pixel2);

DP_Tile *DP_tile_new_zebra(unsigned int context_id, DP_Pixel15 pixel1,
                           DP_Pixel15 pixel2);

DP_Tile *DP_tile_censored_inc(void);

DP_Tile *DP_tile_incref(DP_Tile *tile);

DP_Tile *DP_tile_incref_nullable(DP_Tile *tile_or_null);

DP_Tile *DP_tile_incref_by(DP_Tile *tile, int refcount);

DP_Tile *DP_tile_incref_by_nullable(DP_Tile *tile_or_null, int refcount);

void DP_tile_decref(DP_Tile *tile);

void DP_tile_decref_nullable(DP_Tile *tile_or_null);

int DP_tile_refcount(DP_Tile *tile);

bool DP_tile_transient(DP_Tile *tile);


unsigned int DP_tile_context_id(DP_Tile *tile);

DP_Pixel15 *DP_tile_pixels(DP_Tile *tile);

DP_Pixel15 DP_tile_pixel_at(DP_Tile *tile, int x, int y);

bool DP_tile_blank(DP_Tile *tile);

bool DP_tile_opaque(DP_Tile *tile_or_null);

bool DP_tile_same_pixel(DP_Tile *tile_or_null, DP_Pixel15 *out_pixel);


size_t DP_tile_compress(DP_Tile *tile, DP_Pixel8 *pixel_buffer,
                        unsigned char *(*get_output_buffer)(size_t, void *),
                        void *user);


void DP_tile_copy_to_image(DP_Tile *tile_or_null, DP_Image *img, int x, int y);


void DP_tile_sample(DP_Tile *tile_or_null, const uint16_t *mask, int x, int y,
                    int width, int height, int skip, float *in_out_weight,
                    float *in_out_red, float *in_out_green, float *in_out_blue,
                    float *in_out_alpha);


DP_TransientTile *DP_transient_tile_new(DP_Tile *tile, unsigned int context_id);

DP_TransientTile *DP_transient_tile_new_blank(unsigned int context_id);

DP_TransientTile *DP_transient_tile_new_nullable(DP_Tile *tile_or_null,
                                                 unsigned int context_id);

DP_TransientTile *DP_transient_tile_incref(DP_TransientTile *tt);

DP_TransientTile *
DP_transient_tile_incref_nullable(DP_TransientTile *tt_or_null);

void DP_transient_tile_decref(DP_TransientTile *tt);

void DP_transient_tile_decref_nullable(DP_TransientTile *tt_or_null);

int DP_transient_tile_refcount(DP_TransientTile *tt);

DP_Tile *DP_transient_tile_persist(DP_TransientTile *tt);


unsigned int DP_transient_tile_context_id(DP_Tile *tt);

DP_Pixel15 *DP_transient_tile_pixels(DP_TransientTile *tt);

DP_Pixel15 DP_transient_tile_pixel_at(DP_TransientTile *tt, int x, int y);

void DP_transient_tile_pixel_at_set(DP_TransientTile *tt, int x, int y,
                                    DP_Pixel15 pixel);

void DP_transient_tile_pixel_at_put(DP_TransientTile *tt, int blend_mode, int x,
                                    int y, DP_Pixel15 pixel);

bool DP_transient_tile_blank(DP_TransientTile *tt);

void DP_transient_tile_merge(DP_TransientTile *DP_RESTRICT tt,
                             DP_Tile *DP_RESTRICT t, uint16_t opacity,
                             int blend_mode);

DP_TransientTile *
DP_transient_tile_merge_nullable(DP_TransientTile *DP_RESTRICT tt_or_null,
                                 DP_Tile *DP_RESTRICT t, uint16_t opacity,
                                 int blend_mode);

void DP_transient_tile_brush_apply(DP_TransientTile *tt, DP_UPixel15 src,
                                   int blend_mode, const uint16_t *mask,
                                   uint16_t opacity, int x, int y, int w, int h,
                                   int skip);

void DP_transient_tile_brush_apply_posterize(DP_TransientTile *tt,
                                             int posterize_num,
                                             const uint16_t *mask,
                                             uint16_t opacity, int x, int y,
                                             int w, int h, int skip);


#endif
