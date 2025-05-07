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
#include "layer_content.h"
#include "layer_group.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "tile.h"
#include "view_mode.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpmsg/blend_mode.h>


struct DP_LayerListEntry {
    bool is_group;
    union {
        DP_LayerContent *content;
        DP_TransientLayerContent *transient_content;
        DP_LayerGroup *group;
        DP_TransientLayerGroup *transient_group;
    };
};

#ifdef DP_NO_STRICT_ALIASING

struct DP_LayerList {
    DP_Atomic refcount;
    const bool transient;
    const int count;
    DP_LayerListEntry elements[];
};

struct DP_TransientLayerList {
    DP_Atomic refcount;
    bool transient;
    int count;
    DP_LayerListEntry elements[];
};

#else

struct DP_LayerList {
    DP_Atomic refcount;
    bool transient;
    int count;
    DP_LayerListEntry elements[];
};

#endif


bool DP_layer_list_entry_is_group(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    return lle->is_group;
}

bool DP_layer_list_entry_transient(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    return lle->is_group ? DP_layer_content_transient(lle->content)
                         : DP_layer_group_transient(lle->group);
}

DP_LayerContent *DP_layer_list_entry_content_noinc(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    DP_ASSERT(!lle->is_group);
    return lle->content;
}

DP_TransientLayerContent *
DP_layer_list_entry_transient_content_noinc(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    DP_ASSERT(!lle->is_group);
    DP_ASSERT(DP_layer_content_transient(lle->content));
    return lle->transient_content;
}

DP_LayerGroup *DP_layer_list_entry_group_noinc(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    DP_ASSERT(lle->is_group);
    return lle->group;
}

DP_TransientLayerGroup *
DP_layer_list_entry_transient_group_noinc(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    DP_ASSERT(!lle->is_group);
    DP_ASSERT(DP_layer_group_transient(lle->group));
    return lle->transient_group;
}

int DP_layer_list_entry_width(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    return lle->is_group ? DP_layer_group_width(lle->group)
                         : DP_layer_content_width(lle->content);
}

int DP_layer_list_entry_height(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    return lle->is_group ? DP_layer_group_height(lle->group)
                         : DP_layer_content_height(lle->content);
}

static DP_LayerListEntry *layer_list_entry_incref(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    if (lle->is_group) {
        DP_layer_group_incref(lle->group);
    }
    else {
        DP_layer_content_incref(lle->content);
    }
    return lle;
}

static void layer_list_entry_decref(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    if (lle->is_group) {
        DP_layer_group_decref(lle->group);
    }
    else {
        DP_layer_content_decref(lle->content);
    }
}

static void layer_list_entry_decref_nullable(DP_LayerListEntry *lle)
{
    DP_ASSERT(lle);
    if (lle->is_group) {
        DP_layer_group_decref_nullable(lle->group);
    }
    else {
        DP_layer_content_decref_nullable(lle->content);
    }
}


static size_t layer_list_size(int count)
{
    return DP_FLEX_SIZEOF(DP_LayerList, elements, DP_int_to_size(count));
}

static void *allocate_layer_list(bool transient, int count)
{
    DP_TransientLayerList *tll = DP_malloc(layer_list_size(count));
    DP_atomic_set(&tll->refcount, 1);
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
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    DP_atomic_inc(&ll->refcount);
    return ll;
}

DP_LayerList *DP_layer_list_incref_nullable(DP_LayerList *ll_or_null)
{
    return ll_or_null ? DP_layer_list_incref(ll_or_null) : NULL;
}

void DP_layer_list_decref(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    if (DP_atomic_dec(&ll->refcount)) {
        int count = ll->count;
        for (int i = 0; i < count; ++i) {
            layer_list_entry_decref_nullable(&ll->elements[i]);
        }
        DP_free(ll);
    }
}

void DP_layer_list_decref_nullable(DP_LayerList *ll_or_null)
{
    if (ll_or_null) {
        DP_layer_list_decref(ll_or_null);
    }
}

int DP_layer_list_refcount(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    return DP_atomic_get(&ll->refcount);
}

bool DP_layer_list_transient(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    return ll->transient;
}


static void mark_layer_list_entry(DP_CanvasDiff *diff, DP_LayerListEntry *lle,
                                  bool is_group)
{
    if (is_group) {
        DP_layer_group_diff_mark(lle->group, diff);
    }
    else {
        DP_layer_content_diff_mark(lle->content, diff);
    }
}

