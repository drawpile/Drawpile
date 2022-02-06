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
#include "layer_list.h"
#include "canvas_state.h"
#include "layer.h"
#include "paint.h"
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <SDL_atomic.h>

static_assert(INT16_MAX == 32767, "INT16_MAX has expected size");


#ifdef DP_NO_STRICT_ALIASING

typedef struct DP_LayerList {
    SDL_atomic_t refcount;
    const bool transient;
    const int count;
    struct {
        DP_Layer *const layer;
    } elements[];
} DP_LayerList;

typedef struct DP_TransientLayerList {
    SDL_atomic_t refcount;
    bool transient;
    int count;
    union {
        DP_Layer *layer;
        DP_TransientLayer *transient_layer;
    } elements[];
} DP_TransientLayerList;

#else

typedef struct DP_LayerList {
    SDL_atomic_t refcount;
    bool transient;
    int count;
    union {
        DP_Layer *layer;
        DP_TransientLayer *transient_layer;
    } elements[];
} DP_LayerList;

typedef struct DP_LayerList DP_TransientLayerList;

#endif


static size_t layer_list_size(int count)
{
    return DP_FLEX_SIZEOF(DP_LayerList, elements, DP_int_to_size(count));
}

static void *allocate_layer_list(bool transient, int count)
{
    DP_TransientLayerList *tll = DP_malloc(layer_list_size(count));
    SDL_AtomicSet(&tll->refcount, 1);
    tll->transient = transient;
    tll->count = count;
    return tll;
}

DP_LayerList *DP_layer_list_new(void)
{
    return allocate_layer_list(false, 0);
}

DP_LayerList *DP_layer_list_incref(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    SDL_AtomicIncRef(&ll->refcount);
    return ll;
}

void DP_layer_list_decref(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    if (SDL_AtomicDecRef(&ll->refcount)) {
        int count = ll->count;
        for (int i = 0; i < count; ++i) {
            DP_Layer *l = ll->elements[i].layer;
            if (l) {
                DP_layer_decref(l);
            }
        }
        DP_free(ll);
    }
}

int DP_layer_list_refcount(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    return SDL_AtomicGet(&ll->refcount);
}

bool DP_layer_list_transient(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    return ll->transient;
}


static void diff_layers(DP_LayerList *ll, DP_LayerList *prev,
                        DP_CanvasDiff *diff, int count)
{
    for (int i = 0; i < count; ++i) {
        DP_layer_diff(ll->elements[i].layer, prev->elements[i].layer, diff);
    }
}

static void mark_layers(DP_LayerList *ll_or_prev, DP_CanvasDiff *diff,
                        int start, int end)
{
    for (int i = start; i < end; ++i) {
        DP_layer_diff_mark(ll_or_prev->elements[i].layer, diff);
    }
}

void DP_layer_list_diff(DP_LayerList *ll, DP_LayerList *prev,
                        DP_CanvasDiff *diff)
{
    DP_ASSERT(ll);
    DP_ASSERT(prev);
    DP_ASSERT(diff);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    DP_ASSERT(SDL_AtomicGet(&prev->refcount) > 0);
    if (ll != prev) {
        int new_count = ll->count;
        int old_count = prev->count;
        if (new_count <= old_count) {
            diff_layers(ll, prev, diff, new_count);
            mark_layers(prev, diff, new_count, old_count);
        }
        else {
            diff_layers(ll, prev, diff, old_count);
            mark_layers(ll, diff, old_count, new_count);
        }
    }
}

void DP_layer_list_diff_mark(DP_LayerList *ll, DP_CanvasDiff *diff)
{
    DP_ASSERT(ll);
    DP_ASSERT(diff);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    mark_layers(ll, diff, 0, ll->count);
}


int DP_layer_list_layer_count(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    return ll->count;
}

int DP_layer_list_layer_index_by_id(DP_LayerList *ll, int layer_id)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    int count = ll->count;
    for (int i = 0; i < count; ++i) {
        // Transient layer lists may have null layers allocated in reserve.
        DP_Layer *l = ll->elements[i].layer;
        if (l && DP_layer_id(l) == layer_id) {
            return i;
        }
    }
    return -1;
}

DP_Layer *DP_layer_list_layer_by_id(DP_LayerList *ll, int layer_id)
{
    int i = DP_layer_list_layer_index_by_id(ll, layer_id);
    return i < 0 ? NULL : ll->elements[i].layer;
}

