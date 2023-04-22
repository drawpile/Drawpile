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
#include "ops.h"
#include "annotation.h"
#include "annotation_list.h"
#include "canvas_state.h"
#include "document_metadata.h"
#include "draw_context.h"
#include "image.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "layer_routes.h"
#include "paint.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpmsg/blend_mode.h>


DP_CanvasState *DP_ops_canvas_resize(DP_CanvasState *cs,
                                     unsigned int context_id, int top,
                                     int right, int bottom, int left)
{
    int north = -top;
    int west = -left;
    int east = DP_canvas_state_width(cs) + right;
    int south = DP_canvas_state_height(cs) + bottom;
    if (north >= south || west >= east) {
        DP_error_set("Invalid resize: borders are reversed");
        return NULL;
    }

    int width = east + left;
    int height = south + top;
    if (width < 1 || height < 1 || width > INT16_MAX || height > INT16_MAX) {
        DP_error_set("Invalid resize: %dx%d", width, height);
        return NULL;
    }

    DP_debug("Resize: width %d, height %d", width, height);
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_transient_canvas_state_width_set(tcs, width);
    DP_transient_canvas_state_height_set(tcs, height);
    DP_transient_canvas_state_offsets_add(tcs, -left, -top);

    DP_LayerList *ll = DP_transient_canvas_state_layers_noinc(tcs);
    if (DP_layer_list_count(ll) > 0) {
        DP_TransientLayerList *tll =
            DP_layer_list_resize(ll, context_id, top, right, bottom, left);
        DP_transient_canvas_state_transient_layers_set_noinc(tcs, tll);
    }

    return DP_transient_canvas_state_persist(tcs);
}


static void mark_used_layer_ids(DP_DrawContext *dc, int masked_id,
                                DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int layer_id = DP_layer_props_id(lp);
        if ((layer_id & 0xff00) == masked_id) {
            DP_draw_context_id_generator_mark_used(dc, layer_id & 0xff);
        }
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        if (child_lpl) {
            mark_used_layer_ids(dc, masked_id, child_lpl);
        }
    }
}

static DP_TransientLayerPropsList *
clone_layer_props_list(DP_DrawContext *dc, int masked_id,
                       DP_LayerPropsList *lpl);

static DP_TransientLayerProps *
clone_layer_props(DP_DrawContext *dc, int masked_id, DP_LayerProps *lp)
{
    DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
    if (child_lpl) {
        DP_TransientLayerPropsList *child_tlpl =
            clone_layer_props_list(dc, masked_id, child_lpl);
        if (child_tlpl) {
            return DP_transient_layer_props_new_with_children_noinc(lp,
                                                                    child_tlpl);
        }
        else {
            return NULL;
        }
    }
    else {
        return DP_transient_layer_props_new(lp);
    }
}

static DP_TransientLayerPropsList *
clone_layer_props_list(DP_DrawContext *dc, int masked_id,
                       DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_layer_props_list_new_init(count);
    for (int i = count - 1; i >= 0; --i) {
        int base_id = DP_draw_context_id_generator_next(dc);
        DP_TransientLayerProps *tlp;
        if (base_id != -1
            && (tlp = clone_layer_props(dc, masked_id,
                                        DP_layer_props_list_at_noinc(lpl, i)))
                   != NULL) {
            DP_transient_layer_props_id_set(tlp, base_id | masked_id);
            DP_transient_layer_props_list_set_noinc(tlpl, (DP_LayerProps *)tlp,
                                                    i);
        }
        else {
            DP_transient_layer_props_list_decref(tlpl);
            return NULL;
        }
    }
    return tlpl;
}

DP_CanvasState *DP_ops_layer_tree_create(DP_CanvasState *cs, DP_DrawContext *dc,
                                         int layer_id, int source_id,
                                         int target_id, DP_Tile *tile,
                                         bool into, bool group,
                                         const char *title, size_t title_length)
{
    DP_ASSERT(layer_id != 0);

    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    if (DP_layer_routes_search(lr, layer_id)) {
        DP_error_set("Create layer: id %d already exists", layer_id);
        return NULL;
    }

    DP_LayerRoutesEntry *source_lre;
    if (source_id == 0) {
        source_lre = NULL;
    }
    else {
        source_lre = DP_layer_routes_search(lr, source_id);
        if (!source_lre) {
            DP_error_set("Create layer: source id %d not found", source_id);
            return NULL;
        }
    }

    DP_LayerRoutesEntry *target_lre;
    if (target_id == 0) {
        target_lre = NULL;
    }
    else {
        target_lre = DP_layer_routes_search(lr, target_id);
        // TODO: Have a wrong target id actually be an error, don't just throw
        // the layer at the top. For that we need to fix Drawpile to not pass
        // nonsense in the target id field though, which the 2.2 beta does.
        if (into) {
            if (!target_lre) {
                DP_error_set("Create layer: target id %d not found", target_id);
                return NULL;
            }
            else if (!DP_layer_routes_entry_is_group(target_lre)) {
                DP_error_set(
                    "Create layer: attempt to insert into non-group %d",
                    target_id);
                return NULL;
            }
        }
        else if (!target_lre) {
            DP_warn("Create layer: target id %d not found", target_id);
        }
    }

    // If the source is a group, create a copy of it. This can fail if not
    // enough layer ids are available, so we do it before starting to make
    // anything transient.
    // TODO: this generates layer ids on the fly, which isn't too great an idea
    // because it may cause desync that isn't noticed by the retcon code. This
    // could be solved by making Drawpile generate multiple messages when
    // cloning groups, building up the tree with explicit ids rather than in one
    // big command blob.
    DP_TransientLayerProps *tlp;
    if (source_lre) {
        DP_LayerProps *source_lp = DP_layer_routes_entry_props(source_lre, cs);
        DP_LayerPropsList *source_child_lpl =
            DP_layer_props_children_noinc(source_lp);
        if (source_child_lpl) {
            DP_draw_context_id_generator_reset(dc, layer_id & 0xff);
            int masked_id = layer_id & 0xff00;
            mark_used_layer_ids(dc, masked_id,
                                DP_canvas_state_layer_props_noinc(cs));
            DP_TransientLayerPropsList *tlpl =
                clone_layer_props_list(dc, masked_id, source_child_lpl);
            if (tlpl) {
                tlp = DP_transient_layer_props_new_with_children_noinc(
                    source_lp, tlpl);
            }
            else {
                DP_error_set("Create layer: ran out of layer ids cloning group "
                             "with id %d into new group with id %d",
                             source_id, layer_id);
                return NULL;
            }
        }
        else {
            tlp = DP_transient_layer_props_new(source_lp);
        }
        DP_transient_layer_props_id_set(tlp, layer_id);
    }
    else {
        tlp = DP_transient_layer_props_new_init(layer_id, group);
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerList *tll;
    DP_TransientLayerPropsList *tlpl;
    int target_index;
    if (target_lre) {
        // If inserting into a group, use all layer indexes to make the target's
        // children themselves transient. If not, use one less layer index so
        // that the layer list containing the target is transient and we can
        // put something next to it.
        DP_layer_routes_entry_transient_children(target_lre, tcs, into ? 0 : -1,
                                                 1, &tll, &tlpl);
        if (into) {
            target_index = DP_transient_layer_list_count(tll) - 1;
        }
        else {
            target_index =
                DP_transient_layer_props_list_index_by_id(tlpl, target_id) + 1;
            DP_ASSERT(target_index > 0);
        }
    }
    else {
        tll = DP_transient_canvas_state_transient_layers(tcs, 1);
        tlpl = DP_transient_canvas_state_transient_layer_props(tcs, 1);
        target_index = DP_transient_layer_list_count(tll) - 1;
    }

    if (!source_lre) {
        int width = DP_canvas_state_width(cs);
        int height = DP_canvas_state_height(cs);
        if (group) {
            DP_transient_layer_list_insert_transient_group_noinc(
                tll, DP_transient_layer_group_new_init(width, height, 0),
                target_index);
        }
        else {
            DP_transient_layer_list_insert_transient_content_noinc(
                tll, DP_transient_layer_content_new_init(width, height, tile),
                target_index);
        }
    }
    else if (DP_layer_routes_entry_is_group(source_lre)) {
        DP_LayerGroup *lg = DP_layer_routes_entry_group(source_lre, cs);
        DP_transient_layer_list_insert_group_inc(tll, lg, target_index);
    }
    else {
        DP_LayerContent *lc = DP_layer_routes_entry_content(source_lre, cs);
        DP_transient_layer_list_insert_content_inc(tll, lc, target_index);
    }

    DP_transient_layer_props_title_set(tlp, title, title_length);
    DP_transient_layer_props_list_insert_transient_noinc(tlpl, tlp,
                                                         target_index);
    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);

    return DP_transient_canvas_state_persist(tcs);
}


