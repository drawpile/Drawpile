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
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on libmypaint, using it under the MIT license.
 * See 3rdparty/libmypaint/COPYING for details.
 *
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
#include <dpcommon/cpu.h>
#include <dpcommon/memory_pool.h>
#include <dpcommon/threading.h>
#include <dpmsg/blend_mode.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_Tile {
    DP_ALIGNAS_SIMD DP_Pixel15 pixels[DP_TILE_LENGTH];
    DP_Atomic refcount;
    const bool transient;
    const bool maybe_blank;
    const unsigned int context_id;
};

struct DP_TransientTile {
    DP_ALIGNAS_SIMD DP_Pixel15 pixels[DP_TILE_LENGTH];
    DP_Atomic refcount;
    bool transient;
    bool maybe_blank;
    unsigned int context_id;
};

#else

struct DP_Tile {
    DP_ALIGNAS_SIMD DP_Pixel15 pixels[DP_TILE_LENGTH];
    DP_Atomic refcount;
    bool transient;
    bool maybe_blank;
    unsigned int context_id;
};

#endif


// We want to initialize a static buffer with the same value 4096 times, so this
// is a goofy way to achieve that at compile time without spelling it all out.
#define DP_BIT15_4    DP_BIT15, DP_BIT15, DP_BIT15, DP_BIT15
#define DP_BIT15_16   DP_BIT15_4, DP_BIT15_4, DP_BIT15_4, DP_BIT15_4
#define DP_BIT15_64   DP_BIT15_16, DP_BIT15_16, DP_BIT15_16, DP_BIT15_16
#define DP_BIT15_256  DP_BIT15_64, DP_BIT15_64, DP_BIT15_64, DP_BIT15_64
#define DP_BIT15_1024 DP_BIT15_256, DP_BIT15_256, DP_BIT15_256, DP_BIT15_256
#define DP_BIT15_4096 DP_BIT15_1024, DP_BIT15_1024, DP_BIT15_1024, DP_BIT15_1024

const uint16_t *DP_tile_opaque_mask(void)
{
    static const uint16_t opaque_mask[DP_TILE_LENGTH] = {DP_BIT15_4096};
    return opaque_mask;
}

static DP_MemoryPool tile_memory_pool;
static DP_Mutex *tile_memory_pool_lock = NULL;

static void *alloc_tile(bool transient, bool maybe_blank,
                        unsigned int context_id)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(tile_memory_pool_spinlock);
    if (!tile_memory_pool_lock) {
        DP_atomic_lock(&tile_memory_pool_spinlock);
        if (!tile_memory_pool_lock) {
            tile_memory_pool = DP_memory_pool_new_type(DP_TransientTile, 1024);
            tile_memory_pool_lock = DP_mutex_new();
        }
        DP_atomic_unlock(&tile_memory_pool_spinlock);
    }

    DP_MUTEX_MUST_LOCK(tile_memory_pool_lock);
    DP_TransientTile *tt = DP_memory_pool_alloc_el(&tile_memory_pool);
    DP_MUTEX_MUST_UNLOCK(tile_memory_pool_lock);

    DP_atomic_set(&tt->refcount, 1);
    tt->transient = transient;
    tt->maybe_blank = maybe_blank;
    tt->context_id = context_id;

    return tt;
}


DP_MemoryPoolStatistics DP_tile_memory_usage(void)
{
    if (tile_memory_pool_lock) {
        DP_MUTEX_MUST_LOCK(tile_memory_pool_lock);
        DP_MemoryPoolStatistics mps =
            DP_memory_pool_statistics(&tile_memory_pool);
        DP_MUTEX_MUST_UNLOCK(tile_memory_pool_lock);
        return mps;
    }
    else {
        return (DP_MemoryPoolStatistics){sizeof(DP_TransientTile), 0, 0, 0};
    }
}