DP_Layer *DP_layer_list_at_noinc(DP_LayerList *ll, int index)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ll->count);
    return ll->elements[index].layer;
}


void DP_layer_list_merge_to_flat_image(DP_LayerList *ll, DP_TransientLayer *tl,
                                       unsigned int flags)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    DP_ASSERT(!ll->transient);
    DP_ASSERT(tl);
    int count = ll->count;
    bool include_fixed = flags & DP_FLAT_IMAGE_INCLUDE_FIXED_LAYERS;
    bool include_sublayers = flags & DP_FLAT_IMAGE_INCLUDE_SUBLAYERS;
    for (int i = 0; i < count; ++i) {
        DP_Layer *l = ll->elements[i].layer;
        if (DP_layer_visible(l) && (include_fixed || !DP_layer_fixed(l))) {
            if (include_sublayers) {
                DP_Layer *sl = DP_layer_merge_to_flat_image(l);
                DP_transient_layer_merge(tl, sl, 0);
                DP_layer_decref(sl);
            }
            else {
                DP_transient_layer_merge(tl, l, 0);
            }
        }
    }
}

void DP_layer_list_flatten_tile_to(DP_LayerList *ll, int tile_index,
                                   DP_TransientTile *tt)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    DP_ASSERT(!ll->transient);
    DP_ASSERT(tt);
    int count = ll->count;
    for (int i = 0; i < count; ++i) {
        DP_Layer *l = ll->elements[i].layer;
        if (DP_layer_visible(l)) {
            DP_layer_flatten_tile_to(l, tile_index, tt);
        }
    }
}

DP_TransientLayerList *DP_layer_list_layer_reorder(DP_LayerList *ll,
                                                   const int *layer_ids,
                                                   int layer_id_count)
{
    int layer_count = ll->count;
    DP_TransientLayerList *tll = allocate_layer_list(true, layer_count);
    for (int i = 0; i < layer_count; ++i) {
        tll->elements[i].layer = NULL;
    }

    int fill = 0;
    for (int i = 0; i < layer_id_count; ++i) {
        int layer_id = layer_ids[i];
        if (DP_transient_layer_list_layer_index_by_id(tll, layer_id) == -1) {
            int from_index = DP_layer_list_layer_index_by_id(ll, layer_id);
            if (from_index != -1) {
                DP_Layer *l = ll->elements[from_index].layer;
                tll->elements[fill].layer = DP_layer_incref(l);
                ++fill;
            }
            else {
                DP_warn("Layer reorder: unknown layer id %d", layer_id);
            }
        }
        else {
            DP_warn("Layer reorder: duplicate layer id %d", layer_id);
        }
    }

    // If further layers remain, just move them over in the order they appear.
    for (int i = 0; fill < layer_count && i < layer_count; ++i) {
        DP_Layer *l = DP_layer_list_at_noinc(ll, i);
        int layer_id = DP_layer_id(l);
        if (DP_transient_layer_list_layer_index_by_id(tll, layer_id) == -1) {
            tll->elements[fill].layer = DP_layer_incref(l);
            ++fill;
        }
    }

    return tll;
}


DP_TransientLayerList *DP_transient_layer_list_new(DP_LayerList *ll,
                                                   int reserve)
{
    DP_ASSERT(ll);
    DP_ASSERT(SDL_AtomicGet(&ll->refcount) > 0);
    DP_ASSERT(!ll->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("New transient layer list");
    int count = ll->count;
    DP_TransientLayerList *tll = allocate_layer_list(true, count + reserve);
    for (int i = 0; i < count; ++i) {
        tll->elements[i].layer = DP_layer_incref(ll->elements[i].layer);
    }
    for (int i = 0; i < reserve; ++i) {
        tll->elements[count + i].layer = NULL;
    }
    return tll;
}

DP_TransientLayerList *DP_transient_layer_list_new_init(void)
{
    return allocate_layer_list(true, 0);
}

DP_TransientLayerList *
DP_transient_layer_list_reserve(DP_TransientLayerList *tll, int reserve)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d elements in layer list", reserve);
    if (reserve > 0) {
        int old_count = tll->count;
        int new_count = old_count + reserve;
        tll = DP_realloc(tll, layer_list_size(new_count));
        tll->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tll->elements[i].layer = NULL;
        }
    }
    return tll;
}

DP_TransientLayerList *
DP_transient_layer_list_incref(DP_TransientLayerList *tll)
{
    return (DP_TransientLayerList *)DP_layer_list_incref((DP_LayerList *)tll);
}

