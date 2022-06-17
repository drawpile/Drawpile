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
#include "tile.h"
#include "compress.h"
#include "image.h"
#include "pixels.h"
#include <dpcommon/atomic.h>
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_Tile {
    DP_Atomic refcount;
    const bool transient;
    const unsigned int context_id;
    DP_Pixel pixels[DP_TILE_LENGTH];
};

struct DP_TransientTile {
    DP_Atomic refcount;
    bool transient;
    unsigned int context_id;
    DP_Pixel pixels[DP_TILE_LENGTH];
};

#else

struct DP_Tile {
    DP_Atomic refcount;
    bool transient;
    unsigned int context_id;
    DP_Pixel pixels[DP_TILE_LENGTH];
};

#endif


static void *alloc_tile(bool transient, unsigned int context_id)
{
    DP_TransientTile *tt = DP_malloc(sizeof(*tt));
    DP_atomic_set(&tt->refcount, 1);
    tt->transient = transient;
    tt->context_id = context_id;
    return tt;
}


DP_Tile *DP_tile_new(unsigned int context_id)
{
    return DP_tile_new_from_bgra(context_id, 0);
}

DP_Tile *DP_tile_new_from_bgra(unsigned int context_id, uint32_t bgra)
{
    DP_TransientTile *tt = alloc_tile(false, context_id);
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        tt->pixels[i].color = bgra;
    }
    return (DP_Tile *)tt;
}


struct DP_TileInflateArgs {
    unsigned int context_id;
    DP_TransientTile *tt;
};

static unsigned char *get_output_buffer(size_t out_size, void *user)
{
    if (out_size == DP_TILE_BYTES) {
        struct DP_TileInflateArgs *args = user;
        DP_TransientTile *tt = alloc_tile(false, args->context_id);
        args->tt = tt;
        return (unsigned char *)tt->pixels;
    }
    else {
        DP_error_set("Tile decompression needs size %zu, but got %zu",
                     (size_t)DP_TILE_BYTES, out_size);
        return NULL;
    }
}

DP_Tile *DP_tile_new_from_compressed(unsigned int context_id,
                                     const unsigned char *image,
                                     size_t image_size)
{
    struct DP_TileInflateArgs args = {context_id, NULL};
    if (DP_compress_inflate(image, image_size, get_output_buffer, &args)) {
#if DP_BYTE_ORDER == DP_LITTLE_ENDIAN
        // Nothing else to do here.
#elif DP_BYTE_ORDER == DP_BIG_ENDIAN
        // Gotta byte-swap the pixels.
        for (int i = 0; i < DP_TILE_LENGTH; ++i) {
            args.tt->pixels[i].color = DP_swap_uint32(args.tt->pixels[i].color);
        }
#else
#    error "Unknown byte order"
#endif
        return (DP_Tile *)args.tt;
    }
    else {
        DP_tile_decref_nullable((DP_Tile *)args.tt);
        return NULL;
    }
}


DP_Tile *DP_tile_incref(DP_Tile *tile)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    DP_atomic_inc(&tile->refcount);
    return tile;
}

DP_Tile *DP_tile_incref_nullable(DP_Tile *tile_or_null)
{
    return tile_or_null ? DP_tile_incref(tile_or_null) : NULL;
}

DP_Tile *DP_tile_incref_by(DP_Tile *tile, int refcount)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    DP_ASSERT(refcount >= 0);
    DP_atomic_add(&tile->refcount, refcount);
    return tile;
}

DP_Tile *DP_tile_incref_by_nullable(DP_Tile *tile_or_null, int refcount)
{
    return tile_or_null ? DP_tile_incref_by(tile_or_null, refcount) : NULL;
}

void DP_tile_decref(DP_Tile *tile)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    if (DP_atomic_dec(&tile->refcount)) {
        DP_free(tile);
    }
}

void DP_tile_decref_nullable(DP_Tile *tile_or_null)
{
    if (tile_or_null) {
        DP_tile_decref(tile_or_null);
    }
}

bool DP_tile_transient(DP_Tile *tile)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    return tile->transient;
}


DP_Pixel *DP_tile_pixels(DP_Tile *tile)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    return tile->pixels;
}

DP_Pixel DP_tile_pixel_at(DP_Tile *tile, int x, int y)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < DP_TILE_SIZE);
    DP_ASSERT(y < DP_TILE_SIZE);
    return tile->pixels[y * DP_TILE_SIZE + x];
}

bool DP_tile_blank(DP_Tile *tile)
{
    DP_Pixel *pixels = DP_tile_pixels(tile);
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        // Colors should be premultiplied.
        if (pixels[i].color != 0) {
            return false;
        }
    }
    return true;
}

bool DP_tile_same_pixel(DP_Tile *tile_or_null, DP_Pixel *out_pixel)
{
    DP_Pixel pixel;
    if (tile_or_null) {
        DP_Pixel *pixels = DP_tile_pixels(tile_or_null);
        pixel = pixels[0];
        for (int i = 1; i < DP_TILE_LENGTH; ++i) {
            if (pixels[i].color != pixel.color) {
                return false;
            }
        }
    }
    else {
        pixel.color = 0;
    }

    if (out_pixel) {
        *out_pixel = pixel;
    }
    return true;
}