static void diff_layers(DP_LayerList *ll, DP_LayerPropsList *lpl,
                        DP_LayerList *prev_ll, DP_LayerPropsList *prev_lpl,
                        DP_CanvasDiff *diff, int only_layer_id, int count)
{
    int clip = 0;
    int prev_clip = 0;
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = &ll->elements[i];
        bool is_group = lle->is_group;
        DP_LayerListEntry *prev_lle = &prev_ll->elements[i];
        bool prev_is_group = prev_lle->is_group;

        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_LayerProps *prev_lp = DP_layer_props_list_at_noinc(prev_lpl, i);
        if (i != 0) {
            if (DP_layer_props_clip(lp)) {
                ++clip;
            }
            else {
                clip = 0;
            }

            if (DP_layer_props_clip(prev_lp)) {
                ++prev_clip;
            }
            else {
                prev_clip = 0;
            }
        }

        if (clip != prev_clip) {
            mark_layer_list_entry(diff, lle, is_group);
            mark_layer_list_entry(diff, prev_lle, prev_is_group);
        }
        else if (is_group && prev_is_group) {
            DP_layer_group_diff(lle->group, lp, prev_lle->group, prev_lp, diff,
                                only_layer_id);
        }
        else if (!is_group && !prev_is_group) {
            DP_layer_content_diff(lle->content, lp, prev_lle->content, prev_lp,
                                  diff, only_layer_id);
        }
        else if (is_group) {
            DP_layer_group_diff_mark(lle->group, diff);
            DP_layer_content_diff_mark(prev_lle->content, diff);
        }
        else {
            DP_layer_content_diff_mark(lle->content, diff);
            DP_layer_group_diff_mark(prev_lle->group, diff);
        }
    }
}

static void mark_layers(DP_LayerList *ll, DP_CanvasDiff *diff, int start,
                        int end)
{
    for (int i = start; i < end; ++i) {
        DP_LayerListEntry *lle = &ll->elements[i];
        if (lle->is_group) {
            DP_layer_group_diff_mark(lle->group, diff);
        }
        else {
            DP_layer_content_diff_mark(lle->content, diff);
        }
    }
}

void DP_layer_list_diff(DP_LayerList *ll, DP_LayerPropsList *lpl,
                        DP_LayerList *prev_ll, DP_LayerPropsList *prev_lpl,
                        DP_CanvasDiff *diff, int only_layer_id)
{
    DP_ASSERT(ll);
    DP_ASSERT(lpl);
    DP_ASSERT(prev_ll);
    DP_ASSERT(prev_lpl);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    DP_ASSERT(DP_atomic_get(&prev_ll->refcount) > 0);
    if (ll != prev_ll || lpl != prev_lpl) {
        int new_count = ll->count;
        int old_count = prev_ll->count;
        if (new_count <= old_count) {
            diff_layers(ll, lpl, prev_ll, prev_lpl, diff, only_layer_id,
                        new_count);
            mark_layers(prev_ll, diff, new_count, old_count);
        }
        else {
            diff_layers(ll, lpl, prev_ll, prev_lpl, diff, only_layer_id,
                        old_count);
            mark_layers(ll, diff, old_count, new_count);
        }
    }
}

void DP_layer_list_diff_mark(DP_LayerList *ll, DP_CanvasDiff *diff)
{
    DP_ASSERT(ll);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    mark_layers(ll, diff, 0, ll->count);
}


int DP_layer_list_count(DP_LayerList *ll)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    return ll->count;
}

DP_LayerListEntry *DP_layer_list_at_noinc(DP_LayerList *ll, int index)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < ll->count);
    return &ll->elements[index];
}

DP_LayerContent *DP_layer_list_content_at_noinc(DP_LayerList *ll, int index)
{
    return DP_layer_list_entry_content_noinc(DP_layer_list_at_noinc(ll, index));
}

DP_LayerGroup *DP_layer_list_group_at_noinc(DP_LayerList *ll, int index)
{
    return DP_layer_list_entry_group_noinc(DP_layer_list_at_noinc(ll, index));
}

DP_TransientLayerList *DP_layer_list_resize(DP_LayerList *ll,
                                            unsigned int context_id, int top,
                                            int right, int bottom, int left)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    int count = ll->count;
    DP_TransientLayerList *tll = allocate_layer_list(true, count);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = &ll->elements[i];
        if (lle->is_group) {
            DP_TransientLayerGroup *tlg = DP_layer_group_resize(
                lle->group, context_id, top, right, bottom, left);
            tll->elements[i] =
                (DP_LayerListEntry){true, {.transient_group = tlg}};
        }
        else {
            DP_TransientLayerContent *tlc = DP_layer_content_resize(
                lle->content, context_id, top, right, bottom, left);
            tll->elements[i] =
                (DP_LayerListEntry){false, {.transient_content = tlc}};
        }
    }
    return tll;
}

