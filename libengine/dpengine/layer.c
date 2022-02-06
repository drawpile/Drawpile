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
#include "layer.h"
#include "blend_mode.h"
#include "canvas_diff.h"
#include "canvas_state.h"
#include "image.h"
#include "layer_list.h"
#include "paint.h"
#include "pixels.h"
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <SDL_atomic.h>


typedef struct DP_LayerTitle {
    SDL_atomic_t refcount;
    size_t length;
    char title[];
} DP_LayerTitle;

#ifdef DP_NO_STRICT_ALIASING

typedef struct DP_LayerData {
    SDL_atomic_t refcount;
    const bool transient;
    const int width, height;
    union {
        DP_Tile *const tile;
    } elements[];
} DP_LayerData;

typedef struct DP_TransientLayerData {
    SDL_atomic_t refcount;
    bool transient;
    int width, height;
    union {
        DP_Tile *tile;
        DP_TransientTile *transient_tile;
    } elements[];
} DP_TransientLayerData;

struct DP_Layer {
    SDL_atomic_t refcount;
    const bool transient;
    const int id;
    const uint8_t opacity;
    int blend_mode;
    const bool hidden;
    const bool censored;
    const bool fixed;
    DP_LayerTitle *const title;
    struct {
        DP_LayerData *data;
    };
    struct {
        DP_LayerList *sublayers;
    };
};

struct DP_TransientLayer {
    SDL_atomic_t refcount;
    bool transient;
    int id;
    uint8_t opacity;
    int blend_mode;
    bool hidden;
    bool censored;
    bool fixed;
    DP_LayerTitle *title;
    union {
        DP_LayerData *data;
        DP_TransientLayerData *transient_data;
    };
    union {
        DP_LayerList *sublayers;
        DP_TransientLayerList *transient_sublayers;
    };
};

#else

typedef struct DP_LayerData {
    SDL_atomic_t refcount;
    bool transient;
    int width, height;
    union {
        DP_Tile *tile;
        DP_TransientTile *transient_tile;
    } elements[];
} DP_LayerData;

typedef struct DP_LayerData DP_TransientLayerData;

struct DP_Layer {
    SDL_atomic_t refcount;
    bool transient;
    int id;
    uint8_t opacity;
    int blend_mode;
    bool hidden;
    bool censored;
    bool fixed;
    DP_LayerTitle *title;
    union {
        DP_LayerData *data;
        DP_TransientLayerData *transient_data;
    };
    union {
        DP_LayerList *sublayers;
        DP_TransientLayerList *transient_sublayers;
    };
};

#endif


static DP_LayerTitle *layer_title_new(const char *title, size_t length)
{
    DP_ASSERT(length < SIZE_MAX);
    DP_LayerTitle *lt = DP_malloc(sizeof(*lt) + length + 1);
    SDL_AtomicSet(&lt->refcount, 1);
    if (length > 0) {
        DP_ASSERT(title);
        memcpy(lt->title, title, length);
    }
    lt->title[length] = '\0';
    lt->length = length;
    return lt;
}

static DP_LayerTitle *layer_title_incref(DP_LayerTitle *lt)
{
    DP_ASSERT(lt);
    DP_ASSERT(SDL_AtomicGet(&lt->refcount) > 0);
    SDL_AtomicIncRef(&lt->refcount);
    return lt;
}

static DP_LayerTitle *layer_title_incref_nullable(DP_LayerTitle *lt)
{
    return lt ? layer_title_incref(lt) : NULL;
}

static void layer_title_decref(DP_LayerTitle *lt)
{
    DP_ASSERT(lt);
    DP_ASSERT(SDL_AtomicGet(&lt->refcount) > 0);
    if (SDL_AtomicDecRef(&lt->refcount)) {
        DP_free(lt);
    }
}

static void layer_title_decref_nullable(DP_LayerTitle *lt)
{
    if (lt) {
        layer_title_decref(lt);
    }
}


static void *alloc_layer_data(int width, int height)
{
    size_t count = DP_int_to_size(DP_tile_total_round(width, height));
    DP_TransientLayerData *tld =
        DP_malloc(DP_FLEX_SIZEOF(DP_TransientLayerData, elements, count));
    SDL_AtomicSet(&tld->refcount, 1);
    tld->transient = true;
    tld->width = width;
    tld->height = height;
    return tld;
}

static DP_LayerData *layer_data_incref(DP_LayerData *ld)
{
    DP_ASSERT(ld);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    SDL_AtomicIncRef(&ld->refcount);
    return ld;
}

static void layer_data_decref(DP_LayerData *ld)
{
    DP_ASSERT(ld);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    if (SDL_AtomicDecRef(&ld->refcount)) {
        int count = DP_tile_total_round(ld->width, ld->height);
        for (int i = 0; i < count; ++i) {
            DP_tile_decref_nullable(ld->elements[i].tile);
        }
        DP_free(ld);
    }
}

static bool diff_tile(void *data, int tile_index)
{
    DP_ASSERT(data);
    DP_ASSERT(tile_index >= 0);
    DP_LayerData *a = ((DP_LayerData **)data)[0];
    DP_LayerData *b = ((DP_LayerData **)data)[1];
    DP_ASSERT(tile_index < DP_tile_total_round(a->width, a->height));
    DP_ASSERT(tile_index < DP_tile_total_round(b->width, b->height));
    return a->elements[tile_index].tile != b->elements[tile_index].tile;
}

static void layer_data_diff(DP_LayerData *ld, DP_LayerData *prev,
                            DP_CanvasDiff *diff)
{
    DP_ASSERT(ld);
    DP_ASSERT(prev);
    DP_ASSERT(diff);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    DP_ASSERT(SDL_AtomicGet(&prev->refcount) > 0);
    DP_ASSERT(ld->width == prev->width);   // Different sizes could be
    DP_ASSERT(ld->height == prev->height); // supported, but aren't yet.
    DP_canvas_diff_check(diff, diff_tile, (DP_LayerData *[]){ld, prev});
}

