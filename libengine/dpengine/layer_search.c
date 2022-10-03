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
 */
#include "layer_search.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include <dpcommon/common.h>


DP_LayerSearch DP_layer_search_make(int layer_id, int layer_list_index)
{
    return (DP_LayerSearch){layer_id, layer_list_index, NULL, NULL};
}


static int match_search(DP_LayerListEntry *lle, DP_LayerProps *lp,
                        DP_DrawContext *dc, int searches_count,
                        DP_LayerSearch *searches)
{
    int matches = 0;
    int layer_id = DP_layer_props_id(lp);
    for (int i = 0; i < searches_count; ++i) {
        DP_LayerSearch *search = &searches[i];
        if (layer_id == search->layer_id) {
            DP_ASSERT(!search->lle);
            DP_ASSERT(!search->lp);
            search->lle = lle;
            search->lp = lp;
            int layer_index_list = search->layer_index_list;
            if (layer_index_list != 0) {
                DP_draw_context_layer_indexes_flush(dc, layer_index_list);
            }
            ++matches;
        }
    }
    return matches;
}

static int search_layer_list(DP_LayerList *ll, DP_LayerPropsList *lpl,
                             DP_DrawContext *dc, int searches_count,
                             DP_LayerSearch *searches, int found);

static int search_children(DP_LayerListEntry *lle, DP_LayerProps *lp,
                           DP_DrawContext *dc, int searches_count,
                           DP_LayerSearch *searches, int found)
{
    DP_ASSERT(lle);
    DP_ASSERT(lp);
    DP_ASSERT(found < searches_count);
    DP_LayerList *ll;
    DP_LayerPropsList *lpl;
    if (DP_layer_list_entry_is_group(lle)) {
        DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
        ll = DP_layer_group_children_noinc(lg);
        lpl = DP_layer_props_children_noinc(lp);
        return search_layer_list(ll, lpl, dc, searches_count, searches, found);
    }
    else {
        // DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
        // ll = DP_layer_content_sub_contents_noinc(lc);
        // lpl = DP_layer_content_sub_props_noinc(lc);
        return found;
    }
}

static int search_layer_list_entry(DP_LayerListEntry *lle, DP_LayerProps *lp,
                                   DP_DrawContext *dc, int searches_count,
                                   DP_LayerSearch *searches, int found)
{
    DP_ASSERT(lle);
    DP_ASSERT(lp);
    DP_ASSERT(found < searches_count);
    found += match_search(lle, lp, dc, searches_count, searches);
    if (found == searches_count) {
        return found; // All layers found, stop searching.
    }
    else {
        return search_children(lle, lp, dc, searches_count, searches, found);
    }
}

static int search_layer_list(DP_LayerList *ll, DP_LayerPropsList *lpl,
                             DP_DrawContext *dc, int searches_count,
                             DP_LayerSearch *searches, int found)
{
    DP_ASSERT(ll);
    DP_ASSERT(lpl);
    DP_ASSERT(found < searches_count);

    int layer_count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == layer_count);
    DP_draw_context_layer_indexes_push(dc);
    for (int i = 0; i < layer_count && found < searches_count; ++i) {
        DP_draw_context_layer_indexes_set(dc, i);
        found = search_layer_list_entry(DP_layer_list_at_noinc(ll, i),
                                        DP_layer_props_list_at_noinc(lpl, i),
                                        dc, searches_count, searches, found);
    }
    DP_draw_context_layer_indexes_pop(dc);

    return found;
}

int DP_layer_search(DP_CanvasState *cs, DP_DrawContext *dc, int searches_count,
                    DP_LayerSearch *searches)
{
    DP_ASSERT(cs);
    DP_ASSERT(searches_count > 0);
    for (int i = 0; i < searches_count; ++i) {
        DP_ASSERT(!searches[i].lle);
        DP_ASSERT(!searches[i].lp);
    }

    DP_draw_context_layer_indexes_clear(dc);
    DP_LayerList *ll = DP_canvas_state_layers_noinc(cs);
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    return search_layer_list(ll, lpl, dc, searches_count, searches, 0);
}


DP_TransientLayerContent *
DP_layer_search_transient_content(DP_TransientCanvasState *tcs,
                                  int layer_index_count, int *layer_indexes)
{
    DP_ASSERT(tcs);
    DP_ASSERT(layer_index_count > 0);
    DP_ASSERT(layer_indexes);

    DP_TransientLayerList *tll =
        DP_transient_canvas_state_transient_layers(tcs, 0);
    int group_indexes_count = layer_index_count - 1;

    for (int i = 0; i < group_indexes_count; ++i) {
        int group_index = layer_indexes[i];
        DP_TransientLayerGroup *tlg =
            DP_transient_layer_list_transient_group_at_noinc(tll, group_index);
        tll = DP_transient_layer_group_transient_children(tlg, 0);
    }

    int last_index = layer_indexes[group_indexes_count];
    return DP_transient_layer_list_transient_content_at_noinc(tll, last_index);
}


DP_TransientLayerProps *
DP_layer_search_transient_props(DP_TransientCanvasState *tcs,
                                int layer_index_count, int *layer_indexes)
{
    DP_ASSERT(tcs);
    DP_ASSERT(layer_index_count > 0);
    DP_ASSERT(layer_indexes);

    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 0);
    int group_indexes_count = layer_index_count - 1;

    for (int i = 0; i < group_indexes_count; ++i) {
        int group_index = layer_indexes[i];
        DP_TransientLayerProps *tlp =
            DP_transient_layer_props_list_transient_at_noinc(tlpl, group_index);
        tlpl = DP_transient_layer_props_transient_children(tlp, 0);
    }

    int last_index = layer_indexes[group_indexes_count];
    return DP_transient_layer_props_list_transient_at_noinc(tlpl, last_index);
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

void DP_layer_search_transient_children(DP_TransientCanvasState *tcs,
                                        int layer_index_count,
                                        int *layer_indexes, int reserve,
                                        DP_TransientLayerList **out_tll,
                                        DP_TransientLayerPropsList **out_tlpl)
{
    DP_ASSERT(tcs);
    DP_ASSERT(layer_index_count >= 0);
    DP_ASSERT(reserve >= 0);
    DP_ASSERT(out_tll);
    DP_ASSERT(out_tlpl);

    if (layer_index_count == 0) {
        *out_tll = DP_transient_canvas_state_transient_layers(tcs, reserve);
        *out_tlpl =
            DP_transient_canvas_state_transient_layer_props(tcs, reserve);
    }
    else {
        DP_TransientLayerList *tll =
            DP_transient_canvas_state_transient_layers(tcs, 0);
        DP_TransientLayerPropsList *tlpl =
            DP_transient_canvas_state_transient_layer_props(tcs, 0);

        for (int i = 0; i < layer_index_count - 1; ++i) {
            to_transient(&tll, &tlpl, layer_indexes[i], 0);
        }
        to_transient(&tll, &tlpl, layer_indexes[layer_index_count - 1],
                     reserve);

        *out_tll = tll;
        *out_tlpl = tlpl;
    }
}