static int count_clipping_layers(DP_LayerPropsList *lpl, int i, int count)
{
    int clip_count = 0;
    for (int j = i + 1;
         j < count && DP_layer_props_clip(DP_layer_props_list_at_noinc(lpl, j));
         ++j) {
        ++clip_count;
    }
    return clip_count;
}

static void layer_list_entry_merge_to_flat_image(
    DP_LayerListEntry *lle, DP_LayerProps *lp, DP_TransientLayerContent *tlc,
    uint16_t parent_opacity, bool include_sublayers, bool reveal_censored,
    bool pass_through_censored, bool clip)
{
    if (lle->is_group) {
        DP_layer_group_merge_to_flat_image(lle->group, lp, tlc, parent_opacity,
                                           include_sublayers,
                                           pass_through_censored, clip);
    }
    else {
        DP_LayerContent *lc = lle->content;
        uint16_t opacity =
            DP_fix15_mul(parent_opacity, DP_layer_props_opacity(lp));
        int blend_mode =
            DP_blend_mode_clip(DP_layer_props_blend_mode(lp), clip);
        bool censored = pass_through_censored
                     || (!reveal_censored && DP_layer_props_censored(lp));
        if (include_sublayers) {
            DP_LayerContent *sub_lc = DP_layer_content_merge_sublayers(lc);
            DP_transient_layer_content_merge(tlc, 0, sub_lc, opacity,
                                             blend_mode, censored);
            DP_layer_content_decref(sub_lc);
        }
        else {
            DP_transient_layer_content_merge(tlc, 0, lc, opacity, blend_mode,
                                             censored);
        }
    }
}

static DP_TransientLayerProps *make_clip_properties(DP_LayerProps *lp)
{
    DP_TransientLayerProps *clip_tlp = DP_transient_layer_props_new(lp);
    DP_transient_layer_props_blend_mode_set(clip_tlp, DP_BLEND_MODE_NORMAL);
    DP_transient_layer_props_censored_set(clip_tlp, false);
    DP_transient_layer_props_opacity_set(clip_tlp, DP_BIT15);
    DP_transient_layer_props_sketch_opacity_set(clip_tlp, 0);
    DP_transient_layer_props_sketch_tint_set(clip_tlp, 0);
    return clip_tlp;
}

static void layer_list_entry_merge_clipping_to_flat_image(
    DP_LayerList *ll, DP_LayerPropsList *lpl, int i, int clip_count,
    DP_TransientLayerContent *tlc, uint16_t parent_opacity,
    bool include_sublayers, bool reveal_censored, bool pass_through_censored,
    bool clip)
{
    DP_LayerListEntry *lle = &ll->elements[i];
    DP_TransientLayerContent *clip_tlc = DP_transient_layer_content_new_init(
        DP_layer_list_entry_width(lle), DP_layer_list_entry_height(lle), NULL);

    DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
    DP_TransientLayerProps *clip_tlp = make_clip_properties(lp);
    layer_list_entry_merge_to_flat_image(lle, (DP_LayerProps *)clip_tlp,
                                         clip_tlc, DP_BIT15, include_sublayers,
                                         reveal_censored, false, false);
    DP_transient_layer_props_decref(clip_tlp);

    for (int j = 0; j < clip_count; ++j) {
        int clip_index = i + j + 1;
        DP_LayerListEntry *clip_lle = &ll->elements[clip_index];
        DP_LayerProps *clip_lp = DP_layer_props_list_at_noinc(lpl, clip_index);
        layer_list_entry_merge_to_flat_image(clip_lle, clip_lp, clip_tlc,
                                             DP_BIT15, include_sublayers,
                                             reveal_censored, false, true);
    }

    DP_transient_layer_content_merge(
        tlc, 0, (DP_LayerContent *)clip_tlc,
        DP_fix15_mul(parent_opacity, DP_layer_props_opacity(lp)),
        DP_blend_mode_clip(DP_layer_props_blend_mode(lp), clip),
        pass_through_censored
            || (!reveal_censored && DP_layer_props_censored(lp)));

    DP_transient_layer_content_decref(clip_tlc);
}