static bool mark_both(void *data, int tile_index)
{
    DP_ASSERT(data);
    DP_ASSERT(tile_index >= 0);
    DP_LayerData *a = ((DP_LayerData **)data)[0];
    DP_LayerData *b = ((DP_LayerData **)data)[1];
    DP_ASSERT(tile_index < DP_tile_total_round(a->width, a->height));
    DP_ASSERT(tile_index < DP_tile_total_round(b->width, b->height));
    return a->elements[tile_index].tile || b->elements[tile_index].tile;
}

static void layer_data_diff_mark_both(DP_LayerData *ld, DP_LayerData *prev,
                                      DP_CanvasDiff *diff)
{
    DP_ASSERT(ld);
    DP_ASSERT(prev);
    DP_ASSERT(diff);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    DP_ASSERT(SDL_AtomicGet(&prev->refcount) > 0);
    DP_ASSERT(ld->width == prev->width);   // Different sizes could be
    DP_ASSERT(ld->height == prev->height); // supported, but aren't yet.
    DP_canvas_diff_check(diff, mark_both, (DP_LayerData *[]){ld, prev});
}

static bool mark(void *data, int tile_index)
{
    DP_ASSERT(data);
    DP_ASSERT(tile_index >= 0);
    DP_LayerData *ld = data;
    DP_ASSERT(tile_index < DP_tile_total_round(ld->width, ld->height));
    return ld->elements[tile_index].tile;
}

static void layer_data_diff_mark(DP_LayerData *ld, DP_CanvasDiff *diff)
{
    DP_ASSERT(ld);
    DP_ASSERT(diff);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    DP_canvas_diff_check(diff, mark, ld);
}

static bool layer_data_has_content(DP_LayerData *ld)
{
    DP_ASSERT(ld);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    int count = DP_tile_total_round(ld->width, ld->height);
    for (int i = 0; i < count; ++i) {
        DP_Tile *tile = ld->elements[i].tile;
        if (tile && !DP_tile_blank(tile)) {
            return true;
        }
    }
    return false;
}

static DP_Pixel layer_data_pixel_at(DP_LayerData *ld, int x, int y)
{
    DP_ASSERT(ld);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < ld->width);
    DP_ASSERT(y < ld->height);
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int wt = DP_tile_count_round(ld->width);
    DP_Tile *t = ld->elements[yt * wt + xt].tile;
    if (t) {
        return DP_tile_pixel_at(t, x - xt * DP_TILE_SIZE,
                                y - yt * DP_TILE_SIZE);
    }
    else {
        return (DP_Pixel){0};
    }
}


static DP_TransientLayerData *transient_layer_data_new(DP_LayerData *ld)
{
    DP_ASSERT(ld);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    DP_ASSERT(!ld->transient);
    DP_debug("New transient layer data");
    int width = ld->width;
    int height = ld->height;
    DP_TransientLayerData *tld = alloc_layer_data(width, height);
    int count = DP_tile_total_round(width, height);
    for (int i = 0; i < count; ++i) {
        tld->elements[i].tile = DP_tile_incref_nullable(ld->elements[i].tile);
    }
    return tld;
}

static DP_LayerData *transient_layer_data_persist(DP_TransientLayerData *tld)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    tld->transient = false;
    int count = DP_tile_total_round(tld->width, tld->height);
    for (int i = 0; i < count; ++i) {
        DP_Tile *tile = tld->elements[i].tile;
        if (tile && DP_tile_transient(tile)) {
            DP_transient_tile_persist(tld->elements[i].transient_tile);
        }
    }
    return (DP_LayerData *)tld;
}

static void transient_layer_data_init_null_tiles(DP_TransientLayerData *tld)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    int tile_count = DP_tile_total_round(tld->width, tld->height);
    for (int i = 0; i < tile_count; ++i) {
        tld->elements[i].tile = NULL;
    }
}

static DP_TransientTile *create_transient_tile(DP_TransientLayerData *tld,
                                               unsigned int context_id, int i)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < DP_tile_total_round(tld->width, tld->height));
    DP_ASSERT(!tld->elements[i].tile);
    DP_TransientTile *tt = DP_transient_tile_new_blank(context_id);
    tld->elements[i].transient_tile = tt;
    return tt;
}

static DP_TransientTile *get_transient_tile(DP_TransientLayerData *tld,
                                            unsigned int context_id, int i)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < DP_tile_total_round(tld->width, tld->height));
    DP_Tile *tile = tld->elements[i].tile;
    DP_ASSERT(tile);
    if (!DP_tile_transient(tile)) {
        tld->elements[i].transient_tile =
            DP_transient_tile_new(tile, context_id);
        DP_tile_decref(tile);
    }
    return tld->elements[i].transient_tile;
}

static DP_TransientTile *
get_or_create_transient_tile(DP_TransientLayerData *tld,
                             unsigned int context_id, int i)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < DP_tile_total_round(tld->width, tld->height));
    DP_Tile *tile = tld->elements[i].tile;
    if (!tile) {
        tld->elements[i].transient_tile =
            DP_transient_tile_new_blank(context_id);
    }
    else if (!DP_tile_transient(tile)) {
        tld->elements[i].transient_tile =
            DP_transient_tile_new(tile, context_id);
        DP_tile_decref(tile);
    }
    return tld->elements[i].transient_tile;
}

