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
#include "layer_routes.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpmsg/message.h>
#include <uthash_inc.h>


typedef struct DP_LayerRoutesEntry {
    UT_hash_handle hh;
    bool is_group;
    int layer_id;
    int index_count;
    int indexes[];
} DP_LayerRoutesEntry;


struct DP_LayerRoutes {
    DP_Atomic refcount;
    DP_LayerRoutesEntry *entries;
};


DP_LayerRoutes *DP_layer_routes_new(void)
{
    DP_LayerRoutes *lr = DP_malloc(sizeof(*lr));
    *lr = (DP_LayerRoutes){DP_ATOMIC_INIT(1), NULL};
    return lr;
}

static void insert(DP_LayerRoutes *lr, DP_DrawContext *dc, DP_LayerProps *lp,
                   bool is_group)
{
    int index_count;
    int *indexes = DP_draw_context_layer_indexes(dc, &index_count);

    DP_LayerRoutesEntry *lre = DP_malloc(DP_FLEX_SIZEOF(
        DP_LayerRoutesEntry, indexes, DP_int_to_size(index_count)));
    lre->layer_id = DP_layer_props_id(lp);
    lre->is_group = is_group;
    lre->index_count = index_count;
    for (int i = 0; i < index_count; ++i) {
        lre->indexes[i] = indexes[i];
    }

    HASH_ADD_INT(lr->entries, layer_id, lre);
}

static void index_layers(DP_LayerRoutes *lr, DP_DrawContext *dc,
                         DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    DP_draw_context_layer_indexes_push(dc);
    for (int i = 0; i < count; ++i) {
        DP_draw_context_layer_indexes_set(dc, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        insert(lr, dc, lp, child_lpl);
        if (child_lpl) {
            index_layers(lr, dc, child_lpl);
        }
    }
    DP_draw_context_layer_indexes_pop(dc);
}

DP_LayerRoutes *DP_layer_routes_new_index(DP_LayerPropsList *lpl,
                                          DP_DrawContext *dc)
{
    DP_ASSERT(lpl);
    DP_ASSERT(dc);
    DP_LayerRoutes *lr = DP_layer_routes_new();
    DP_draw_context_layer_indexes_clear(dc);
    index_layers(lr, dc, lpl);
    return lr;
}

DP_LayerRoutes *DP_layer_routes_incref(DP_LayerRoutes *lr)
{
    DP_ASSERT(lr);
    DP_ASSERT(DP_atomic_get(&lr->refcount) > 0);
    DP_atomic_inc(&lr->refcount);
    return lr;
}

DP_LayerRoutes *DP_layer_routes_incref_nullable(DP_LayerRoutes *lr_or_null)
{
    return lr_or_null ? DP_layer_routes_incref(lr_or_null) : NULL;
}

void DP_layer_routes_decref(DP_LayerRoutes *lr)
{
    DP_ASSERT(lr);
    DP_ASSERT(DP_atomic_get(&lr->refcount) > 0);
    if (DP_atomic_dec(&lr->refcount)) {
        DP_LayerRoutesEntry *lre, *tmp;
        HASH_ITER(hh, lr->entries, lre, tmp) {
            HASH_DEL(lr->entries, lre);
            DP_free(lre);
        }
        DP_free(lr);
    }
}

void DP_layer_routes_decref_nullable(DP_LayerRoutes *lr_or_null)
{
    if (lr_or_null) {
        DP_layer_routes_decref(lr_or_null);
    }
}

DP_LayerRoutesEntry *DP_layer_routes_search(DP_LayerRoutes *lr, int layer_id)
{
    DP_ASSERT(lr);
    DP_ASSERT(DP_atomic_get(&lr->refcount) > 0);
    DP_LayerRoutesEntry *lre;
    HASH_FIND_INT(lr->entries, &layer_id, lre);
    return lre;
}


struct DP_MakeLayerTreeOrderContext {
    int source_id;
    int target_id;
    bool into;
    bool below;
    DP_LayerProps *source_lp;
    DP_LayerPropsList *root_lpl;
    int root_id;
    int index;
};

static int count_common_layer_indexes(int source_index_count,
                                      int *source_indexes,
                                      int target_index_count,
                                      int *target_indexes)
{
    int min_count = DP_min_int(source_index_count, target_index_count);
    int i;
    for (i = 0; i < min_count; ++i) {
        if (source_indexes[i] != target_indexes[i]) {
            break;
        }
    }
    return i;
}

static int count_order_layers(DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    int total = count;
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        if (child_lpl) {
            total += count_order_layers(child_lpl);
        }
    }
    return total;
}