void DP_layer_list_merge_to_flat_image(DP_LayerList *ll, DP_LayerPropsList *lpl,
                                       DP_TransientLayerContent *tlc,
                                       uint16_t parent_opacity,
                                       bool include_sublayers,
                                       bool reveal_censored,
                                       bool pass_through_censored, bool clip)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    DP_ASSERT(lpl);
    DP_ASSERT(DP_layer_props_list_refcount(lpl) > 0);
    DP_ASSERT(ll->count == DP_layer_props_list_count(lpl));
    int count = ll->count;
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int clip_count = count_clipping_layers(lpl, i, count);
        if (DP_layer_props_visible(lp)) {
            if (clip_count == 0) {
                layer_list_entry_merge_to_flat_image(
                    &ll->elements[i], lp, tlc, parent_opacity,
                    include_sublayers, reveal_censored, pass_through_censored,
                    clip);
            }
            else {
                layer_list_entry_merge_clipping_to_flat_image(
                    ll, lpl, i, clip_count, tlc, parent_opacity,
                    include_sublayers, reveal_censored, pass_through_censored,
                    clip);
            }
        }
        i += clip_count;
    }
}

DP_TransientTile *DP_layer_list_entry_flatten_tile_to(
    DP_LayerListEntry *lle, DP_LayerProps *lp, int tile_index,
    DP_TransientTile *tt, uint16_t parent_opacity, DP_UPixel8 parent_tint,
    bool include_sublayers, bool pass_through_censored, bool clip,
    const DP_ViewModeContext *vmc)
{
    if (lle->is_group) {
        return DP_layer_group_flatten_tile_to(
            lle->group, lp, tile_index, tt, parent_opacity, parent_tint,
            include_sublayers, pass_through_censored, clip, vmc);
    }
    else {
        DP_ViewModeResult vmr =
            DP_view_mode_context_apply(vmc, lp, parent_opacity);
        if (vmr.visible) {
            bool censored =
                pass_through_censored || DP_layer_props_censored(lp);
            return DP_layer_content_flatten_tile_to(
                lle->content, tile_index, tt, vmr.opacity,
                DP_blend_mode_clip(vmr.blend_mode, clip),
                vmr.tint.a == 0 ? parent_tint : vmr.tint, censored,
                include_sublayers);
        }
        else {
            return tt;
        }
    }
}

static DP_TransientTile *flatten_clipping_tile(
    void *user,
    DP_ViewModeContext (*fn)(void *, int, DP_LayerListEntry **,
                             DP_LayerProps **),
    int tile_index, bool include_sublayers, const DP_ViewModeContext *vmc,
    DP_LayerListEntry *lle, DP_LayerProps *lp, int i, int clip_count)
{
    DP_TransientTile *clip_tt = NULL;
    DP_TransientLayerProps *clip_tlp = make_clip_properties(lp);
    clip_tt = DP_layer_list_entry_flatten_tile_to(
        lle, (DP_LayerProps *)clip_tlp, tile_index, clip_tt, DP_BIT15,
        (DP_UPixel8){.color = 0}, include_sublayers, false, false, vmc);
    DP_transient_layer_props_decref(clip_tlp);
    if (clip_tt) {
        for (int j = 0; j < clip_count; ++j) {
            DP_LayerListEntry *clip_lle;
            DP_LayerProps *clip_lp;
            DP_ViewModeContext clip_vmc =
                fn(user, i + j + 1, &clip_lle, &clip_lp);
            clip_tt = DP_layer_list_entry_flatten_tile_to(
                clip_lle, clip_lp, tile_index, clip_tt, DP_BIT15,
                (DP_UPixel8){.color = 0}, include_sublayers, false, true,
                &clip_vmc);
        }
    }
    return clip_tt;
}

DP_TransientTile *DP_layer_list_flatten_clipping_tile_to(
    void *user,
    DP_ViewModeContext (*fn)(void *, int, DP_LayerListEntry **,
                             DP_LayerProps **),
    int i, int clip_count, int tile_index, DP_TransientTile *tt_or_null,
    uint16_t parent_opacity, bool include_sublayers,
    const DP_ViewModeContext *vmc)
{
    DP_LayerListEntry *lle;
    DP_LayerProps *lp;
    fn(user, i, &lle, &lp);
    DP_TransientTile *clip_tt = flatten_clipping_tile(
        user, fn, tile_index, include_sublayers, vmc, lle, lp, i, clip_count);
    if (clip_tt) {
        DP_ViewModeResult vmr =
            DP_view_mode_context_apply(vmc, lp, parent_opacity);
        if (vmr.visible) {
            if (DP_layer_props_censored(lp)) {
                tt_or_null = DP_transient_tile_merge_nullable(
                    tt_or_null, DP_tile_censored_noinc(), vmr.opacity,
                    vmr.blend_mode);
            }
            else {
                if (vmr.tint.a != 0) {
                    DP_transient_tile_tint(clip_tt, vmr.tint);
                }
                tt_or_null = DP_transient_tile_merge_nullable(
                    tt_or_null, (DP_Tile *)clip_tt, vmr.opacity,
                    vmr.blend_mode);
            }
        }
        DP_transient_tile_decref(clip_tt);
    }
    return tt_or_null;
}