void DP_transient_layer_data_brush_stamp_apply(DP_TransientLayerData *tld,
                                               unsigned int context_id,
                                               DP_Pixel src, int blend_mode,
                                               DP_BrushStamp *stamp)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    DP_ASSERT(stamp);

    int width = tld->width;
    int height = tld->height;
    int top = DP_brush_stamp_top(stamp);
    int left = DP_brush_stamp_left(stamp);
    int d = DP_brush_stamp_diameter(stamp);
    if (left + d <= 0 || top + d <= 0 || left >= width || top >= height) {
        return; // Out of bounds, nothing to do.
    }

    int bottom = DP_min_int(top + d, height);
    int right = DP_min_int(left + d, width);
    int xtiles = DP_tile_count_round(width);
    uint8_t *mask = DP_brush_stamp_data(stamp);

    int y = top < 0 ? 0 : top;
    int yb = top < 0 ? -top : 0;
    int x0 = left < 0 ? 0 : left;
    int xb0 = left < 0 ? -left : 0;
    while (y < bottom) {
        int yindex = y / DP_TILE_SIZE;
        int yt = y - yindex * DP_TILE_SIZE;
        int hb = yt + d - yb < DP_TILE_SIZE ? d - yb : DP_TILE_SIZE - yt;
        int x = x0;
        int xb = xb0;
        while (x < right) {
            int xindex = x / DP_TILE_SIZE;
            int xt = x - xindex * DP_TILE_SIZE;
            int wb = xt + d - xb < DP_TILE_SIZE ? d - xb : DP_TILE_SIZE - xt;
            int i = xtiles * yindex + xindex;

            DP_TransientTile *tt =
                get_or_create_transient_tile(tld, context_id, i);
            DP_transient_tile_brush_apply(tt, src, blend_mode,
                                          mask + yb * d + xb, xt, yt, wb, hb,
                                          d - wb);

            x = (xindex + 1) * DP_TILE_SIZE;
            xb = xb + wb;
        }
        y = (yindex + 1) * DP_TILE_SIZE;
        yb = yb + hb;
    }
}

static void transient_layer_data_pixel_at_put(DP_TransientLayerData *tld,
                                              unsigned int context_id,
                                              int blend_mode, int x, int y,
                                              DP_Pixel pixel)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < tld->width);
    DP_ASSERT(y < tld->height);
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int wt = DP_tile_count_round(tld->width);
    DP_TransientTile *tt =
        get_or_create_transient_tile(tld, context_id, yt * wt + xt);
    DP_transient_tile_pixel_at_put(tt, blend_mode, x - xt * DP_TILE_SIZE,
                                   y - yt * DP_TILE_SIZE, pixel);
}

static void transient_layer_data_put_image(DP_TransientLayerData *tld,
                                           DP_Image *img,
                                           unsigned int context_id,
                                           int blend_mode, int left, int top)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    DP_ASSERT(img);

    int img_width = DP_image_width(img);
    int img_height = DP_image_height(img);
    int img_start_x = left < 0 ? -left : 0;
    int img_start_y = top < 0 ? -top : 0;
    int start_x = DP_max_int(left, 0);
    int start_y = DP_max_int(top, 0);
    int width = DP_min_int(tld->width - start_x, img_width - img_start_x);
    int height = DP_min_int(tld->height - start_y, img_height - img_start_y);
    for (int y = 0; y < height; ++y) {
        int cur_y = start_y + y;
        int img_y = img_start_y + y;
        for (int x = 0; x < width; ++x) {
            DP_Pixel pixel = DP_image_pixel_at(img, img_start_x + x, img_y);
            transient_layer_data_pixel_at_put(tld, context_id, blend_mode,
                                              start_x + x, cur_y, pixel);
        }
    }
}

static void transient_layer_data_fill_rect(DP_TransientLayerData *tld,
                                           unsigned int context_id,
                                           int blend_mode, int left, int top,
                                           int right, int bottom,
                                           DP_Pixel pixel)
{
    uint8_t alpha = blend_mode == DP_BLEND_MODE_REPLACE ? 255 : pixel.a;
    uint8_t mask[DP_TILE_LENGTH];
    memset(mask, alpha, sizeof(mask));
    bool blend_blank = DP_blend_mode_blank_tile_behavior(blend_mode)
                        != DP_BLEND_MODE_BLANK_TILE_SKIP
                    && alpha != 0;

    int tx_min = left / DP_TILE_SIZE;
    int tx_max = (right - 1) / DP_TILE_SIZE;
    int ty_min = top / DP_TILE_SIZE;
    int ty_max = (bottom - 1) / DP_TILE_SIZE;
    int xtiles = DP_tile_counts_round(tld->width, tld->height).x;

    for (int ty = ty_min; ty <= ty_max; ++ty) {
        int cy = ty * DP_TILE_SIZE;
        for (int tx = tx_min; tx <= tx_max; ++tx) {
            int cx = tx * DP_TILE_SIZE;
            int x = DP_max_int(cx, left) - cx;
            int y = DP_max_int(cy, top) - cy;
            int w = DP_min_int((tx + 1) * DP_TILE_SIZE, right) - cx - x;
            int h = DP_min_int((ty + 1) * DP_TILE_SIZE, bottom) - cy - y;
            int i = ty * xtiles + tx;

            DP_TransientTile *tt;
            if (tld->elements[i].tile) {
                tt = get_or_create_transient_tile(tld, context_id, i);
            }
            else if (blend_blank) {
                tt = DP_transient_tile_new_blank(context_id);
                tld->elements[i].transient_tile = tt;
            }
            else {
                continue; // Nothing to do on a blank tile.
            }
            DP_transient_tile_brush_apply(tt, pixel, blend_mode, mask, x, y, w,
                                          h, 0);
        }
    }
}

static void transient_layer_data_fill(DP_TransientLayerData *tld,
                                      unsigned int context_id, int blend_mode,
                                      DP_Pixel pixel)
{
    bool is_replacement =
        blend_mode == DP_BLEND_MODE_REPLACE
        || (blend_mode == DP_BLEND_MODE_NORMAL && pixel.a == 255);
    if (is_replacement) {
        DP_Tile *tile = DP_tile_new_from_bgra(context_id, pixel.color);
        int tile_count = DP_tile_total_round(tld->width, tld->height);
        DP_tile_incref_by(tile, tile_count - 1);
        for (int i = 0; i < tile_count; ++i) {
            DP_tile_decref_nullable(tld->elements[i].tile);
            tld->elements[i].tile = tile;
        }
    }
    else {
        transient_layer_data_fill_rect(tld, context_id, blend_mode, 0, 0,
                                       tld->width, tld->height, pixel);
    }
}