static DP_TransientLayerProps *get_layer_props(DP_TransientCanvasState *tcs,
                                               DP_LayerRoutesEntry *lre,
                                               int sublayer_id)
{
    if (sublayer_id == 0) {
        return DP_layer_routes_entry_transient_props(lre, tcs);
    }
    else {
        DP_TransientLayerContent *tlc =
            DP_layer_routes_entry_transient_content(lre, tcs);
        DP_TransientLayerProps *sub_tlp;
        DP_transient_layer_content_transient_sublayer(tlc, sublayer_id, NULL,
                                                      &sub_tlp);
        return sub_tlp;
    }
}

DP_CanvasState *DP_ops_layer_attributes(DP_CanvasState *cs, int layer_id,
                                        int sublayer_id, uint16_t opacity,
                                        int blend_mode, bool censored,
                                        bool isolated)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (!lre) {
        DP_error_set("Layer attributes: layer id %d not found", layer_id);
        return NULL;
    }
    else if (DP_layer_routes_entry_is_group(lre) && sublayer_id != 0) {
        DP_error_set(
            "Layer attributes: sublayer %d requested, but layer %d is a group",
            sublayer_id, layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerProps *tlp = get_layer_props(tcs, lre, sublayer_id);

    DP_transient_layer_props_opacity_set(tlp, opacity);
    DP_transient_layer_props_blend_mode_set(tlp, blend_mode);
    DP_transient_layer_props_censored_set(tlp, censored);
    DP_transient_layer_props_isolated_set(tlp, isolated);

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_layer_order(DP_CanvasState *cs, DP_DrawContext *dc,
                                   int layer_id_count,
                                   int (*get_layer_id)(void *, int), void *user)
{
    DP_LayerList *ll = DP_canvas_state_layers_noinc(cs);
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    int layer_count = DP_layer_list_count(ll);
    DP_ASSERT(layer_count == DP_layer_props_list_count(lpl));

    DP_TransientLayerList *tll = DP_transient_layer_list_new_init(layer_count);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_layer_props_list_new_init(layer_count);
    DP_TransientCanvasState *tcs =
        DP_transient_canvas_state_new_with_layers_noinc(cs, tll, tlpl);

    int fill = 0;
    for (int i = 0; i < layer_id_count; ++i) {
        int layer_id = get_layer_id(user, i);
        if (DP_transient_layer_props_list_index_by_id(tlpl, layer_id) == -1) {
            int from_index = DP_layer_props_list_index_by_id(lpl, layer_id);
            if (from_index != -1) {
                DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, from_index);
                DP_LayerProps *lp =
                    DP_layer_props_list_at_noinc(lpl, from_index);
                DP_transient_layer_list_set_inc(tll, lle, fill);
                DP_transient_layer_props_list_set_inc(tlpl, lp, fill);
                ++fill;
            }
            else {
                DP_warn("Layer order: unknown layer id %d", layer_id);
            }
        }
        else {
            DP_warn("Layer order: duplicate layer id %d", layer_id);
        }
    }

    // If further layers remain, just move them over in the order they appear.
    for (int i = 0; fill < layer_count && i < layer_count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int layer_id = DP_layer_props_id(lp);
        if (DP_transient_layer_props_list_index_by_id(tlpl, layer_id) == -1) {
            DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
            DP_transient_layer_list_set_inc(tll, lle, fill);
            DP_transient_layer_props_list_set_inc(tlpl, lp, fill);
            ++fill;
        }
    }

    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
    return DP_transient_canvas_state_persist(tcs);
}


struct DP_LayerTreeMoveLayer {
    DP_LayerProps *lp;
    union {
        DP_LayerContent *lc;
        DP_LayerGroup *lg;
    };
};

static bool layer_tree_move_parent_valid(DP_LayerRoutesEntry *layer_lre,
                                         DP_LayerRoutesEntry *parent_lre)
{
    int layer_count = DP_layer_routes_entry_index_count(layer_lre);
    int parent_count = DP_layer_routes_entry_index_count(parent_lre);
    int min_count = DP_min_int(layer_count, parent_count);
    int i;
    for (i = 0; i < min_count; ++i) {
        int layer_index = DP_layer_routes_entry_index_at(layer_lre, i);
        int parent_index = DP_layer_routes_entry_index_at(parent_lre, i);
        if (layer_index != parent_index) {
            break;
        }
    }
    return i != layer_count;
}

static bool layer_tree_move_sibling_valid(DP_LayerRoutesEntry *parent_lre,
                                          DP_LayerRoutesEntry *sibling_lre)
{
    int parent_count =
        parent_lre ? DP_layer_routes_entry_index_count(parent_lre) : 0;
    int sibling_count = DP_layer_routes_entry_index_count(sibling_lre);
    if (parent_count + 1 != sibling_count) {
        return false;
    }

    for (int i = 0; i < parent_count; ++i) {
        int parent_index = DP_layer_routes_entry_index_at(parent_lre, i);
        int sibling_index = DP_layer_routes_entry_index_at(sibling_lre, i);
        if (parent_index != sibling_index) {
            return false;
        }
    }

    return true;
}

static DP_LayerRoutesEntry *layer_tree_move_check_routes(DP_CanvasState *cs,
                                                         int layer_id,
                                                         int parent_id,
                                                         int sibling_id)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *layer_lre = DP_layer_routes_search(lr, layer_id);
    if (!layer_lre) {
        DP_error_set("Layer tree move: id %d not found", layer_id);
        return NULL;
    }

    DP_LayerRoutesEntry *parent_lre;
    if (parent_id == 0) {
        parent_lre = NULL;
    }
    else {
        parent_lre = DP_layer_routes_search(lr, parent_id);
        if (!parent_lre) {
            DP_error_set("Layer tree move: parent id %d not found", parent_id);
            return NULL;
        }
        else if (!DP_layer_routes_entry_is_group(parent_lre)) {
            DP_error_set("Layer tree move: parent id %d is not a group",
                         parent_id);
            return NULL;
        }
        else if (!layer_tree_move_parent_valid(layer_lre, parent_lre)) {
            DP_error_set("Layer tree move: parent %d is child of layer %d",
                         parent_id, layer_id);
            return NULL;
        }
    }

    if (sibling_id != 0) {
        DP_LayerRoutesEntry *sibling_lre =
            DP_layer_routes_search(lr, sibling_id);
        if (!sibling_lre) {
            DP_error_set("Layer tree move: sibling id %d not found",
                         sibling_id);
            return NULL;
        }
        else if (!layer_tree_move_sibling_valid(parent_lre, sibling_lre)) {
            DP_error_set(
                "Layer tree move: sibling id %d not child of parent id %d",
                sibling_id, parent_id);
            return NULL;
        }
    }

    return layer_lre;
}

