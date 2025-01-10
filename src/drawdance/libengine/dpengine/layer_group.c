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
#include "layer_group.h"
#include "layer_content.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "tile.h"
#include "view_mode.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpmsg/blend_mode.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_LayerGroup {
    DP_Atomic refcount;
    const bool transient;
    const int width, height;
    DP_LayerList *const children;
};

struct DP_TransientLayerGroup {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    union {
        DP_LayerList *children;
        DP_TransientLayerList *transient_children;
    };
};

#else

struct DP_LayerGroup {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    union {
        DP_LayerList *children;
        DP_TransientLayerList *transient_children;
    };
};

#endif


DP_LayerGroup *DP_layer_group_incref(DP_LayerGroup *lg)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_atomic_inc(&lg->refcount);
    return lg;
}

DP_LayerGroup *DP_layer_group_incref_nullable(DP_LayerGroup *lg_or_null)
{
    return lg_or_null ? DP_layer_group_incref(lg_or_null) : NULL;
}

void DP_layer_group_decref(DP_LayerGroup *lg)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    if (DP_atomic_dec(&lg->refcount)) {
        DP_layer_list_decref(lg->children);
        DP_free(lg);
    }
}

void DP_layer_group_decref_nullable(DP_LayerGroup *lg_or_null)
{
    if (lg_or_null) {
        DP_layer_group_decref(lg_or_null);
    }
}

int DP_layer_group_refcount(DP_LayerGroup *lg)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    return DP_atomic_get(&lg->refcount);
}

bool DP_layer_group_transient(DP_LayerGroup *lg)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    return lg->transient;
}

static void layer_group_diff_children(DP_LayerGroup *lg, DP_LayerProps *lp,
                                      DP_LayerGroup *prev_lg,
                                      DP_LayerProps *prev_lp,
                                      DP_CanvasDiff *diff, int only_layer_id)
{
    DP_LayerPropsList *lpl = DP_layer_props_children_noinc(lp);
    DP_ASSERT(lpl);
    DP_LayerPropsList *prev_lpl = DP_layer_props_children_noinc(prev_lp);
    DP_ASSERT(prev_lpl);
    DP_layer_list_diff(lg->children, lpl, prev_lg->children, prev_lpl, diff,
                       only_layer_id);
}

static void layer_group_diff_groups(DP_LayerGroup *lg, DP_LayerProps *lp,
                                    DP_LayerGroup *prev_lg,
                                    DP_LayerProps *prev_lp, DP_CanvasDiff *diff,
                                    int only_layer_id)
{
    if (DP_layer_props_differ(lp, prev_lp)) {
        DP_layer_list_diff_mark(lg->children, diff);
        DP_layer_list_diff_mark(prev_lg->children, diff);
    }
    else {
        layer_group_diff_children(lg, lp, prev_lg, prev_lp, diff,
                                  only_layer_id);
    }
}

void DP_layer_group_diff(DP_LayerGroup *lg, DP_LayerProps *lp,
                         DP_LayerGroup *prev_lg, DP_LayerProps *prev_lp,
                         DP_CanvasDiff *diff, int only_layer_id)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(lp);
    DP_ASSERT(prev_lg);
    DP_ASSERT(DP_atomic_get(&prev_lg->refcount) > 0);
    DP_ASSERT(prev_lp);
    DP_ASSERT(diff);

    bool visible, prev_visible;
    if (only_layer_id == 0) {
        visible = DP_layer_props_visible(lp);
        prev_visible = DP_layer_props_visible(prev_lp);
    }
    else {
        visible = only_layer_id == DP_layer_props_id(lp);
        prev_visible = only_layer_id == DP_layer_props_id(prev_lp);
    }

    if (visible) {
        if (prev_visible) {
            layer_group_diff_groups(lg, lp, prev_lg, prev_lp, diff, 0);
        }
        else {
            DP_layer_list_diff_mark(lg->children, diff);
        }
    }
    else if (prev_visible) {
        DP_layer_list_diff_mark(prev_lg->children, diff);
    }
    else if (only_layer_id != 0) {
        layer_group_diff_children(lg, lp, prev_lg, prev_lp, diff,
                                  only_layer_id);
    }
}

void DP_layer_group_diff_mark(DP_LayerGroup *lg, DP_CanvasDiff *diff)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(diff);
    DP_layer_list_diff_mark(lg->children, diff);
}

int DP_layer_group_width(DP_LayerGroup *lg)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    return lg->width;
}

int DP_layer_group_height(DP_LayerGroup *lg)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    return lg->height;
}