static void build_order_layers(struct DP_MakeLayerTreeOrderContext *c,
                               DP_LayerPropsList *lpl, uint16_t *out,
                               uint16_t *parent_child_count);

static void append_order_layer(struct DP_MakeLayerTreeOrderContext *c,
                               uint16_t *out, uint16_t *parent_child_count,
                               DP_LayerProps *lp, bool insert_source)
{
    if (parent_child_count) {
        ++*parent_child_count;
    }
    uint16_t *child_count = &out[c->index++];
    *child_count = 0;
    out[c->index++] = DP_int_to_uint16(DP_layer_props_id(lp));
    DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
    if (insert_source) {
        append_order_layer(c, out, child_count, c->source_lp, false);
        build_order_layers(c, child_lpl, out, child_count);
    }
    else if (child_lpl) {
        build_order_layers(c, child_lpl, out, child_count);
    }
}

static void build_order_layers(struct DP_MakeLayerTreeOrderContext *c,
                               DP_LayerPropsList *lpl, uint16_t *out,
                               uint16_t *parent_child_count)
{
    int count = DP_layer_props_list_count(lpl);
    for (int i = count - 1; i >= 0; --i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int id = DP_layer_props_id(lp);
        if (id == c->target_id) {
            if (c->into) {
                append_order_layer(c, out, parent_child_count, lp, true);
            }
            else if (c->below) {
                append_order_layer(c, out, parent_child_count, lp, false);
                append_order_layer(c, out, parent_child_count, c->source_lp,
                                   false);
            }
            else {
                append_order_layer(c, out, parent_child_count, c->source_lp,
                                   false);
                append_order_layer(c, out, parent_child_count, lp, false);
            }
        }
        else if (id != c->source_id) {
            append_order_layer(c, out, parent_child_count, lp, false);
        }
    }
}

static void set_order_layers(DP_UNUSED int count, uint16_t *out, void *user)
{
    struct DP_MakeLayerTreeOrderContext *c = user;
    if (c->into && c->target_id == c->root_id) {
        append_order_layer(c, out, NULL, c->source_lp, false);
    }
    build_order_layers(c, c->root_lpl, out, NULL);
    DP_ASSERT(c->index == count);
}

DP_Message *DP_layer_routes_layer_tree_order_make(DP_LayerRoutes *lr,
                                                  DP_CanvasState *cs,
                                                  unsigned int context_id,
                                                  int source_id, int target_id,
                                                  bool into, bool below)
{
    DP_debug("Make layer tree order source %d target %d into %d below %d",
             source_id, target_id, into, below);
    if (source_id == target_id) {
        DP_error_set("Make layer tree order: source and target are both %d",
                     source_id);
        return NULL;
    }

    DP_LayerRoutesEntry *source_lre = DP_layer_routes_search(lr, source_id);
    if (!source_lre) {
        DP_error_set("Make layer tree order: source layer %d not found",
                     source_id);
        return NULL;
    }

    DP_LayerRoutesEntry *target_lre = DP_layer_routes_search(lr, target_id);
    if (!target_lre) {
        DP_error_set("Make layer tree order: target layer %d not found",
                     target_id);
        return NULL;
    }
    else if (into && !target_lre->is_group) {
        DP_error_set("Make layer tree order: target %d is not a group",
                     target_id);
        return NULL;
    }

    int source_index_count = source_lre->index_count;
    int *source_indexes = source_lre->indexes;
    int common_count = count_common_layer_indexes(
        source_index_count, source_indexes,
        target_lre->index_count - (into ? 0 : 1), target_lre->indexes);

    if (common_count == source_index_count) {
        DP_error_set("Make layer tree order: target %d is child of source %d",
                     target_id, source_id);
        return NULL;
    }

    DP_LayerProps *lp;
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    for (int i = 0; i < common_count; ++i) {
        lp = DP_layer_props_list_at_noinc(lpl, source_indexes[i]);
        lpl = DP_layer_props_children_noinc(lp);
    }

    int root_id = common_count > 0 ? DP_layer_props_id(lp) : 0;
    DP_LayerProps *source_lp = DP_layer_routes_entry_props(source_lre, cs);
    struct DP_MakeLayerTreeOrderContext params = {
        source_id, target_id, into, below, source_lp, lpl, root_id, 0};

    // The message contains pairs of child count and layer id.
    int layer_pair_count = count_order_layers(lpl) * 2;
    return DP_msg_layer_tree_order_new(context_id, DP_int_to_uint16(root_id),
                                       set_order_layers, layer_pair_count,
                                       &params);
}


