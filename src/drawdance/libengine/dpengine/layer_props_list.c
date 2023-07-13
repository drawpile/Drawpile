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
#include "layer_props_list.h"
#include "layer_props.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpmsg/blend_mode.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_LayerPropsList {
    DP_Atomic refcount;
    const bool transient;
    const int count;
    struct {
        DP_LayerProps *const layer_props;
    } elements[];
};

struct DP_TransientLayerPropsList {
    DP_Atomic refcount;
    bool transient;
    int count;
    union DP_TransientLayerPropsElement {
        DP_LayerProps *layer_props;
        DP_TransientLayerProps *transient_layer_props;
    } elements[];
};

#else

struct DP_LayerPropsList {
    DP_Atomic refcount;
    bool transient;
    int count;
    union DP_TransientLayerPropsElement {
        DP_LayerProps *layer_props;
        DP_TransientLayerProps *transient_layer_props;
    } elements[];
};

#endif


static size_t layer_props_list_size(int count)
{
    return DP_FLEX_SIZEOF(DP_LayerPropsList, elements, DP_int_to_size(count));
}

static void *allocate_layer_props_list(bool transient, int count)
{
    DP_TransientLayerPropsList *tlpl = DP_malloc(layer_props_list_size(count));
    DP_atomic_set(&tlpl->refcount, 1);
    tlpl->transient = transient;
    tlpl->count = count;
    return tlpl;
}


DP_LayerPropsList *DP_layer_props_list_new(void)
{
    return allocate_layer_props_list(false, 0);
}

DP_LayerPropsList *DP_layer_props_list_incref(DP_LayerPropsList *lpl)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    DP_atomic_inc(&lpl->refcount);
    return lpl;
}

DP_LayerPropsList *
DP_layer_props_list_incref_nullable(DP_LayerPropsList *lpl_or_null)
{
    return lpl_or_null ? DP_layer_props_list_incref(lpl_or_null) : NULL;
}

void DP_layer_props_list_decref(DP_LayerPropsList *lpl)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    if (DP_atomic_dec(&lpl->refcount)) {
        int count = lpl->count;
        for (int i = 0; i < count; ++i) {
            DP_layer_props_decref_nullable(lpl->elements[i].layer_props);
        }
        DP_free(lpl);
    }
}

void DP_layer_props_list_decref_nullable(DP_LayerPropsList *lpl_or_null)
{
    if (lpl_or_null) {
        DP_layer_props_list_decref(lpl_or_null);
    }
}

int DP_layer_props_list_refcount(DP_LayerPropsList *lpl)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    return DP_atomic_get(&lpl->refcount);
}

bool DP_layer_props_list_transient(DP_LayerPropsList *lpl)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    return lpl->transient;
}

int DP_layer_props_list_count(DP_LayerPropsList *lpl)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    return lpl->count;
}

DP_LayerProps *DP_layer_props_list_at_noinc(DP_LayerPropsList *lpl, int index)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < lpl->count);
    return lpl->elements[index].layer_props;
}

int DP_layer_props_list_index_by_id(DP_LayerPropsList *lpl, int layer_id)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    int count = lpl->count;
    for (int i = 0; i < count; ++i) {
        // Transient layer lists may have null layers allocated in reserve.
        DP_LayerProps *lp = lpl->elements[i].layer_props;
        if (lp && DP_layer_props_id(lp) == layer_id) {
            return i;
        }
    }
    return -1;
}

bool DP_layer_props_list_can_decrease_opacity(DP_LayerPropsList *lpl)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    int count = lpl->count;
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = lpl->elements[i].layer_props;

        // Recurse if this is a pass-through group.
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        bool can_decrease_opacity;
        if (!child_lpl || DP_layer_props_isolated(lp)) {
            can_decrease_opacity = DP_blend_mode_can_decrease_opacity(
                DP_layer_props_blend_mode(lp));
        }
        else {
            can_decrease_opacity =
                DP_layer_props_list_can_decrease_opacity(child_lpl);
        }

        if (can_decrease_opacity) {
            return true;
        }
    }
    return false;
}


DP_TransientLayerPropsList *DP_transient_layer_props_list_new_init(int reserve)
{
    DP_TransientLayerPropsList *tlpl = allocate_layer_props_list(true, reserve);
    for (int i = 0; i < reserve; ++i) {
        tlpl->elements[i].layer_props = NULL;
    }
    return tlpl;
}

DP_TransientLayerPropsList *
DP_transient_layer_props_list_new(DP_LayerPropsList *lpl, int reserve)
{
    DP_ASSERT(lpl);
    DP_ASSERT(DP_atomic_get(&lpl->refcount) > 0);
    DP_ASSERT(!lpl->transient);
    DP_ASSERT(reserve >= 0);
    int count = lpl->count;
    DP_TransientLayerPropsList *tlpl =
        allocate_layer_props_list(true, count + reserve);
    for (int i = 0; i < count; ++i) {
        tlpl->elements[i].layer_props =
            DP_layer_props_incref(lpl->elements[i].layer_props);
    }
    for (int i = 0; i < reserve; ++i) {
        tlpl->elements[count + i].layer_props = NULL;
    }
    return tlpl;
}

DP_TransientLayerPropsList *
DP_transient_layer_props_list_reserve(DP_TransientLayerPropsList *tlpl,
                                      int reserve)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d elements in layer props list", reserve);
    if (reserve > 0) {
        int old_count = tlpl->count;
        int new_count = old_count + reserve;
        tlpl = DP_realloc(tlpl, layer_props_list_size(new_count));
        tlpl->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tlpl->elements[i].layer_props = NULL;
        }
    }
    return tlpl;
}

