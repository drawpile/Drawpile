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
#include "selection.h"
#include "selection_set.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include "user_cursors.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/ids.h>


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
    if (!DP_canvas_state_dimensions_in_bounds(width, height)) {
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

    if (left > 0 || top > 0) {
        DP_AnnotationList *al =
            DP_transient_canvas_state_annotations_noinc(tcs);
        int annotation_count = DP_annotation_list_count(al);
        if (annotation_count > 0) {
            DP_TransientAnnotationList *tal =
                DP_transient_canvas_state_transient_annotations(tcs, 0);
            for (int i = 0; i < annotation_count; ++i) {
                DP_TransientAnnotation *ta =
                    DP_transient_annotation_list_transient_at_noinc(tal, i);
                DP_transient_annotation_x_set(ta, DP_transient_annotation_x(ta)
                                                      + left);
                DP_transient_annotation_y_set(ta, DP_transient_annotation_y(ta)
                                                      + top);
            }
        }
    }

    DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
    if (ss) {
        DP_TransientSelectionSet *tss =
            DP_selection_set_resize(ss, top, right, bottom, left);
        if (tss) {
            DP_transient_canvas_state_transient_selections_set_noinc(tcs, tss);
        }
        else {
            DP_transient_canvas_state_transient_selections_clear(tcs);
        }
    }

    return DP_transient_canvas_state_persist(tcs);
}


static void mark_used_layer_ids(DP_DrawContext *dc, unsigned int context_id,
                                DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int layer_id = DP_layer_props_id(lp);
        if (DP_layer_id_owner(layer_id, context_id)) {
            DP_draw_context_id_generator_mark_used(
                dc, DP_layer_id_element_id(layer_id));
        }
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        if (child_lpl) {
            mark_used_layer_ids(dc, context_id, child_lpl);
        }
    }
}

static DP_TransientLayerPropsList *
clone_layer_props_list(DP_DrawContext *dc, unsigned int context_id,
                       DP_LayerPropsList *lpl);

