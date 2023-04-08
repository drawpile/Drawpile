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
#include "draw_context.h"
#include "frame.h"
#include "image.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "layer_routes.h"
#include "paint.h"
#include "tile.h"
#include "timeline.h"
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
            && (tlp = clone_layer_props(
                    dc, masked_id, DP_layer_props_list_at_noinc(lpl, i)))) {
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

DP_CanvasState *DP_ops_layer_create(DP_CanvasState *cs, DP_DrawContext *dc,
                                    int layer_id, int source_id, int target_id,
                                    DP_Tile *tile, bool into, bool group,
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


typedef struct DP_LayerOrderContext {
    DP_DrawContext *dc;
    int order_count;
    struct DP_LayerOrderPair (*get_order)(void *, int);
    void *user;
    int next_index;
    int capacity;
    int used;
    struct DP_LayerPoolEntry *pool;
} DP_LayerOrderContext;

static DP_LayerOrderContext
layer_order_context_make(DP_DrawContext *dc, int order_count,
                         struct DP_LayerOrderPair (*get_order)(void *, int),
                         void *user)
{
    DP_ASSERT(dc);
    DP_ASSERT(get_order);
    return (DP_LayerOrderContext){dc, order_count, get_order, user,
                                  0,  0,           0,         NULL};
}

static void layer_order_context_reset_next(DP_LayerOrderContext *lrc)
{
    DP_ASSERT(lrc);
    lrc->next_index = 0;
}

static int layer_order_context_next_child_count(DP_LayerOrderContext *lrc)
{
    DP_ASSERT(lrc);
    int i = lrc->next_index++;
    int order_count = lrc->order_count;
    if (i < order_count) {
        struct DP_LayerOrderPair p = lrc->get_order(lrc->user, i);
        return p.child_count;
    }
    else {
        return -1;
    }
}

static void layer_order_context_next(DP_LayerOrderContext *lrc,
                                     int *out_layer_id, int *out_child_count)
{
    DP_ASSERT(lrc);
    DP_ASSERT(out_layer_id);
    DP_ASSERT(out_child_count);
    int i = lrc->next_index++;
    DP_ASSERT(i < lrc->order_count);
    struct DP_LayerOrderPair p = lrc->get_order(lrc->user, i);
    *out_layer_id = p.layer_id;
    *out_child_count = p.child_count;
}

static void layer_order_context_init_pool(DP_LayerOrderContext *lrc)
{
    DP_ASSERT(lrc);
    lrc->pool = DP_draw_context_layer_pool(lrc->dc, &lrc->capacity);
    DP_ASSERT(lrc->capacity > 0);
}

static void layer_order_context_insert(DP_LayerOrderContext *lrc,
                                       DP_LayerListEntry *lle,
                                       DP_LayerProps *lp)
{
    DP_ASSERT(lrc);
    DP_ASSERT(lle);
    DP_ASSERT(lp);
    int i = lrc->used;
    if (i == lrc->capacity) {
        lrc->capacity *= 2;
        lrc->pool = DP_draw_context_layer_pool_resize(lrc->dc, lrc->capacity);
    }
    lrc->used = i + 1;
    lrc->pool[i] = (struct DP_LayerPoolEntry){lle, lp};
}

static bool layer_order_context_remove(DP_LayerOrderContext *lrc, int layer_id,
                                       DP_LayerListEntry **out_lle,
                                       DP_LayerProps **out_lp)
{
    DP_ASSERT(lrc);
    DP_ASSERT(out_lle);
    DP_ASSERT(out_lp);
    int used = lrc->used;
    struct DP_LayerPoolEntry *pool = lrc->pool;
    for (int i = 0; i < used; ++i) {
        struct DP_LayerPoolEntry *lpe = &pool[i];
        DP_LayerProps *lp = lpe->lp;
        if (lp && DP_layer_props_id(lp) == layer_id) {
            DP_LayerListEntry *lle = lpe->lle;
            DP_ASSERT(lle);
            *lpe = (struct DP_LayerPoolEntry){NULL, NULL};
            *out_lle = lle;
            *out_lp = lp;
            return true;
        }
    }
    return false;
}

static bool layer_order_context_check_pool_empty(DP_LayerOrderContext *lrc)
{
    DP_ASSERT(lrc);
    int used = lrc->used;
    struct DP_LayerPoolEntry *pool = lrc->pool;
    for (int i = 0; i < used; ++i) {
        struct DP_LayerPoolEntry *lpe = &pool[i];
        if (lpe->lle) {
            DP_ASSERT(lpe->lp);
            DP_error_set("Layer order: missing layers");
            return false;
        }
        else {
            DP_ASSERT(!lpe->lp);
        }
    }
    return true;
}


union DP_LayerOrderContentOrGroup {
    DP_LayerContent *lc;
    DP_LayerGroup *lg;
};

static void gather_layer_pool(DP_LayerOrderContext *lrc, DP_LayerList *ll,
                              DP_LayerPropsList *lpl)
{
    DP_ASSERT(lrc);
    DP_ASSERT(ll);
    DP_ASSERT(lpl);
    int count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        layer_order_context_insert(lrc, lle, lp);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_LayerList *child_ll = DP_layer_group_children_noinc(
                DP_layer_list_entry_group_noinc(lle));
            DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
            gather_layer_pool(lrc, child_ll, child_lpl);
        }
    }
}

