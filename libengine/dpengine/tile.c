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
#include "draw_context.h"
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
    DP_Pixel15 pixels[DP_TILE_LENGTH];
};

struct DP_TransientTile {
    DP_Atomic refcount;
    bool transient;
    unsigned int context_id;
    DP_Pixel15 pixels[DP_TILE_LENGTH];
};

#else

struct DP_Tile {
    DP_Atomic refcount;
    bool transient;
    unsigned int context_id;
    DP_Pixel15 pixels[DP_TILE_LENGTH];
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

DP_Tile *DP_tile_new_from_pixel15(unsigned int context_id, DP_Pixel15 pixel)
{
    DP_TransientTile *tt = alloc_tile(false, context_id);
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        tt->pixels[i] = pixel;
    }
    return (DP_Tile *)tt;
}

DP_Tile *DP_tile_new_from_upixel15(unsigned int context_id, DP_UPixel15 pixel)
{
    return DP_tile_new_from_pixel15(context_id, DP_pixel15_premultiply(pixel));
}

DP_Tile *DP_tile_new_from_bgra(unsigned int context_id, uint32_t bgra)
{
    return DP_tile_new_from_upixel15(context_id, DP_upixel15_from_color(bgra));
}


struct DP_TileInflateArgs {
    DP_Pixel8 *buffer;
    unsigned int context_id;
    DP_TransientTile *tt;
};

static unsigned char *get_output_buffer(size_t out_size, void *user)
{
    if (out_size == DP_TILE_COMPRESSED_BYTES) {
        struct DP_TileInflateArgs *args = user;
        args->tt = alloc_tile(false, args->context_id);
        return (unsigned char *)args->buffer;
    }
    else {
        DP_error_set("Tile decompression needs size %zu, but got %zu",
                     (size_t)DP_TILE_COMPRESSED_BYTES, out_size);
        return NULL;
    }
}

DP_Tile *DP_tile_new_from_compressed(DP_DrawContext *dc,
                                     unsigned int context_id,
                                     const unsigned char *image,
                                     size_t image_size)
{
    if (image_size == 4) {
        uint32_t bgra = DP_read_bigendian_uint32(image);
        return DP_tile_new_from_bgra(context_id, bgra);
    }
    else {
        struct DP_TileInflateArgs args = {
            DP_draw_context_tile_decompression_buffer(dc),
            context_id,
            NULL,
        };
        if (DP_compress_inflate(image, image_size, get_output_buffer, &args)
            && DP_pixels8_to_15_checked(args.tt->pixels, args.buffer,
                                        DP_TILE_LENGTH)) {
            return (DP_Tile *)args.tt;
        }
        else {
            DP_tile_decref_nullable((DP_Tile *)args.tt);
            return NULL;
        }
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

int DP_tile_refcount(DP_Tile *tile)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    return DP_atomic_get(&tile->refcount);
}

bool DP_tile_transient(DP_Tile *tile)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    return tile->transient;
}


unsigned int DP_tile_context_id(DP_Tile *tile)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    return tile->context_id;
}

DP_Pixel15 *DP_tile_pixels(DP_Tile *tile)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    return tile->pixels;
}

DP_Pixel15 DP_tile_pixel_at(DP_Tile *tile, int x, int y)
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
    DP_Pixel15 *pixels = DP_tile_pixels(tile);
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        if (pixels[i].a != 0) {
            return false;
        }
    }
    return true;
}

bool DP_tile_same_pixel(DP_Tile *tile_or_null, DP_Pixel15 *out_pixel)
{
    DP_Pixel15 pixel;
    if (tile_or_null) {
        DP_Pixel15 *pixels = DP_tile_pixels(tile_or_null);
        pixel = pixels[0];
        for (int i = 1; i < DP_TILE_LENGTH; ++i) {
            DP_Pixel15 q = pixels[i];
            if (pixel.b != q.b || pixel.g != q.g || pixel.r != q.r
                || pixel.a != q.a) {
                return false;
            }
        }
    }
    else {
        pixel = DP_pixel15_zero();
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
    DP_Pixel8 *dst = DP_image_pixels(img) + y * img_width + x;
    size_t bytes = DP_int_to_size(width) * sizeof(*dst);

    if (tile_or_null) {
        DP_ASSERT(DP_atomic_get(&tile_or_null->refcount) > 0);
        DP_Pixel15 *src = tile_or_null->pixels;
        for (int i = 0; i < height; ++i) {
            DP_pixels15_to_8(dst + i * img_width, src + i * DP_TILE_SIZE,
                             width);
        }
    }
    else {
        for (int i = 0; i < height; ++i) {
            memset(dst + i * img_width, 0, bytes);
        }
    }
}


static DP_TileWeightedAverage sample_empty(const uint16_t *mask, int width,
                                           int height, int skip)
{
    float sum = 0.0f;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            sum += DP_uint16_to_float(mask[y * width + x]) / (float)DP_BIT15;
        }
        mask += skip;
    }
    return (DP_TileWeightedAverage){sum, 0.0f, 0.0f, 0.0f, 0.0f};
}