static struct DP_LayerTreeMoveLayer
layer_tree_move_delete(DP_TransientCanvasState *tcs,
                       DP_LayerRoutesEntry *layer_lre)
{
    DP_TransientLayerList *tll;
    DP_TransientLayerPropsList *tlpl;
    DP_layer_routes_entry_transient_children(layer_lre, tcs, -1, 0, &tll,
                                             &tlpl);

    // We don't need to increment the reference count here because we still hold
    // a reference to the previous canvas state, which keeps the layer alive.
    int last_index = DP_layer_routes_entry_index_last(layer_lre);
    struct DP_LayerTreeMoveLayer ltml;
    ltml.lp = DP_transient_layer_props_list_at_noinc(tlpl, last_index);
    DP_LayerListEntry *lle = DP_transient_layer_list_at_noinc(tll, last_index);
    if (DP_layer_props_children_noinc(ltml.lp)) {
        ltml.lg = DP_layer_list_entry_group_noinc(lle);
    }
    else {
        ltml.lc = DP_layer_list_entry_content_noinc(lle);
    }

    DP_transient_layer_list_delete_at(tll, last_index);
    DP_transient_layer_props_list_delete_at(tlpl, last_index);

    return ltml;
}

void layer_tree_move_insert(DP_TransientCanvasState *tcs,
                            struct DP_LayerTreeMoveLayer ltml, int parent_id,
                            int sibling_id)
{
    DP_LayerRoutes *lr = DP_transient_canvas_state_layer_routes_noinc(tcs);

    DP_TransientLayerList *tll;
    DP_TransientLayerPropsList *tlpl;
    if (parent_id == 0) {
        tll = DP_transient_canvas_state_transient_layers(tcs, 1);
        tlpl = DP_transient_canvas_state_transient_layer_props(tcs, 1);
    }
    else {
        DP_LayerRoutesEntry *parent_lre = DP_layer_routes_search(lr, parent_id);
        DP_layer_routes_entry_transient_children(parent_lre, tcs, 0, 1, &tll,
                                                 &tlpl);
    }

    int index;
    if (sibling_id == 0) {
        // Subtract 1 because an extra element has been reserved for insertion.
        index = DP_transient_layer_list_count(tll) - 1;
    }
    else {
        DP_LayerRoutesEntry *sibling_lre =
            DP_layer_routes_search(lr, sibling_id);
        index = DP_layer_routes_entry_index_last(sibling_lre);
    }

    DP_transient_layer_props_list_insert_inc(tlpl, ltml.lp, index);
    if (DP_layer_props_children_noinc(ltml.lp)) {
        DP_transient_layer_list_insert_group_inc(tll, ltml.lg, index);
    }
    else {
        DP_transient_layer_list_insert_content_inc(tll, ltml.lc, index);
    }
}