DP_TransientLayerPropsList *
DP_transient_layer_props_list_incref(DP_TransientLayerPropsList *tlpl)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    return (DP_TransientLayerPropsList *)DP_layer_props_list_incref(
        (DP_LayerPropsList *)tlpl);
}

void DP_transient_layer_props_list_decref(DP_TransientLayerPropsList *tlpl)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_layer_props_list_decref((DP_LayerPropsList *)tlpl);
}

int DP_transient_layer_props_list_refcount(DP_TransientLayerPropsList *tlpl)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    return DP_layer_props_list_refcount((DP_LayerPropsList *)tlpl);
}

DP_LayerPropsList *
DP_transient_layer_props_list_persist(DP_TransientLayerPropsList *tlpl)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    tlpl->transient = false;
    int count = tlpl->count;
    for (int i = 0; i < count; ++i) {
        DP_ASSERT(tlpl->elements[i].layer_props);
        if (DP_layer_props_transient(tlpl->elements[i].layer_props)) {
            DP_transient_layer_props_persist(
                tlpl->elements[i].transient_layer_props);
        }
    }
    return (DP_LayerPropsList *)tlpl;
}

int DP_transient_layer_props_list_count(DP_TransientLayerPropsList *tlpl)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    return DP_layer_props_list_count((DP_LayerPropsList *)tlpl);
}

DP_LayerProps *
DP_transient_layer_props_list_at_noinc(DP_TransientLayerPropsList *tlpl,
                                       int index)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    return DP_layer_props_list_at_noinc((DP_LayerPropsList *)tlpl, index);
}

DP_TransientLayerProps *DP_transient_layer_props_list_transient_at_noinc(
    DP_TransientLayerPropsList *tlpl, int index)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlpl->count);
    DP_LayerProps *lp = tlpl->elements[index].layer_props;
    if (!DP_layer_props_transient(lp)) {
        tlpl->elements[index].transient_layer_props =
            DP_transient_layer_props_new(lp);
        DP_layer_props_decref(lp);
    }
    return tlpl->elements[index].transient_layer_props;
}

DP_TransientLayerProps *
DP_transient_layer_props_list_transient_at_with_children_noinc(
    DP_TransientLayerPropsList *tlpl, int index,
    DP_TransientLayerPropsList *transient_children)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlpl->count);
    DP_ASSERT(transient_children);
    DP_LayerProps *lp = tlpl->elements[index].layer_props;
    DP_ASSERT(!DP_layer_props_transient(lp));
    tlpl->elements[index].transient_layer_props =
        DP_transient_layer_props_new_with_children_noinc(lp,
                                                         transient_children);
    DP_layer_props_decref(lp);
    return tlpl->elements[index].transient_layer_props;
}

int DP_transient_layer_props_list_index_by_id(DP_TransientLayerPropsList *tlpl,
                                              int layer_id)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    return DP_layer_props_list_index_by_id((DP_LayerPropsList *)tlpl, layer_id);
}

void DP_transient_layer_props_list_set_noinc(DP_TransientLayerPropsList *tlpl,
                                             DP_LayerProps *lp, int index)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_ASSERT(lp);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlpl->count);
    DP_ASSERT(!tlpl->elements[index].layer_props);
    tlpl->elements[index].layer_props = lp;
}

void DP_transient_layer_props_list_set_inc(DP_TransientLayerPropsList *tlpl,
                                           DP_LayerProps *lp, int index)
{
    DP_transient_layer_props_list_set_noinc(tlpl, DP_layer_props_incref(lp),
                                            index);
}

static void insert_at(DP_TransientLayerPropsList *tlpl, int index,
                      union DP_TransientLayerPropsElement element)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_ASSERT(!tlpl->elements[tlpl->count - 1].layer_props);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlpl->count);
    memmove(&tlpl->elements[index + 1], &tlpl->elements[index],
            sizeof(*tlpl->elements) * DP_int_to_size(tlpl->count - index - 1));
    tlpl->elements[index] = element;
}

void DP_transient_layer_props_list_insert_inc(DP_TransientLayerPropsList *tlpl,
                                              DP_LayerProps *lp, int index)
{
    DP_ASSERT(lp);
    insert_at(tlpl, index,
              (union DP_TransientLayerPropsElement){
                  .layer_props = DP_layer_props_incref(lp)});
}

void DP_transient_layer_props_list_insert_transient_noinc(
    DP_TransientLayerPropsList *tlpl, DP_TransientLayerProps *tlp, int index)
{
    DP_ASSERT(tlp);
    insert_at(
        tlpl, index,
        (union DP_TransientLayerPropsElement){.transient_layer_props = tlp});
}

void DP_transient_layer_props_list_delete_at(DP_TransientLayerPropsList *tlpl,
                                             int index)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlpl->count);
    DP_layer_props_decref(tlpl->elements[index].layer_props);
    int new_count = --tlpl->count;
    memmove(&tlpl->elements[index], &tlpl->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tlpl->elements[0]));
}

void DP_transient_layer_props_list_merge_at(DP_TransientLayerPropsList *tlpl,
                                            int index)
{
    DP_ASSERT(tlpl);
    DP_ASSERT(DP_atomic_get(&tlpl->refcount) > 0);
    DP_ASSERT(tlpl->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tlpl->count);
    DP_LayerProps *lp = tlpl->elements[index].layer_props;
    DP_TransientLayerProps *tlp = DP_transient_layer_props_new_merge(lp);
    tlpl->elements[index].transient_layer_props = tlp;
    DP_layer_props_decref(lp);
}