static DP_ViewModeContext get_clip_layer(void *user, int i,
                                         DP_LayerListEntry **out_lle,
                                         DP_LayerProps **out_lp)
{
    DP_LayerList *ll = ((void **)user)[0];
    DP_LayerPropsList *lpl = ((void **)user)[1];
    const DP_ViewModeContext *vmc = ((void **)user)[2];
    *out_lle = DP_layer_list_at_noinc(ll, i);
    *out_lp = DP_layer_props_list_at_noinc(lpl, i);
    return *vmc; // XXX: Probably not the right thing?
}

DP_TransientTile *DP_layer_list_flatten_tile_to(
    DP_LayerList *ll, DP_LayerPropsList *lpl, int tile_index,
    DP_TransientTile *tt_or_null, uint16_t parent_opacity,
    DP_UPixel8 parent_tint, bool include_sublayers, bool pass_through_censored,
    bool clip, const DP_ViewModeContext *vmc)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    DP_ASSERT(lpl);
    DP_ASSERT(DP_layer_props_list_refcount(lpl) > 0);
    DP_ASSERT(ll->count == DP_layer_props_list_count(lpl));
    DP_ASSERT(vmc);
    int count = ll->count;
    DP_TransientTile *tt = tt_or_null;
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = &ll->elements[i];
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int clip_count = count_clipping_layers(lpl, i, count);
        if (clip_count == 0) {
            tt = DP_layer_list_entry_flatten_tile_to(
                lle, lp, tile_index, tt, parent_opacity, parent_tint,
                include_sublayers, pass_through_censored, clip, vmc);
        }
        else {
            tt = DP_layer_list_flatten_clipping_tile_to(
                (void *[]){ll, lpl, (void *)vmc}, get_clip_layer, i, clip_count,
                tile_index, tt, parent_opacity, include_sublayers, vmc);
            i += clip_count;
        }
    }
    return tt;
}

void DP_layer_list_entry_flatten_pixel(
    DP_LayerListEntry *lle, DP_LayerProps *lp, int x, int y, DP_Pixel15 *pixel,
    uint16_t parent_opacity, DP_UPixel8 parent_tint, bool pass_through_censored,
    const DP_ViewModeContext *vmc)
{
    DP_ASSERT(lle);
    DP_ASSERT(lp);
    DP_ASSERT(pixel);
    DP_ASSERT(vmc);
    if (lle->is_group) {
        DP_layer_group_flatten_pixel(lle->group, lp, x, y, pixel,
                                     parent_opacity, parent_tint,
                                     pass_through_censored, vmc);
    }
    else {
        DP_ViewModeResult vmr =
            DP_view_mode_context_apply(vmc, lp, parent_opacity);
        if (vmr.visible) {
            bool censored =
                pass_through_censored || DP_layer_props_censored(lp);
            DP_layer_content_flatten_pixel(
                lle->content, x, y, pixel, vmr.opacity, vmr.blend_mode,
                vmr.tint.a == 0 ? parent_tint : vmr.tint, censored);
        }
    }
}

static DP_Pixel15 flatten_clipping_pixel(
    void *user,
    DP_ViewModeContext (*fn)(void *, int, DP_LayerListEntry **,
                             DP_LayerProps **),
    int x, int y, const DP_ViewModeContext *vmc, DP_LayerListEntry *lle,
    DP_LayerProps *lp, int i, int clip_count)
{
    DP_Pixel15 clip_pixel = DP_pixel15_zero();
    DP_TransientLayerProps *clip_tlp = make_clip_properties(lp);
    DP_layer_list_entry_flatten_pixel(lle, (DP_LayerProps *)clip_tlp, x, y,
                                      &clip_pixel, DP_BIT15,
                                      (DP_UPixel8){.color = 0}, false, vmc);
    DP_transient_layer_props_decref(clip_tlp);
    if (clip_pixel.a != 0) {
        for (int j = 0; j < clip_count; ++j) {
            DP_LayerListEntry *clip_lle;
            DP_LayerProps *clip_lp;
            DP_ViewModeContext clip_vmc =
                fn(user, i + j + 1, &clip_lle, &clip_lp);
            DP_layer_list_entry_flatten_pixel(
                clip_lle, clip_lp, x, y, &clip_pixel, DP_BIT15,
                (DP_UPixel8){.color = 0}, false, &clip_vmc);
        }
    }
    return clip_pixel;
}