DP_LayerList *DP_layer_group_children_noinc(DP_LayerGroup *lg)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    return lg->children;
}

bool DP_layer_group_bounds(DP_LayerGroup *lg, DP_Rect *out_bounds)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_Rect bounds;
    bool have_bounds = false;

    DP_LayerList *ll = lg->children;
    int count = DP_layer_list_count(ll);
    for (int i = 0; i < count; ++i) {
        DP_Rect child_bounds;
        bool have_child_bounds;
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            have_child_bounds = DP_layer_group_bounds(
                DP_layer_list_entry_group_noinc(lle), &child_bounds);
        }
        else {
            have_child_bounds = DP_layer_content_bounds(
                DP_layer_list_entry_content_noinc(lle), &child_bounds);
        }

        if (have_child_bounds) {
            if (have_bounds) {
                bounds = DP_rect_union(bounds, child_bounds);
            }
            else {
                have_bounds = true;
                bounds = child_bounds;
            }
        }
    }

    if (have_bounds && out_bounds) {
        *out_bounds = bounds;
    }
    return have_bounds;
}

int DP_layer_group_search_change_bounds(DP_LayerGroup *lg, DP_LayerProps *lp,
                                        unsigned int context_id, int *out_x,
                                        int *out_y, int *out_width,
                                        int *out_height)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_LayerList *ll = lg->children;
    DP_LayerPropsList *lpl = DP_layer_props_children_noinc(lp);
    DP_ASSERT(lpl);
    int count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            int layer_id = DP_layer_group_search_change_bounds(
                DP_layer_list_entry_group_noinc(lle),
                DP_layer_props_list_at_noinc(lpl, i), context_id, out_x, out_y,
                out_width, out_height);
            if (layer_id != 0) {
                return layer_id;
            }
        }
        else if (DP_layer_content_search_change_bounds(
                     DP_layer_list_entry_content_noinc(lle), context_id, out_x,
                     out_y, out_width, out_height)) {
            return DP_layer_props_id(DP_layer_props_list_at_noinc(lpl, i));
        }
    }
    return 0;
}

DP_TransientLayerGroup *DP_layer_group_resize(DP_LayerGroup *lg,
                                              unsigned int context_id, int top,
                                              int right, int bottom, int left)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    int width = lg->width + left + right;
    int height = lg->height + top + bottom;
    DP_LayerList *ll = lg->children;
    int count = DP_layer_list_count(ll);

    DP_TransientLayerGroup *tlg =
        DP_transient_layer_group_new_init(width, height, count);
    DP_TransientLayerList *tll = tlg->transient_children;

    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_TransientLayerGroup *ctlg =
                DP_layer_group_resize(DP_layer_list_entry_group_noinc(lle),
                                      context_id, top, right, bottom, left);
            DP_transient_layer_list_insert_transient_group_noinc(tll, ctlg, i);
        }
        else {
            DP_TransientLayerContent *ctlc =
                DP_layer_content_resize(DP_layer_list_entry_content_noinc(lle),
                                        context_id, top, right, bottom, left);
            DP_transient_layer_list_insert_transient_content_noinc(tll, ctlc,
                                                                   i);
        }
    }

    return tlg;
}

DP_Pixel8 *DP_layer_group_to_pixels8(DP_LayerGroup *lg, DP_LayerProps *lp,
                                     int x, int y, int width, int height,
                                     bool reveal_censored)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(lp);
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);
    // TODO: Don't merge the whole group just to cut out a part of it.
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init(lg->width, lg->height, NULL);
    DP_layer_list_merge_to_flat_image(lg->children,
                                      DP_layer_props_children_noinc(lp), tlc,
                                      DP_BIT15, true, reveal_censored, false);
    DP_Pixel8 *pixels = DP_layer_content_to_pixels8((DP_LayerContent *)tlc, x,
                                                    y, width, height);
    DP_transient_layer_content_decref(tlc);
    return pixels;
}

DP_TransientLayerContent *DP_layer_group_merge(DP_LayerGroup *lg,
                                               DP_LayerProps *lp,
                                               bool include_sublayers)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(lp);
    DP_LayerPropsList *lpl = DP_layer_props_children_noinc(lp);
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init(lg->width, lg->height, NULL);
    DP_layer_list_merge_to_flat_image(lg->children, lpl, tlc, DP_BIT15,
                                      include_sublayers, true, false);
    return tlc;
}