static bool skip_layers(DP_LayerOrderContext *lrc, int count)
{
    for (int i = 0; i < count; ++i) {
        int child_count = layer_order_context_next_child_count(lrc);
        if (child_count == -1 || !skip_layers(lrc, child_count)) {
            return false;
        }
    }
    return true;
}

static int count_root_layers(DP_LayerOrderContext *lrc)
{
    int layer_count = 0;
    int order_count = lrc->order_count;
    while (lrc->next_index < order_count) {
        ++layer_count;
        int child_count = layer_order_context_next_child_count(lrc);
        if (child_count == -1 || !skip_layers(lrc, child_count)) {
            return -1;
        }
    }
    return layer_count;
}

static bool order_into(DP_LayerOrderContext *lrc, DP_TransientLayerList *tll,
                       DP_TransientLayerPropsList *tlpl);

static bool order_layer_group(DP_LayerOrderContext *lrc, int child_count,
                              DP_LayerGroup *lg, DP_LayerProps *lp,
                              DP_LayerGroup **out_lg, DP_LayerProps **out_lp)
{
    DP_TransientLayerList *tll = DP_transient_layer_list_new_init(child_count);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_layer_props_list_new_init(child_count);
    if (order_into(lrc, tll, tlpl)) {
        *out_lg =
            (DP_LayerGroup *)DP_transient_layer_group_new_with_children_noinc(
                lg, tll);
        *out_lp =
            (DP_LayerProps *)DP_transient_layer_props_new_with_children_noinc(
                lp, tlpl);
        return true;
    }
    else {
        DP_transient_layer_list_decref(tll);
        DP_transient_layer_props_list_decref(tlpl);
        return false;
    }
}

static bool order_layer_content(int child_count, DP_LayerContent *lc,
                                DP_LayerProps *lp, DP_LayerContent **out_lc,
                                DP_LayerProps **out_lp)
{
    if (child_count == 0) {
        *out_lc = DP_layer_content_incref(lc);
        *out_lp = DP_layer_props_incref(lp);
        return true;
    }
    else {
        DP_error_set(
            "Layer order: id %d has child count of %d, but is not a group",
            DP_layer_props_id(lp), child_count);
        return false;
    }
}

static bool order_layer(DP_LayerOrderContext *lrc, int layer_id,
                        int child_count,
                        union DP_LayerOrderContentOrGroup *out_lc_or_lg,
                        DP_LayerProps **out_lp)
{
    DP_LayerListEntry *lle;
    DP_LayerProps *lp;
    if (layer_order_context_remove(lrc, layer_id, &lle, &lp)) {
        if (DP_layer_list_entry_is_group(lle)) {
            DP_ASSERT(DP_layer_props_children_noinc(lp));
            return order_layer_group(lrc, child_count,
                                     DP_layer_list_entry_group_noinc(lle), lp,
                                     &out_lc_or_lg->lg, out_lp);
        }
        else {
            DP_ASSERT(!DP_layer_props_children_noinc(lp));
            return order_layer_content(child_count,
                                       DP_layer_list_entry_content_noinc(lle),
                                       lp, &out_lc_or_lg->lc, out_lp);
        }
    }
    else {
        DP_error_set("Layer order: id %d not found", layer_id);
        return false;
    }
}