DP_TileWeightedAverage DP_tile_weighted_average(DP_Tile *tile_or_null,
                                                const uint16_t *mask, int x,
                                                int y, int width, int height,
                                                int skip)
{
    if (tile_or_null) {
        DP_TileWeightedAverage twa;
        DP_Pixel15 *src = tile_or_null->pixels + y * DP_TILE_SIZE + x;
        DP_sample_mask(src, mask, width, height, skip, DP_TILE_SIZE - width,
                       &twa.weight, &twa.red, &twa.green, &twa.blue,
                       &twa.alpha);
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

DP_TransientTile *DP_transient_tile_incref(DP_TransientTile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    return (DP_TransientTile *)DP_tile_incref((DP_Tile *)tt);
}

DP_TransientTile *
DP_transient_tile_incref_nullable(DP_TransientTile *tt_or_null)
{
    return tt_or_null ? DP_transient_tile_incref(tt_or_null) : NULL;
}

void DP_transient_tile_decref(DP_TransientTile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_tile_decref((DP_Tile *)tt);
}

void DP_transient_tile_decref_nullable(DP_TransientTile *tt_or_null)
{
    if (tt_or_null) {
        DP_transient_tile_decref(tt_or_null);
    }
}

int DP_transient_tile_refcount(DP_TransientTile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    return DP_tile_refcount((DP_Tile *)tt);
}

DP_Tile *DP_transient_tile_persist(DP_TransientTile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    tt->transient = false;
    return (DP_Tile *)tt;
}


unsigned int DP_transient_tile_context_id(DP_Tile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    return tt->context_id;
}

DP_Pixel15 *DP_transient_tile_pixels(DP_Tile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    return tt->pixels;
}

DP_Pixel15 DP_transient_tile_pixel_at(DP_TransientTile *tt, int x, int y)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    return DP_tile_pixel_at((DP_Tile *)tt, x, y);
}

void DP_transient_tile_pixel_at_set(DP_TransientTile *tt, int x, int y,
                                    DP_Pixel15 pixel)
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
                                    int y, DP_Pixel15 pixel)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < DP_TILE_SIZE);
    DP_ASSERT(y < DP_TILE_SIZE);
    DP_blend_pixels(&tt->pixels[y * DP_TILE_SIZE + x], &pixel, 1, DP_BIT15,
                    blend_mode);
}

void DP_transient_tile_merge(DP_TransientTile *DP_RESTRICT tt,
                             DP_Tile *DP_RESTRICT t, uint16_t opacity,
                             int blend_mode)
{
    DP_ASSERT(tt);
    DP_ASSERT(t);
    DP_blend_pixels(tt->pixels, t->pixels, DP_TILE_LENGTH, opacity, blend_mode);
}

DP_TransientTile *
DP_transient_tile_merge_nullable(DP_TransientTile *DP_RESTRICT tt_or_null,
                                 DP_Tile *DP_RESTRICT t, uint16_t opacity,
                                 int blend_mode)
{
    DP_TransientTile *tt =
        tt_or_null ? tt_or_null : DP_transient_tile_new_blank(0);
    DP_transient_tile_merge(tt, t, opacity, blend_mode);
    return tt;
}

void DP_transient_tile_brush_apply(DP_TransientTile *tt, DP_UPixel15 src,
                                   int blend_mode, const uint16_t *mask,
                                   uint16_t opacity, int x, int y, int w, int h,
                                   int skip)
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
    DP_blend_mask(tt->pixels + y * DP_TILE_SIZE + x, src, blend_mode, mask,
                  opacity, w, h, skip, DP_TILE_SIZE - w);
}

void DP_transient_tile_brush_apply_posterize(DP_TransientTile *tt,
                                             int posterize_num,
                                             const uint16_t *mask,
                                             uint16_t opacity, int x, int y,
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
    DP_posterize_mask(tt->pixels + y * DP_TILE_SIZE + x, posterize_num, mask,
                      opacity, w, h, skip, DP_TILE_SIZE - w);
}