int DP_layer_routes_entry_layer_id(DP_LayerRoutesEntry *lre)
{
    DP_ASSERT(lre);
    return lre->layer_id;
}

bool DP_layer_routes_entry_is_group(DP_LayerRoutesEntry *lre)
{
    DP_ASSERT(lre);
    return lre->is_group;
}

int DP_layer_routes_entry_index_count(DP_LayerRoutesEntry *lre)
{
    DP_ASSERT(lre);
    return lre->index_count;
}

int DP_layer_routes_entry_index_at(DP_LayerRoutesEntry *lre, int index)
{
    DP_ASSERT(lre);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < lre->index_count);
    return lre->indexes[index];
}

int DP_layer_routes_entry_index_last(DP_LayerRoutesEntry *lre)
{
    DP_ASSERT(lre);
    return lre->indexes[lre->index_count - 1];
}


static DP_LayerListEntry *get_layer_list_entry(DP_LayerRoutesEntry *lre,
                                               DP_CanvasState *cs)
{
    int *indexes = lre->indexes;
    int group_indexes_count = lre->index_count - 1;
    DP_LayerList *ll = DP_canvas_state_layers_noinc(cs);

    for (int i = 0; i < group_indexes_count; ++i) {
        int group_index = indexes[i];
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, group_index);
        DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
        ll = DP_layer_group_children_noinc(lg);
    }

    int last_index = indexes[group_indexes_count];
    return DP_layer_list_at_noinc(ll, last_index);
}

DP_LayerContent *DP_layer_routes_entry_content(DP_LayerRoutesEntry *lre,
                                               DP_CanvasState *cs)
{
    DP_ASSERT(lre);
    DP_ASSERT(cs);
    return DP_layer_list_entry_content_noinc(get_layer_list_entry(lre, cs));
}

DP_LayerGroup *DP_layer_routes_entry_group(DP_LayerRoutesEntry *lre,
                                           DP_CanvasState *cs)
{
    DP_ASSERT(lre);
    DP_ASSERT(cs);
    return DP_layer_list_entry_group_noinc(get_layer_list_entry(lre, cs));
}

DP_LayerProps *DP_layer_routes_entry_props(DP_LayerRoutesEntry *lre,
                                           DP_CanvasState *cs)
{
    DP_ASSERT(lre);
    DP_ASSERT(cs);

    int *indexes = lre->indexes;
    int group_indexes_count = lre->index_count - 1;
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);

    for (int i = 0; i < group_indexes_count; ++i) {
        int group_index = indexes[i];
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, group_index);
        lpl = DP_layer_props_children_noinc(lp);
    }

    int last_index = indexes[group_indexes_count];
    return DP_layer_props_list_at_noinc(lpl, last_index);
}


void DP_layer_routes_entry_children(DP_LayerRoutesEntry *lre,
                                    DP_CanvasState *cs, DP_LayerList **out_ll,
                                    DP_LayerPropsList **out_lpl)
{
    DP_ASSERT(lre);
    DP_ASSERT(cs);
    DP_ASSERT(out_ll);
    DP_ASSERT(out_lpl);

    int index_count = lre->index_count;
    int *indexes = lre->indexes;
    DP_LayerList *ll = DP_canvas_state_layers_noinc(cs);
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);

    for (int i = 0; i < index_count; ++i) {
        int group_index = indexes[i];

        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, group_index);
        DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
        ll = DP_layer_group_children_noinc(lg);

        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, group_index);
        lpl = DP_layer_props_children_noinc(lp);
    }

    *out_ll = ll;
    *out_lpl = lpl;
}