void DP_tile_copy_to_image(DP_Tile *tile_or_null, DP_Image *img, int x, int y)
{
    DP_ASSERT(img);
    int img_width = DP_image_width(img);
    int width = DP_min_int(img_width - x, DP_TILE_SIZE);
    int height = DP_min_int(DP_image_height(img) - y, DP_TILE_SIZE);
    DP_Pixel *dst = DP_image_pixels(img) + y * img_width + x;
    size_t bytes = DP_int_to_size(width) * sizeof(*dst);

    if (tile_or_null) {
        DP_ASSERT(DP_atomic_get(&tile_or_null->refcount) > 0);
        DP_Pixel *src = tile_or_null->pixels;
        for (int i = 0; i < height; ++i) {
            memcpy(dst + i * img_width, src + i * DP_TILE_SIZE, bytes);
        }
    }
    else {
        for (int i = 0; i < height; ++i) {
            memset(dst + i * img_width, 0, bytes);
        }
    }
}


static DP_TileWeightedAverage sample_empty(uint8_t *mask, int width, int height,
                                           int skip)
{
    uint32_t sum = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            sum += mask[y * width + x];
        }
        mask += skip;
    }
    return (DP_TileWeightedAverage){sum, 0, 0, 0, 0};
}

DP_TileWeightedAverage DP_tile_weighted_average(DP_Tile *tile_or_null,
                                                uint8_t *mask, int x, int y,
                                                int width, int height, int skip)
{
    if (tile_or_null) {
        DP_TileWeightedAverage twa;
        DP_Pixel *src = tile_or_null->pixels + y * DP_TILE_SIZE + x;
        DP_pixels_sample_mask(src, mask, width, height, skip,
                              DP_TILE_SIZE - width, &twa.weight, &twa.red,
                              &twa.green, &twa.blue, &twa.alpha);
        return twa;
    }
    else {
        return sample_empty(mask, width, height, skip);
    }
}


DP_TransientTile *DP_transient_tile_new(DP_Tile *tile, unsigned int context_id)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    DP_TransientTile *tt = alloc_tile(true, context_id);
    memcpy(tt->pixels, tile->pixels, DP_TILE_BYTES);
    return tt;
}

DP_TransientTile *DP_transient_tile_new_blank(unsigned int context_id)
{
    DP_TransientTile *tt = alloc_tile(true, context_id);
    memset(tt->pixels, 0, DP_TILE_BYTES);
    return tt;
}

DP_TransientTile *DP_transient_tile_new_nullable(DP_Tile *tile_or_null,
                                                 unsigned int context_id)
{
    return tile_or_null ? DP_transient_tile_new(tile_or_null, context_id)
                        : DP_transient_tile_new_blank(context_id);
}

DP_Tile *DP_transient_tile_persist(DP_TransientTile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    tt->transient = false;
    return (DP_Tile *)tt;
}


DP_Pixel DP_transient_tile_pixel_at(DP_TransientTile *tt, int x, int y)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    return DP_tile_pixel_at((DP_Tile *)tt, x, y);
}

void DP_transient_tile_pixel_at_set(DP_TransientTile *tt, int x, int y,
                                    DP_Pixel pixel)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < DP_TILE_SIZE);
    DP_ASSERT(y < DP_TILE_SIZE);
    tt->pixels[y * DP_TILE_SIZE + x] = pixel;
}

void DP_transient_tile_pixel_at_put(DP_TransientTile *tt, int blend_mode, int x,
                                    int y, DP_Pixel pixel)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < DP_TILE_SIZE);
    DP_ASSERT(y < DP_TILE_SIZE);
    DP_pixels_composite(&tt->pixels[y * DP_TILE_SIZE + x], &pixel, 1, 255,
                        blend_mode);
}

void DP_transient_tile_merge(DP_TransientTile *DP_RESTRICT tt,
                             DP_Tile *DP_RESTRICT t, uint8_t opacity,
                             int blend_mode)
{
    DP_ASSERT(tt);
    DP_ASSERT(t);
    DP_pixels_composite(tt->pixels, t->pixels, DP_TILE_LENGTH, opacity,
                        blend_mode);
}

void DP_transient_tile_brush_apply(DP_TransientTile *tt, DP_Pixel src,
                                   int blend_mode, uint8_t *mask, int x, int y,
                                   int w, int h, int skip)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(mask);
    DP_ASSERT(x >= 0);
    DP_ASSERT(x < DP_TILE_SIZE);
    DP_ASSERT(y >= 0);
    DP_ASSERT(y < DP_TILE_SIZE);
    DP_ASSERT(x + w <= DP_TILE_SIZE);
    DP_ASSERT(y + h <= DP_TILE_SIZE);
    DP_pixels_composite_mask(tt->pixels + y * DP_TILE_SIZE + x, src, blend_mode,
                             mask, w, h, skip, DP_TILE_SIZE - w);
}
