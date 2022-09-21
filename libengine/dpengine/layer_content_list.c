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
#include "layer_content_list.h"
#include "canvas_state.h"
#include "layer_content.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_LayerContentList {
    DP_Atomic refcount;
    const bool transient;
    const int count;
    struct {
        DP_LayerContent *const layer_content;
    } elements[];
};

struct DP_TransientLayerContentList {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_LayerContent *layer_content;
        DP_TransientLayerContent *transient_layer_content;
    } elements[];
};

#else

struct DP_LayerContentList {
    DP_Atomic refcount;
    bool transient;
    int count;
    union {
        DP_LayerContent *layer_content;
        DP_TransientLayerContent *transient_layer_content;
    } elements[];
};

#endif


static size_t layer_content_list_size(int count)
{
    return DP_FLEX_SIZEOF(DP_LayerContentList, elements, DP_int_to_size(count));
}

static void *allocate_layer_content_list(bool transient, int count)
{
    DP_TransientLayerContentList *tlcl =
        DP_malloc(layer_content_list_size(count));
    DP_atomic_set(&tlcl->refcount, 1);
    tlcl->transient = transient;
    tlcl->count = count;
    return tlcl;
}


DP_LayerContentList *DP_layer_content_list_new(void)
{
    return allocate_layer_content_list(false, 0);
}

DP_LayerContentList *DP_layer_content_list_incref(DP_LayerContentList *lcl)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    DP_atomic_inc(&lcl->refcount);
    return lcl;
}

DP_LayerContentList *
DP_layer_content_list_incref_nullable(DP_LayerContentList *lcl_or_null)
{
    return lcl_or_null ? DP_layer_content_list_incref(lcl_or_null) : NULL;
}

void DP_layer_content_list_decref(DP_LayerContentList *lcl)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    if (DP_atomic_dec(&lcl->refcount)) {
        int count = lcl->count;
        for (int i = 0; i < count; ++i) {
            DP_layer_content_decref_nullable(lcl->elements[i].layer_content);
        }
        DP_free(lcl);
    }
}

void DP_layer_content_list_decref_nullable(DP_LayerContentList *lcl_or_null)
{
    if (lcl_or_null) {
        DP_layer_content_list_decref(lcl_or_null);
    }
}

int DP_layer_content_list_refcount(DP_LayerContentList *lcl)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    return DP_atomic_get(&lcl->refcount);
}

bool DP_layer_content_list_transient(DP_LayerContentList *lcl)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    return lcl->transient;
}


static void diff_layers(DP_LayerContentList *lcl, DP_LayerPropsList *lpl,
                        DP_LayerContentList *prev_lcl,
                        DP_LayerPropsList *prev_lpl, DP_CanvasDiff *diff,
                        int count)
{
    for (int i = 0; i < count; ++i) {
        DP_layer_content_diff(DP_layer_content_list_at_noinc(lcl, i),
                              DP_layer_props_list_at_noinc(lpl, i),
                              DP_layer_content_list_at_noinc(prev_lcl, i),
                              DP_layer_props_list_at_noinc(prev_lpl, i), diff);
    }
}

static void mark_layers(DP_LayerContentList *lcl, DP_CanvasDiff *diff,
                        int start, int end)
{
    for (int i = start; i < end; ++i) {
        DP_layer_content_diff_mark(DP_layer_content_list_at_noinc(lcl, i),
                                   diff);
    }
}

void DP_layer_content_list_diff(DP_LayerContentList *lcl,
                                DP_LayerPropsList *lpl,
                                DP_LayerContentList *prev_lcl,
                                DP_LayerPropsList *prev_lpl,
                                DP_CanvasDiff *diff)
{
    DP_ASSERT(lcl);
    DP_ASSERT(lpl);
    DP_ASSERT(prev_lcl);
    DP_ASSERT(prev_lpl);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    DP_ASSERT(DP_atomic_get(&prev_lcl->refcount) > 0);
    if (lcl != prev_lcl || lpl != prev_lpl) {
        int new_count = lcl->count;
        int old_count = prev_lcl->count;
        if (new_count <= old_count) {
            diff_layers(lcl, lpl, prev_lcl, prev_lpl, diff, new_count);
            mark_layers(prev_lcl, diff, new_count, old_count);
        }
        else {
            diff_layers(lcl, lpl, prev_lcl, prev_lpl, diff, old_count);
            mark_layers(lcl, diff, old_count, new_count);
        }
    }
}