void DP_layer_list_flatten_clipping_pixel(
    void *user,
    DP_ViewModeContext (*fn)(void *, int, DP_LayerListEntry **,
                             DP_LayerProps **),
    int i, int clip_count, int x, int y, DP_Pixel15 *pixel,
    uint16_t parent_opacity, const DP_ViewModeContext *vmc)
{
    DP_ASSERT(fn);
    DP_ASSERT(pixel);
    DP_ASSERT(vmc);
    DP_LayerListEntry *lle;
    DP_LayerProps *lp;
    fn(user, i, &lle, &lp);
    DP_Pixel15 clip_pixel =
        flatten_clipping_pixel(user, fn, x, y, vmc, lle, lp, i, clip_count);
    if (clip_pixel.a != 0) {
        DP_ViewModeResult vmr =
            DP_view_mode_context_apply(vmc, lp, parent_opacity);
        if (vmr.visible) {
            if (DP_layer_props_censored(lp)) {
                clip_pixel =
                    DP_tile_pixel_at(DP_tile_censored_noinc(), x % DP_TILE_SIZE,
                                     y % DP_TILE_SIZE);
            }
            else if (vmr.tint.a != 0) {
                DP_tint_pixels(&clip_pixel, 1, vmr.tint);
            }
            DP_blend_pixels(pixel, &clip_pixel, 1, vmr.opacity, vmr.blend_mode);
        }
    }
}

void DP_layer_list_flatten_pixel(DP_LayerList *ll, DP_LayerPropsList *lpl,
                                 int x, int y, DP_Pixel15 *pixel,
                                 uint16_t parent_opacity,
                                 DP_UPixel8 parent_tint,
                                 bool pass_through_censored,
                                 const DP_ViewModeContext *vmc)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    DP_ASSERT(lpl);
    DP_ASSERT(DP_layer_props_list_refcount(lpl) > 0);
    DP_ASSERT(ll->count == DP_layer_props_list_count(lpl));
    DP_ASSERT(vmc);
    int count = ll->count;
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = &ll->elements[i];
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int clip_count = count_clipping_layers(lpl, i, count);
        if (clip_count == 0) {
            DP_layer_list_entry_flatten_pixel(lle, lp, x, y, pixel,
                                              parent_opacity, parent_tint,
                                              pass_through_censored, vmc);
        }
        else {
            DP_layer_list_flatten_clipping_pixel(
                (void *[]){ll, lpl, (void *)vmc}, get_clip_layer, i, clip_count,
                x, y, pixel, parent_opacity, vmc);
            i += clip_count;
        }
    }
}


DP_TransientLayerList *DP_transient_layer_list_new_init(int reserve)
{
    DP_TransientLayerList *tll = allocate_layer_list(true, reserve);
    for (int i = 0; i < reserve; ++i) {
        tll->elements[i] = (DP_LayerListEntry){false, {NULL}};
    }
    return tll;
}

DP_TransientLayerList *DP_transient_layer_list_new(DP_LayerList *ll,
                                                   int reserve)
{
    DP_ASSERT(ll);
    DP_ASSERT(DP_atomic_get(&ll->refcount) > 0);
    DP_ASSERT(!ll->transient);
    DP_ASSERT(reserve >= 0);
    int count = ll->count;
    DP_TransientLayerList *tll = allocate_layer_list(true, count + reserve);
    for (int i = 0; i < count; ++i) {
        tll->elements[i] = *layer_list_entry_incref(&ll->elements[i]);
    }
    for (int i = 0; i < reserve; ++i) {
        tll->elements[count + i] = (DP_LayerListEntry){false, {NULL}};
    }
    return tll;
}

