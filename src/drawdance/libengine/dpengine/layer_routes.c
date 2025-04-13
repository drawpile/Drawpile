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
#include "selection.h"
#include "selection_set.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpmsg/ids.h>
#include <dpmsg/message.h>
#include <uthash_inc.h>


typedef struct DP_LayerRoutesEntry {
    UT_hash_handle hh;
    bool is_group;
    int layer_id;
    DP_LayerRoutesEntry *parent_lre;
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

static DP_LayerRoutesEntry *insert(DP_LayerRoutes *lr, DP_DrawContext *dc,
                                   DP_LayerProps *lp, bool is_group,
                                   DP_LayerRoutesEntry *parent_lre)
{
    int index_count;
    int *indexes = DP_draw_context_layer_indexes(dc, &index_count);

    DP_LayerRoutesEntry *lre = DP_malloc(DP_FLEX_SIZEOF(
        DP_LayerRoutesEntry, indexes, DP_int_to_size(index_count)));
    lre->layer_id = DP_layer_props_id(lp);
    lre->is_group = is_group;
    lre->parent_lre = parent_lre;
    lre->index_count = index_count;
    for (int i = 0; i < index_count; ++i) {
        lre->indexes[i] = indexes[i];
    }

    HASH_ADD_INT(lr->entries, layer_id, lre);
    return lre;
}

static void index_layers(DP_LayerRoutes *lr, DP_DrawContext *dc,
                         DP_LayerPropsList *lpl,
                         DP_LayerRoutesEntry *parent_lre)
{
    int count = DP_layer_props_list_count(lpl);
    DP_draw_context_layer_indexes_push(dc);
    for (int i = 0; i < count; ++i) {
        DP_draw_context_layer_indexes_set(dc, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        DP_LayerRoutesEntry *lre = insert(lr, dc, lp, child_lpl, parent_lre);
        if (child_lpl) {
            index_layers(lr, dc, child_lpl, lre);
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
    index_layers(lr, dc, lpl, NULL);
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

int DP_layer_routes_search_parent_id(DP_LayerRoutes *lr, int layer_id)
{
    DP_ASSERT(lr);
    DP_LayerRoutesEntry *child_lre = DP_layer_routes_search(lr, layer_id);
    if (child_lre) {
        DP_LayerRoutesEntry *lre = DP_layer_routes_entry_parent(child_lre);
        if (lre) {
            return lre->layer_id;
        }
    }
    return 0;
}


static void get_children(int index_count, int *indexes, DP_CanvasState *cs,
                         DP_LayerList **out_ll, DP_LayerPropsList **out_lpl)
{
    DP_ASSERT(cs);
    DP_ASSERT(out_ll || out_lpl);
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

    if (out_ll) {
        *out_ll = ll;
    }
    if (out_lpl) {
        *out_lpl = lpl;
    }
}

static int get_layer_tree_move_parent_id(DP_CanvasState *cs,
                                         DP_LayerRoutesEntry *target_lre)
{
    int target_parent_index = target_lre->index_count - 2;
    int *target_indexes = target_lre->indexes;
    if (target_parent_index >= 0) {
        DP_LayerPropsList *lpl;
        get_children(target_parent_index, target_indexes, cs, NULL, &lpl);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(
            lpl, target_indexes[target_parent_index]);
        return DP_layer_props_id(lp);
    }
    else {
        return 0;
    }
}

static int get_layer_tree_move_sibling_id_above(DP_CanvasState *cs,
                                                DP_LayerRoutesEntry *target_lre)
{
    int target_last_index = target_lre->index_count - 1;
    int *target_indexes = target_lre->indexes;
    int sibling_index = target_indexes[target_last_index] + 1;
    DP_LayerPropsList *lpl;
    get_children(target_last_index, target_indexes, cs, NULL, &lpl);
    if (sibling_index < DP_layer_props_list_count(lpl)) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, sibling_index);
        return DP_layer_props_id(lp);
    }
    else {
        return 0;
    }
}

DP_Message *DP_layer_routes_layer_tree_move_make(DP_LayerRoutes *lr,
                                                 DP_CanvasState *cs,
                                                 unsigned int context_id,
                                                 int source_id, int target_id,
                                                 bool into, bool below)
{
    DP_ASSERT(lr);
    DP_ASSERT(cs);
    DP_debug("Layer tree move source %d target %d into %d below %d", source_id,
             target_id, into, below);

    if (source_id == 0 || target_id == 0 || source_id == target_id) {
        DP_error_set(
            "Make layer tree move: invalid source id %d and target id %d given",
            source_id, target_id);
        return NULL;
    }

    DP_LayerRoutesEntry *source_lre = DP_layer_routes_search(lr, source_id);
    if (!source_lre) {
        DP_error_set("Make layer tree move: source id %d not found", source_id);
        return NULL;
    }

    DP_LayerRoutesEntry *target_lre = DP_layer_routes_search(lr, target_id);
    if (!target_lre) {
        DP_error_set("Make layer tree move: target id %d not found", target_id);
        return NULL;
    }

    int parent_id, sibling_id;
    if (into) {
        parent_id = target_id;
        sibling_id = 0;
    }
    else {
        parent_id = get_layer_tree_move_parent_id(cs, target_lre);
        if (source_id == parent_id) {
            DP_error_set("Make layer tree move: source and parent are both %d",
                         parent_id);
            return NULL;
        }

        if (below) {
            sibling_id = target_id;
        }
        else {
            sibling_id = get_layer_tree_move_sibling_id_above(cs, target_lre);
        }

        if (source_id == sibling_id) {
            DP_error_set("Make layer tree move: source and sibling are both %d",
                         sibling_id);
            return NULL;
        }
        else if (parent_id == sibling_id && parent_id != 0) {
            DP_error_set("Make layer tree move: parent and sibling are both %d",
                         sibling_id);
            return NULL;
        }
    }

    DP_debug("Layer tree move: layer %d, parent %d, sibling %d", source_id,
             parent_id, sibling_id);
    return DP_msg_layer_tree_move_new(context_id, DP_int_to_uint16(source_id),
                                      DP_int_to_uint16(parent_id),
                                      DP_int_to_uint16(sibling_id));
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

int *DP_layer_routes_entry_indexes(DP_LayerRoutesEntry *lre, int *out_count)
{
    DP_ASSERT(lre);
    if (out_count) {
        *out_count = lre->index_count;
    }
    return lre->indexes;
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

DP_LayerListEntry *DP_layer_routes_entry_layer(DP_LayerRoutesEntry *lre,
                                               DP_CanvasState *cs)
{
    DP_ASSERT(lre);
    DP_ASSERT(cs);
    return get_layer_list_entry(lre, cs);
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
    get_children(lre->index_count, lre->indexes, cs, out_ll, out_lpl);
}

DP_LayerRoutesEntry *DP_layer_routes_entry_parent(DP_LayerRoutesEntry *lre)
{
    DP_ASSERT(lre);
    return lre->parent_lre;
}

void DP_layer_routes_entry_parent_opacity_tint(DP_LayerRoutesEntry *lre,
                                               DP_CanvasState *cs,
                                               uint16_t *out_parent_opacity,
                                               DP_UPixel8 *out_parent_tint)
{
    DP_ASSERT(lre);
    DP_ASSERT(cs);
    DP_ASSERT(out_parent_opacity);
    DP_ASSERT(out_parent_tint);

    int *indexes = lre->indexes;
    int group_indexes_count = lre->index_count - 1;
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    uint16_t parent_opacity = DP_BIT15;
    DP_UPixel8 parent_tint = {0};

    for (int i = 0; i < group_indexes_count; ++i) {
        int group_index = indexes[i];
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, group_index);
        lpl = DP_layer_props_children_noinc(lp);
        uint16_t sketch_opacity = DP_layer_props_sketch_opacity(lp);
        if (sketch_opacity == 0) {
            parent_opacity =
                DP_fix15_mul(parent_opacity, DP_layer_props_opacity(lp));
        }
        else {
            parent_opacity = DP_fix15_mul(parent_opacity, sketch_opacity);
            uint32_t tint = DP_layer_props_sketch_tint(lp);
            if (tint != 0) {
                parent_tint.color = tint;
            }
        }
    }

    *out_parent_opacity = parent_opacity;
    *out_parent_tint = parent_tint;
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


DP_LayerRoutesSelEntry DP_layer_routes_search_sel(DP_LayerRoutes *lr,
                                                  DP_CanvasState *cs,
                                                  int layer_or_selection_id)
{
    DP_ASSERT(lr);
    DP_ASSERT(cs);
    DP_LayerRoutesSelEntry lrse;
    lrse.is_selection = DP_layer_id_selection_ids(
        layer_or_selection_id, &lrse.sel.context_id, &lrse.sel.selection_id);
    if (lrse.is_selection) {
        DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
        lrse.sel.index =
            ss ? DP_selection_set_search_index(ss, lrse.sel.context_id,
                                               lrse.sel.selection_id)
               : -1;
        lrse.exists = lrse.sel.index >= 0;
    }
    else {
        lrse.lre = DP_layer_routes_search(lr, layer_or_selection_id);
        lrse.exists = lrse.lre != NULL;
    }
    return lrse;
}

DP_LayerRoutesSelEntry
DP_layer_routes_sel_entry_from_selection_index(DP_CanvasState *cs, int index)
{
    DP_ASSERT(cs);
    DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
    DP_Selection *sel = DP_selection_set_at_noinc(ss, index);
    return (DP_LayerRoutesSelEntry){
        true,
        true,
        {.sel = {DP_selection_context_id(sel), DP_selection_id(sel), index}}};
}

bool DP_layer_routes_sel_entry_is_valid_source(DP_LayerRoutesSelEntry *lrse)
{
    DP_ASSERT(lrse);
    return lrse->exists && (lrse->is_selection || !lrse->lre->is_group);
}

bool DP_layer_routes_sel_entry_is_valid_target(DP_LayerRoutesSelEntry *lrse)
{
    DP_ASSERT(lrse);
    if (lrse->is_selection) {
        return lrse->exists || lrse->sel.selection_id > 0;
    }
    else {
        return lrse->exists && !lrse->lre->is_group;
    }
}

bool DP_layer_routes_sel_entry_is_group(DP_LayerRoutesSelEntry *lrse)
{
    DP_ASSERT(lrse);
    return !lrse->is_selection && DP_layer_routes_entry_is_group(lrse->lre);
}

int *DP_layer_routes_sel_entry_indexes(DP_LayerRoutesSelEntry *lrse,
                                       int *out_count)
{
    DP_ASSERT(lrse);
    DP_ASSERT(!lrse->is_selection);
    return DP_layer_routes_entry_indexes(lrse->lre, out_count);
}

DP_LayerContent *DP_layer_routes_sel_entry_content(DP_LayerRoutesSelEntry *lrse,
                                                   DP_CanvasState *cs)
{
    DP_ASSERT(lrse);
    DP_ASSERT(lrse->exists);
    if (lrse->is_selection) {
        DP_ASSERT(lrse->sel.index >= 0);
        DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
        DP_ASSERT(ss);
        DP_Selection *sel = DP_selection_set_at_noinc(ss, lrse->sel.index);
        return DP_selection_content_noinc(sel);
    }
    else {
        return DP_layer_routes_entry_content(lrse->lre, cs);
    }
}

DP_TransientLayerContent *
DP_layer_routes_sel_entry_transient_content(DP_LayerRoutesSelEntry *lrse,
                                            DP_TransientCanvasState *tcs)
{
    DP_ASSERT(lrse);
    if (lrse->is_selection) {
        DP_TransientSelection *tsel;
        if (lrse->exists) {
            DP_TransientSelectionSet *tss =
                DP_transient_canvas_state_transient_selection_set_noinc_nullable(
                    tcs, 0);
            tsel = DP_transient_selection_set_transient_at_noinc(
                tss, lrse->sel.index);
        }
        else {
            DP_TransientSelectionSet *tss =
                DP_transient_canvas_state_transient_selection_set_noinc_nullable(
                    tcs, 1);
            tsel = DP_transient_selection_new_init(
                lrse->sel.context_id, lrse->sel.selection_id,
                DP_transient_canvas_state_width(tcs),
                DP_transient_canvas_state_height(tcs));
            int index = DP_transient_selection_set_count(tss) - 1;
            DP_transient_selection_set_insert_transient_at_noinc(tss, index,
                                                                 tsel);
            lrse->exists = true;
            lrse->sel.index = index;
        }
        return DP_transient_selection_transient_content_noinc(tsel);
    }
    else {
        DP_ASSERT(lrse->exists);
        return DP_layer_routes_entry_transient_content(lrse->lre, tcs);
    }
}