void DP_layer_content_list_diff_mark(DP_LayerContentList *lcl,
                                     DP_CanvasDiff *diff)
{
    DP_ASSERT(lcl);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    mark_layers(lcl, diff, 0, lcl->count);
}


int DP_layer_content_list_count(DP_LayerContentList *lcl)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    return lcl->count;
}

DP_LayerContent *DP_layer_content_list_at_noinc(DP_LayerContentList *lcl,
                                                int index)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < lcl->count);
    return lcl->elements[index].layer_content;
}

DP_TransientLayerContentList *
DP_layer_content_list_resize(DP_LayerContentList *lcl, unsigned int context_id,
                             int top, int right, int bottom, int left)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    int count = lcl->count;
    DP_TransientLayerContentList *tlcl =
        allocate_layer_content_list(true, count);
    for (int i = 0; i < count; ++i) {
        DP_LayerContent *lc = lcl->elements[i].layer_content;
        tlcl->elements[i].transient_layer_content =
            DP_layer_content_resize(lc, context_id, top, right, bottom, left);
    }
    return tlcl;
}

void DP_layer_content_list_merge_to_flat_image(DP_LayerContentList *lcl,
                                               DP_LayerPropsList *lpl,
                                               DP_TransientLayerContent *tlc,
                                               unsigned int flags)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    DP_ASSERT(lpl);
    DP_ASSERT(DP_layer_props_list_refcount(lpl) > 0);
    DP_ASSERT(lcl->count == DP_layer_props_list_count(lpl));
    int count = lcl->count;
    bool include_fixed = flags & DP_FLAT_IMAGE_INCLUDE_FIXED_LAYERS;
    bool include_sublayers = flags & DP_FLAT_IMAGE_INCLUDE_SUBLAYERS;
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        bool want_layer = DP_layer_props_visible(lp)
                       && (include_fixed || !DP_layer_props_fixed(lp));
        if (want_layer) {
            DP_LayerContent *lc = lcl->elements[i].layer_content;
            uint16_t opacity = DP_layer_props_opacity(lp);
            int blend_mode = DP_layer_props_blend_mode(lp);
            if (include_sublayers) {
                DP_LayerContent *sub_lc =
                    DP_layer_content_merge_to_flat_image(lc);
                DP_transient_layer_content_merge(tlc, 0, sub_lc, opacity,
                                                 blend_mode);
                DP_layer_content_decref(sub_lc);
            }
            else {
                DP_transient_layer_content_merge(tlc, 0, lc, opacity,
                                                 blend_mode);
            }
        }
    }
}

void DP_layer_content_list_flatten_tile_to(DP_LayerContentList *lcl,
                                           DP_LayerPropsList *lpl,
                                           int tile_index, DP_TransientTile *tt)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    DP_ASSERT(!lcl->transient);
    DP_ASSERT(lpl);
    DP_ASSERT(DP_layer_props_list_refcount(lpl) > 0);
    DP_ASSERT(lcl->count == DP_layer_props_list_count(lpl));
    DP_ASSERT(tt);
    int count = lcl->count;
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (DP_layer_props_visible(lp)) {
            DP_LayerContent *lc = lcl->elements[i].layer_content;
            DP_layer_content_flatten_tile_to(lc, tile_index, tt,
                                             DP_layer_props_opacity(lp),
                                             DP_layer_props_blend_mode(lp));
        }
    }
}


DP_TransientLayerContentList *
DP_transient_layer_content_list_new_init(int reserve)
{
    DP_TransientLayerContentList *tlcl =
        allocate_layer_content_list(true, reserve);
    for (int i = 0; i < reserve; ++i) {
        tlcl->elements[i].layer_content = NULL;
    }
    return tlcl;
}

DP_TransientLayerContentList *
DP_transient_layer_content_list_new(DP_LayerContentList *lcl, int reserve)
{
    DP_ASSERT(lcl);
    DP_ASSERT(DP_atomic_get(&lcl->refcount) > 0);
    DP_ASSERT(!lcl->transient);
    DP_ASSERT(reserve >= 0);
    int count = lcl->count;
    DP_TransientLayerContentList *tlcl =
        allocate_layer_content_list(true, count + reserve);
    for (int i = 0; i < count; ++i) {
        tlcl->elements[i].layer_content =
            DP_layer_content_incref(lcl->elements[i].layer_content);
    }
    for (int i = 0; i < reserve; ++i) {
        tlcl->elements[count + i].layer_content = NULL;
    }
    return tlcl;
}