void DP_layer_group_merge_to_flat_image(DP_LayerGroup *lg, DP_LayerProps *lp,
                                        DP_TransientLayerContent *tlc,
                                        uint16_t parent_opacity,
                                        bool include_sublayers,
                                        bool pass_through_censored)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(lp);
    DP_ASSERT(tlc);
    DP_ASSERT(parent_opacity <= DP_BIT15);
    DP_LayerPropsList *lpl = DP_layer_props_children_noinc(lp);
    uint16_t opacity = DP_fix15_mul(parent_opacity, DP_layer_props_opacity(lp));
    bool censored = pass_through_censored || DP_layer_props_censored(lp);
    if (DP_layer_props_isolated(lp)) {
        // Flatten the group into a temporary layer with full opacity, then
        // merge the result with the group's blend mode and opacity.
        DP_TransientLayerContent *gtlc = DP_transient_layer_content_new_init(
            DP_transient_layer_content_width(tlc),
            DP_transient_layer_content_height(tlc), NULL);
        DP_layer_list_merge_to_flat_image(lg->children, lpl, gtlc, DP_BIT15,
                                          include_sublayers, true, false);
        DP_transient_layer_content_merge(tlc, 0, (DP_LayerContent *)gtlc,
                                         opacity, DP_layer_props_blend_mode(lp),
                                         censored);
        DP_transient_layer_content_decref(gtlc);
    }
    else {
        // Flatten the containing layers one by one, disregarding the blend
        // mode, but taking the opacity into account individually.
        DP_layer_list_merge_to_flat_image(lg->children, lpl, tlc, opacity,
                                          include_sublayers, true, censored);
    }
}

DP_TransientTile *DP_layer_group_flatten_tile(DP_LayerGroup *lg,
                                              DP_LayerProps *lp, int tile_index,
                                              bool include_sublayers)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(lp);
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(lg->width, lg->height));
    DP_ViewModeContext vmc = DP_view_mode_context_make_default();
    return DP_layer_list_flatten_tile_to(
        lg->children, DP_layer_props_children_noinc(lp), tile_index, NULL,
        DP_BIT15, include_sublayers, false, &vmc);
}

DP_TransientTile *
DP_layer_group_flatten_tile_to(DP_LayerGroup *lg, DP_LayerProps *lp,
                               int tile_index, DP_TransientTile *tt_or_null,
                               uint16_t parent_opacity, bool include_sublayers,
                               bool pass_through_censored,
                               const DP_ViewModeContext *vmc)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(lp);
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(lg->width, lg->height));
    DP_ASSERT(parent_opacity <= DP_BIT15);
    DP_ASSERT(vmc);

    DP_ViewModeResult vmr = DP_view_mode_context_apply(vmc, lp, parent_opacity);
    if (!vmr.visible) {
        return tt_or_null;
    }

    DP_LayerPropsList *lpl = DP_layer_props_children_noinc(lp);
    bool censored = pass_through_censored || DP_layer_props_censored(lp);
    if (vmr.isolated) {
        // Flatten the group into a temporary layer with full opacity, then
        // merge the result with the group's blend mode and opacity.
        DP_TransientTile *gtt = DP_layer_list_flatten_tile_to(
            lg->children, lpl, tile_index, NULL, DP_BIT15, include_sublayers,
            false, &vmr.child_vmc);
        if (gtt) {
            DP_TransientTile *tt = DP_transient_tile_merge_nullable(
                tt_or_null,
                censored ? DP_tile_censored_noinc() : (DP_Tile *)gtt,
                vmr.opacity, vmr.blend_mode);
            DP_transient_tile_decref(gtt);
            return tt;
        }
        else {
            return tt_or_null;
        }
    }
    else {
        // Flatten the containing layers one by one, disregarding the blend
        // mode, but taking the opacity into account individually.
        return DP_layer_list_flatten_tile_to(
            lg->children, lpl, tile_index, tt_or_null, vmr.opacity,
            include_sublayers, censored, &vmr.child_vmc);
    }
}


static DP_TransientLayerGroup *alloc_layer_group(int width, int height)
{
    DP_TransientLayerGroup *tlg = DP_malloc(sizeof(*tlg));
    *tlg = (DP_TransientLayerGroup){
        DP_ATOMIC_INIT(1), true, width, height, {NULL}};
    return tlg;
}

DP_TransientLayerGroup *DP_transient_layer_group_new(DP_LayerGroup *lg)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(!lg->transient);
    DP_TransientLayerGroup *tlg = alloc_layer_group(lg->width, lg->height);
    tlg->children = DP_layer_list_incref(lg->children);
    return tlg;
}