DP_TransientLayerContent *
DP_layer_routes_entry_indexes_transient_content(int index_count, int *indexes,
                                                DP_TransientCanvasState *tcs)
{
    DP_ASSERT(index_count > 0);
    DP_ASSERT(indexes);
    DP_ASSERT(tcs);

    int group_indexes_count = index_count - 1;
    DP_TransientLayerList *tll =
        DP_transient_canvas_state_transient_layers(tcs, 0);

    for (int i = 0; i < group_indexes_count; ++i) {
        int group_index = indexes[i];
        DP_TransientLayerGroup *tlg =
            DP_transient_layer_list_transient_group_at_noinc(tll, group_index);
        tll = DP_transient_layer_group_transient_children(tlg, 0);
    }

    int last_index = indexes[group_indexes_count];
    return DP_transient_layer_list_transient_content_at_noinc(tll, last_index);
}

DP_TransientLayerContent *
DP_layer_routes_entry_transient_content(DP_LayerRoutesEntry *lre,
                                        DP_TransientCanvasState *tcs)
{
    DP_ASSERT(lre);
    DP_ASSERT(tcs);
    return DP_layer_routes_entry_indexes_transient_content(lre->index_count,
                                                           lre->indexes, tcs);
}


DP_TransientLayerProps *
DP_layer_routes_entry_indexes_transient_props(int index_count, int *indexes,
                                              DP_TransientCanvasState *tcs)
{
    DP_ASSERT(index_count > 0);
    DP_ASSERT(indexes);
    DP_ASSERT(tcs);

    int group_indexes_count = index_count - 1;
    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 0);

    for (int i = 0; i < group_indexes_count; ++i) {
        int group_index = indexes[i];
        DP_TransientLayerProps *tlp =
            DP_transient_layer_props_list_transient_at_noinc(tlpl, group_index);
        tlpl = DP_transient_layer_props_transient_children(tlp, 0);
    }

    int last_index = indexes[group_indexes_count];
    return DP_transient_layer_props_list_transient_at_noinc(tlpl, last_index);
}

DP_TransientLayerProps *
DP_layer_routes_entry_transient_props(DP_LayerRoutesEntry *lre,
                                      DP_TransientCanvasState *tcs)
{
    DP_ASSERT(lre);
    DP_ASSERT(tcs);
    return DP_layer_routes_entry_indexes_transient_props(lre->index_count,
                                                         lre->indexes, tcs);
}


static void to_transient(DP_TransientLayerList **tllp,
                         DP_TransientLayerPropsList **tlplp, int index,
                         int reserve)
{
    DP_TransientLayerGroup *tlg =
        DP_transient_layer_list_transient_group_at_noinc(*tllp, index);
    *tllp = DP_transient_layer_group_transient_children(tlg, reserve);
    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_list_transient_at_noinc(*tlplp, index);
    *tlplp = DP_transient_layer_props_transient_children(tlp, reserve);
}

void DP_layer_routes_entry_transient_children(
    DP_LayerRoutesEntry *lre, DP_TransientCanvasState *tcs, int offset,
    int reserve, DP_TransientLayerList **out_tll,
    DP_TransientLayerPropsList **out_tlpl)
{
    DP_ASSERT(lre);
    DP_ASSERT(tcs);
    DP_ASSERT(offset <= 0);
    DP_ASSERT(reserve >= 0);
    DP_ASSERT(out_tll);
    DP_ASSERT(out_tlpl);

    int index_count = lre->index_count + offset;
    int *indexes = lre->indexes;
    if (index_count == 0) {
        *out_tll = DP_transient_canvas_state_transient_layers(tcs, reserve);
        *out_tlpl =
            DP_transient_canvas_state_transient_layer_props(tcs, reserve);
    }
    else {
        DP_TransientLayerList *tll =
            DP_transient_canvas_state_transient_layers(tcs, 0);
        DP_TransientLayerPropsList *tlpl =
            DP_transient_canvas_state_transient_layer_props(tcs, 0);

        for (int i = 0; i < index_count - 1; ++i) {
            to_transient(&tll, &tlpl, indexes[i], 0);
        }
        to_transient(&tll, &tlpl, indexes[index_count - 1], reserve);

        *out_tll = tll;
        *out_tlpl = tlpl;
    }
}