DP_Tile *DP_tile_new(unsigned int context_id)
{
    return DP_tile_new_from_bgra(context_id, 0);
}

DP_Tile *DP_tile_new_from_pixel15(unsigned int context_id, DP_Pixel15 pixel)
{
    DP_TransientTile *tt = alloc_tile(false, pixel.a == 0, context_id);
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        tt->pixels[i] = pixel;
    }
    return (DP_Tile *)tt;
}

DP_Tile *DP_tile_new_from_upixel15(unsigned int context_id, DP_UPixel15 pixel)
{
    return DP_tile_new_from_pixel15(context_id, DP_pixel15_premultiply(pixel));
}

DP_Tile *DP_tile_new_from_pixels8(unsigned int context_id,
                                  const DP_Pixel8 *pixels)
{
    DP_TransientTile *tt = alloc_tile(false, true, context_id);
    DP_pixels8_to_15(tt->pixels, pixels, DP_TILE_LENGTH);
    return (DP_Tile *)tt;
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

static unsigned char *get_inflate_output_buffer(size_t out_size, void *user)
{
    if (out_size == DP_TILE_COMPRESSED_BYTES) {
        struct DP_TileInflateArgs *args = user;
        args->tt = alloc_tile(false, true, args->context_id);
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
            DP_draw_context_tile8_buffer(dc),
            context_id,
            NULL,
        };
        if (DP_compress_inflate(image, image_size, get_inflate_output_buffer,
                                &args)) {
            DP_pixels8_to_15_checked(args.tt->pixels, args.buffer,
                                     DP_TILE_LENGTH);
            return (DP_Tile *)args.tt;
        }
        else {
            DP_tile_decref_nullable((DP_Tile *)args.tt);
            return NULL;
        }
    }
}

DP_Tile *DP_tile_new_zebra(unsigned int context_id, DP_Pixel15 pixel1,
                           DP_Pixel15 pixel2)
{
    DP_TransientTile *tt =
        alloc_tile(true, pixel1.a == 0 && pixel2.a == 0, context_id);
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        tt->pixels[i] = ((i / DP_TILE_SIZE + i % DP_TILE_SIZE) / 32) % 2 == 0
                          ? pixel1
                          : pixel2;
    }
    return DP_transient_tile_persist(tt);
}

DP_Tile *DP_tile_censored_noinc(void)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(censor_tile_lock);
    static DP_Tile *censor_tile;
    if (!censor_tile) {
        DP_atomic_lock(&censor_tile_lock);
        if (!censor_tile) {
            censor_tile =
                DP_tile_new_zebra(0, (DP_Pixel15){4497, 4883, 5268, DP_BIT15},
                                  (DP_Pixel15){10151, 10280, 10408, DP_BIT15});
        }
        DP_atomic_unlock(&censor_tile_lock);
    }
    return censor_tile;
}

DP_Tile *DP_tile_censored_inc(void)
{
    return DP_tile_incref(DP_tile_censored_noinc());
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
        DP_MUTEX_MUST_LOCK(tile_memory_pool_lock);
        DP_memory_pool_free_el(&tile_memory_pool, tile);
        DP_MUTEX_MUST_UNLOCK(tile_memory_pool_lock);
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

const DP_Pixel15 *DP_tile_pixels(DP_Tile *tile)
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
    static const DP_Pixel15 blank_pixels[DP_TILE_LENGTH] = {0};
    return memcmp(tile->pixels, blank_pixels, DP_TILE_BYTES) == 0;
}

bool DP_tile_opaque(DP_Tile *tile_or_null)
{
    if (tile_or_null) {
        DP_Pixel15 *pixels = tile_or_null->pixels;
        for (int i = 1; i < DP_TILE_LENGTH; ++i) {
            if (pixels[i].a < DP_BIT15) {
                return false;
            }
        }
        return true;
    }
    else {
        return false;
    }
}