DP_CanvasState *DP_ops_layer_tree_move(DP_CanvasState *cs, DP_DrawContext *dc,
                                       int layer_id, int parent_id,
                                       int sibling_id)
{
    DP_LayerRoutesEntry *layer_lre =
        layer_tree_move_check_routes(cs, layer_id, parent_id, sibling_id);
    if (!layer_lre) {
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    struct DP_LayerTreeMoveLayer ltml = layer_tree_move_delete(tcs, layer_lre);
    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
    layer_tree_move_insert(tcs, ltml, parent_id, sibling_id);
    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_layer_retitle(DP_CanvasState *cs, int layer_id,
                                     const char *title, size_t title_length)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (!lre) {
        DP_error_set("Layer retitle: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerProps *tlp = get_layer_props(tcs, lre, 0);
    DP_transient_layer_props_title_set(tlp, title, title_length);
    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_layer_delete(DP_CanvasState *cs, DP_DrawContext *dc,
                                    unsigned int context_id, int layer_id,
                                    bool merge)
{
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    int count = DP_layer_props_list_count(lpl);
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (DP_layer_props_id(lp) == layer_id) {
            if (DP_layer_props_children_noinc(lp)) {
                DP_error_set("Layer delete: layer is a group");
                return NULL;
            }

            int merge_layer_id = 0;
            if (merge) {
                if (i > 0) {
                    merge_layer_id = DP_layer_props_id(
                        DP_layer_props_list_at_noinc(lpl, i - 1));
                }
                else {
                    DP_warn("Layer delete: attempt to merge down bottom layer");
                }
            }

            return DP_ops_layer_tree_delete(cs, dc, context_id, layer_id,
                                            merge_layer_id);
        }
    }

    return NULL;
}

static void merge_group(DP_TransientCanvasState *tcs, DP_LayerRoutesEntry *lre)
{
    DP_TransientLayerList *tll;
    DP_TransientLayerPropsList *tlpl;
    DP_layer_routes_entry_transient_children(lre, tcs, -1, 0, &tll, &tlpl);

    int last_index = DP_layer_routes_entry_index_last(lre);
    DP_LayerProps *lp =
        DP_transient_layer_props_list_at_noinc(tlpl, last_index);
    DP_transient_layer_list_merge_at(tll, lp, last_index);
    DP_transient_layer_props_list_merge_at(tlpl, last_index);
}

static void delete_layer(DP_TransientCanvasState *tcs, unsigned int context_id,
                         DP_LayerRoutesEntry *delete_lre,
                         DP_LayerRoutesEntry *merge_lre)
{
    DP_TransientLayerList *delete_tll;
    DP_TransientLayerPropsList *delete_tlpl;
    DP_layer_routes_entry_transient_children(delete_lre, tcs, -1, 0,
                                             &delete_tll, &delete_tlpl);

    int delete_last_index = DP_layer_routes_entry_index_last(delete_lre);
    DP_LayerProps *delete_lp =
        DP_transient_layer_props_list_at_noinc(delete_tlpl, delete_last_index);

    if (merge_lre) {
        DP_TransientLayerContent *merge_tlc =
            DP_layer_routes_entry_transient_content(merge_lre, tcs);

        uint16_t opacity = DP_layer_props_opacity(delete_lp);
        int blend_mode = DP_layer_props_blend_mode(delete_lp);

        DP_LayerListEntry *delete_lle =
            DP_transient_layer_list_at_noinc(delete_tll, delete_last_index);
        if (DP_layer_list_entry_is_group(delete_lle)) {
            DP_layer_group_merge_to_flat_image(
                DP_layer_list_entry_group_noinc(delete_lle), delete_lp,
                merge_tlc, DP_BIT15, true);
        }
        else {
            DP_transient_layer_content_merge(
                merge_tlc, context_id,
                DP_layer_list_entry_content_noinc(delete_lle), opacity,
                blend_mode);
        }
    }

    DP_transient_layer_list_delete_at(delete_tll, delete_last_index);
    DP_transient_layer_props_list_delete_at(delete_tlpl, delete_last_index);
}

DP_CanvasState *DP_ops_layer_tree_delete(DP_CanvasState *cs, DP_DrawContext *dc,
                                         unsigned int context_id, int layer_id,
                                         int merge_layer_id)
{
    DP_ASSERT(layer_id != 0);
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);

    DP_LayerRoutesEntry *delete_lre = DP_layer_routes_search(lr, layer_id);
    if (!delete_lre) {
        DP_error_set("Layer tree delete: id %d not found", layer_id);
        return NULL;
    }

    DP_LayerRoutesEntry *merge_lre;
    if (merge_layer_id != 0) {
        merge_lre = DP_layer_routes_search(lr, merge_layer_id);
        if (!merge_lre) {
            DP_error_set("Layer tree delete: merge id %d not found",
                         merge_layer_id);
            return NULL;
        }
        else if (DP_layer_routes_entry_is_group(merge_lre)) {
            // Merging a group into itself means collapsing the group.
            if (layer_id != merge_layer_id) {
                DP_error_set("Layer tree delete: merge target %d is a group",
                             merge_layer_id);
                return NULL;
            }
        }
        else if (layer_id == merge_layer_id) {
            DP_error_set(
                "Layer tree delete: invalid merge of layer %d into itself",
                layer_id);
            return NULL;
        }
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);

    if (layer_id == merge_layer_id) {
        merge_group(tcs, delete_lre);
    }
    else {
        delete_layer(tcs, context_id, delete_lre,
                     merge_layer_id == 0 ? NULL : merge_lre);
    }

    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
    DP_transient_canvas_state_timeline_cleanup(tcs);
    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_layer_visibility(DP_CanvasState *cs, int layer_id,
                                        bool visible)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (!lre) {
        DP_error_set("Layer visibility: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerProps *tlp = get_layer_props(tcs, lre, 0);
    DP_transient_layer_props_hidden_set(tlp, !visible);
    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasStateChange DP_ops_put_image(DP_CanvasState *cs,
                                      unsigned int context_id, int layer_id,
                                      int blend_mode, int x, int y, int width,
                                      int height, const unsigned char *image,
                                      size_t image_size)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (!lre) {
        DP_error_set("Put image: id %d not found", layer_id);
        return DP_canvas_state_change_null();
    }
    else if (DP_layer_routes_entry_is_group(lre)) {
        DP_error_set("Put image: id %d is a group", layer_id);
        return DP_canvas_state_change_null();
    }

    DP_Image *img =
        DP_image_new_from_compressed(width, height, image, image_size);
    if (!img) {
        return DP_canvas_state_change_null();
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc =
        DP_layer_routes_entry_transient_content(lre, tcs);
    DP_transient_layer_content_put_image(tlc, context_id, blend_mode, x, y,
                                         img);
    DP_image_free(img);

    return (DP_CanvasStateChange){
        DP_transient_canvas_state_persist(tcs),
        {context_id, layer_id, x + width / 2, y + height / 2}};
}


static bool looks_like_translation_only(DP_Rect src_rect, DP_Quad dst_quad)
{
    DP_Rect dst_bounds = DP_quad_bounds(dst_quad);
    return DP_rect_width(src_rect) == DP_rect_width(dst_bounds)
        && DP_rect_height(src_rect) == DP_rect_height(dst_bounds)
        && dst_quad.x1 < dst_quad.x2;
}

static DP_CanvasState *
move_image(DP_CanvasState *cs, DP_LayerRoutesEntry *src_lre,
           DP_LayerRoutesEntry *dst_lre, unsigned int context_id,
           const DP_Rect *src_rect, DP_Image *mask, DP_Image *src_img,
           int offset_x, int offset_y, DP_Image *dst_img)
{
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *src_tlc =
        DP_layer_routes_entry_transient_content(src_lre, tcs);
    DP_TransientLayerContent *dst_tlc =
        DP_layer_routes_entry_transient_content(dst_lre, tcs);

    if (mask) {
        DP_transient_layer_content_put_image(
            src_tlc, context_id, DP_BLEND_MODE_ERASE, DP_rect_x(*src_rect),
            DP_rect_y(*src_rect), mask);
    }
    else {
        DP_transient_layer_content_fill_rect(
            src_tlc, context_id, DP_BLEND_MODE_REPLACE, src_rect->x1,
            src_rect->y1, src_rect->x2 + 1, src_rect->y2 + 1,
            DP_upixel15_zero());
    }

    DP_transient_layer_content_put_image(
        dst_tlc, context_id, DP_BLEND_MODE_NORMAL, offset_x, offset_y, dst_img);

    if (dst_img != src_img) {
        DP_image_free(dst_img);
    }
    DP_image_free(src_img);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasStateChange DP_ops_move_region(DP_CanvasState *cs, DP_DrawContext *dc,
                                        unsigned int context_id,
                                        int src_layer_id, int dst_layer_id,
                                        const DP_Rect *src_rect,
                                        const DP_Quad *dst_quad,
                                        int interpolation, DP_Image *mask)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *src_lre = DP_layer_routes_search(lr, src_layer_id);
    if (!src_lre) {
        DP_error_set("Move region: source id %d not found", src_layer_id);
        return DP_canvas_state_change_null();
    }
    else if (DP_layer_routes_entry_is_group(src_lre)) {
        DP_error_set("Move region: source id %d is a group", src_layer_id);
        return DP_canvas_state_change_null();
    }

    DP_LayerRoutesEntry *dst_lre;
    if (dst_layer_id == src_layer_id) {
        dst_lre = src_lre;
    }
    else {
        dst_lre = DP_layer_routes_search(lr, dst_layer_id);
        if (!dst_lre) {
            DP_error_set("Move region: target id %d not found", dst_layer_id);
            return DP_canvas_state_change_null();
        }
        else if (DP_layer_routes_entry_is_group(dst_lre)) {
            DP_error_set("Move region: target id %d is a group", dst_layer_id);
            return DP_canvas_state_change_null();
        }
    }

    DP_Image *src_img = DP_layer_content_select(
        DP_layer_routes_entry_content(src_lre, cs), src_rect, mask);

    int offset_x, offset_y;
    DP_Image *dst_img;
    if (looks_like_translation_only(*src_rect, *dst_quad)) {
        offset_x = dst_quad->x1;
        offset_y = dst_quad->y2;
        dst_img = src_img;
    }
    else {
        dst_img = DP_image_transform(src_img, dc, dst_quad, interpolation,
                                     &offset_x, &offset_y);
        if (!dst_img) {
            DP_free(src_img);
            return DP_canvas_state_change_null();
        }
    }

    DP_Rect dst_bounds = DP_quad_bounds(*dst_quad);
    return (DP_CanvasStateChange){
        move_image(cs, src_lre, dst_lre, context_id, src_rect, mask, src_img,
                   offset_x, offset_y, dst_img),
        {context_id, dst_layer_id,
         DP_rect_x(dst_bounds) + DP_rect_width(dst_bounds) / 2,
         DP_rect_y(dst_bounds) + DP_rect_height(dst_bounds) / 2}};
}

DP_CanvasStateChange DP_ops_move_rect(DP_CanvasState *cs,
                                      unsigned int context_id, int src_layer_id,
                                      int dst_layer_id, const DP_Rect *src_rect,
                                      int dst_x, int dst_y, DP_Image *mask)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *src_lre = DP_layer_routes_search(lr, src_layer_id);
    if (!src_lre) {
        DP_error_set("Move rect: source id %d not found", src_layer_id);
        return DP_canvas_state_change_null();
    }
    else if (DP_layer_routes_entry_is_group(src_lre)) {
        DP_error_set("Move rect: source id %d is a group", src_layer_id);
        return DP_canvas_state_change_null();
    }

    DP_LayerRoutesEntry *dst_lre;
    if (dst_layer_id == src_layer_id) {
        dst_lre = src_lre;
    }
    else {
        dst_lre = DP_layer_routes_search(lr, dst_layer_id);
        if (!dst_lre) {
            DP_error_set("Move rect: target id %d not found", dst_layer_id);
            return DP_canvas_state_change_null();
        }
        else if (DP_layer_routes_entry_is_group(dst_lre)) {
            DP_error_set("Move rect: target id %d is a group", dst_layer_id);
            return DP_canvas_state_change_null();
        }
    }

    DP_Image *src_img = DP_layer_content_select(
        DP_layer_routes_entry_content(src_lre, cs), src_rect, mask);

    return (DP_CanvasStateChange){
        move_image(cs, src_lre, dst_lre, context_id, src_rect, mask, src_img,
                   dst_x, dst_y, src_img),
        {context_id, dst_layer_id, dst_x + DP_rect_width(*src_rect) / 2,
         dst_y + DP_rect_height(*src_rect) / 2}};
}


DP_CanvasStateChange DP_ops_fill_rect(DP_CanvasState *cs,
                                      unsigned int context_id, int layer_id,
                                      int blend_mode, int left, int top,
                                      int right, int bottom, DP_UPixel15 pixel)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (!lre) {
        DP_error_set("Fill rect: id %d not found", layer_id);
        return DP_canvas_state_change_null();
    }
    else if (DP_layer_routes_entry_is_group(lre)) {
        DP_error_set("Fill rect: id %d is a group", layer_id);
        return DP_canvas_state_change_null();
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc =
        DP_layer_routes_entry_transient_content(lre, tcs);
    DP_transient_layer_content_fill_rect(tlc, context_id, blend_mode, left, top,
                                         right, bottom, pixel);

    return (DP_CanvasStateChange){
        DP_transient_canvas_state_persist(tcs),
        {context_id, layer_id, (left + right) / 2, (top + bottom)}};
}


DP_CanvasState *DP_ops_put_tile(DP_CanvasState *cs, DP_Tile *tile, int layer_id,
                                int sublayer_id, int x, int y, int repeat)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (!lre) {
        DP_error_set("Put tile: id %d not found", layer_id);
        return NULL;
    }
    else if (DP_layer_routes_entry_is_group(lre)) {
        DP_error_set("Put tile: id %d is a group", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc =
        DP_layer_routes_entry_transient_content(lre, tcs);
    DP_TransientLayerContent *target;
    if (sublayer_id == 0) {
        target = tlc;
    }
    else {
        DP_transient_layer_content_transient_sublayer(tlc, sublayer_id, &target,
                                                      NULL);
    }

    DP_transient_layer_content_put_tile_inc(target, tile, x, y, repeat);

    return DP_transient_canvas_state_persist(tcs);
}


typedef struct DP_PenUpContext {
    unsigned int context_id;
    DP_CanvasState *cs;
    DP_TransientCanvasState *tcs;
} DP_PenUpContext;

static DP_TransientLayerContent *
pen_up_merge_sublayer(DP_PenUpContext *puc, DP_DrawContext *dc,
                      DP_TransientLayerContent *tlc, int sublayer_index)
{
    if (!tlc) {
        if (!puc->tcs) {
            puc->tcs = DP_transient_canvas_state_new(puc->cs);
        }
        int index_count;
        int *indexes = DP_draw_context_layer_indexes(dc, &index_count);
        tlc = DP_layer_routes_entry_indexes_transient_content(
            index_count, indexes, puc->tcs);
    }
    DP_transient_layer_content_merge_sublayer_at(tlc, puc->context_id,
                                                 sublayer_index);
    return tlc;
}

static void pen_up_layer_content(DP_PenUpContext *puc, DP_DrawContext *dc,
                                 int target_sublayer_id, DP_LayerContent *lc)
{
    DP_LayerPropsList *sub_lpl = DP_layer_content_sub_props_noinc(lc);
    DP_TransientLayerContent *tlc = NULL;
    int count = DP_layer_props_list_count(sub_lpl);
    int i = 0;
    while (i < count) {
        DP_LayerProps *sub_lp = DP_layer_props_list_at_noinc(sub_lpl, i);
        int sublayer_id = DP_layer_props_id(sub_lp);
        // An id of 0 means to merge all sublayers, so in that case keep going.
        // Other ids are of individual users, which can only have one sublayer
        // per layer content, so we can break out in that case.
        if (target_sublayer_id == 0 && sublayer_id >= 0) {
            tlc = pen_up_merge_sublayer(puc, dc, tlc, i);
            --count;
        }
        else if (sublayer_id == target_sublayer_id) {
            pen_up_merge_sublayer(puc, dc, NULL, i);
            break;
        }
        else {
            ++i;
        }
    }
}

static void pen_up_layers(DP_PenUpContext *puc, DP_DrawContext *dc,
                          int target_sublayer_id, DP_LayerList *ll)
{
    int count = DP_layer_list_count(ll);
    DP_draw_context_layer_indexes_push(dc);
    for (int i = 0; i < count; ++i) {
        DP_draw_context_layer_indexes_set(dc, i);
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            pen_up_layers(puc, dc, target_sublayer_id,
                          DP_layer_group_children_noinc(lg));
        }
        else {
            DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
            pen_up_layer_content(puc, dc, target_sublayer_id, lc);
        }
    }
    DP_draw_context_layer_indexes_pop(dc);
}

DP_CanvasState *DP_ops_pen_up(DP_CanvasState *cs, DP_DrawContext *dc,
                              unsigned int context_id)
{
    // If the user was drawing in indirect mode, there'll be sublayers with
    // their id to merge. We walk the layer stack and merge them as we encounter
    // them, only creating a new transient canvas state if actually necessary.
    DP_PenUpContext puc = {context_id, cs, NULL};
    int target_sublayer_id = DP_uint_to_int(context_id);
    DP_draw_context_layer_indexes_clear(dc);
    pen_up_layers(&puc, dc, target_sublayer_id,
                  DP_canvas_state_layers_noinc(cs));
    if (puc.tcs) {
        return DP_transient_canvas_state_persist(puc.tcs);
    }
    else {
        return DP_canvas_state_incref(cs);
    }
}


DP_CanvasState *DP_ops_annotation_create(DP_CanvasState *cs, int annotation_id,
                                         int x, int y, int width, int height)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    if (DP_annotation_list_index_by_id(al, annotation_id) != -1) {
        DP_error_set("Annotation create: id %d already exists", annotation_id);
        return NULL;
    }

    int index = DP_annotation_list_count(al);
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, 1);
    DP_Annotation *a = DP_annotation_new(annotation_id, x, y, width, height);
    DP_transient_annotation_list_insert_noinc(tal, a, index);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_annotation_reshape(DP_CanvasState *cs, int annotation_id,
                                          int x, int y, int width, int height)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    int index = DP_annotation_list_index_by_id(al, annotation_id);
    if (index < 0) {
        DP_error_set("Annotation reshape: id %d not found", annotation_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, 0);
    DP_TransientAnnotation *ta =
        DP_transient_annotation_list_transient_at_noinc(tal, index);

    DP_transient_annotation_x_set(ta, x);
    DP_transient_annotation_y_set(ta, y);
    DP_transient_annotation_width_set(ta, width);
    DP_transient_annotation_height_set(ta, height);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_annotation_edit(DP_CanvasState *cs, int annotation_id,
                                       uint32_t background_color, bool protect,
                                       int valign, const char *text,
                                       size_t text_length)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    int index = DP_annotation_list_index_by_id(al, annotation_id);
    if (index < 0) {
        DP_error_set("Annotation edit: id %d not found", annotation_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, 0);
    DP_TransientAnnotation *ta =
        DP_transient_annotation_list_transient_at_noinc(tal, index);

    DP_transient_annotation_background_color_set(ta, background_color);
    DP_transient_annotation_protect_set(ta, protect);
    DP_transient_annotation_valign_set(ta, valign);
    DP_transient_annotation_text_set(ta, text, text_length);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_annotation_delete(DP_CanvasState *cs, int annotation_id)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    int index = DP_annotation_list_index_by_id(al, annotation_id);
    if (index < 0) {
        DP_error_set("Annotation delete: id %d not found", annotation_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, 0);
    DP_transient_annotation_list_delete_at(tal, index);

    return DP_transient_canvas_state_persist(tcs);
}


/*
DP_CanvasStateChange DP_ops_draw_dabs(DP_CanvasState *cs, int sublayer_id,
                                      int sublayer_blend_mode,
                                      int sublayer_opacity,
                                      DP_PaintDrawDabsParams *params)
{
    DP_ASSERT(params);
    DP_ASSERT(sublayer_id >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_blend_mode >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_blend_mode < DP_BLEND_MODE_COUNT);
    DP_ASSERT(sublayer_id == 0 || sublayer_opacity >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_opacity <= DP_BIT15);

    DP_DrawContext *dc = params->draw_context;
    if (!search_layer("Draw dabs", cs, params->draw_context, 1,
                      params->layer_id, ALLOW_LAYERS, NULL)) {
        return DP_canvas_state_change_null();
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc = get_transient_content(tcs, dc, 1);

    DP_TransientLayerContent *target;
    if (sublayer_id == 0) {
        target = tlc;
    }
    else {
        DP_LayerPropsList *lpl =
            DP_transient_layer_content_sub_props_noinc(tlc);
        int sublayer_index = DP_layer_props_list_index_by_id(lpl, sublayer_id);
        if (sublayer_index < 0) {
            DP_TransientLayerProps *tlp;
            DP_transient_layer_content_list_transient_sublayer(tlc, sublayer_id,
                                                               &target, &tlp);
            // Only set these once, when the sublayer is created. They should
            // always be the same values for a single sublayer anyway.
            DP_transient_layer_props_blend_mode_set(tlp, sublayer_blend_mode);
            DP_transient_layer_props_opacity_set(
                tlp, DP_int_to_uint16(sublayer_opacity));
        }
        else {
            DP_transient_layer_content_list_transient_sublayer_at(
                tlc, sublayer_index, &target, NULL);
        }
    }

    DP_UserCursor uc = DP_paint_draw_dabs(params, target);

    return (DP_CanvasStateChange){DP_transient_canvas_state_persist(tcs), uc};
}
*/

DP_CanvasStateChange
DP_ops_draw_dabs(DP_CanvasState *cs, DP_DrawContext *dc,
                 bool (*next)(void *, DP_PaintDrawDabsParams *), void *user)
{
    // Drawing dabs is by far the most common operation and they come in
    // bunches, so we support batching them for the sake of speed. This makes
    // this operation kinda complicated, but the speedup is worth it.
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_TransientCanvasState *tcs = NULL;
    DP_TransientLayerContent *tlc = NULL;
    DP_TransientLayerContent *sub_tlc = NULL;
    int last_layer_id = -1;
    int last_sublayer_id = -1;
    DP_UserCursor uc;

    DP_PaintDrawDabsParams params;
    while (next(user, &params)) {
        if (params.dab_count < 1) {
            continue;
        }

        int blend_mode = params.blend_mode;
        if (!DP_blend_mode_valid_for_brush(blend_mode)) {
            DP_debug("Draw dabs: blend mode %s not applicable to brushes",
                     DP_blend_mode_enum_name_unprefixed(blend_mode));
            continue;
        }

        int layer_id = params.layer_id;
        if (layer_id != last_layer_id) {
            DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
            if (lre && !DP_layer_routes_entry_is_group(lre)) {
                last_layer_id = layer_id;
                last_sublayer_id = -1;
                if (!tcs) {
                    tcs = DP_transient_canvas_state_new(cs);
                }
                tlc = DP_layer_routes_entry_transient_content(lre, tcs);
            }
            else {
                DP_debug("Draw dabs: bad layer id %d", layer_id);
                continue;
            }
        }

        DP_TransientLayerContent *target;
        if (params.indirect) {
            params.blend_mode = DP_BLEND_MODE_ALPHA_DARKEN;
            int sublayer_id = DP_uint_to_int(params.context_id);
            if (last_sublayer_id != sublayer_id) {
                last_sublayer_id = sublayer_id;
                DP_LayerPropsList *lpl =
                    DP_transient_layer_content_sub_props_noinc(tlc);
                int sublayer_index =
                    DP_layer_props_list_index_by_id(lpl, sublayer_id);
                if (sublayer_index < 0) {
                    DP_TransientLayerProps *tlp;
                    DP_transient_layer_content_transient_sublayer(
                        tlc, sublayer_id, &sub_tlc, &tlp);
                    // Only set these once, when the sublayer is created. They
                    // should always be the same values for a single sublayer.
                    DP_transient_layer_props_blend_mode_set(tlp, blend_mode);
                    DP_transient_layer_props_opacity_set(
                        tlp, DP_channel8_to_15(DP_uint32_to_uint8(
                                 (params.color & 0xff000000) >> 24)));
                }
                else {
                    DP_transient_layer_content_transient_sublayer_at(
                        tlc, sublayer_index, &sub_tlc, NULL);
                }
            }
            target = sub_tlc;
        }
        else {
            target = tlc;
        }

        // If multiple different users are involved in this draw dabs operation,
        // we'll only get the last one's cursor out of this. That's probably not
        // noticeable in practice though, since enough of their cursor movements
        // will still come through enough for their markers to show up roughly
        // where they should. So not worth fixing this I think.
        uc = DP_paint_draw_dabs(dc, &params, target);
    }

    if (tcs) {
        return (DP_CanvasStateChange){DP_transient_canvas_state_persist(tcs),
                                      uc};
    }
    else {
        return DP_canvas_state_change_null();
    }
}


DP_CanvasState *DP_ops_track_create(DP_CanvasState *cs, int new_id,
                                    int insert_id, int source_id,
                                    const char *title, size_t title_length)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int track_count = DP_timeline_count(tl);

    int index = 0;
    DP_Track *source = NULL;
    for (int i = 0; i < track_count; ++i) {
        DP_Track *t = DP_timeline_at_noinc(tl, i);
        int track_id = DP_track_id(t);
        if (track_id == new_id) {
            DP_error_set("Track create: id %d already exists", new_id);
            return NULL;
        }
        if (track_id == insert_id) {
            index = i + 1;
        }
        if (track_id == source_id) {
            source = t;
        }
    }

    if (insert_id != 0 && index == 0) {
        DP_error_set("Track create: insert id %d not found", insert_id);
        return NULL;
    }

    if (source_id != 0 && !source) {
        DP_error_set("Track create: source id %d not found", source_id);
        return NULL;
    }

    DP_TransientTrack *tt = source ? DP_transient_track_new(source, 0)
                                   : DP_transient_track_new_init(0);
    DP_transient_track_id_set(tt, new_id);
    DP_transient_track_title_set(tt, title, title_length);

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 1);
    DP_transient_timeline_insert_transient_noinc(ttl, tt, index);
    return DP_transient_canvas_state_persist(tcs);
}


static DP_CanvasState *retitle_track(DP_CanvasState *cs, int index,
                                     const char *title, size_t title_length)
{
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 0);
    DP_TransientTrack *tt =
        DP_transient_timeline_transient_at_noinc(ttl, index, 0);
    DP_transient_track_title_set(tt, title, title_length);
    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_track_retitle(DP_CanvasState *cs, int track_id,
                                     const char *title, size_t title_length)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int track_count = DP_timeline_count(tl);

    for (int i = 0; i < track_count; ++i) {
        DP_Track *t = DP_timeline_at_noinc(tl, i);
        if (DP_track_id(t) == track_id) {
            return retitle_track(cs, i, title, title_length);
        }
    }

    DP_error_set("Track retitle: track %d not found", track_id);
    return NULL;
}


static DP_CanvasState *delete_track(DP_CanvasState *cs, int index)
{
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 0);
    DP_transient_timeline_delete_at(ttl, index);
    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_track_delete(DP_CanvasState *cs, int track_id)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int track_count = DP_timeline_count(tl);

    for (int i = 0; i < track_count; ++i) {
        DP_Track *t = DP_timeline_at_noinc(tl, i);
        if (DP_track_id(t) == track_id) {
            return delete_track(cs, i);
        }
    }

    DP_error_set("Track delete: track %d not found", track_id);
    return NULL;
}

DP_CanvasState *DP_ops_track_order(DP_CanvasState *cs, int track_id_count,
                                   int (*get_track_id)(void *, int), void *user)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int track_count = DP_timeline_count(tl);

    DP_TransientTimeline *ttl = DP_transient_timeline_new_init(track_count);
    DP_TransientCanvasState *tcs =
        DP_transient_canvas_state_new_with_timeline_noinc(cs, ttl);

    int fill = 0;
    for (int i = 0; i < track_id_count; ++i) {
        int track_id = get_track_id(user, i);
        if (DP_transient_timeline_index_by_id(ttl, track_id) == -1) {
            int from_index = DP_timeline_index_by_id(tl, track_id);
            if (from_index != -1) {
                DP_Track *t = DP_timeline_at_noinc(tl, from_index);
                DP_transient_timeline_set_inc(ttl, t, fill);
                ++fill;
            }
            else {
                DP_warn("Track order: unknown track id %d", track_id);
            }
        }
        else {
            DP_warn("Track order: duplicate track id %d", track_id);
        }
    }

    // If further tracks remain, just move them over in the order they appear.
    for (int i = 0; fill < track_count && i < track_count; ++i) {
        DP_Track *t = DP_timeline_at_noinc(tl, i);
        int track_id = DP_track_id(t);
        if (DP_transient_timeline_index_by_id(ttl, track_id) == -1) {
            DP_transient_timeline_set_inc(ttl, t, fill);
            ++fill;
        }
    }

    return DP_transient_canvas_state_persist(tcs);
}