DP_TransientLayerContentList *
DP_transient_layer_content_list_reserve(DP_TransientLayerContentList *tlcl,
                                        int reserve)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d elements in layer content list", reserve);
    if (reserve > 0) {
        int old_count = tlcl->count;
        int new_count = old_count + reserve;
        tlcl = DP_realloc(tlcl, layer_content_list_size(new_count));
        tlcl->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tlcl->elements[i].layer_content = NULL;
        }
    }
    return tlcl;
}

DP_TransientLayerContentList *
DP_transient_layer_content_list_incref(DP_TransientLayerContentList *tlcl)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    return (DP_TransientLayerContentList *)DP_layer_content_list_incref(
        (DP_LayerContentList *)tlcl);
}

void DP_transient_layer_content_list_decref(DP_TransientLayerContentList *tlcl)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    DP_layer_content_list_decref((DP_LayerContentList *)tlcl);
}

int DP_transient_layer_content_list_refcount(DP_TransientLayerContentList *tlcl)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    return DP_layer_content_list_refcount((DP_LayerContentList *)tlcl);
}

DP_LayerContentList *
DP_transient_layer_content_list_persist(DP_TransientLayerContentList *tlcl)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    tlcl->transient = false;
    int count = tlcl->count;
    for (int i = 0; i < count; ++i) {
        DP_ASSERT(tlcl->elements[i].layer_content);
        if (DP_layer_content_transient(tlcl->elements[i].layer_content)) {
            DP_transient_layer_content_persist(
                tlcl->elements[i].transient_layer_content);
        }
    }
    return (DP_LayerContentList *)tlcl;
}

int DP_transient_layer_content_list_count(DP_TransientLayerContentList *tlcl)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    return DP_layer_content_list_count((DP_LayerContentList *)tlcl);
}

DP_LayerContent *
DP_transient_layer_content_list_at_noinc(DP_TransientLayerContentList *tlcl,
                                         int index)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    return DP_layer_content_list_at_noinc((DP_LayerContentList *)tlcl, index);
}

DP_TransientLayerContent *DP_transient_layer_content_list_transient_at_noinc(
    DP_TransientLayerContentList *tlcl, int index)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlcl->count);
    DP_LayerContent *lc = tlcl->elements[index].layer_content;
    if (!DP_layer_content_transient(lc)) {
        tlcl->elements[index].transient_layer_content =
            DP_transient_layer_content_new(lc);
        DP_layer_content_decref(lc);
    }
    return tlcl->elements[index].transient_layer_content;
}

void DP_transient_layer_content_list_insert_inc(
    DP_TransientLayerContentList *tlcl, DP_LayerContent *lc, int index)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    DP_ASSERT(!tlcl->elements[tlcl->count - 1].layer_content);
    DP_ASSERT(lc);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlcl->count);
    memmove(&tlcl->elements[index + 1], &tlcl->elements[index],
            sizeof(*tlcl->elements) * DP_int_to_size(tlcl->count - index - 1));
    tlcl->elements[index].layer_content = DP_layer_content_incref(lc);
}

void DP_transient_layer_content_list_insert_transient_noinc(
    DP_TransientLayerContentList *tlcl, DP_TransientLayerContent *tlc,
    int index)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    DP_ASSERT(!tlcl->elements[tlcl->count - 1].layer_content);
    DP_ASSERT(tlc);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlcl->count);
    memmove(&tlcl->elements[index + 1], &tlcl->elements[index],
            sizeof(*tlcl->elements) * DP_int_to_size(tlcl->count - index - 1));
    tlcl->elements[index].transient_layer_content = tlc;
}

void DP_transient_layer_content_list_delete_at(
    DP_TransientLayerContentList *tlcl, int index)
{
    DP_ASSERT(tlcl);
    DP_ASSERT(DP_atomic_get(&tlcl->refcount) > 0);
    DP_ASSERT(tlcl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlcl->count);
    DP_layer_content_decref(tlcl->elements[index].layer_content);
    int new_count = --tlcl->count;
    memmove(&tlcl->elements[index], &tlcl->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tlcl->elements[0]));
}