static DP_TransientLayerProps *clone_layer_props(DP_DrawContext *dc,
                                                 unsigned int context_id,
                                                 DP_LayerProps *lp)
{
    DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
    if (child_lpl) {
        DP_TransientLayerPropsList *child_tlpl =
            clone_layer_props_list(dc, context_id, child_lpl);
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
clone_layer_props_list(DP_DrawContext *dc, unsigned int context_id,
                       DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_layer_props_list_new_init(count);
    for (int i = count - 1; i >= 0; --i) {
        int element_id = DP_draw_context_id_generator_next(dc);
        DP_TransientLayerProps *tlp;
        if (element_id != -1
            && (tlp = clone_layer_props(dc, context_id,
                                        DP_layer_props_list_at_noinc(lpl, i)))
                   != NULL) {
            DP_transient_layer_props_id_set(
                tlp, DP_layer_id_make(context_id, element_id));
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
            return NULL;
        }
    }

    // If the source is a group, create a copy of it. This can fail if not
    // enough layer ids are available, so we do it before starting to make
    // anything transient.
    DP_TransientLayerProps *tlp;
    if (source_lre) {
        DP_LayerProps *source_lp = DP_layer_routes_entry_props(source_lre, cs);
        DP_LayerPropsList *source_child_lpl =
            DP_layer_props_children_noinc(source_lp);
        if (source_child_lpl) {
            DP_draw_context_id_generator_reset(
                dc, DP_layer_id_element_id(layer_id));
            unsigned int context_id = DP_layer_id_context_id(layer_id);
            mark_used_layer_ids(dc, context_id,
                                DP_canvas_state_layer_props_noinc(cs));
            DP_TransientLayerPropsList *tlpl =
                clone_layer_props_list(dc, context_id, source_child_lpl);
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
                                        bool isolated, bool clip)
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
    DP_transient_layer_props_clip_set(tlp, clip);

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

        // Adjust the blend mode according to the clipping group state. Merging
        // a clipped layer into an unclipped layer should preserve alpha,
        // whereas merging a clipped layer into another clipped layer should
        // affect it. This will give correct results when merging layers down.
        DP_BlendMode alpha_affecting;
        DP_BlendMode alpha_preserving;
        if (DP_layer_props_clip(delete_lp)
            && DP_blend_mode_alpha_preserve_pair(blend_mode, &alpha_affecting,
                                                 &alpha_preserving)) {
            if (DP_layer_props_clip(DP_layer_routes_entry_props(
                    merge_lre, (DP_CanvasState *)tcs))) {
                blend_mode = (int)alpha_affecting;
            }
            else {
                blend_mode = (int)alpha_preserving;
            }
        }

        DP_LayerListEntry *delete_lle =
            DP_transient_layer_list_at_noinc(delete_tll, delete_last_index);
        if (DP_layer_list_entry_is_group(delete_lle)) {
            DP_layer_group_merge_to_flat_image(
                DP_layer_list_entry_group_noinc(delete_lle), delete_lp,
                merge_tlc, DP_BIT15, true, false, false);
        }
        else {
            DP_transient_layer_content_merge(
                merge_tlc, context_id,
                DP_layer_list_entry_content_noinc(delete_lle), opacity,
                blend_mode, false);
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


DP_CanvasState *DP_ops_put_image(DP_CanvasState *cs,
                                 DP_UserCursors *ucs_or_null,
                                 unsigned int context_id, int layer_id,
                                 int blend_mode, int x, int y, DP_Image *img)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesSelEntry lrse = DP_layer_routes_search_sel(lr, cs, layer_id);
    if (!DP_layer_routes_sel_entry_is_valid_target(&lrse)) {
        DP_error_set("Put image: invalid target id %d", layer_id);
        return NULL;
    }

    if (ucs_or_null && !lrse.is_selection) {
        DP_user_cursors_activate(ucs_or_null, context_id);
        DP_user_cursors_move(ucs_or_null, context_id, layer_id,
                             x + DP_image_width(img) / 2,
                             y + DP_image_height(img) / 2);
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc =
        DP_layer_routes_sel_entry_transient_content(&lrse, tcs);
    DP_transient_layer_content_put_image(tlc, context_id, blend_mode, x, y,
                                         img);
    DP_image_free(img);
    return DP_transient_canvas_state_persist(tcs);
}


static bool looks_like_translation_only(DP_Rect src_rect, DP_Quad dst_quad)
{
    DP_Rect dst_bounds = DP_quad_bounds(dst_quad);
    return DP_rect_width(src_rect) == DP_rect_width(dst_bounds)
        && DP_rect_height(src_rect) == DP_rect_height(dst_bounds)
        && dst_quad.x1 < dst_quad.x2;
}

static DP_CanvasState *
move_image(DP_CanvasState *cs, DP_LayerRoutesSelEntry *src_lrse,
           DP_LayerRoutesSelEntry *dst_lrse, unsigned int context_id,
           const DP_Rect *src_rect, DP_Image *mask, DP_Image *src_img,
           int offset_x, int offset_y, int blend_mode, DP_Image *dst_img)
{
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *src_tlc =
        DP_layer_routes_sel_entry_transient_content(src_lrse, tcs);
    DP_TransientLayerContent *dst_tlc =
        DP_layer_routes_sel_entry_transient_content(dst_lrse, tcs);

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

    DP_transient_layer_content_put_image(dst_tlc, context_id, blend_mode,
                                         offset_x, offset_y, dst_img);

    if (dst_img != src_img) {
        DP_image_free(dst_img);
    }
    DP_image_free(src_img);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_transform_region(
    DP_CanvasState *cs, DP_DrawContext *dc, DP_UserCursors *ucs_or_null,
    unsigned int context_id, int src_layer_id, int dst_layer_id,
    const DP_Rect *src_rect, const DP_Quad *dst_quad, int interpolation,
    int blend_mode, uint8_t opacity, DP_Image *mask)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesSelEntry src_lrse =
        DP_layer_routes_search_sel(lr, cs, src_layer_id);
    if (!DP_layer_routes_sel_entry_is_valid_source(&src_lrse)) {
        DP_error_set("Move region: invalid source id %d", src_layer_id);
        return NULL;
    }

    DP_LayerRoutesSelEntry dst_lrse_buffer;
    DP_LayerRoutesSelEntry *dst_lrse;
    if (dst_layer_id == src_layer_id) {
        dst_lrse = &src_lrse;
    }
    else {
        dst_lrse_buffer = DP_layer_routes_search_sel(lr, cs, dst_layer_id);
        dst_lrse = &dst_lrse_buffer;
    }

    if (!DP_layer_routes_sel_entry_is_valid_target(dst_lrse)) {
        DP_error_set("Move region: invalid target id %d", dst_layer_id);
        return NULL;
    }

    DP_Image *src_img = DP_layer_content_select(
        DP_layer_routes_sel_entry_content(&src_lrse, cs), src_rect, mask,
        DP_channel8_to_15(opacity));

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
            return NULL;
        }
    }

    if (ucs_or_null && (!src_lrse.is_selection || !dst_lrse->is_selection)) {
        DP_user_cursors_activate(ucs_or_null, context_id);
        DP_Rect dst_bounds = DP_quad_bounds(*dst_quad);
        DP_user_cursors_move(
            ucs_or_null, context_id, dst_layer_id,
            DP_rect_x(dst_bounds) + DP_rect_width(dst_bounds) / 2,
            DP_rect_y(dst_bounds) + DP_rect_height(dst_bounds) / 2);
    }

    return move_image(cs, &src_lrse, dst_lrse, context_id, src_rect, mask,
                      src_img, offset_x, offset_y, blend_mode, dst_img);
}

DP_CanvasState *DP_ops_move_rect(DP_CanvasState *cs,
                                 DP_UserCursors *ucs_or_null,
                                 unsigned int context_id, int src_layer_id,
                                 int dst_layer_id, const DP_Rect *src_rect,
                                 int dst_x, int dst_y, int blend_mode,
                                 uint8_t opacity, DP_Image *mask)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesSelEntry src_lrse =
        DP_layer_routes_search_sel(lr, cs, src_layer_id);
    if (!DP_layer_routes_sel_entry_is_valid_source(&src_lrse)) {
        DP_error_set("Move rect: invalid source id %d", src_layer_id);
        return NULL;
    }

    DP_LayerRoutesSelEntry dst_lrse_buffer;
    DP_LayerRoutesSelEntry *dst_lrse;
    if (dst_layer_id == src_layer_id) {
        dst_lrse = &src_lrse;
    }
    else {
        dst_lrse_buffer = DP_layer_routes_search_sel(lr, cs, dst_layer_id);
        dst_lrse = &dst_lrse_buffer;
    }

    if (!DP_layer_routes_sel_entry_is_valid_target(dst_lrse)) {
        DP_error_set("Move rect: invalid target id %d", dst_layer_id);
        return NULL;
    }

    if (ucs_or_null && (!src_lrse.is_selection || !dst_lrse->is_selection)) {
        DP_user_cursors_activate(ucs_or_null, context_id);
        DP_user_cursors_move(ucs_or_null, context_id, dst_layer_id,
                             dst_x + DP_rect_width(*src_rect) / 2,
                             dst_y + DP_rect_height(*src_rect) / 2);
    }

    DP_Image *src_img = DP_layer_content_select(
        DP_layer_routes_sel_entry_content(&src_lrse, cs), src_rect, mask,
        DP_channel8_to_15(opacity));
    return move_image(cs, &src_lrse, dst_lrse, context_id, src_rect, mask,
                      src_img, dst_x, dst_y, blend_mode, src_img);
}


DP_CanvasState *DP_ops_fill_rect(DP_CanvasState *cs,
                                 DP_UserCursors *ucs_or_null,
                                 unsigned int context_id, int layer_id,
                                 int blend_mode, int left, int top, int right,
                                 int bottom, DP_UPixel15 pixel)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesSelEntry lrse = DP_layer_routes_search_sel(lr, cs, layer_id);
    if (!DP_layer_routes_sel_entry_is_valid_target(&lrse)) {
        DP_error_set("Fill rect: invalid target id %d", layer_id);
        return NULL;
    }

    if (ucs_or_null && !lrse.is_selection) {
        DP_user_cursors_activate(ucs_or_null, context_id);
        DP_user_cursors_move(ucs_or_null, context_id, layer_id,
                             (left + right) / 2, (top + bottom) / 2);
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc =
        DP_layer_routes_sel_entry_transient_content(&lrse, tcs);
    DP_transient_layer_content_fill_rect(tlc, context_id, blend_mode, left, top,
                                         right, bottom, pixel);
    return DP_transient_canvas_state_persist(tcs);
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
pen_up_merge_sublayer(DP_PenUpContext *puc, int index_count, int *indexes,
                      DP_TransientLayerContent *tlc, int sublayer_index)
{
    if (!tlc) {
        if (!puc->tcs) {
            puc->tcs = DP_transient_canvas_state_new(puc->cs);
        }
        tlc = DP_layer_routes_entry_indexes_transient_content(
            index_count, indexes, puc->tcs);
    }
    DP_transient_layer_content_merge_sublayer_at(tlc, puc->context_id,
                                                 sublayer_index);
    return tlc;
}

static void pen_up_layer(DP_PenUpContext *puc, int index_count, int *indexes,
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
            tlc = pen_up_merge_sublayer(puc, index_count, indexes, tlc, i);
            --count;
        }
        else if (sublayer_id == target_sublayer_id) {
            pen_up_merge_sublayer(puc, index_count, indexes, NULL, i);
            break;
        }
        else {
            ++i;
        }
    }
}

static DP_TransientLayerContent *pen_up_merge_sublayer_selection(
    DP_PenUpContext *puc, DP_LayerRoutesSelEntry *lrse,
    DP_TransientLayerContent *tlc, int sublayer_index)
{
    if (!tlc) {
        if (!puc->tcs) {
            puc->tcs = DP_transient_canvas_state_new(puc->cs);
        }
        tlc = DP_layer_routes_sel_entry_transient_content(lrse, puc->tcs);
    }
    DP_transient_layer_content_merge_sublayer_at(tlc, puc->context_id,
                                                 sublayer_index);
    return tlc;
}

static void pen_up_selection(DP_PenUpContext *puc, DP_LayerRoutesSelEntry *lrse,
                             int target_sublayer_id)
{
    DP_LayerContent *lc = DP_layer_routes_sel_entry_content(lrse, puc->cs);
    DP_LayerPropsList *sub_lpl = DP_layer_content_sub_props_noinc(lc);
    DP_TransientLayerContent *tlc = NULL;
    int count = DP_layer_props_list_count(sub_lpl);
    int i = 0;
    while (i < count) {
        DP_LayerProps *sub_lp = DP_layer_props_list_at_noinc(sub_lpl, i);
        int sublayer_id = DP_layer_props_id(sub_lp);
        // An id of 0 means to merge all sublayers, see above.
        if (target_sublayer_id == 0 && sublayer_id >= 0) {
            tlc = pen_up_merge_sublayer_selection(puc, lrse, tlc, i);
            --count;
        }
        else if (sublayer_id == target_sublayer_id) {
            pen_up_merge_sublayer_selection(puc, lrse, NULL, i);
            break;
        }
        else {
            ++i;
        }
    }
}

static void pen_up_all_layers(DP_PenUpContext *puc, DP_DrawContext *dc,
                              int target_sublayer_id, DP_LayerList *ll)
{
    int count = DP_layer_list_count(ll);
    int index_count;
    int *indexes = DP_draw_context_layer_indexes_push_get(dc, &index_count);
    for (int i = 0; i < count; ++i) {
        DP_draw_context_layer_indexes_set(dc, i);
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            pen_up_all_layers(puc, dc, target_sublayer_id,
                              DP_layer_group_children_noinc(lg));
        }
        else {
            DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
            pen_up_layer(puc, index_count, indexes, target_sublayer_id, lc);
        }
    }
    DP_draw_context_layer_indexes_pop(dc);
}