DP_Layer *DP_layer_incref(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    SDL_AtomicIncRef(&l->refcount);
    return l;
}

void DP_layer_decref(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    if (SDL_AtomicDecRef(&l->refcount)) {
        DP_layer_list_decref(l->sublayers);
        layer_data_decref(l->data);
        layer_title_decref_nullable(l->title);
        DP_free(l);
    }
}

int DP_layer_refcount(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return SDL_AtomicGet(&l->refcount);
}

bool DP_layer_transient(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->transient;
}

void DP_layer_diff(DP_Layer *l, DP_Layer *prev, DP_CanvasDiff *diff)
{
    DP_ASSERT(l);
    DP_ASSERT(prev);
    DP_ASSERT(diff);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    DP_ASSERT(SDL_AtomicGet(&prev->refcount) > 0);
    if (l != prev) {
        if (l->opacity != prev->opacity || l->blend_mode != prev->blend_mode
            || l->hidden != prev->hidden || l->censored != prev->censored) {
            layer_data_diff_mark_both(l->data, prev->data, diff);
        }
        else {
            layer_data_diff(l->data, prev->data, diff);
        }
        DP_layer_list_diff(l->sublayers, prev->sublayers, diff);
    }
}

void DP_layer_diff_mark(DP_Layer *l, DP_CanvasDiff *diff)
{
    DP_ASSERT(l);
    DP_ASSERT(diff);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    layer_data_diff_mark(l->data, diff);
    DP_layer_list_diff_mark(l->sublayers, diff);
}


int DP_layer_id(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->id;
}

uint8_t DP_layer_opacity(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->opacity;
}

bool DP_layer_hidden(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->hidden;
}

bool DP_layer_fixed(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->fixed;
}

DP_LayerList *DP_layer_sublayers_noinc(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->sublayers;
}

bool DP_layer_visible(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->opacity > 0 && !l->hidden;
}

int DP_layer_width(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->data->width;
}

int DP_layer_height(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return l->data->height;
}

const char *DP_layer_title(DP_Layer *l, size_t *out_length)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);

    DP_LayerTitle *lt = l->title;
    const char *title;
    size_t length;
    if (lt) {
        DP_ASSERT(SDL_AtomicGet(&lt->refcount) > 0);
        title = lt->title;
        length = lt->length;
    }
    else {
        title = NULL;
        length = 0;
    }

    if (out_length) {
        *out_length = length;
    }
    return title;
}


DP_Tile *DP_layer_tile_at(DP_Layer *l, int x, int y)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < DP_tile_count_round(l->data->width));
    DP_ASSERT(y < DP_tile_count_round(l->data->height));
    DP_LayerData *ld = l->data;
    int xcount = DP_tile_count_round(ld->width);
    return ld->elements[y * xcount + x].tile;
}


static void transient_layer_data_merge(DP_TransientLayerData *tld,
                                       DP_LayerData *ld,
                                       unsigned int context_id, uint8_t opacity,
                                       int blend_mode)
{
    DP_ASSERT(tld);
    DP_ASSERT(SDL_AtomicGet(&tld->refcount) > 0);
    DP_ASSERT(tld->transient);
    DP_ASSERT(ld);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    DP_ASSERT(tld->width == ld->width);
    DP_ASSERT(tld->height == ld->height);
    int count = DP_tile_total_round(ld->width, ld->height);
    DP_BlendModeBlankTileBehavior on_blank =
        DP_blend_mode_blank_tile_behavior(blend_mode);
    for (int i = 0; i < count; ++i) {
        DP_Tile *t = ld->elements[i].tile;
        if (t) {
            if (tld->elements[i].tile) {
                DP_TransientTile *tt = get_transient_tile(tld, context_id, i);
                DP_ASSERT((void *)tt != (void *)t);
                DP_transient_tile_merge(tt, t, opacity, blend_mode);
            }
            else if (on_blank == DP_BLEND_MODE_BLANK_TILE_BLEND) {
                // For blend modes like normal and behind, do regular blending.
                DP_TransientTile *tt =
                    create_transient_tile(tld, context_id, i);
                DP_transient_tile_merge(tt, t, opacity, blend_mode);
            }
            else {
                // For most other blend modes merging with transparent pixels
                // doesn't do anything. For example erasing nothing or multiply
                // with nothing just leads to more nothing. Skip the empty tile.
                DP_ASSERT(on_blank == DP_BLEND_MODE_BLANK_TILE_SKIP);
            }
        }
    }
}


DP_Layer *DP_layer_merge_to_flat_image(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    DP_LayerList *ll = l->sublayers;
    int count = DP_layer_list_layer_count(ll);
    if (count > 0) {
        DP_TransientLayer *tl = DP_transient_layer_new(l);
        DP_transient_layer_merge_all_sublayers(tl, 0);
        return DP_transient_layer_persist(tl);
    }
    else {
        return DP_layer_incref(l);
    }
}

static DP_Tile *flatten_tile(DP_Layer *l, int tile_index)
{
    DP_LayerData *ld = l->data;
    DP_ASSERT(ld);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(ld->width, ld->height));
    DP_Tile *t = ld->elements[tile_index].tile;
    DP_LayerList *ll = l->sublayers;
    if (DP_layer_list_layer_count(ll) == 0) {
        return DP_tile_incref_nullable(t);
    }
    else {
        DP_TransientTile *tt = DP_transient_tile_new_nullable(t, 0);
        DP_layer_list_flatten_tile_to(l->sublayers, tile_index, tt);
        return DP_transient_tile_persist(tt);
    }
}

void DP_layer_flatten_tile_to(DP_Layer *l, int tile_index, DP_TransientTile *tt)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    DP_Tile *t = flatten_tile(l, tile_index);
    if (t) {
        DP_transient_tile_merge(tt, t, l->opacity, l->blend_mode);
        DP_tile_decref(t);
    }
}