static void insert_or_replace_key_frame(DP_TransientTimeline *ttl, int t_index,
                                        int kf_index, int frame_index,
                                        DP_Track *t, DP_KeyFrame *kf,
                                        bool replace)
{
    if (replace) {
        DP_TransientTrack *tt =
            DP_transient_timeline_transient_at_noinc(ttl, t_index, 0);
        DP_transient_track_replace_noinc(tt, frame_index, kf, kf_index);
    }
    else {
        DP_TransientTrack *tt =
            DP_transient_timeline_transient_at_noinc(ttl, t_index, 1);
        DP_transient_track_insert_noinc(
            tt, frame_index, kf,
            kf_index == -1 ? DP_track_key_frame_count(t) : kf_index);
    }
}

static DP_CanvasState *
set_key_frame(const char *what, DP_CanvasState *cs, int track_id,
              int frame_index,
              DP_KeyFrame *(*get_key_frame)(void *, DP_KeyFrame *), void *user)
{
    DP_DocumentMetadata *dm = DP_canvas_state_metadata_noinc(cs);
    int frame_count = DP_document_metadata_frame_count(dm);
    if (frame_index >= frame_count) {
        DP_error_set("Key frame %s: frame index %d beyond frame count %d", what,
                     frame_index, frame_count);
        return NULL;
    }

    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int t_index = DP_timeline_index_by_id(tl, track_id);
    if (t_index == -1) {
        DP_error_set("Key frame %s: track id %d not found", what, track_id);
        return NULL;
    }

    DP_Track *t = DP_timeline_at_noinc(tl, t_index);
    bool replace;
    int kf_index =
        DP_track_key_frame_search_at_or_after(t, frame_index, &replace);

    DP_KeyFrame *kf = get_key_frame(
        user, replace ? DP_track_key_frame_at_noinc(t, kf_index) : NULL);
    if (!kf) {
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 0);
    insert_or_replace_key_frame(ttl, t_index, kf_index, frame_index, t, kf,
                                replace);

    return DP_transient_canvas_state_persist(tcs);
}