void DP_transient_layer_list_decref(DP_TransientLayerList *tll)
{
    DP_layer_list_decref((DP_LayerList *)tll);
}

int DP_transient_layer_list_refcount(DP_TransientLayerList *tll)
{
    return DP_layer_list_refcount((DP_LayerList *)tll);
}

DP_LayerList *DP_transient_layer_list_persist(DP_TransientLayerList *tll)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    tll->transient = false;
    int count = tll->count;
    for (int i = 0; i < count; ++i) {
        if (DP_layer_transient(tll->elements[i].layer)) {
            DP_transient_layer_persist(tll->elements[i].transient_layer);
        }
    }
    return (DP_LayerList *)tll;
}


int DP_transient_layer_list_layer_index_by_id(DP_TransientLayerList *tll,
                                              int layer_id)
{
    return DP_layer_list_layer_index_by_id((DP_LayerList *)tll, layer_id);
}

DP_Layer *DP_transient_layer_list_layer_by_id(DP_TransientLayerList *tll,
                                              int layer_id)
{
    return DP_layer_list_layer_by_id((DP_LayerList *)tll, layer_id);
}

DP_Layer *DP_transient_layer_list_at(DP_TransientLayerList *tll, int index)
{
    return DP_layer_list_at_noinc((DP_LayerList *)tll, index);
}

DP_TransientLayer *
DP_transient_layer_list_transient_at(DP_TransientLayerList *tll, int index)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    DP_Layer *l = tll->elements[index].layer;
    if (!DP_layer_transient(l)) {
        tll->elements[index].transient_layer = DP_transient_layer_new(l);
        DP_layer_decref(l);
    }
    return tll->elements[index].transient_layer;
}

void DP_transient_layer_list_remove_at(DP_TransientLayerList *tll, int index)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    DP_layer_decref(tll->elements[index].layer);
    int count = --tll->count;
    for (int i = index; i < count; ++i) {
        tll->elements[index] = tll->elements[index + 1];
    }
}


static void insert_noinc(DP_TransientLayerList *tll, DP_TransientLayer *tl,
                         int i)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(!tll->elements[tll->count - 1].layer);
    DP_ASSERT(tl);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < tll->count);
    memmove(&tll->elements[i + 1], &tll->elements[i],
            sizeof(*tll->elements) * DP_int_to_size(tll->count - i - 1));
    tll->elements[i].transient_layer = tl;
}


void DP_transient_layer_list_resize(DP_TransientLayerList *tll,
                                    unsigned int context_id, int top, int right,
                                    int bottom, int left)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    int count = tll->count;
    for (int i = 0; i < count; ++i) {
        DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, i);
        DP_transient_layer_resize(tl, context_id, top, right, bottom, left);
    }
}

DP_TransientLayer *
DP_transient_layer_list_layer_create(DP_TransientLayerList *tll, int layer_id,
                                     int source_id, DP_Tile *tile, bool insert,
                                     bool copy, int width, int height,
                                     const char *title, size_t title_length)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    if (DP_transient_layer_list_layer_index_by_id(tll, layer_id) >= 0) {
        DP_error_set("Create layer: id %d already exists", layer_id);
        return NULL;
    }

    int source_index;
    if (source_id > 0) {
        source_index =
            DP_transient_layer_list_layer_index_by_id(tll, source_id);
        if (source_index < 0) {
            DP_error_set("Create layer: source id %d not found", source_id);
            return NULL;
        }
    }
    else {
        source_index = -1;
    }

    DP_TransientLayer *tl;
    if (copy) {
        if (source_index < 0) {
            DP_error_set("Create layer: no copy source specified");
            return false;
        }
        tl = DP_transient_layer_new(tll->elements[source_index].layer);
        DP_transient_layer_id_set(tl, layer_id);
    }
    else {
        tl = DP_transient_layer_new_init(layer_id, width, height, tile);
    }
    DP_transient_layer_title_set(tl, title, title_length);

    int target_index = insert ? source_index + 1 : tll->count - 1;
    insert_noinc(tll, tl, target_index);
    return tl;
}

bool DP_transient_layer_list_layer_attr(DP_TransientLayerList *tll,
                                        int layer_id, int sublayer_id,
                                        uint8_t opacity, int blend_mode,
                                        bool censored, bool fixed)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Layer attributes: id %d not found", layer_id);
        return false;
    }

    DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, index);
    DP_transient_layer_layer_attr(tl, sublayer_id, opacity, blend_mode,
                                  censored, fixed);
    return true;
}