static DP_Image *layer_data_to_image(DP_LayerData *ld)
{
    DP_ASSERT(ld);
    DP_ASSERT(SDL_AtomicGet(&ld->refcount) > 0);

    int width = ld->width;
    int height = ld->height;
    if (width <= 0 || height <= 0) {
        DP_error_set("Can't create image from a layer with zero pixels");
        return NULL;
    }

    DP_Image *img = DP_image_new(width, height);
    DP_TileCounts tile_counts = DP_tile_counts_round(width, height);
    DP_debug("Layer to image %dx%d tiles", tile_counts.x, tile_counts.y);
    for (int y = 0; y < tile_counts.y; ++y) {
        for (int x = 0; x < tile_counts.x; ++x) {
            DP_tile_copy_to_image(ld->elements[y * tile_counts.x + x].tile, img,
                                  x * DP_TILE_SIZE, y * DP_TILE_SIZE);
        }
    }
    return img;
}

DP_Image *DP_layer_to_image(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    return layer_data_to_image(l->data);
}


DP_TransientLayer *DP_transient_layer_new(DP_Layer *l)
{
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    DP_ASSERT(!l->transient);
    DP_debug("New transient layer %d", l->id);
    DP_TransientLayer *tl = DP_malloc(sizeof(*tl));
    *tl = (DP_TransientLayer){
        {-1},
        true,
        l->id,
        l->opacity,
        l->blend_mode,
        l->hidden,
        l->censored,
        l->fixed,
        layer_title_incref_nullable(l->title),
        {.data = layer_data_incref(l->data)},
        {.sublayers = DP_layer_list_incref(l->sublayers)},
    };
    SDL_AtomicSet(&tl->refcount, 1);
    return tl;
}

DP_TransientLayer *DP_transient_layer_new_init(int id, int width, int height,
                                               DP_Tile *tile)
{
    DP_ASSERT(width >= 0);
    DP_ASSERT(height >= 0);

    DP_TransientLayerData *tld = alloc_layer_data(width, height);
    int tile_count = DP_tile_total_round(width, height);
    DP_tile_incref_by_nullable(tile, tile_count);
    for (int i = 0; i < tile_count; ++i) {
        tld->elements[i].tile = tile;
    }

    DP_TransientLayer *tl = DP_malloc(sizeof(*tl));
    *tl = (DP_TransientLayer){
        {-1},
        true,
        id,
        255,
        DP_BLEND_MODE_NORMAL,
        false,
        false,
        false,
        NULL,
        {.transient_data = tld},
        {.transient_sublayers = DP_transient_layer_list_new_init()},
    };
    SDL_AtomicSet(&tl->refcount, 1);
    return tl;
}

DP_TransientLayer *DP_transient_layer_incref(DP_TransientLayer *tl)
{
    return (DP_TransientLayer *)DP_layer_incref((DP_Layer *)tl);
}

void DP_transient_layer_decref(DP_TransientLayer *tl)
{
    DP_layer_decref((DP_Layer *)tl);
}

int DP_transient_layer_refcount(DP_TransientLayer *tl)
{
    return DP_layer_refcount((DP_Layer *)tl);
}

DP_Layer *DP_transient_layer_persist(DP_TransientLayer *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    tl->transient = false;
    if (tl->data->transient) {
        transient_layer_data_persist(tl->transient_data);
    }
    if (DP_layer_list_transient(tl->sublayers)) {
        DP_transient_layer_list_persist(tl->transient_sublayers);
    }
    return (DP_Layer *)tl;
}


int DP_transient_layer_id(DP_TransientLayer *tl)
{
    return DP_layer_id((DP_Layer *)tl);
}

uint8_t DP_transient_layer_opacity(DP_TransientLayer *tl)
{
    return DP_layer_opacity((DP_Layer *)tl);
}

bool DP_transient_layer_hidden(DP_TransientLayer *tl)
{
    return DP_layer_hidden((DP_Layer *)tl);
}

bool DP_transient_layer_fixed(DP_TransientLayer *tl)
{
    return DP_layer_fixed((DP_Layer *)tl);
}

DP_LayerList *DP_transient_layer_sublayers(DP_TransientLayer *tl)
{
    return DP_layer_sublayers_noinc((DP_Layer *)tl);
}

bool DP_transient_layer_visible(DP_TransientLayer *tl)
{
    return DP_layer_visible((DP_Layer *)tl);
}

int DP_transient_layer_width(DP_TransientLayer *tl)
{
    return DP_layer_width((DP_Layer *)tl);
}

int DP_transient_layer_height(DP_TransientLayer *tl)
{
    return DP_layer_height((DP_Layer *)tl);
}


void DP_transient_layer_id_set(DP_TransientLayer *tl, int layer_id)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    tl->id = layer_id;
}

void DP_transient_layer_title_set(DP_TransientLayer *tl, const char *title,
                                  size_t length)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    layer_title_decref_nullable(tl->title);
    tl->title = layer_title_new(title, length);
}

void DP_transient_layer_opacity_set(DP_TransientLayer *tl, uint8_t opacity)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    tl->opacity = opacity;
}

void DP_transient_layer_blend_mode_set(DP_TransientLayer *tl, int blend_mode)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    tl->blend_mode = blend_mode;
}

void DP_transient_layer_censored_set(DP_TransientLayer *tl, bool censored)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    tl->censored = censored;
}

void DP_transient_layer_hidden_set(DP_TransientLayer *tl, bool hidden)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    tl->hidden = hidden;
}

void DP_transient_layer_fixed_set(DP_TransientLayer *tl, bool fixed)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    tl->fixed = fixed;
}