DP_TransientLayerList *
DP_transient_layer_list_reserve(DP_TransientLayerList *tll, int reserve)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(reserve >= 0);
    DP_debug("Reserve %d elements in layer content list", reserve);
    if (reserve > 0) {
        int old_count = tll->count;
        int new_count = old_count + reserve;
        tll = DP_realloc(tll, layer_list_size(new_count));
        tll->count = new_count;
        for (int i = old_count; i < new_count; ++i) {
            tll->elements[i] = (DP_LayerListEntry){false, {NULL}};
        }
    }
    return tll;
}

DP_TransientLayerList *
DP_transient_layer_list_incref(DP_TransientLayerList *tll)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    return (DP_TransientLayerList *)DP_layer_list_incref((DP_LayerList *)tll);
}

void DP_transient_layer_list_decref(DP_TransientLayerList *tll)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_layer_list_decref((DP_LayerList *)tll);
}

int DP_transient_layer_list_refcount(DP_TransientLayerList *tll)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    return DP_layer_list_refcount((DP_LayerList *)tll);
}

DP_LayerList *DP_transient_layer_list_persist(DP_TransientLayerList *tll)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    tll->transient = false;
    int count = tll->count;
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = &tll->elements[i];
        if (lle->is_group) {
            DP_ASSERT(lle->group);
            if (DP_layer_group_transient(lle->group)) {
                DP_transient_layer_group_persist(lle->transient_group);
            }
        }
        else {
            DP_ASSERT(lle->content);
            if (DP_layer_content_transient(lle->content)) {
                DP_transient_layer_content_persist(lle->transient_content);
            }
        }
    }
    return (DP_LayerList *)tll;
}

int DP_transient_layer_list_count(DP_TransientLayerList *tll)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    return DP_layer_list_count((DP_LayerList *)tll);
}

DP_LayerListEntry *DP_transient_layer_list_at_noinc(DP_TransientLayerList *tll,
                                                    int index)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    return DP_layer_list_at_noinc((DP_LayerList *)tll, index);
}

DP_TransientLayerContent *
DP_transient_layer_list_transient_content_at_noinc(DP_TransientLayerList *tll,
                                                   int index)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    DP_LayerListEntry *lle = &tll->elements[index];
    DP_ASSERT(!lle->is_group);
    DP_LayerContent *lc = lle->content;
    if (!DP_layer_content_transient(lc)) {
        lle->transient_content = DP_transient_layer_content_new(lc);
        DP_layer_content_decref(lc);
    }
    return lle->transient_content;
}

DP_TransientLayerGroup *
DP_transient_layer_list_transient_group_at_noinc(DP_TransientLayerList *tll,
                                                 int index)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    DP_LayerListEntry *lle = &tll->elements[index];
    DP_ASSERT(lle->is_group);
    DP_LayerGroup *lg = lle->group;
    if (!DP_layer_group_transient(lg)) {
        lle->transient_group = DP_transient_layer_group_new(lg);
        DP_layer_group_decref(lg);
    }
    return lle->transient_group;
}

DP_TransientLayerGroup *
DP_transient_layer_list_transient_group_at_with_children_noinc(
    DP_TransientLayerList *tll, int index,
    DP_TransientLayerList *transient_children)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    DP_ASSERT(transient_children);
    DP_LayerListEntry *lle = &tll->elements[index];
    DP_ASSERT(lle->is_group);
    DP_LayerGroup *lg = lle->group;
    DP_ASSERT(!DP_layer_group_transient(lg));
    lle->transient_group = DP_transient_layer_group_new_with_children_noinc(
        lg, transient_children);
    DP_layer_group_decref(lg);
    return lle->transient_group;
}

static void set_element_at(DP_TransientLayerList *tll, int index,
                           DP_LayerListEntry lle)
{
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    DP_ASSERT(!tll->elements[index].is_group);
    DP_ASSERT(!tll->elements[index].content);
    tll->elements[index] = lle;
}

void DP_transient_layer_list_set_content_noinc(DP_TransientLayerList *tll,
                                               DP_LayerContent *lc, int index)
{
    DP_ASSERT(lc);
    set_element_at(tll, index, (DP_LayerListEntry){false, {.content = lc}});
}

void DP_transient_layer_list_set_content_inc(DP_TransientLayerList *tll,
                                             DP_LayerContent *lc, int index)
{
    DP_ASSERT(lc);
    DP_transient_layer_list_set_content_noinc(tll, DP_layer_content_incref(lc),
                                              index);
}


void DP_transient_layer_list_set_group_noinc(DP_TransientLayerList *tll,
                                             DP_LayerGroup *lg, int index)
{
    DP_ASSERT(lg);
    set_element_at(tll, index, (DP_LayerListEntry){true, {.group = lg}});
}