static DP_KeyFrame *get_key_frame_set(void *user, DP_KeyFrame *prev_kf)
{
    int layer_id = *(int *)user;
    if (prev_kf) {
        DP_TransientKeyFrame *tkf = DP_transient_key_frame_new(prev_kf);
        DP_transient_key_frame_layer_id_set(tkf, layer_id);
        return DP_transient_key_frame_persist(tkf);
    }
    else {
        return DP_key_frame_new_init(layer_id);
    }
}

DP_CanvasState *DP_ops_key_frame_set(DP_CanvasState *cs, int track_id,
                                     int frame_index, int layer_id)
{
    return set_key_frame("set", cs, track_id, frame_index, get_key_frame_set,
                         &layer_id);
}

struct KeyFrameCopyParams {
    DP_CanvasState *cs;
    int source_track_id;
    int source_frame_index;
};

static DP_KeyFrame *get_key_frame_copy(void *user,
                                       DP_UNUSED DP_KeyFrame *prev_kf)
{
    struct KeyFrameCopyParams *params = user;
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(params->cs);

    int track_id = params->source_track_id;
    int t_index = DP_timeline_index_by_id(tl, track_id);
    if (t_index == -1) {
        DP_error_set("Key frame copy: source track id %d not found", track_id);
        return NULL;
    }

    DP_Track *t = DP_timeline_at_noinc(tl, t_index);
    int frame_index = params->source_frame_index;
    int kf_index = DP_track_key_frame_search_at(t, frame_index);
    if (kf_index == -1) {
        DP_error_set("Key frame copy: source key frame at %d not found",
                     frame_index);
        return NULL;
    }

    return DP_track_key_frame_at_inc(t, kf_index);
}