static DP_TransientLayerList *get_transient_sublayers(DP_TransientLayer *tl,
                                                      int reserve)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_ASSERT(reserve >= 0);
    DP_LayerList *ll = tl->sublayers;
    if (!DP_layer_list_transient(ll)) {
        tl->transient_sublayers = DP_transient_layer_list_new(ll, reserve);
        DP_layer_list_decref(ll);
    }
    else if (reserve > 0) {
        tl->transient_sublayers =
            DP_transient_layer_list_reserve(tl->transient_sublayers, reserve);
    }
    return tl->transient_sublayers;
}

DP_TransientLayer *
DP_transient_layer_transient_sublayer_create(DP_TransientLayer *tl,
                                             int sublayer_id)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_ASSERT(sublayer_id != 0);
    DP_ASSERT(DP_layer_list_layer_index_by_id(tl->sublayers, sublayer_id) < 0);
    DP_TransientLayerList *tll = get_transient_sublayers(tl, 1);
    DP_LayerData *ld = tl->data;
    // TODO: this function does a bunch of redundant checks. Replace it.
    return DP_transient_layer_list_layer_create(tll, sublayer_id, -1, NULL,
                                                true, false, ld->width,
                                                ld->height, NULL, 0);
}

DP_TransientLayer *
DP_transient_layer_transient_sublayer_at(DP_TransientLayer *tl, int index)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_TransientLayerList *tll = get_transient_sublayers(tl, 0);
    return DP_transient_layer_list_transient_at(tll, index);
}

void DP_transient_layer_merge_all_sublayers(DP_TransientLayer *tl,
                                            unsigned int context_id)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_LayerList *ll = tl->sublayers;
    int count = DP_layer_list_layer_count(ll);
    for (int i = 0; i < count; ++i) {
        DP_Layer *sl = DP_layer_list_at_noinc(ll, i);
        DP_transient_layer_merge(tl, sl, context_id);
    }
    DP_layer_list_decref(ll);
    tl->sublayers = DP_layer_list_new();
}

void DP_transient_layer_merge_sublayer_at(DP_TransientLayer *tl,
                                          unsigned int context_id, int index)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_TransientLayerList *tll = get_transient_sublayers(tl, 0);
    DP_Layer *sl = DP_transient_layer_list_at(tll, index);
    DP_transient_layer_merge(tl, sl, context_id);
    DP_transient_layer_list_remove_at(tll, index);
}


static DP_TransientLayerData *get_transient_layer_data(DP_TransientLayer *tl)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_LayerData *ld = tl->data;
    if (!ld->transient) {
        tl->transient_data = transient_layer_data_new(ld);
        layer_data_decref(ld);
    }
    return tl->transient_data;
}


void DP_transient_layer_merge(DP_TransientLayer *tl, DP_Layer *l,
                              unsigned int context_id)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_ASSERT(l);
    DP_ASSERT(SDL_AtomicGet(&l->refcount) > 0);
    DP_debug("Merge layer %d into transient layer %d using blend mode %s",
             l->id, tl->id, DP_blend_mode_enum_name(l->blend_mode));
    DP_TransientLayerData *tld = get_transient_layer_data(tl);
    DP_LayerData *ld = l->data;
    transient_layer_data_merge(tld, ld, context_id, l->opacity, l->blend_mode);
}


static void resize_sublayers(DP_TransientLayer *tl, unsigned int context_id,
                             int top, int right, int bottom, int left)
{
    int count = DP_layer_list_layer_count(tl->sublayers);
    if (count > 0) {
        DP_TransientLayerList *tll = get_transient_sublayers(tl, 0);
        for (int i = 0; i < count; ++i) {
            DP_transient_layer_resize(
                DP_transient_layer_list_transient_at(tll, i), context_id, top,
                right, bottom, left);
        }
    }
}

static void resize_layer_content_aligned(DP_LayerData *ld,
                                         DP_TransientLayerData *tld, int top,
                                         int left)
{
    DP_TileCounts new_counts = DP_tile_counts_round(tld->width, tld->height);
    DP_TileCounts old_counts = DP_tile_counts_round(ld->width, ld->height);
    DP_TileCounts offsets = DP_tile_counts_round(-top, -left);
    for (int y = 0; y < new_counts.y; ++y) {
        for (int x = 0; x < new_counts.x; ++x) {
            int old_x = offsets.x + x;
            int old_y = offsets.y + y;
            bool out_of_bounds = old_x < 0 || old_x >= old_counts.x || old_y < 0
                              || old_y >= old_counts.y;
            DP_Tile *tile = out_of_bounds
                              ? NULL
                              : ld->elements[old_y * old_counts.x + old_x].tile;
            tld->elements[y * new_counts.x + x].tile =
                DP_tile_incref_nullable(tile);
        }
    }
}


static void calculate_resize_offsets(int offset, int *out_pos, int *out_crop)
{
    if (offset < 0) {
        *out_pos = 0;
        *out_crop = -offset;
    }
    else {
        *out_pos = offset;
        *out_crop = 0;
    }
}

static void resize_layer_content_copy(DP_LayerData *ld,
                                      DP_TransientLayerData *tld,
                                      unsigned int context_id, int top,
                                      int left)
{
    // TODO: This operation is super expensive and obliterates change
    // information. It could be solved by storing an x and y pixel offset in the
    // layer instead. Need more functionality to test that out though.
    transient_layer_data_init_null_tiles(tld);

    DP_Image *img = layer_data_to_image(ld);
    int width = DP_image_width(img);
    int height = DP_image_height(img);

    int x, y, crop_x, crop_y;
    calculate_resize_offsets(left, &x, &crop_x);
    calculate_resize_offsets(top, &y, &crop_y);
    if (crop_x != 0 || crop_y != 0) {
        if (crop_x < width && crop_y < height) {
            DP_Image *tmp = DP_image_new_subimage(
                img, crop_x, crop_y, width - crop_x, height - crop_y);
            DP_image_free(img);
            img = tmp;
        }
        else {
            DP_image_free(img);
            return; // Nothing to do, resulting image is out of bounds.
        }
    }

    transient_layer_data_put_image(tld, img, context_id, DP_BLEND_MODE_REPLACE,
                                   x, y);
    DP_image_free(img);
}

