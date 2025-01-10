/*
 * Copyright (C) 2022 askmeaboufoom
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
#include "view_mode.h"
#include "canvas_state.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "layer_routes.h"
#include "local_state.h"
#include "timeline.h"
#include "track.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/vector.h>
#include <dpmsg/blend_mode.h>


#define TYPE_NORMAL       0
#define TYPE_NOTHING      1
#define TYPE_LAYER        2
#define TYPE_FRAME_MANUAL 3
#define TYPE_FRAME_RENDER 4
#define TYPE_CALLBACK     5


typedef struct DP_ViewModeTrack {
    int layer_id;
    DP_Vector hidden_layer_ids;
    const DP_OnionSkin *onion_skin;
} DP_ViewModeTrack;

struct DP_OnionSkins {
    bool wrap;
    int count_below;
    int count_above;
    DP_OnionSkin skins[];
};


void DP_view_mode_buffer_init(DP_ViewModeBuffer *vmb)
{
    *vmb = (DP_ViewModeBuffer){0, 0, NULL};
}

void DP_view_mode_buffer_dispose(DP_ViewModeBuffer *vmb)
{
    if (vmb) {
        int track_count = vmb->capacity; // Not count, we want to clean em all.
        for (int i = 0; i < track_count; ++i) {
            DP_vector_dispose(&vmb->tracks[i].hidden_layer_ids);
        }
        DP_free(vmb->tracks);
        *vmb = (DP_ViewModeBuffer){0, 0, NULL};
    }
}

static DP_ViewModeTrack *view_mode_buffer_push(DP_ViewModeBuffer *vmb)
{
    int capacity = vmb->capacity;
    int index = vmb->count;
    if (capacity <= index) {
        int new_capacity = DP_max_int(capacity * 2, 8);
        vmb->tracks = DP_realloc(
            vmb->tracks, sizeof(*vmb->tracks) * DP_int_to_size(new_capacity));
        vmb->capacity = new_capacity;
        for (int i = capacity; i < new_capacity; ++i) {
            vmb->tracks[i] = (DP_ViewModeTrack){0, DP_VECTOR_NULL, NULL};
        }
    }
    vmb->count = index + 1;
    return &vmb->tracks[index];
}

static void view_mode_buffer_pop(DP_ViewModeBuffer *vmb)
{
    --vmb->count;
}


static DP_ViewModeFilter make_normal_filter(void)
{
    return (DP_ViewModeFilter){TYPE_NORMAL, {0}};
}

static DP_ViewModeContext make_normal_context(void)
{
    return (DP_ViewModeContext){TYPE_NORMAL, {0}};
}

static DP_ViewModeContext make_nothing_context(void)
{
    return (DP_ViewModeContext){TYPE_NOTHING, {0}};
}

static DP_ViewModeFilter make_layer_filter(int layer_id)
{
    return (DP_ViewModeFilter){TYPE_LAYER, {.layer_id = layer_id}};
}

static DP_ViewModeContext make_layer_context(int layer_id)
{
    return (DP_ViewModeContext){TYPE_LAYER, {.layer_id = layer_id}};
}

static DP_ViewModeContext make_frame_context(int internal_type, int track_index,
                                             DP_ViewModeBuffer *vmb)
{
    return (DP_ViewModeContext){internal_type, {.frame = {track_index, vmb}}};
}


DP_ViewModeFilter DP_view_mode_filter_make_default(void)
{
    return make_normal_filter();
}

static DP_KeyFrameLayer get_key_frame_layer(int layer_id, int count,
                                            const DP_KeyFrameLayer *kfls)
{
    for (int i = 0; i < count; ++i) {
        if (kfls[i].layer_id == layer_id) {
            return kfls[i];
        }
    }
    return (DP_KeyFrameLayer){layer_id, 0};
}

static bool build_frame_layer_visibility(DP_LayerProps *lp, int count,
                                         const DP_KeyFrameLayer *kfls,
                                         bool parent_hidden,
                                         bool check_layer_visibility,
                                         DP_AddVisibleLayerFn fn, void *user)
{
    int layer_id = DP_layer_props_id(lp);
    DP_KeyFrameLayer kfl = get_key_frame_layer(layer_id, count, kfls);
    bool hidden = DP_key_frame_layer_hidden(&kfl)
               || (parent_hidden && !DP_key_frame_layer_revealed(&kfl))
               || (check_layer_visibility && !DP_layer_props_visible(lp));

    bool all_children_hidden = true;
    DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
    if (child_lpl) {
        int child_count = DP_layer_props_list_count(child_lpl);
        for (int i = 0; i < child_count; ++i) {
            DP_LayerProps *child_lp =
                DP_layer_props_list_at_noinc(child_lpl, i);
            bool child_visible =
                build_frame_layer_visibility(child_lp, count, kfls, hidden,
                                             check_layer_visibility, fn, user);
            if (child_visible) {
                all_children_hidden = false;
            }
        }
    }

    bool visible = !hidden || !all_children_hidden;
    fn(user, layer_id, visible);
    return visible;
}

static void add_hidden_layer_id(void *user, int layer_id, bool visible)
{
    DP_Vector *hidden_layer_ids = user;
    if (!visible) {
        DP_VECTOR_PUSH_TYPE(hidden_layer_ids, int, layer_id);
    }
}

static bool build_view_frame(DP_ViewModeTrack *vmt, const DP_OnionSkin *os,
                             DP_LayerProps *lp, DP_KeyFrame *kf)
{
    vmt->layer_id = DP_layer_props_id(lp);
    vmt->onion_skin = os;

    int count;
    const DP_KeyFrameLayer *kfls = DP_key_frame_layers(kf, &count);
    DP_Vector *hidden_layer_ids = &vmt->hidden_layer_ids;
    hidden_layer_ids->used = 0;
    if (count != 0) {
        if (!hidden_layer_ids->elements) {
            DP_VECTOR_INIT_TYPE(hidden_layer_ids, int, 8);
        }
        bool any_visible =
            build_frame_layer_visibility(lp, count, kfls, false, false,
                                         add_hidden_layer_id, hidden_layer_ids);
        if (!any_visible) {
            return false; // Don't need this frame after all.
        }
    }
    return true;
}

static void build_key_frame(DP_ViewModeBuffer *vmb, DP_CanvasState *cs,
                            DP_Track *t, int i, const DP_OnionSkin *os)
{
    DP_KeyFrame *kf = DP_track_key_frame_at_noinc(t, i);
    int layer_id = DP_key_frame_layer_id(kf);
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre =
        layer_id == 0 ? NULL : DP_layer_routes_search(lr, layer_id);
    if (lre) {
        DP_ViewModeTrack *vmt = view_mode_buffer_push(vmb);
        DP_LayerProps *lp = DP_layer_routes_entry_props(lre, cs);
        if (!build_view_frame(vmt, os, lp, kf)) {
            view_mode_buffer_pop(vmb);
        }
    }
}

static int imod(int x, int y)
{
    return ((x % y) + y) % y; // Modulo with positive results.
}

static void build_track_onion_skins(DP_ViewModeBuffer *vmb, DP_CanvasState *cs,
                                    DP_Track *t, const DP_OnionSkins *oss,
                                    int target)
{
    int key_frame_count = DP_track_key_frame_count(t);
    bool wrap = DP_onion_skins_wrap(oss);

    int count_below = DP_onion_skins_count_below(oss);
    for (int i = 0; i < count_below; ++i) {
        int j = target - count_below + i;
        int skin_index = wrap ? imod(j, key_frame_count) : j;
        const DP_OnionSkin *os = DP_onion_skins_skin_below_at(oss, i);
        bool is_visible =
            skin_index >= 0 && skin_index < key_frame_count && os->opacity != 0;
        if (is_visible) {
            build_key_frame(vmb, cs, t, skin_index, os);
        }
    }

    int count_above = DP_onion_skins_count_above(oss);
    for (int i = 0; i < count_above; ++i) {
        int j = target + i + 1;
        int skin_index = wrap ? imod(j, key_frame_count) : j;
        const DP_OnionSkin *os = DP_onion_skins_skin_above_at(oss, i);
        bool is_visible =
            skin_index >= 0 && skin_index < key_frame_count && os->opacity != 0;
        if (is_visible) {
            build_key_frame(vmb, cs, t, skin_index, os);
        }
    }
}

static void build_track(DP_ViewModeBuffer *vmb, DP_CanvasState *cs, DP_Track *t,
                        int frame_index, const DP_OnionSkins *oss)
{
    int i = DP_track_key_frame_search_at_or_before(t, frame_index);
    if (i == -1) {
        // We didn't find any frame here, but there may be onion skins above.
        if (oss && DP_track_onion_skin(t)) {
            int j = DP_track_key_frame_search_at_or_after(t, frame_index, NULL);
            if (j != -1) {
                build_track_onion_skins(vmb, cs, t, oss, j - 1);
            }
        }
    }
    else {
        if (oss && DP_track_onion_skin(t)) {
            build_track_onion_skins(vmb, cs, t, oss, i);
        }
        build_key_frame(vmb, cs, t, i, NULL);
    }
}

static DP_ViewModeFilter make_frame_filter(DP_ViewModeBuffer *vmb,
                                           DP_CanvasState *cs, int frame_index,
                                           const DP_OnionSkins *oss,
                                           int internal_type)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int track_count = DP_timeline_count(tl);
    vmb->count = 0;
    for (int i = 0; i < track_count; ++i) {
        DP_Track *t = DP_timeline_at_noinc(tl, i);
        if (!DP_track_hidden(t)) {
            build_track(vmb, cs, t, frame_index, oss);
        }
    }
    return (DP_ViewModeFilter){internal_type, {.vmb = vmb}};
}

DP_ViewModeFilter DP_view_mode_filter_make_frame_render(DP_ViewModeBuffer *vmb,
                                                        DP_CanvasState *cs,
                                                        int frame_index)
{
    return make_frame_filter(vmb, cs, frame_index, NULL, TYPE_FRAME_RENDER);
}

DP_ViewModeFilter DP_view_mode_filter_make(DP_ViewModeBuffer *vmb,
                                           DP_ViewMode vm, DP_CanvasState *cs,
                                           int layer_id, int frame_index,
                                           const DP_OnionSkins *oss)
{
    switch (vm) {
    case DP_VIEW_MODE_NORMAL:
        return make_normal_filter();
    case DP_VIEW_MODE_LAYER:
        return make_layer_filter(layer_id);
    case DP_VIEW_MODE_GROUP: {
        DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
        int group_id = DP_layer_routes_search_parent_id(lr, layer_id);
        return group_id > 0 ? make_layer_filter(group_id)
                            : make_normal_filter();
    }
    case DP_VIEW_MODE_FRAME:
        return make_frame_filter(vmb, cs, frame_index, oss, TYPE_FRAME_MANUAL);
    default:
        DP_UNREACHABLE();
    }
}

DP_ViewModeFilter DP_view_mode_filter_make_from_active(DP_ViewModeBuffer *vmb,
                                                       DP_ViewMode vm,
                                                       DP_CanvasState *cs,
                                                       int active,
                                                       const DP_OnionSkins *oss)
{
    return DP_view_mode_filter_make(vmb, vm, cs, active, active, oss);
}

DP_ViewModeFilter
DP_view_mode_filter_make_callback(DP_ViewModeCallback *callback)
{
    if (callback) {
        return (DP_ViewModeFilter){TYPE_CALLBACK, {.callback = callback}};
    }
    else {
        return make_normal_filter();
    }
}

bool DP_view_mode_filter_excludes_everything(const DP_ViewModeFilter *vmf)
{
    DP_ASSERT(vmf);
    return vmf->internal_type == TYPE_NOTHING;
}


static DP_ViewModeResult make_result(bool visible, bool isolated,
                                     uint16_t opacity, int blend_mode,
                                     DP_ViewModeContext child_vmc)
{
    return (DP_ViewModeResult){visible, isolated, opacity, blend_mode,
                               child_vmc};
}

static DP_ViewModeResult apply_layer(int layer_id, DP_LayerProps *lp)
{
    if (DP_layer_props_id(lp) == layer_id) {
        return make_result(true, DP_layer_props_isolated(lp),
                           DP_layer_props_opacity(lp), DP_BLEND_MODE_NORMAL,
                           make_normal_context());
    }
    else if (DP_layer_props_children_noinc(lp)) {
        return make_result(true, false, DP_BIT15, DP_BLEND_MODE_NORMAL,
                           make_layer_context(layer_id));
    }
    else {
        return make_result(false, false, 0, DP_BLEND_MODE_NORMAL,
                           make_nothing_context());
    }
}

static bool is_hidden_in_frame(DP_LayerProps *lp, DP_ViewModeTrack *vmt)
{
    DP_Vector *hidden_layer_ids = &vmt->hidden_layer_ids;
    size_t used = hidden_layer_ids->used;
    if (used != 0) {
        int layer_id = DP_layer_props_id(lp);
        int *elements = hidden_layer_ids->elements;
        for (size_t i = 0; i < used; ++i) {
            if (elements[i] == layer_id) {
                return true;
            }
        }
    }
    return false;
}

static DP_ViewModeResult apply_frame(int internal_type, int track_index,
                                     DP_ViewModeBuffer *vmb, DP_LayerProps *lp)
{
    DP_ASSERT(track_index >= 0);
    DP_ASSERT(track_index < vmb->count);
    DP_ViewModeTrack *vmt = &vmb->tracks[track_index];
    if (vmt->layer_id == 0 || is_hidden_in_frame(lp, vmt)) {
        return make_result(false, false, 0, DP_BLEND_MODE_NORMAL,
                           make_nothing_context());
    }
    else {
        return make_result(true, DP_layer_props_isolated(lp),
                           DP_layer_props_opacity(lp),
                           DP_layer_props_blend_mode(lp),
                           make_frame_context(internal_type, track_index, vmb));
    }
}

static DP_ViewModeResult apply_callback(DP_ViewModeCallback *callback,
                                        DP_LayerProps *lp,
                                        uint16_t parent_opacity)
{
    if (callback->is_visible(callback->user, lp)) {
        return make_result(
            true, DP_layer_props_isolated(lp),
            DP_fix15_mul(parent_opacity, DP_layer_props_opacity(lp)),
            DP_layer_props_blend_mode(lp),
            (DP_ViewModeContext){TYPE_CALLBACK, {.callback = callback}});
    }
    else {
        return make_result(false, false, 0, DP_BLEND_MODE_NORMAL,
                           make_nothing_context());
    }
}

static bool is_frame_type(int internal_type)
{
    return internal_type == TYPE_FRAME_MANUAL
        || internal_type == TYPE_FRAME_RENDER;
}

DP_ViewModeContextRoot
DP_view_mode_context_root_init(const DP_ViewModeFilter *vmf, DP_CanvasState *cs)
{
    DP_ASSERT(vmf);
    DP_ASSERT(cs);
    int count = is_frame_type(vmf->internal_type)
                  ? vmf->vmb->count
                  : DP_layer_list_count(DP_canvas_state_layers_noinc(cs));
    return (DP_ViewModeContextRoot){*vmf, count};
}

DP_ViewModeContext DP_view_mode_context_make_default(void)
{
    return make_normal_context();
}

bool DP_view_mode_context_excludes_everything(const DP_ViewModeContext *vmc)
{
    DP_ASSERT(vmc);
    return vmc->internal_type == TYPE_NOTHING;
}

DP_ViewModeContext DP_view_mode_context_root_at(
    const DP_ViewModeContextRoot *vmcr, DP_CanvasState *cs, int index,
    DP_LayerListEntry **out_lle, DP_LayerProps **out_lp,
    const DP_OnionSkin **out_os, uint16_t *out_parent_opacity)
{
    DP_ASSERT(vmcr);
    DP_ASSERT(cs);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < vmcr->count);
    DP_ASSERT(out_lle);
    DP_ASSERT(out_lp);
    const DP_ViewModeFilter *vmf = &vmcr->vmf;
    int internal_type = vmf->internal_type;
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    DP_LayerList *ll = DP_canvas_state_layers_noinc(cs);
    if (is_frame_type(internal_type)) {
        DP_ASSERT(index < vmf->vmb->count);
        DP_ViewModeBuffer *vmb = vmf->vmb;
        DP_ViewModeTrack *vmt = &vmb->tracks[index];
        int layer_id = vmt->layer_id;
        if (layer_id != 0) {
            DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
            DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
            if (lre) {
                *out_lle = DP_layer_routes_entry_layer(lre, cs);
                *out_lp = DP_layer_routes_entry_props(lre, cs);
                *out_os = vmt->onion_skin;
                *out_parent_opacity =
                    DP_layer_routes_entry_parent_opacity(lre, cs);
                return make_frame_context(internal_type, index, vmb);
            }
            else {
                DP_warn("Track %d layer id %d not found", index, layer_id);
            }
        }
        *out_lle = NULL;
        *out_lp = NULL;
        *out_os = NULL;
        *out_parent_opacity = DP_BIT15;
        return make_nothing_context();
    }
    else {
        *out_lle = DP_layer_list_at_noinc(ll, index);
        *out_lp = DP_layer_props_list_at_noinc(lpl, index);
        *out_os = NULL;
        *out_parent_opacity = DP_BIT15;
        DP_ViewModeContext vmc;
        vmc.internal_type = internal_type;
        if (internal_type == TYPE_CALLBACK) {
            vmc.callback = vmf->callback;
        }
        else {
            vmc.layer_id = vmf->layer_id;
        }
        return vmc;
    }
}

static bool is_effectively_visible(DP_LayerProps *lp, uint16_t parent_opacity,
                                   uint16_t *out_effective_opacity)
{
    if (parent_opacity > 0) {
        uint16_t opacity = DP_layer_props_opacity(lp);
        if (opacity > 0 && !DP_layer_props_hidden(lp)) {
            *out_effective_opacity = DP_fix15_mul(parent_opacity, opacity);
            return true;
        }
    }
    return false;
}

DP_ViewModeResult DP_view_mode_context_apply(const DP_ViewModeContext *vmc,
                                             DP_LayerProps *lp,
                                             uint16_t parent_opacity)
{
    DP_ASSERT(vmc);
    DP_ASSERT(lp);
    switch (vmc->internal_type) {
    case TYPE_NORMAL: {
        uint16_t effective_opacity;
        return is_effectively_visible(lp, parent_opacity, &effective_opacity)
                 ? make_result(true, DP_layer_props_isolated(lp),
                               effective_opacity, DP_layer_props_blend_mode(lp),
                               make_normal_context())
                 : make_result(false, false, 0, DP_BLEND_MODE_NORMAL,
                               make_nothing_context());
    }
    case TYPE_NOTHING:
        return make_result(false, false, 0, DP_BLEND_MODE_NORMAL,
                           make_nothing_context());
    case TYPE_LAYER:
        return apply_layer(vmc->layer_id, lp);
    case TYPE_FRAME_MANUAL: {
        uint16_t effective_opacity;
        return is_effectively_visible(lp, parent_opacity, &effective_opacity)
                 ? apply_frame(TYPE_FRAME_MANUAL, vmc->frame.track_index,
                               vmc->frame.vmb, lp)
                 : make_result(false, false, 0, DP_BLEND_MODE_NORMAL,
                               make_nothing_context());
    }
    case TYPE_FRAME_RENDER:
        return apply_frame(TYPE_FRAME_RENDER, vmc->frame.track_index,
                           vmc->frame.vmb, lp);
    case TYPE_CALLBACK:
        return apply_callback(vmc->callback, lp, parent_opacity);
    default:
        DP_UNREACHABLE();
    }
}


static void get_track_layers_visible_in_frame(DP_CanvasState *cs, DP_Track *t,
                                              int frame_index,
                                              bool check_layer_visibility,
                                              DP_AddVisibleLayerFn fn,
                                              void *user)
{
    int kf_index = DP_track_key_frame_search_at_or_before(t, frame_index);
    if (kf_index != -1) {
        DP_KeyFrame *kf = DP_track_key_frame_at_noinc(t, kf_index);
        int layer_id = DP_key_frame_layer_id(kf);
        DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
        DP_LayerRoutesEntry *lre =
            layer_id == 0 ? NULL : DP_layer_routes_search(lr, layer_id);
        if (lre) {
            DP_LayerProps *lp = DP_layer_routes_entry_props(lre, cs);
            int kfl_count;
            const DP_KeyFrameLayer *kfls = DP_key_frame_layers(kf, &kfl_count);
            build_frame_layer_visibility(lp, kfl_count, kfls, false,
                                         check_layer_visibility, fn, user);
        }
    }
}

static void get_layers_visible_in_frame_with(DP_CanvasState *cs,
                                             DP_LocalState *ls,
                                             bool check_layer_visibility,
                                             DP_AddVisibleLayerFn fn,
                                             void *user)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int track_count = DP_timeline_count(tl);
    int frame_index = DP_local_state_active_frame_index(ls);
    for (int i = 0; i < track_count; ++i) {
        DP_Track *t = DP_timeline_at_noinc(tl, i);
        if (DP_local_state_track_visible(ls, DP_track_id(t))) {
            get_track_layers_visible_in_frame(cs, t, frame_index,
                                              check_layer_visibility, fn, user);
        }
    }
}

void DP_view_mode_get_layers_visible_in_frame(DP_CanvasState *cs,
                                              DP_LocalState *ls,
                                              DP_AddVisibleLayerFn fn,
                                              void *user)
{
    get_layers_visible_in_frame_with(cs, ls, false, fn, user);
}

void DP_view_mode_get_layers_visible_in_track_frame(DP_CanvasState *cs,
                                                    int track_id,
                                                    int frame_index,
                                                    DP_AddVisibleLayerFn fn,
                                                    void *user)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    int index = DP_timeline_index_by_id(tl, track_id);
    if (index != -1) {
        DP_Track *t = DP_timeline_at_noinc(tl, index);
        get_track_layers_visible_in_frame(cs, t, frame_index, false, fn, user);
    }
}


static bool is_pickable(DP_LayerProps *lp)
{
    // The user wants to point at a pixel and either get the layer or the last
    // editor of that pixel. That means we don't take hidden, fully opaque or
    // erase layers into account, since those don't contribute pixels to the
    // canvas. We don't take masked-off or erased areas into account, so you
    // might still get unexpected results, but that's a pretty hard problem.
    return DP_layer_props_visible(lp)
        && DP_layer_props_blend_mode(lp) != DP_BLEND_MODE_ERASE;
}

static bool pick_recursive(DP_LayerList *ll, DP_LayerPropsList *lpl, int x,
                           int y, DP_ViewModePick *out_pick);

static bool pick_content(DP_LayerContent *lc, DP_LayerProps *lp, int x, int y,
                         DP_ViewModePick *out_pick)
{
    bool pick_sublayer_or_layer =
        pick_recursive(DP_layer_content_sub_contents_noinc(lc),
                       DP_layer_content_sub_props_noinc(lc), x, y, out_pick)
        || DP_layer_content_pick_at(lc, x, y, &out_pick->context_id);
    if (pick_sublayer_or_layer) {
        out_pick->layer_id = DP_layer_props_id(lp);
        return true;
    }
    else {
        return false;
    }
}

static bool pick_entry(DP_LayerListEntry *lle, DP_LayerProps *lp, int x, int y,
                       DP_ViewModePick *out_pick)
{
    if (is_pickable(lp)) {
        if (DP_layer_list_entry_is_group(lle)) {
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            DP_LayerList *child_ll = DP_layer_group_children_noinc(lg);
            DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
            if (pick_recursive(child_ll, child_lpl, x, y, out_pick)) {
                return true;
            }
        }
        else {
            DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
            if (pick_content(lc, lp, x, y, out_pick)) {
                return true;
            }
        }
    }
    return false;
}

static bool pick_recursive(DP_LayerList *ll, DP_LayerPropsList *lpl, int x,
                           int y, DP_ViewModePick *out_pick)
{
    int count = DP_layer_list_count(ll);
    DP_ASSERT(count == DP_layer_props_list_count(lpl));
    for (int i = count - 1; i >= 0; --i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (pick_entry(lle, lp, x, y, out_pick)) {
            return true;
        }
    }
    return false;
}

static bool pick_normal(DP_CanvasState *cs, int x, int y,
                        DP_ViewModePick *out_pick)
{
    return pick_recursive(DP_canvas_state_layers_noinc(cs),
                          DP_canvas_state_layer_props_noinc(cs), x, y,
                          out_pick);
}

static bool pick_lre(DP_LayerRoutesEntry *lre, DP_CanvasState *cs, int x, int y,
                     DP_ViewModePick *out_pick)
{
    if (lre) {
        DP_LayerListEntry *lle = DP_layer_routes_entry_layer(lre, cs);
        DP_LayerProps *lp = DP_layer_routes_entry_props(lre, cs);
        return pick_entry(lle, lp, x, y, out_pick);
    }
    else {
        return false;
    }
}

static bool pick_layer(DP_CanvasState *cs, DP_LocalState *ls, int x, int y,
                       DP_ViewModePick *out_pick)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre =
        DP_layer_routes_search(lr, DP_local_state_active_layer_id(ls));
    return pick_lre(lre, cs, x, y, out_pick);
}

static bool pick_group(DP_CanvasState *cs, DP_LocalState *ls, int x, int y,
                       DP_ViewModePick *out_pick)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *child_lre =
        DP_layer_routes_search(lr, DP_local_state_active_layer_id(ls));
    if (child_lre) {
        DP_LayerRoutesEntry *lre = DP_layer_routes_entry_parent(child_lre);
        return pick_lre(lre, cs, x, y, out_pick);
    }
    else {
        return false;
    }
}

struct DP_PickFrameContext {
    DP_CanvasState *cs;
    int x, y;
    DP_ViewModePick *out_pick;
    bool found;
};

static void pick_frame_layer(void *user, int layer_id, bool visible)
{
    struct DP_PickFrameContext *c = user;
    if (visible) {
        DP_CanvasState *cs = c->cs;
        DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
        DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
        if (lre && !DP_layer_routes_entry_is_group(lre)) {
            DP_LayerContent *lc = DP_layer_routes_entry_content(lre, cs);
            DP_LayerProps *lp = DP_layer_routes_entry_props(lre, cs);
            if (pick_content(lc, lp, c->x, c->y, c->out_pick)) {
                c->found = true;
            }
        }
    }
}

static bool pick_frame(DP_CanvasState *cs, DP_LocalState *ls, int x, int y,
                       DP_ViewModePick *out_pick)
{
    struct DP_PickFrameContext c = {cs, x, y, out_pick, false};
    get_layers_visible_in_frame_with(cs, ls, true, pick_frame_layer, &c);
    return c.found;
}

DP_ViewModePick DP_view_mode_pick(DP_CanvasState *cs, DP_LocalState *ls, int x,
                                  int y)
{
    DP_ASSERT(cs);
    bool in_bounds = x >= 0 && y >= 0 && x < DP_canvas_state_width(cs)
                  && y < DP_canvas_state_height(cs);
    if (in_bounds) {
        DP_ViewModePick pick;
        switch (DP_local_state_view_mode(ls)) {
        case DP_VIEW_MODE_NORMAL:
            if (pick_normal(cs, x, y, &pick)) {
                return pick;
            }
            break;
        case DP_VIEW_MODE_LAYER:
            if (pick_layer(cs, ls, x, y, &pick)) {
                return pick;
            }
            break;
        case DP_VIEW_MODE_GROUP:
            if (pick_group(cs, ls, x, y, &pick)) {
                return pick;
            }
            break;
        case DP_VIEW_MODE_FRAME:
            if (pick_frame(cs, ls, x, y, &pick)) {
                return pick;
            }
            break;
        default:
            break;
        }
    }
    return (DP_ViewModePick){0, -1};
}


DP_OnionSkins *DP_onion_skins_new(bool wrap, int count_below, int count_above)
{
    DP_ASSERT(count_below >= 0);
    DP_ASSERT(count_above >= 0);
    DP_OnionSkins *oss = DP_malloc_zeroed(DP_FLEX_SIZEOF(
        DP_OnionSkins, skins, DP_int_to_size(count_below + count_above)));
    oss->wrap = wrap;
    oss->count_below = count_below;
    oss->count_above = count_above;
    return oss;
}

DP_OnionSkins *DP_onion_skins_new_clone(DP_OnionSkins *oss)
{
    DP_ASSERT(oss);
    int count_below = oss->count_below;
    int count_above = oss->count_above;
    DP_OnionSkins *new_oss =
        DP_onion_skins_new(oss->wrap, count_below, count_above);
    memcpy(new_oss->skins, oss->skins,
           sizeof(*oss->skins)
               * (DP_int_to_size(count_below) + DP_int_to_size(count_above)));
    return new_oss;
}

DP_OnionSkins *DP_onion_skins_new_clone_nullable(DP_OnionSkins *oss_or_null)
{
    return oss_or_null ? DP_onion_skins_new_clone(oss_or_null) : NULL;
}

void DP_onion_skins_free(DP_OnionSkins *oss)
{
    DP_free(oss);
}

bool DP_onion_skins_equal(DP_OnionSkins *a, DP_OnionSkins *b)
{
    if (a == b) {
        return true;
    }
    else if (a && b && a->wrap == b->wrap && a->count_above == b->count_above
             && a->count_below == b->count_below) {
        int count = a->count_above + a->count_below;
        for (int i = 0; i < count; ++i) {
            DP_OnionSkin *sa = &a->skins[i];
            DP_OnionSkin *sb = &b->skins[i];
            bool skins_differ =
                sa->opacity != sb->opacity || sa->tint.b != sb->tint.b
                || sa->tint.g != sb->tint.g || sa->tint.r != sb->tint.r
                || sa->tint.a != sb->tint.a;
            if (skins_differ) {
                return false;
            }
        }
        return true;
    }
    else {
        return false;
    }
}

bool DP_onion_skins_wrap(const DP_OnionSkins *oss)
{
    DP_ASSERT(oss);
    return oss->wrap;
}

int DP_onion_skins_count_below(const DP_OnionSkins *oss)
{
    DP_ASSERT(oss);
    return oss->count_below;
}

int DP_onion_skins_count_above(const DP_OnionSkins *oss)
{
    DP_ASSERT(oss);
    return oss->count_above;
}

const DP_OnionSkin *DP_onion_skins_skin_below_at(const DP_OnionSkins *oss,
                                                 int index)
{
    DP_ASSERT(oss);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < oss->count_below);
    return &oss->skins[index];
}

const DP_OnionSkin *DP_onion_skins_skin_above_at(const DP_OnionSkins *oss,
                                                 int index)
{
    DP_ASSERT(oss);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < oss->count_above);
    return &oss->skins[oss->count_below + index];
}

void DP_onion_skins_skin_below_at_set(DP_OnionSkins *oss, int index,
                                      uint16_t opacity, DP_UPixel15 tint)
{
    DP_ASSERT(oss);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < oss->count_below);
    DP_ASSERT(opacity <= DP_BIT15);
    DP_ASSERT(tint.b <= DP_BIT15);
    DP_ASSERT(tint.g <= DP_BIT15);
    DP_ASSERT(tint.r <= DP_BIT15);
    DP_ASSERT(tint.a <= DP_BIT15);
    oss->skins[index] = (DP_OnionSkin){opacity, tint};
}

void DP_onion_skins_skin_above_at_set(DP_OnionSkins *oss, int index,
                                      uint16_t opacity, DP_UPixel15 tint)
{
    DP_ASSERT(oss);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < oss->count_above);
    DP_ASSERT(opacity <= DP_BIT15);
    DP_ASSERT(tint.b <= DP_BIT15);
    DP_ASSERT(tint.g <= DP_BIT15);
    DP_ASSERT(tint.r <= DP_BIT15);
    DP_ASSERT(tint.a <= DP_BIT15);
    oss->skins[oss->count_below + index] = (DP_OnionSkin){opacity, tint};
}