DP_CanvasState *DP_ops_key_frame_copy(DP_CanvasState *cs, int track_id,
                                      int frame_index, int source_track_id,
                                      int source_frame_index)
{
    struct KeyFrameCopyParams params = {cs, source_track_id,
                                        source_frame_index};
    return set_key_frame("copy", cs, track_id, frame_index, get_key_frame_copy,
                         &params);
}

DP_CanvasState *DP_ops_key_frame_retitle(DP_CanvasState *cs, int track_id,
                                         int frame_index, const char *title,
                                         size_t title_length)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int t_index = DP_timeline_index_by_id(tl, track_id);
    if (t_index == -1) {
        DP_error_set("Key frame retitle: track id %d not found", track_id);
        return NULL;
    }

    DP_Track *t = DP_timeline_at_noinc(tl, t_index);
    int kf_index = DP_track_key_frame_search_at(t, frame_index);
    if (kf_index == -1) {
        DP_error_set("Key frame retitle: no frame at index %d", frame_index);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 0);
    DP_TransientTrack *tt =
        DP_transient_timeline_transient_at_noinc(ttl, t_index, 0);
    DP_TransientKeyFrame *tkf =
        DP_transient_track_transient_at_noinc(tt, kf_index);

    DP_transient_key_frame_title_set(tkf, title, title_length);

    return DP_transient_canvas_state_persist(tcs);
}