static bool order_next_layer(DP_LayerOrderContext *lrc,
                             union DP_LayerOrderContentOrGroup *out_lc_or_lg,
                             DP_LayerProps **out_lp)
{
    int layer_id, child_count;
    layer_order_context_next(lrc, &layer_id, &child_count);
    return order_layer(lrc, layer_id, child_count, out_lc_or_lg, out_lp);
}

static bool order_into(DP_LayerOrderContext *lrc, DP_TransientLayerList *tll,
                       DP_TransientLayerPropsList *tlpl)
{
    int list_count = DP_transient_layer_list_count(tll);
    DP_ASSERT(DP_transient_layer_props_list_count(tlpl) == list_count);
    // The incoming layer order is reversed, so fill them up from the back.
    for (int i = list_count - 1; i >= 0; --i) {
        union DP_LayerOrderContentOrGroup lc_or_lg;
        DP_LayerProps *lp;
        if (order_next_layer(lrc, &lc_or_lg, &lp)) {
            if (DP_layer_props_children_noinc(lp)) {
                DP_transient_layer_list_set_group_noinc(tll, lc_or_lg.lg, i);
            }
            else {
                DP_transient_layer_list_set_content_noinc(tll, lc_or_lg.lc, i);
            }
            DP_transient_layer_props_list_set_noinc(tlpl, lp, i);
        }
        else {
            return false;
        }
    }
    return true;
}

DP_CanvasState *DP_ops_layer_order(
    DP_CanvasState *cs, DP_DrawContext *dc, int root_layer_id, int order_count,
    struct DP_LayerOrderPair (*get_order)(void *, int), void *user)
{
    // Figure out where we're starting from. An id of zero means we want to
    // reorder the root, otherwise it's a group somewhere in the layer stack.
    DP_LayerList *root_ll;
    DP_LayerPropsList *root_lpl;
    DP_LayerRoutesEntry *lre;
    if (root_layer_id == 0) {
        lre = NULL;
        root_ll = DP_canvas_state_layers_noinc(cs);
        root_lpl = DP_canvas_state_layer_props_noinc(cs);
    }
    else {
        DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
        lre = DP_layer_routes_search(lr, root_layer_id);
        if (!lre) {
            DP_error_set("Layer order: id %d not found", root_layer_id);
            return NULL;
        }
        else if (!DP_layer_routes_entry_is_group(lre)) {
            DP_error_set("Layer order: id %d is not a group", root_layer_id);
            return NULL;
        }
        DP_layer_routes_entry_children(lre, cs, &root_ll, &root_lpl);
    }

    // We have to walk the given layer order tree once to figure out how many
    // elements to reserve for the root. The given order count is the size of
    // the entire tree including child elements, so we can't use that directly.
    DP_LayerOrderContext lrc =
        layer_order_context_make(dc, order_count, get_order, user);
    int root_reserve = count_root_layers(&lrc);
    if (root_reserve == -1) {
        DP_error_set("Layer order: invalid tree");
        return NULL;
    }
    DP_ASSERT(lrc.next_index == lrc.order_count);

    DP_TransientLayerList *root_tll =
        DP_transient_layer_list_new_init(root_reserve);
    DP_TransientLayerPropsList *root_tlpl =
        DP_transient_layer_props_list_new_init(root_reserve);

    // We collect all target layers into a flat mapping. The elements in it are
    // removed during the reordering, avoiding duplicates and we can check that
    // all layers have been reordered by checking if it's empty afterwards.
    layer_order_context_init_pool(&lrc);
    gather_layer_pool(&lrc, root_ll, root_lpl);