void DP_transient_layer_list_set_group_inc(DP_TransientLayerList *tll,
                                           DP_LayerGroup *lg, int index)
{
    DP_ASSERT(lg);
    DP_transient_layer_list_set_group_noinc(tll, DP_layer_group_incref(lg),
                                            index);
}

void DP_transient_layer_list_set_inc(DP_TransientLayerList *tll,
                                     DP_LayerListEntry *lle, int index)
{
    DP_ASSERT(lle);
    if (lle->is_group) {
        DP_transient_layer_list_set_group_inc(tll, lle->group, index);
    }
    else {
        DP_transient_layer_list_set_content_inc(tll, lle->content, index);
    }
}

static void insert_element_at(DP_TransientLayerList *tll, int index,
                              DP_LayerListEntry lle)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(!tll->elements[tll->count - 1].is_group);
    DP_ASSERT(!tll->elements[tll->count - 1].content);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    memmove(&tll->elements[index + 1], &tll->elements[index],
            sizeof(*tll->elements) * DP_int_to_size(tll->count - index - 1));
    tll->elements[index] = lle;
}

void DP_transient_layer_list_insert_content_inc(DP_TransientLayerList *tll,
                                                DP_LayerContent *lc, int index)
{
    DP_ASSERT(lc);
    DP_ASSERT(!DP_layer_content_transient(lc));
    insert_element_at(
        tll, index,
        (DP_LayerListEntry){false, {.content = DP_layer_content_incref(lc)}});
}

void DP_transient_layer_list_insert_group_inc(DP_TransientLayerList *tll,
                                              DP_LayerGroup *lg, int index)
{
    DP_ASSERT(lg);
    DP_ASSERT(!DP_layer_group_transient(lg));
    insert_element_at(
        tll, index,
        (DP_LayerListEntry){true, {.group = DP_layer_group_incref(lg)}});
}

void DP_transient_layer_list_set_transient_content_noinc(
    DP_TransientLayerList *tll, DP_TransientLayerContent *tlc, int index)
{
    DP_ASSERT(tlc);
    set_element_at(tll, index,
                   (DP_LayerListEntry){false, {.transient_content = tlc}});
}

void DP_transient_layer_list_set_transient_group_noinc(
    DP_TransientLayerList *tll, DP_TransientLayerGroup *tlg, int index)
{
    DP_ASSERT(tlg);
    set_element_at(tll, index,
                   (DP_LayerListEntry){true, {.transient_group = tlg}});
}

void DP_transient_layer_list_insert_transient_content_noinc(
    DP_TransientLayerList *tll, DP_TransientLayerContent *tlc, int index)
{
    DP_ASSERT(tlc);
    insert_element_at(tll, index,
                      (DP_LayerListEntry){false, {.transient_content = tlc}});
}

void DP_transient_layer_list_insert_transient_group_noinc(
    DP_TransientLayerList *tll, DP_TransientLayerGroup *tlg, int index)
{
    DP_ASSERT(tlg);
    insert_element_at(tll, index,
                      (DP_LayerListEntry){true, {.transient_group = tlg}});
}

void DP_transient_layer_list_delete_at(DP_TransientLayerList *tll, int index)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    layer_list_entry_decref(&tll->elements[index]);
    int new_count = --tll->count;
    memmove(&tll->elements[index], &tll->elements[index + 1],
            DP_int_to_size(new_count - index) * sizeof(tll->elements[0]));
}

void DP_transient_layer_list_merge_at(DP_TransientLayerList *tll,
                                      DP_LayerProps *lp, int index)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(lp);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < tll->count);
    DP_ASSERT(tll->elements[index].is_group);
    DP_LayerGroup *lg = tll->elements[index].group;
    DP_TransientLayerContent *tlc = DP_layer_group_merge(lg, lp, true);
    DP_layer_group_decref(lg);
    tll->elements[index] =
        (DP_LayerListEntry){.is_group = false, .transient_content = tlc};
}

void DP_transient_layer_list_clamp(DP_TransientLayerList *tll, int count)
{
    DP_ASSERT(tll);
    DP_ASSERT(DP_atomic_get(&tll->refcount) > 0);
    DP_ASSERT(tll->transient);
    DP_ASSERT(count >= 0);
    DP_ASSERT(count <= tll->count);
    int old_count = tll->count;
    for (int i = count; i < old_count; ++i) {
        layer_list_entry_decref_nullable(&tll->elements[i]);
    }
    tll->count = count;
}