static void pen_up_all_selections(DP_PenUpContext *puc, int target_sublayer_id)
{
    DP_CanvasState *cs = puc->cs;
    DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
    if (ss) {
        int count = DP_selection_set_count(ss);
        for (int i = 0; i < count; ++i) {
            DP_LayerRoutesSelEntry lrse =
                DP_layer_routes_sel_entry_from_selection_index(cs, i);
            pen_up_selection(puc, &lrse, target_sublayer_id);
        }
    }
}

static void pen_up_single(DP_PenUpContext *puc, int layer_id,
                          int target_sublayer_id)
{
    DP_CanvasState *cs = puc->cs;
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesSelEntry lrse = DP_layer_routes_search_sel(lr, cs, layer_id);
    if (DP_layer_routes_sel_entry_is_valid_source(&lrse)) {
        if (lrse.is_selection) {
            pen_up_selection(puc, &lrse, target_sublayer_id);
        }
        else {
            int index_count;
            int *indexes =
                DP_layer_routes_sel_entry_indexes(&lrse, &index_count);
            DP_LayerContent *lc = DP_layer_routes_sel_entry_content(&lrse, cs);
            pen_up_layer(puc, index_count, indexes, target_sublayer_id, lc);
        }
    }
}

DP_CanvasState *DP_ops_pen_up(DP_CanvasState *cs, DP_DrawContext *dc,
                              DP_UserCursors *ucs_or_null,
                              unsigned int context_id, int layer_id)
{
    if (ucs_or_null) {
        DP_user_cursors_pen_up(ucs_or_null, context_id);
    }

    // If the user was drawing in indirect mode, there'll be sublayers with
    // their id to merge. We walk the layer stack and merge them as we encounter
    // them, only creating a new transient canvas state if actually necessary.
    DP_PenUpContext puc = {context_id, cs, NULL};
    int target_sublayer_id = DP_uint_to_int(context_id);
    if (layer_id == 0) { // All layers.
        DP_draw_context_layer_indexes_clear(dc);
        pen_up_all_layers(&puc, dc, target_sublayer_id,
                          DP_canvas_state_layers_noinc(cs));
        pen_up_all_selections(&puc, target_sublayer_id);
    }
    else {
        pen_up_single(&puc, layer_id, target_sublayer_id);
    }
    return puc.tcs ? DP_transient_canvas_state_persist(puc.tcs)
                   : DP_canvas_state_incref(cs);
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
                                       bool alias, bool rasterize, int valign,
                                       const char *text, size_t text_length)
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
    DP_transient_annotation_alias_set(ta, alias);
    DP_transient_annotation_rasterize_set(ta, rasterize);
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