bool DP_tile_same_pixel(DP_Tile *tile_or_null, DP_Pixel15 *out_pixel)
{
    DP_Pixel15 pixel;
    if (tile_or_null) {
        DP_Pixel15 *pixels = tile_or_null->pixels;
        pixel = pixels[0];
        for (int i = 1; i < DP_TILE_LENGTH; ++i) {
            DP_Pixel15 q = pixels[i];
            if (!DP_pixel15_equal(pixel, q)) {
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

bool DP_tile_pixels_equal(DP_Tile *t1, DP_Tile *t2)
{
    DP_ASSERT(t1);
    DP_ASSERT(t2);
    DP_ASSERT(DP_atomic_get(&t1->refcount) > 0);
    DP_ASSERT(DP_atomic_get(&t2->refcount) > 0);
    return memcmp(t1->pixels, t2->pixels, DP_TILE_BYTES) == 0;
}

bool DP_tile_pixels_equal_pixel(DP_Tile *tile, DP_Pixel15 pixel)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    DP_Pixel15 *pixels = tile->pixels;
    for (int i = 0; i < DP_TILE_LENGTH; ++i) {
        if (!DP_pixel15_equal(pixels[i], pixel)) {
            return false;
        }
    }
    return true;
}


size_t DP_tile_compress_pixel(DP_Pixel15 pixel,
                              unsigned char *(*get_output_buffer)(size_t,
                                                                  void *),
                              void *user)
{
    unsigned char *buffer = get_output_buffer(4, user);
    if (buffer) {
        uint32_t color =
            DP_upixel15_to_8(DP_pixel15_unpremultiply(pixel)).color;
        DP_write_bigendian_uint32(color, buffer);
        return 4;
    }
    else {
        return 0; // The function should have already set the error message.
    }
}

size_t DP_tile_compress(DP_Tile *tile, DP_Pixel8 *pixel_buffer,
                        unsigned char *(*get_output_buffer)(size_t, void *),
                        void *user)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    DP_ASSERT(pixel_buffer);
    DP_Pixel15 pixel;
    if (DP_tile_same_pixel(tile, &pixel)) {
        return DP_tile_compress_pixel(pixel, get_output_buffer, user);
    }
    else {
        DP_pixels15_to_8(pixel_buffer, tile->pixels, DP_TILE_LENGTH);
        return DP_compress_deflate((const unsigned char *)pixel_buffer,
                                   DP_TILE_COMPRESSED_BYTES, get_output_buffer,
                                   user);
    }
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

void DP_tile_copy_to_pixels8(DP_Tile *tile_or_null, DP_Pixel8 *pixels, int x,
                             int y, int pixels_width, int pixels_height)
{
    DP_ASSERT(pixels);
    int width = DP_min_int(pixels_width - x, DP_TILE_SIZE);
    int height = DP_min_int(pixels_height - y, DP_TILE_SIZE);
    DP_Pixel8 *dst = pixels + y * pixels_width + x;
    size_t bytes = DP_int_to_size(width) * sizeof(*dst);

    if (tile_or_null) {
        DP_ASSERT(DP_atomic_get(&tile_or_null->refcount) > 0);
        DP_Pixel15 *src = tile_or_null->pixels;
        for (int i = 0; i < height; ++i) {
            DP_pixels15_to_8(dst + i * pixels_width, src + i * DP_TILE_SIZE,
                             width);
        }
    }
    else {
        for (int i = 0; i < height; ++i) {
            memset(dst + i * pixels_width, 0, bytes);
        }
    }
}

void DP_tile_copy_to_upixels8(DP_Tile *tile_or_null, DP_UPixel8 *pixels, int x,
                              int y, int pixels_width, int pixels_height)
{
    DP_ASSERT(pixels);
    int width = DP_min_int(pixels_width - x, DP_TILE_SIZE);
    int height = DP_min_int(pixels_height - y, DP_TILE_SIZE);
    DP_UPixel8 *dst = pixels + y * pixels_width + x;
    size_t bytes = DP_int_to_size(width) * sizeof(*dst);

    if (tile_or_null) {
        DP_ASSERT(DP_atomic_get(&tile_or_null->refcount) > 0);
        DP_Pixel15 *src = tile_or_null->pixels;
        for (int i = 0; i < height; ++i) {
            DP_pixels15_to_8_unpremultiply(dst + i * pixels_width,
                                           src + i * DP_TILE_SIZE, width);
        }
    }
    else {
        for (int i = 0; i < height; ++i) {
            memset(dst + i * pixels_width, 0, bytes);
        }
    }
}


// Based on libmypaint, see license above.
static void sample_tile(DP_Pixel15 *src, const uint16_t *mask, int w, int h,
                        int mask_skip, int base_skip, bool opaque,
                        float *in_out_weight, float *in_out_red,
                        float *in_out_green, float *in_out_blue,
                        float *in_out_alpha)
{
    // The sum of values from a single tile fit into a 32 bit integer,
    // so we use those and only convert to a float once at the end.
    uint_fast32_t weight = 0;
    uint_fast32_t red = 0;
    uint_fast32_t green = 0;
    uint_fast32_t blue = 0;
    uint_fast32_t alpha = 0;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x, ++mask, ++src) {
            uint_fast32_t m = *mask;
            DP_Pixel15 p = *src;
            // When working in opaque mode, disregard low alpha values because
            // the resulting unpremultiplied colors are just too inacurrate.
            if (!opaque || (m > 512 && p.a > 512)) {
                weight += m;
                red += m * p.r / (uint_fast32_t)DP_BIT15;
                green += m * p.g / (uint_fast32_t)DP_BIT15;
                blue += m * p.b / (uint_fast32_t)DP_BIT15;
                alpha += m * p.a / (uint_fast32_t)DP_BIT15;
            }
        }
        src += base_skip;
        mask += mask_skip;
    }

    *in_out_weight += (float)weight;
    *in_out_red += (float)red;
    *in_out_green += (float)green;
    *in_out_blue += (float)blue;
    *in_out_alpha += (float)alpha;
}

static void sample_blank(const uint16_t *mask, int w, int h, int mask_skip,
                         float *in_out_weight)
{
    uint_fast32_t weight = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x, ++mask) {
            weight += *mask;
        }
        mask += mask_skip;
    }
    *in_out_weight += (float)weight;
}