static void resize_layer_content(DP_LayerData *ld, DP_TransientLayerData *tld,
                                 unsigned int context_id, int top, int left)
{
    if (left % DP_TILE_SIZE == 0 && top % DP_TILE_SIZE == 0) {
        DP_debug("Resize: layer is aligned");
        resize_layer_content_aligned(ld, tld, top, left);
    }
    else {
        DP_debug("Resize: layer must be copied");
        resize_layer_content_copy(ld, tld, context_id, top, left);
    }
}

void DP_transient_layer_resize(DP_TransientLayer *tl, unsigned int context_id,
                               int top, int right, int bottom, int left)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    resize_sublayers(tl, context_id, top, right, bottom, left);

    DP_LayerData *ld = tl->data;
    int width = ld->width + left + right;
    int height = ld->height + top + bottom;

    DP_TransientLayerData *tld = alloc_layer_data(width, height);
    if (layer_data_has_content(ld)) {
        resize_layer_content(ld, tld, context_id, top, left);
    }
    else {
        DP_debug("Resize: layer is blank");
        transient_layer_data_init_null_tiles(tld);
    }

    tl->transient_data = tld;
    layer_data_decref(ld);
}

void DP_transient_layer_layer_attr(DP_TransientLayer *tl, int sublayer_id,
                                   uint8_t opacity, int blend_mode,
                                   bool censored, bool fixed)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    if (sublayer_id > 0) {
        DP_LayerList *ll = tl->sublayers;
        int sublayer_index = DP_layer_list_layer_index_by_id(ll, sublayer_id);
        DP_TransientLayer *tsl;
        if (sublayer_index < 0) {
            tsl = DP_transient_layer_transient_sublayer_create(tl, sublayer_id);
            DP_ASSERT(tsl);
        }
        else {
            tsl = DP_transient_layer_transient_sublayer_at(tl, sublayer_index);
            DP_ASSERT(tsl);
        }
        DP_transient_layer_layer_attr(tsl, 0, opacity, blend_mode, censored,
                                      fixed);
    }
    else {
        DP_transient_layer_opacity_set(tl, opacity);
        DP_transient_layer_blend_mode_set(tl, blend_mode);
        DP_transient_layer_censored_set(tl, censored);
        DP_transient_layer_fixed_set(tl, fixed);
    }
}

void DP_transient_layer_layer_retitle(DP_TransientLayer *tl, const char *title,
                                      size_t title_length)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    layer_title_decref(tl->title);
    tl->title = layer_title_new(title, title_length);
}

bool DP_transient_layer_put_image(DP_TransientLayer *tl,
                                  unsigned int context_id, int blend_mode,
                                  int x, int y, int width, int height,
                                  const unsigned char *image, size_t image_size)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);

    DP_Image *img =
        DP_image_new_from_compressed(width, height, image, image_size);
    if (img) {
        DP_TransientLayerData *tld = get_transient_layer_data(tl);
        transient_layer_data_put_image(tld, img, context_id, blend_mode, x, y);
        DP_image_free(img);
        return true;
    }
    else {
        return false;
    }
}

void DP_transient_layer_fill_rect(DP_TransientLayer *tl,
                                  unsigned int context_id, int blend_mode,
                                  int left, int top, int right, int bottom,
                                  uint32_t color)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_ASSERT(left >= 0);
    DP_ASSERT(top >= 0);
    DP_ASSERT(right <= tl->data->width);
    DP_ASSERT(bottom <= tl->data->height);
    DP_ASSERT(left < right);
    DP_ASSERT(top < bottom);
    DP_TransientLayerData *tld = get_transient_layer_data(tl);
    DP_Pixel pixel = {color};
    if (left == 0 && top == 0 && right == tld->width && bottom == tld->height) {
        transient_layer_data_fill(tld, context_id, blend_mode, pixel);
    }
    else {
        transient_layer_data_fill_rect(tld, context_id, blend_mode, left, top,
                                       right, bottom, pixel);
    }
}

static bool put_tile(DP_TransientLayer *tl, DP_Tile *tile, int x, int y,
                     int repeat)
{
    DP_LayerData *ld = tl->data;
    DP_TileCounts tile_counts = DP_tile_counts_round(ld->width, ld->height);
    int tile_total = tile_counts.x * tile_counts.y;
    int start = y * tile_counts.x + x;
    if (start < tile_total) {
        int end = start + repeat + 1;
        if (end > tile_total) {
            DP_warn("Put tile: ending index %d clamped to %d", end, tile_total);
            end = tile_total;
        }
        DP_tile_incref_by(tile, end - start);
        DP_TransientLayerData *tld = get_transient_layer_data(tl);
        for (int i = start; i < end; ++i) {
            DP_tile_decref_nullable(tld->elements[i].tile);
            tld->elements[i].tile = tile;
        }
        return true;
    }
    else {
        DP_error_set("Put tile: starting index %d beyond total %d", start,
                     tile_total);
    }
    return false;
}

bool DP_transient_layer_put_tile(DP_TransientLayer *tl, DP_Tile *tile,
                                 int sublayer_id, int x, int y, int repeat)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_ASSERT(tile);
    if (sublayer_id > 0) {
        DP_LayerList *ll = tl->sublayers;
        int sublayer_index = DP_layer_list_layer_index_by_id(ll, sublayer_id);
        DP_TransientLayer *tsl;
        if (sublayer_index < 0) {
            tsl = DP_transient_layer_transient_sublayer_create(tl, sublayer_id);
            DP_ASSERT(tsl);
        }
        else {
            tsl = DP_transient_layer_transient_sublayer_at(tl, sublayer_index);
            DP_ASSERT(tsl);
        }
        return DP_transient_layer_put_tile(tsl, tile, 0, x, y, repeat);
    }
    else {
        return put_tile(tl, tile, x, y, repeat);
    }
}