static DP_Selection *draw_dabs_search_selection(DP_CanvasState *cs,
                                                unsigned int context_id,
                                                int selection_id)
{
    DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
    return ss ? DP_selection_set_search_noinc(ss, context_id, selection_id)
              : NULL;
}

static void draw_dabs_adjust_indirect_mode(DP_PaintDrawDabsParams *params,
                                           DP_TransientLayerProps *sub_tlp,
                                           int alternate_blend_mode)
{
    uint32_t existing_color = DP_transient_layer_props_sketch_tint(sub_tlp);
    if (existing_color & (uint32_t)0xff000000u) {
        // Color has already changed once, so keep using the alternate mode.
        params->blend_mode = alternate_blend_mode;
    }
    else {
        uint32_t current_color = params->color & (uint32_t)0xffffffu;
        if (current_color != existing_color) {
            // Color is different from the initial one, switch to the more
            // expensive mode and flag the sketch tint accordingly.
            params->blend_mode = alternate_blend_mode;
            DP_transient_layer_props_sketch_tint_set(sub_tlp,
                                                     (uint32_t)0xff000000u);
        }
    }
}

static void draw_dabs_check_indirect_color(DP_PaintDrawDabsParams *params,
                                           DP_TransientLayerProps *sub_tlp)
{
    // The compare density blend modes only work properly if the
    // color doesn't change. Check if that happened and switch
    // them over to the more intensive marker mode if needed.
    switch (params->blend_mode) {
    case DP_BLEND_MODE_COMPARE_DENSITY:
        draw_dabs_adjust_indirect_mode(params, sub_tlp,
                                       DP_BLEND_MODE_MARKER_ALPHA_WASH);
        break;
    case DP_BLEND_MODE_COMPARE_DENSITY_SOFT:
        draw_dabs_adjust_indirect_mode(params, sub_tlp,
                                       DP_BLEND_MODE_MARKER_ALPHA);
        break;
    default:
        break;
    }
}