DP_TransientLayerGroup *
DP_transient_layer_group_new_with_children_noinc(DP_LayerGroup *lg,
                                                 DP_TransientLayerList *tll)
{
    DP_ASSERT(lg);
    DP_ASSERT(DP_atomic_get(&lg->refcount) > 0);
    DP_ASSERT(!lg->transient);
    DP_TransientLayerGroup *tlg = alloc_layer_group(lg->width, lg->height);
    tlg->transient_children = tll;
    return tlg;
}

DP_TransientLayerGroup *DP_transient_layer_group_new_init(int width, int height,
                                                          int reserve)
{
    DP_ASSERT(reserve >= 0);
    return DP_transient_layer_group_new_init_with_transient_children_noinc(
        width, height, DP_transient_layer_list_new_init(reserve));
}

DP_TransientLayerGroup *
DP_transient_layer_group_new_init_with_transient_children_noinc(
    int width, int height, DP_TransientLayerList *tll)
{
    DP_TransientLayerGroup *tlg = alloc_layer_group(width, height);
    tlg->transient_children = tll;
    return tlg;
}

DP_TransientLayerGroup *
DP_transient_layer_group_incref(DP_TransientLayerGroup *tlg)
{
    DP_ASSERT(tlg);
    DP_ASSERT(DP_atomic_get(&tlg->refcount) > 0);
    DP_ASSERT(tlg->transient);
    return (DP_TransientLayerGroup *)DP_layer_group_incref(
        (DP_LayerGroup *)tlg);
}

void DP_transient_layer_group_decref(DP_TransientLayerGroup *tlg)
{
    DP_ASSERT(tlg);
    DP_ASSERT(DP_atomic_get(&tlg->refcount) > 0);
    DP_ASSERT(tlg->transient);
    DP_layer_group_decref((DP_LayerGroup *)tlg);
}

int DP_transient_layer_group_refcount(DP_TransientLayerGroup *tlg)
{
    DP_ASSERT(tlg);
    DP_ASSERT(DP_atomic_get(&tlg->refcount) > 0);
    DP_ASSERT(tlg->transient);
    return DP_layer_group_refcount((DP_LayerGroup *)tlg);
}

DP_LayerGroup *DP_transient_layer_group_persist(DP_TransientLayerGroup *tlg)
{
    DP_ASSERT(tlg);
    DP_ASSERT(DP_atomic_get(&tlg->refcount) > 0);
    DP_ASSERT(tlg->transient);
    tlg->transient = false;
    if (DP_layer_list_transient(tlg->children)) {
        DP_transient_layer_list_persist(tlg->transient_children);
    }
    return (DP_LayerGroup *)tlg;
}

int DP_transient_layer_group_width(DP_TransientLayerGroup *tlg)
{
    DP_ASSERT(tlg);
    DP_ASSERT(DP_atomic_get(&tlg->refcount) > 0);
    DP_ASSERT(tlg->transient);
    return DP_layer_group_width((DP_LayerGroup *)tlg);
}

int DP_transient_layer_group_height(DP_TransientLayerGroup *tlg)
{
    DP_ASSERT(tlg);
    DP_ASSERT(DP_atomic_get(&tlg->refcount) > 0);
    DP_ASSERT(tlg->transient);
    return DP_layer_group_height((DP_LayerGroup *)tlg);
}

DP_LayerList *
DP_transient_layer_group_children_noinc(DP_TransientLayerGroup *tlg)
{
    DP_ASSERT(tlg);
    DP_ASSERT(DP_atomic_get(&tlg->refcount) > 0);
    DP_ASSERT(tlg->transient);
    return DP_layer_group_children_noinc((DP_LayerGroup *)tlg);
}

DP_TransientLayerList *
DP_transient_layer_group_transient_children(DP_TransientLayerGroup *tlg,
                                            int reserve)
{
    DP_ASSERT(tlg);
    DP_ASSERT(DP_atomic_get(&tlg->refcount) > 0);
    DP_ASSERT(tlg->transient);
    DP_ASSERT(reserve >= 0);
    DP_LayerList *ll = tlg->children;
    if (!DP_layer_list_transient(ll)) {
        tlg->transient_children = DP_transient_layer_list_new(ll, reserve);
        DP_layer_list_decref(ll);
    }
    else if (reserve > 0) {
        tlg->transient_children =
            DP_transient_layer_list_reserve(tlg->transient_children, reserve);
    }
    return tlg->transient_children;
}