static DP_Image *select_pixels(DP_LayerData *ld, DP_Rect src_rect,
                               DP_Image *mask)
{
    int src_width = DP_rect_width(src_rect);
    int src_height = DP_rect_height(src_rect);
    DP_Image *src_img = DP_image_new(src_width, src_height);

    // TODO: make this smarter and prettier.
    int src_x = DP_rect_x(src_rect);
    int src_y = DP_rect_y(src_rect);
    int layer_width = ld->width;
    int layer_height = ld->height;
    for (int y = 0; y < src_height; ++y) {
        int layer_y = src_y + y;
        if (layer_y >= 0 && layer_y < layer_height) {
            for (int x = 0; x < src_width; ++x) {
                int layer_x = src_x + x;
                if (layer_x >= 0 && layer_y < layer_width
                    && (!mask || DP_image_pixel_at(mask, x, y).color != 0)) {
                    DP_Pixel pixel = layer_data_pixel_at(ld, layer_x, layer_y);
                    DP_image_pixel_at_set(src_img, x, y, pixel);
                }
            }
        }
    }

    return src_img;
}

static bool looks_like_translation_only(DP_Rect src_rect, DP_Quad dst_quad)
{
    DP_Rect dst_bounds = DP_quad_bounds(dst_quad);
    return DP_rect_width(src_rect) == DP_rect_width(dst_bounds)
        && DP_rect_height(src_rect) == DP_rect_width(dst_bounds)
        && dst_quad.x1 < dst_quad.x2;
}

bool DP_transient_layer_region_move(DP_TransientLayer *tl, DP_DrawContext *dc,
                                    unsigned int context_id,
                                    const DP_Rect *src_rect,
                                    const DP_Quad *dst_quad, DP_Image *mask)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_ASSERT(src_rect);
    DP_ASSERT(DP_rect_width(*src_rect) > 0);
    DP_ASSERT(DP_rect_height(*src_rect) > 0);

    DP_Image *src_img = select_pixels(tl->data, *src_rect, mask);

    int offset_x, offset_y;
    DP_Image *dst_img;
    if (looks_like_translation_only(*src_rect, *dst_quad)) {
        offset_x = dst_quad->x1;
        offset_y = dst_quad->y2;
        dst_img = src_img;
    }
    else {
        dst_img =
            DP_image_transform(src_img, dc, dst_quad, &offset_x, &offset_y);
        if (!dst_img) {
            DP_free(src_img);
            return false;
        }
    }

    DP_TransientLayerData *tld = get_transient_layer_data(tl);
    if (mask) {
        transient_layer_data_put_image(
            tld, mask, context_id, DP_BLEND_MODE_ERASE, DP_rect_x(*src_rect),
            DP_rect_y(*src_rect));
    }
    else {
        transient_layer_data_fill_rect(
            tld, context_id, DP_BLEND_MODE_REPLACE, src_rect->x1, src_rect->y1,
            src_rect->x2 + 1, src_rect->y2 + 1, (DP_Pixel){0});
    }

    transient_layer_data_put_image(tld, dst_img, context_id,
                                   DP_BLEND_MODE_NORMAL, offset_x, offset_y);

    if (dst_img != src_img) {
        DP_image_free(dst_img);
    }
    DP_image_free(src_img);
    return true;
}

bool DP_transient_layer_draw_dabs(DP_TransientLayer *tl, int sublayer_id,
                                  int sublayer_blend_mode, int sublayer_opacity,
                                  DP_PaintDrawDabsParams *params)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_ASSERT(params);
    DP_ASSERT(sublayer_id >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_blend_mode >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_blend_mode < DP_BLEND_MODE_COUNT);
    DP_ASSERT(sublayer_id == 0 || sublayer_opacity >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_opacity <= UINT8_MAX);

    DP_TransientLayer *tsl;
    if (sublayer_id == 0) {
        tsl = tl;
    }
    else {
        DP_LayerList *ll = tl->sublayers;
        int sublayer_index = DP_layer_list_layer_index_by_id(ll, sublayer_id);
        if (sublayer_index < 0) {
            tsl = DP_transient_layer_transient_sublayer_create(tl, sublayer_id);
            DP_ASSERT(tsl);
            // Only set these once, when the sublayer is created. They should
            // always be the same values for a single sublayer anyway.
            DP_transient_layer_blend_mode_set(tsl, sublayer_blend_mode);
            DP_transient_layer_opacity_set(tsl,
                                           DP_int_to_uint8(sublayer_opacity));
        }
        else {
            tsl = DP_transient_layer_transient_sublayer_at(tl, sublayer_index);
            DP_ASSERT(tsl);
        }
    }

    return DP_paint_draw_dabs(params, get_transient_layer_data(tsl));
}


bool DP_transient_layer_resize_to(DP_TransientLayer *tl,
                                  unsigned int context_id, int width,
                                  int height)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_LayerData *ld = tl->data;
    int layer_width = ld->width;
    int layer_height = ld->height;
    if (layer_width == width && layer_height == height) {
        return false;
    }
    else {
        DP_transient_layer_resize(tl, context_id, 0, width - layer_width,
                                  height - layer_height, 0);
        return true;
    }
}

void DP_transient_layer_render_tile(DP_TransientLayer *tl, DP_CanvasState *cs,
                                    int tile_index)
{
    DP_ASSERT(tl);
    DP_ASSERT(SDL_AtomicGet(&tl->refcount) > 0);
    DP_ASSERT(tl->transient);
    DP_ASSERT(cs);
    DP_ASSERT(tile_index >= 0);
    DP_TransientLayerData *tld = get_transient_layer_data(tl);
    DP_ASSERT(tile_index < DP_tile_total_round(tld->width, tld->height));
    DP_Tile **pp = &tld->elements[tile_index].tile;
    DP_tile_decref_nullable(*pp);
    *pp = DP_canvas_state_flatten_tile(cs, tile_index);
}