DP_CanvasState *DP_ops_draw_dabs(DP_CanvasState *cs, DP_DrawContext *dc,
                                 DP_UserCursors *ucs_or_null,
                                 bool (*next)(void *, DP_PaintDrawDabsParams *),
                                 void *user)
{
    // Drawing dabs is by far the most common operation and they come in
    // bunches, so we support batching them for the sake of speed. This makes
    // this operation kinda complicated, but the speedup is worth it.
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_TransientCanvasState *tcs = NULL;
    DP_TransientLayerContent *tlc = NULL;
    DP_TransientLayerContent *sub_tlc = NULL;
    DP_TransientLayerProps *sub_tlp = NULL;
    DP_LayerContent *last_mask_lc = NULL;
    int last_layer_id = -1;
    int last_sublayer_id = -1;
    unsigned int last_mask_context_id = 0;
    int last_mask_selection_id = -1;
    int errors = 0;
    enum {
        DP_DRAW_DABS_ERROR_BLEND_MODE,
        DP_DRAW_DABS_ERROR_LAYER_ID,
        DP_DRAW_DABS_ERROR_SELECTION_ID,
    } last_error_type;
    int last_error_arg;

    DP_PaintDrawDabsParams params;
    while (next(user, &params)) {
        if (params.dab_count < 1) {
            continue;
        }

        int blend_mode = params.blend_mode;
        if (!DP_blend_mode_valid_for_brush(blend_mode)) {
            ++errors;
            last_error_type = DP_DRAW_DABS_ERROR_BLEND_MODE;
            last_error_arg = blend_mode;
            DP_debug("Draw dabs: blend mode %s not applicable to brushes",
                     DP_blend_mode_enum_name_unprefixed(blend_mode));
            continue;
        }

        int layer_id = params.layer_id;
        if (!DP_layer_id_normal_or_selection(layer_id)) {
            ++errors;
            last_error_type = DP_DRAW_DABS_ERROR_LAYER_ID;
            last_error_arg = layer_id;
            DP_debug("Draw dabs: invalid layer id %d", layer_id);
            continue;
        }

        if (layer_id != last_layer_id) {
            DP_LayerRoutesSelEntry lrse = DP_layer_routes_search_sel(
                lr, tcs ? (DP_CanvasState *)tcs : cs, layer_id);
            if (DP_layer_routes_sel_entry_is_valid_target(&lrse)) {
                last_layer_id = layer_id;
                last_sublayer_id = -1;
                if (!tcs) {
                    tcs = DP_transient_canvas_state_new(cs);
                }
                tlc = DP_layer_routes_sel_entry_transient_content(&lrse, tcs);
            }
            else {
                ++errors;
                last_error_type = DP_DRAW_DABS_ERROR_LAYER_ID;
                last_error_arg = layer_id;
                DP_debug("Draw dabs: bad layer id %d", layer_id);
                continue;
            }
        }

        int mask_selection_id = params.mask_selection_id;
        DP_LayerContent *mask_lc;
        if (mask_selection_id == 0) {
            mask_lc = NULL;
        }
        else {
            unsigned int mask_context_id = params.context_id;
            if (mask_context_id != last_mask_context_id
                || mask_selection_id != last_mask_selection_id) {
                DP_Selection *sel = draw_dabs_search_selection(
                    cs, mask_context_id, mask_selection_id);
                if (sel) {
                    last_mask_context_id = mask_context_id;
                    last_mask_selection_id = mask_selection_id;
                    last_mask_lc = DP_selection_content_noinc(sel);
                }
                else {
                    ++errors;
                    last_error_type = DP_DRAW_DABS_ERROR_SELECTION_ID;
                    last_error_arg = DP_selection_id_make(mask_context_id,
                                                          mask_selection_id);
                    DP_debug("Draw dabs: bad context id %u selection id %d",
                             mask_context_id, mask_selection_id);
                    continue;
                }
            }
            mask_lc = last_mask_lc;
        }

        DP_TransientLayerContent *target;
        if (DP_paint_mode_indirect(params.paint_mode, &params.blend_mode)) {
            int sublayer_id = DP_uint_to_int(params.context_id);
            if (last_sublayer_id == sublayer_id) {
                draw_dabs_check_indirect_color(&params, sub_tlp);
            }
            else {
                last_sublayer_id = sublayer_id;
                DP_LayerPropsList *lpl =
                    DP_transient_layer_content_sub_props_noinc(tlc);
                int sublayer_index =
                    DP_layer_props_list_index_by_id(lpl, sublayer_id);
                if (sublayer_index < 0) {
                    DP_transient_layer_content_transient_sublayer(
                        tlc, sublayer_id, &sub_tlc, &sub_tlp);
                    // Only set these once, when the sublayer is created. They
                    // should always be the same values for a single sublayer.
                    DP_transient_layer_props_blend_mode_set(sub_tlp,
                                                            blend_mode);
                    DP_transient_layer_props_opacity_set(
                        sub_tlp, DP_channel8_to_15(DP_uint32_to_uint8(
                                     (params.color & (uint32_t)0xff000000u)
                                     >> (uint32_t)24u)));
                    // Stash the initial color inside the sketch tint of the
                    // sublayer. This is used to check if the stroke changed
                    // color along the way, in which case we need to switch to a
                    // more expensive blend mode that can deal with that.
                    DP_transient_layer_props_sketch_tint_set(
                        sub_tlp, (params.color & (uint32_t)0xffffffu));
                }
                else {
                    DP_transient_layer_content_transient_sublayer_at(
                        tlc, sublayer_index, &sub_tlc, &sub_tlp);
                    draw_dabs_check_indirect_color(&params, sub_tlp);
                }

                if (DP_transient_layer_content_mask_noinc_nullable(tlc)
                    != mask_lc) {
                    DP_transient_layer_content_mask_set_inc_nullable(sub_tlc,
                                                                     mask_lc);
                }
            }
            target = sub_tlc;
            mask_lc = NULL;
        }
        else {
            target = tlc;
        }

        DP_paint_draw_dabs(dc, ucs_or_null, &params, target, mask_lc);
    }

    switch (errors) {
    case 0:
        break;
    case 1:
        switch (last_error_type) {
        case DP_DRAW_DABS_ERROR_BLEND_MODE:
            DP_error_set("Draw dabs: blend mode %s not applicable to brushes",
                         DP_blend_mode_enum_name_unprefixed(last_error_arg));
            break;
        case DP_DRAW_DABS_ERROR_LAYER_ID:
            DP_error_set("Draw dabs: bad layer id %d", last_error_arg);
            break;
        case DP_DRAW_DABS_ERROR_SELECTION_ID:
            DP_error_set("Draw dabs: bad selection id %d", last_error_arg);
            break;
        }
        break;
    default:
        switch (last_error_type) {
        case DP_DRAW_DABS_ERROR_BLEND_MODE:
            DP_error_set("Draw dabs: %d errors, last one is: blend mode %s not "
                         "applicable to brushes",
                         errors,
                         DP_blend_mode_enum_name_unprefixed(last_error_arg));
            break;
        case DP_DRAW_DABS_ERROR_LAYER_ID:
            DP_error_set("Draw dabs: %d errors, last one is: bad layer id %d",
                         errors, last_error_arg);
            break;
        case DP_DRAW_DABS_ERROR_SELECTION_ID:
            DP_error_set(
                "Draw dabs: %d errors, last one is: bad selection id %d",
                errors, last_error_arg);
            break;
        }
    }

    return tcs ? DP_transient_canvas_state_persist(tcs) : NULL;
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


DP_Selection *search_selection(DP_CanvasState *cs, unsigned int context_id,
                               int selection_id)
{
    DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
    return ss ? DP_selection_set_search_noinc(ss, context_id, selection_id)
              : NULL;
}

DP_CanvasState *set_selection(DP_CanvasState *cs, unsigned int context_id,
                              int selection_id, DP_LayerContent *lc)
{
    if (DP_layer_content_has_content(lc)) {
        DP_Selection *sel = DP_selection_new_init(context_id, selection_id, lc);
        DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
        DP_TransientSelectionSet *tss;
        if (ss) {
            int index =
                DP_selection_set_search_index(ss, context_id, selection_id);
            if (index < 0) {
                tss = DP_transient_selection_set_new(ss, 1);
                DP_transient_selection_set_insert_at_noinc(
                    tss, DP_selection_set_count(ss), sel);
            }
            else {
                tss = DP_transient_selection_set_new(ss, 0);
                DP_transient_selection_set_replace_at_noinc(tss, index, sel);
            }
        }
        else {
            tss = DP_transient_selection_set_new_init(1);
            DP_transient_selection_set_insert_at_noinc(tss, 0, sel);
        }
        return DP_canvas_state_new_with_selections_noinc(
            cs, DP_transient_selection_set_persist(tss));
    }
    else {
        DP_layer_content_decref(lc);
        return DP_ops_selection_clear(cs, context_id, selection_id);
    }
}

DP_CanvasState *DP_ops_selection_put_replace(DP_CanvasState *cs,
                                             unsigned int context_id,
                                             int selection_id, int left,
                                             int top, int right, int bottom,
                                             DP_Image *mask)
{
    DP_TransientLayerContent *tlc = DP_transient_layer_content_new_init(
        DP_canvas_state_width(cs), DP_canvas_state_height(cs), NULL);

    if (mask) {
        DP_transient_layer_content_put_image(
            tlc, context_id, DP_BLEND_MODE_REPLACE, left, top, mask);
    }
    else {
        DP_transient_layer_content_fill_rect(
            tlc, context_id, DP_BLEND_MODE_REPLACE, left, top, right, bottom,
            (DP_UPixel15){0, 0, 0, DP_BIT15});
    }

    return set_selection(cs, context_id, selection_id,
                         DP_transient_layer_content_persist_mask(tlc));
}

DP_CanvasState *DP_ops_selection_put_unite(DP_CanvasState *cs,
                                           unsigned int context_id,
                                           int selection_id, int left, int top,
                                           int right, int bottom,
                                           DP_Image *mask)
{
    DP_Selection *sel = search_selection(cs, context_id, selection_id);
    if (sel) {
        DP_TransientLayerContent *tlc =
            DP_transient_layer_content_new(DP_selection_content_noinc(sel));

        if (mask) {
            DP_transient_layer_content_put_image(
                tlc, context_id, DP_BLEND_MODE_NORMAL, left, top, mask);
        }
        else {
            DP_transient_layer_content_fill_rect(
                tlc, context_id, DP_BLEND_MODE_NORMAL, left, top, right, bottom,
                (DP_UPixel15){0, 0, 0, DP_BIT15});
        }

        return set_selection(cs, context_id, selection_id,
                             DP_transient_layer_content_persist_mask(tlc));
    }
    else {
        return DP_ops_selection_put_replace(cs, context_id, selection_id, left,
                                            top, right, bottom, mask);
    }
}

DP_CanvasState *DP_ops_selection_put_intersect(DP_CanvasState *cs,
                                               unsigned int context_id,
                                               int selection_id, int left,
                                               int top, int right, int bottom,
                                               DP_Image *mask)
{
    DP_Selection *sel = search_selection(cs, context_id, selection_id);
    if (sel) {
        DP_Tile *t = DP_tile_new_from_pixel15(context_id,
                                              (DP_Pixel15){0, 0, 0, DP_BIT15});
        DP_TransientLayerContent *mask_tlc =
            DP_transient_layer_content_new_init(DP_canvas_state_width(cs),
                                                DP_canvas_state_height(cs), t);
        DP_tile_decref(t);

        if (mask) {
            DP_transient_layer_content_put_image(
                mask_tlc, context_id, DP_BLEND_MODE_ERASE, left, top, mask);
        }
        else {
            DP_transient_layer_content_fill_rect(
                mask_tlc, context_id, DP_BLEND_MODE_ERASE, left, top, right,
                bottom, (DP_UPixel15){0, 0, 0, DP_BIT15});
        }

        DP_TransientLayerContent *tlc =
            DP_transient_layer_content_new(DP_selection_content_noinc(sel));
        DP_transient_layer_content_merge(tlc, context_id,
                                         (DP_LayerContent *)mask_tlc, DP_BIT15,
                                         DP_BLEND_MODE_ERASE, false);
        DP_transient_layer_content_decref(mask_tlc);

        return set_selection(cs, context_id, selection_id,
                             DP_transient_layer_content_persist_mask(tlc));
    }
    else {
        return DP_canvas_state_incref(cs);
    }
}

DP_CanvasState *DP_ops_selection_put_exclude(DP_CanvasState *cs,
                                             unsigned int context_id,
                                             int selection_id, int left,
                                             int top, int right, int bottom,
                                             DP_Image *mask)
{
    DP_Selection *sel = search_selection(cs, context_id, selection_id);
    if (sel) {
        DP_TransientLayerContent *tlc =
            DP_transient_layer_content_new(DP_selection_content_noinc(sel));

        if (mask) {
            DP_transient_layer_content_put_image(
                tlc, context_id, DP_BLEND_MODE_ERASE, left, top, mask);
        }
        else {
            DP_transient_layer_content_fill_rect(
                tlc, context_id, DP_BLEND_MODE_ERASE, left, top, right, bottom,
                (DP_UPixel15){0, 0, 0, DP_BIT15});
        }

        return set_selection(cs, context_id, selection_id,
                             DP_transient_layer_content_persist_mask(tlc));
    }
    else {
        return DP_canvas_state_incref(cs);
    }
}

DP_CanvasState *DP_ops_selection_put_complement(DP_CanvasState *cs,
                                                unsigned int context_id,
                                                int selection_id, int left,
                                                int top, int right, int bottom,
                                                DP_Image *mask)
{
    DP_Selection *sel = search_selection(cs, context_id, selection_id);
    if (sel) {
        DP_TransientLayerContent *tlc = DP_transient_layer_content_new_init(
            DP_canvas_state_width(cs), DP_canvas_state_height(cs), NULL);

        if (mask) {
            DP_transient_layer_content_put_image(
                tlc, context_id, DP_BLEND_MODE_REPLACE, left, top, mask);
        }
        else {
            DP_transient_layer_content_fill_rect(
                tlc, context_id, DP_BLEND_MODE_REPLACE, left, top, right,
                bottom, (DP_UPixel15){0, 0, 0, DP_BIT15});
        }

        DP_transient_layer_content_merge(tlc, context_id,
                                         DP_selection_content_noinc(sel),
                                         DP_BIT15, DP_BLEND_MODE_ERASE, false);

        return set_selection(cs, context_id, selection_id,
                             DP_transient_layer_content_persist_mask(tlc));
    }
    else {
        return DP_ops_selection_put_replace(cs, context_id, selection_id, left,
                                            top, right, bottom, mask);
    }
}

DP_CanvasState *DP_ops_selection_clear(DP_CanvasState *cs,
                                       unsigned int context_id,
                                       int selection_id)
{
    DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
    if (ss) {
        DP_TransientSelectionSet *tss =
            DP_selection_set_remove(ss, context_id, selection_id);
        if (tss) {
            if (DP_transient_selection_set_count(tss) == 0) {
                DP_transient_selection_set_decref(tss);
                return DP_canvas_state_new_with_selections_noinc(cs, NULL);
            }
            else {
                return DP_canvas_state_new_with_selections_noinc(
                    cs, DP_transient_selection_set_persist(tss));
            }
        }
    }
    return DP_canvas_state_incref(cs);
}

DP_CanvasState *DP_ops_sync_selection_tile(DP_CanvasState *cs,
                                           unsigned int context_id,
                                           int selection_id, int index,
                                           DP_Tile *tile_or_null)
{
    DP_LayerRoutesSelEntry lrse =
        DP_layer_routes_search_sel_only(cs, context_id, selection_id);

    // No point putting a blank tile on a blank selection. It's like, how much
    // more blank could it be? The answer is none, none more blank.
    if (!lrse.exists && !tile_or_null) {
        return DP_canvas_state_incref(cs);
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc =
        DP_layer_routes_sel_entry_transient_content(&lrse, tcs);
    DP_transient_layer_content_tile_set_noinc(tlc, tile_or_null, index);
    return DP_transient_canvas_state_persist(tcs);
}