    // Do the actual reordering: walk the given tree and build up a new stack
    // of layers and groups. Afterwards make sure that all layers have been
    // reordered and none have gone missing.
    layer_order_context_reset_next(&lrc);
    bool ok = order_into(&lrc, root_tll, root_tlpl)
           && layer_order_context_check_pool_empty(&lrc);
    if (!ok) {
        DP_transient_layer_list_decref(root_tll);
        DP_transient_layer_props_list_decref(root_tlpl);
        return NULL;
    }
    DP_ASSERT(lrc.next_index == lrc.order_count);

    DP_TransientCanvasState *tcs;
    if (lre) {
        tcs = DP_transient_canvas_state_new(cs);

        DP_TransientLayerList *tll;
        DP_TransientLayerPropsList *tlpl;
        DP_layer_routes_entry_transient_children(lre, tcs, -1, 0, &tll, &tlpl);

        int last_index = DP_layer_routes_entry_index_last(lre);
        DP_transient_layer_list_transient_group_at_with_children_noinc(
            tll, last_index, root_tll);
        DP_transient_layer_props_list_transient_at_with_children_noinc(
            tlpl, last_index, root_tlpl);
    }
    else {
        tcs = DP_transient_canvas_state_new_with_layers_noinc(cs, root_tll,
                                                              root_tlpl);
    }

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
                                    int merge_layer_id)
{
    DP_ASSERT(layer_id != 0);
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);

    DP_LayerRoutesEntry *delete_lre = DP_layer_routes_search(lr, layer_id);
    if (!delete_lre) {
        DP_error_set("Layer delete: id %d not found", layer_id);
        return NULL;
    }

    DP_LayerRoutesEntry *merge_lre;
    if (merge_layer_id != 0) {
        merge_lre = DP_layer_routes_search(lr, merge_layer_id);
        if (!merge_lre) {
            DP_error_set("Layer delete: merge id %d not found", merge_layer_id);
            return NULL;
        }
        else if (DP_layer_routes_entry_is_group(merge_lre)) {
            DP_error_set("Layer delete: merge target %d is a group",
                         merge_layer_id);
            return NULL;
        }
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);

    DP_TransientLayerList *delete_tll;
    DP_TransientLayerPropsList *delete_tlpl;
    DP_layer_routes_entry_transient_children(delete_lre, tcs, -1, 0,
                                             &delete_tll, &delete_tlpl);

    int delete_last_index = DP_layer_routes_entry_index_last(delete_lre);
    DP_LayerProps *delete_lp =
        DP_transient_layer_props_list_at_noinc(delete_tlpl, delete_last_index);

    if (merge_layer_id != 0) {
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
    DP_transient_canvas_state_layer_routes_reindex(tcs, dc);

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

static DP_CanvasState *move_image(DP_CanvasState *cs, DP_LayerRoutesEntry *lre,
                                  unsigned int context_id,
                                  const DP_Rect *src_rect, DP_Image *mask,
                                  DP_Image *src_img, int offset_x, int offset_y,
                                  DP_Image *dst_img)
{
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc =
        DP_layer_routes_entry_transient_content(lre, tcs);

    if (mask) {
        DP_transient_layer_content_put_image(
            tlc, context_id, DP_BLEND_MODE_ERASE, DP_rect_x(*src_rect),
            DP_rect_y(*src_rect), mask);
    }
    else {
        DP_transient_layer_content_fill_rect(
            tlc, context_id, DP_BLEND_MODE_REPLACE, src_rect->x1, src_rect->y1,
            src_rect->x2 + 1, src_rect->y2 + 1, DP_upixel15_zero());
    }

    DP_transient_layer_content_put_image(tlc, context_id, DP_BLEND_MODE_NORMAL,
                                         offset_x, offset_y, dst_img);

    if (dst_img != src_img) {
        DP_image_free(dst_img);
    }
    DP_image_free(src_img);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasStateChange DP_ops_move_region(DP_CanvasState *cs, DP_DrawContext *dc,
                                        unsigned int context_id, int layer_id,
                                        const DP_Rect *src_rect,
                                        const DP_Quad *dst_quad, DP_Image *mask)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (!lre) {
        DP_error_set("Move region: id %d not found", layer_id);
        return DP_canvas_state_change_null();
    }
    else if (DP_layer_routes_entry_is_group(lre)) {
        DP_error_set("Move region: id %d is a group", layer_id);
        return DP_canvas_state_change_null();
    }

    DP_Image *src_img = DP_layer_content_select(
        DP_layer_routes_entry_content(lre, cs), src_rect, mask);

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
            return DP_canvas_state_change_null();
        }
    }

    DP_Rect dst_bounds = DP_quad_bounds(*dst_quad);
    return (DP_CanvasStateChange){
        move_image(cs, lre, context_id, src_rect, mask, src_img, offset_x,
                   offset_y, dst_img),
        {context_id, layer_id,
         DP_rect_x(dst_bounds) + DP_rect_width(dst_bounds) / 2,
         DP_rect_y(dst_bounds) + DP_rect_height(dst_bounds) / 2}};
}