void DP_tile_sample(DP_Tile *tile_or_null, const uint16_t *mask, int x, int y,
                    int width, int height, int skip, bool opaque,
                    float *in_out_weight, float *in_out_red,
                    float *in_out_green, float *in_out_blue,
                    float *in_out_alpha)
{
    if (tile_or_null) {
        DP_Pixel15 *src = tile_or_null->pixels + y * DP_TILE_SIZE + x;
        sample_tile(src, mask, width, height, skip, DP_TILE_SIZE - width,
                    opaque, in_out_weight, in_out_red, in_out_green,
                    in_out_blue, in_out_alpha);
    }
    else if (!opaque) {
        sample_blank(mask, width, height, skip, in_out_weight);
    }
}


DP_TransientTile *DP_transient_tile_new(DP_Tile *tile, unsigned int context_id)
{
    DP_ASSERT(tile);
    DP_ASSERT(DP_atomic_get(&tile->refcount) > 0);
    DP_TransientTile *tt = alloc_tile(true, tile->maybe_blank, context_id);
    memcpy(tt->pixels, tile->pixels, DP_TILE_BYTES);
    return tt;
}

DP_TransientTile *DP_transient_tile_new_transient(DP_TransientTile *tt,
                                                  unsigned int context_id)
{
    return DP_transient_tile_new((DP_Tile *)tt, context_id);
}