bool DP_transient_layer_list_layer_retitle(DP_TransientLayerList *tll,
                                           int layer_id, const char *title,
                                           size_t title_length)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Layer retitle: id %d not found", layer_id);
        return false;
    }

    DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, index);
    DP_transient_layer_layer_retitle(tl, title, title_length);
    return true;
}

static void merge_down_at(DP_TransientLayerList *tll, unsigned int context_id,
                          int index, DP_Layer *l)
{
    DP_TransientLayer *tl =
        DP_transient_layer_list_transient_at(tll, index - 1);
    DP_transient_layer_merge(tl, l, context_id);
}

static void delete_at(DP_TransientLayerList *tll, int index, DP_Layer *l)
{
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    int new_count = --tll->count;
    memmove(&tll->elements[index], &tll->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tll->elements[0]));
    DP_layer_decref(l);
}

bool DP_transient_layer_list_layer_delete(DP_TransientLayerList *tll,
                                          unsigned int context_id, int layer_id,
                                          bool merge)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Layer delete: id %d not found", layer_id);
        return false;
    }

    DP_Layer *l = DP_layer_list_at_noinc((DP_LayerList *)tll, index);
    if (merge) {
        if (index > 0) {
            merge_down_at(tll, context_id, index, l);
        }
        else {
            DP_warn("Attempt to merge down bottom layer ignored");
        }
    }
    delete_at(tll, index, l);

    return true;
}

bool DP_transient_layer_list_layer_visibility(DP_TransientLayerList *tll,
                                              int layer_id, bool visible)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Layer visibility: id %d not found", layer_id);
        return false;
    }

    DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, index);
    DP_transient_layer_hidden_set(tl, !visible);
    return true;
}

bool DP_transient_layer_list_put_image(DP_TransientLayerList *tll,
                                       unsigned int context_id, int layer_id,
                                       int blend_mode, int x, int y, int width,
                                       int height, const unsigned char *image,
                                       size_t image_size)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Put image: id %d not found", layer_id);
        return false;
    }

    DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, index);
    return DP_transient_layer_put_image(tl, context_id, blend_mode, x, y, width,
                                        height, image, image_size);
}

bool DP_transient_layer_list_fill_rect(DP_TransientLayerList *tll,
                                       unsigned int context_id, int layer_id,
                                       int blend_mode, int left, int top,
                                       int right, int bottom, uint32_t color)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Fill rect: id %d not found", layer_id);
        return false;
    }

    DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, index);
    DP_transient_layer_fill_rect(tl, context_id, blend_mode, left, top, right,
                                 bottom, color);
    return true;
}

bool DP_transient_layer_list_region_move(DP_TransientLayerList *tll,
                                         DP_DrawContext *dc,
                                         unsigned int context_id, int layer_id,
                                         const DP_Rect *src_rect,
                                         const DP_Quad *dst_quad,
                                         DP_Image *mask)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Region move: id %d not found", layer_id);
        return false;
    }

    DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, index);
    return DP_transient_layer_region_move(tl, dc, context_id, src_rect,
                                          dst_quad, mask);
}

bool DP_transient_layer_list_put_tile(DP_TransientLayerList *tll, DP_Tile *tile,
                                      int layer_id, int sublayer_id, int x,
                                      int y, int repeat)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Put tile: id %d not found", layer_id);
        return false;
    }

    DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, index);
    return DP_transient_layer_put_tile(tl, tile, sublayer_id, x, y, repeat);
}

bool DP_transient_layer_list_draw_dabs(DP_TransientLayerList *tll, int layer_id,
                                       int sublayer_id, int sublayer_blend_mode,
                                       int sublayer_opacity,
                                       DP_PaintDrawDabsParams *params)
{
    DP_ASSERT(tll);
    DP_ASSERT(SDL_AtomicGet(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(params);

    int index = DP_transient_layer_list_layer_index_by_id(tll, layer_id);
    if (index < 0) {
        DP_error_set("Draw dabs: id %d not found", layer_id);
        return false;
    }

    DP_TransientLayer *tl = DP_transient_layer_list_transient_at(tll, index);
    return DP_transient_layer_draw_dabs(tl, sublayer_id, sublayer_blend_mode,
                                        sublayer_opacity, params);
}