DP_CanvasStateChange DP_ops_move_rect(DP_CanvasState *cs,
                                      unsigned int context_id, int layer_id,
                                      const DP_Rect *src_rect, int dst_x,
                                      int dst_y, DP_Image *mask)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (!lre) {
        DP_error_set("Move rect: id %d not found", layer_id);
        return DP_canvas_state_change_null();
    }
    else if (DP_layer_routes_entry_is_group(lre)) {
        DP_error_set("Move rect: id %d is a group", layer_id);
        return DP_canvas_state_change_null();
    }

    DP_Image *src_img = DP_layer_content_select(
        DP_layer_routes_entry_content(lre, cs), src_rect, mask);

    return (DP_CanvasStateChange){
        move_image(cs, lre, context_id, src_rect, mask, src_img, dst_x, dst_y,
                   src_img),
        {context_id, layer_id, dst_x + DP_rect_width(*src_rect) / 2,
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


DP_CanvasState *DP_ops_timeline_frame_set(DP_CanvasState *cs, int frame_index,
                                          bool insert, int layer_id_count,
                                          int get_layer_id(void *, int),
                                          void *user)
{
    DP_ASSERT(frame_index >= 0);
    DP_ASSERT(layer_id_count >= 0);

    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int old_frame_count = DP_timeline_frame_count(tl);
    if (frame_index > DP_timeline_frame_count(tl)) {
        DP_error_set("Set timeline frame: given frame %d beyond end %d",
                     frame_index, old_frame_count);
        return NULL;
    }

    // Filter out invalid layer ids. Rustpile likes to send lots of zeroes.
    int actual_layer_id_count = 0;
    for (int i = 0; i < layer_id_count; ++i) {
        int layer_id = get_layer_id(user, i);
        if (layer_id > 0 && layer_id <= UINT16_MAX) {
            ++actual_layer_id_count;
        }
    }

    DP_TransientFrame *tf = DP_transient_frame_new_init(actual_layer_id_count);
    int actual_index = 0;
    for (int i = 0; i < layer_id_count; ++i) {
        int layer_id = get_layer_id(user, i);
        if (layer_id > 0 && layer_id <= UINT16_MAX) {
            DP_transient_frame_layer_id_set_at(tf, layer_id, actual_index++);
        }
    }

    bool replace = frame_index < old_frame_count && !insert;
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, replace ? 0 : 1);

    if (replace) {
        DP_transient_timeline_replace_transient_noinc(ttl, tf, frame_index);
    }
    else {
        DP_transient_timeline_insert_transient_noinc(ttl, tf, frame_index);
    }

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_timeline_frame_delete(DP_CanvasState *cs,
                                             int frame_index)
{
    DP_ASSERT(frame_index >= 0);

    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int old_frame_count = DP_timeline_frame_count(tl);
    if (frame_index >= DP_timeline_frame_count(tl)) {
        DP_error_set("Remove timeline frame: given frame %d beyond end %d",
                     frame_index, old_frame_count);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, 0);

    DP_transient_timeline_delete_at(ttl, frame_index);

    return DP_transient_canvas_state_persist(tcs);
}