DP_TransientTile *DP_transient_tile_new_blank(unsigned int context_id)
{
    DP_TransientTile *tt = alloc_tile(true, true, context_id);
    memset(tt->pixels, 0, sizeof(tt->pixels));

    return tt;
}

DP_TransientTile *DP_transient_tile_new_nullable(DP_Tile *tile_or_null,
                                                 unsigned int context_id)
{
    return tile_or_null ? DP_transient_tile_new(tile_or_null, context_id)
                        : DP_transient_tile_new_blank(context_id);
}

DP_TransientTile *DP_transient_tile_new_checker(unsigned int context_id,
                                                DP_Pixel15 pixel1,
                                                DP_Pixel15 pixel2)
{
    DP_TransientTile *tt = alloc_tile(true, false, context_id);
    DP_transient_tile_fill_checker(tt, pixel1, pixel2);
    return tt;
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

DP_Pixel15 *DP_transient_tile_pixels(DP_TransientTile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    tt->maybe_blank = true;
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
    tt->maybe_blank = pixel.a == 0;
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
    if (DP_blend_mode_can_decrease_opacity(blend_mode)) {
        tt->maybe_blank = true;
    }
    DP_blend_pixels(&tt->pixels[y * DP_TILE_SIZE + x], &pixel, 1, DP_BIT15,
                    blend_mode);
}

void DP_transient_tile_clear(DP_TransientTile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    memset(tt->pixels, 0, DP_TILE_BYTES);
    tt->maybe_blank = true;
}

void DP_transient_tile_fill_checker(DP_TransientTile *tt, DP_Pixel15 pixel1,
                                    DP_Pixel15 pixel2)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    int half = DP_TILE_SIZE / 2;
    for (int y = 0; y < half; ++y) {
        for (int x = 0; x < half; ++x) {
            DP_transient_tile_pixel_at_set(tt, x, y, pixel1);
            DP_transient_tile_pixel_at_set(tt, x + half, y, pixel2);
            DP_transient_tile_pixel_at_set(tt, x, y + half, pixel2);
            DP_transient_tile_pixel_at_set(tt, x + half, y + half, pixel1);
        }
    }
    tt->maybe_blank = pixel1.a == 0 && pixel2.a == 0;
}

void DP_transient_tile_copy(DP_TransientTile *tt, DP_Tile *t)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_ASSERT(t);
    DP_ASSERT(DP_atomic_get(&t->refcount) > 0);
    memcpy(tt->pixels, t->pixels, DP_TILE_BYTES);
    tt->maybe_blank = t->maybe_blank;
}

bool DP_transient_tile_blank(DP_TransientTile *tt)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    if (tt->maybe_blank) {
        bool blank = DP_tile_blank((DP_Tile *)tt);
        if (!blank) {
            tt->maybe_blank = false;
        }
        return blank;
    }
    else {
#ifndef NDEBUG
        if (DP_tile_blank((DP_Tile *)tt)) {
            DP_warn("Tile maybe_blank is false, but is actually blank");
        }
#endif
        return false;
    }
}

void DP_transient_tile_merge(DP_TransientTile *DP_RESTRICT tt,
                             DP_Tile *DP_RESTRICT t, uint16_t opacity,
                             int blend_mode)
{
    DP_ASSERT(tt);
    DP_ASSERT(t);
    if (DP_blend_mode_can_decrease_opacity(blend_mode)) {
        tt->maybe_blank = true;
    }
    DP_blend_tile(tt->pixels, t->pixels, opacity, blend_mode);
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
    if (DP_blend_mode_can_decrease_opacity(blend_mode)) {
        tt->maybe_blank = true;
    }
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

void DP_transient_tile_tint(DP_TransientTile *tt, DP_UPixel8 tint)
{
    DP_ASSERT(tt);
    DP_ASSERT(DP_atomic_get(&tt->refcount) > 0);
    DP_ASSERT(tt->transient);
    DP_tint_pixels(tt->pixels, DP_TILE_LENGTH, tint);
}