static bool is_valid_key_frame_layer(DP_LayerRoutes *lr,
                                     DP_KeyFrameLayer *buffer, int used,
                                     int layer_id, unsigned int flags)
{
    if (flags == 0) {
        DP_warn("Key frame layer attributes: flags on layer %d are 0",
                layer_id);
        return false;
    }

    for (int i = 0; i < used; ++i) {
        if (buffer[i].layer_id == layer_id) {
            DP_warn("Key frame layer attributes: duplicate layer id %d",
                    layer_id);
            return false;
        }
    }

    if (!DP_layer_routes_search(lr, layer_id)) {
        DP_warn("Key frame layer attributes: layer id %d not found", layer_id);
        return false;
    }

    return true;
}

DP_CanvasState *DP_ops_key_frame_layer_attributes(
    DP_CanvasState *cs, DP_DrawContext *dc, int track_id, int frame_index,
    int count, struct DP_KeyFrameLayer (*get_layer)(void *, int), void *user)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int t_index = DP_timeline_index_by_id(tl, track_id);
    if (t_index == -1) {
        DP_error_set("Key frame layer attributes: track id %d not found",
                     track_id);
        return NULL;
    }

    DP_Track *t = DP_timeline_at_noinc(tl, t_index);
    int kf_index = DP_track_key_frame_search_at(t, frame_index);
    if (kf_index == -1) {
        DP_error_set("Key frame layer attributes: no frame at index %d",
                     frame_index);
        return NULL;
    }

    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_KeyFrameLayer *buffer = DP_draw_context_pool_require(
        dc, sizeof(*buffer) * DP_int_to_size(count));
    int used = 0;
    for (int i = 0; i < count; ++i) {
        DP_KeyFrameLayer kfl = get_layer(user, i);
        if (is_valid_key_frame_layer(lr, buffer, used, kfl.layer_id,
                                     kfl.flags)) {
            buffer[used++] = kfl;
        }
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 0);
    DP_TransientTrack *tt =
        DP_transient_timeline_transient_at_noinc(ttl, t_index, 0);

    DP_KeyFrame *kf = DP_transient_track_key_frame_at_noinc(tt, kf_index);
    DP_TransientKeyFrame *tkf =
        DP_transient_key_frame_new_with_layers(kf, buffer, used);
    DP_transient_track_replace_transient_noinc(tt, frame_index, tkf, kf_index);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_key_frame_delete(DP_CanvasState *cs, int track_id,
                                        int frame_index, int move_track_id,
                                        int move_frame_index)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int t_index = DP_timeline_index_by_id(tl, track_id);
    if (t_index == -1) {
        DP_error_set("Key frame delete: track id %d not found", track_id);
        return NULL;
    }

    DP_Track *t = DP_timeline_at_noinc(tl, t_index);
    int kf_index = DP_track_key_frame_search_at(t, frame_index);
    if (kf_index == -1) {
        DP_error_set("Key frame delete: no frame at index %d", frame_index);
        return NULL;
    }

    int move_t_index;
    if (move_track_id == 0) {
        move_t_index = -1;
    }
    else {
        move_t_index = DP_timeline_index_by_id(tl, move_track_id);
        if (move_t_index == -1) {
            DP_error_set("Key frame delete: move track id %d not found",
                         track_id);
            return NULL;
        }

        DP_DocumentMetadata *dm = DP_canvas_state_metadata_noinc(cs);
        int frame_count = DP_document_metadata_frame_count(dm);
        if (move_frame_index >= frame_count) {
            DP_error_set(
                "Key frame delete: move frame index %d beyond frame count %d",
                move_frame_index, frame_count);
            return NULL;
        }
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 0);

    if (move_t_index != -1) {
        DP_Track *move_t = DP_transient_timeline_at_noinc(ttl, move_t_index);
        bool replace;
        int move_kf_index = DP_track_key_frame_search_at_or_after(
            move_t, move_frame_index, &replace);
        insert_or_replace_key_frame(
            ttl, move_t_index, move_kf_index, move_frame_index, move_t,
            DP_track_key_frame_at_inc(t, kf_index), replace);
    }

    DP_TransientTrack *tt =
        DP_transient_timeline_transient_at_noinc(ttl, t_index, 0);
    DP_transient_track_delete_at(
        tt, DP_transient_track_key_frame_search_at(tt, frame_index));

    return DP_transient_canvas_state_persist(tcs);
}
