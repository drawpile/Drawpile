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
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "layer_routes.h"
#include "timeline.h"
#include "track.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/vector.h>


#define TYPE_NORMAL       0
#define TYPE_NOTHING      1
#define TYPE_LAYER        2
#define TYPE_FRAME_MANUAL 3


typedef struct DP_ViewModeTrack {
    int layer_id;
    DP_Vector hidden_layer_ids;
    const DP_OnionSkin *onion_skin;
} DP_ViewModeTrack;

struct DP_ViewModeBuffer {
    int capacity;
    int count;
    DP_ViewModeTrack *tracks;
};

struct DP_OnionSkins {
    int count_below;
    int count_above;
    DP_OnionSkin skins[];
};


DP_ViewModeBuffer *DP_view_mode_buffer_new(void)
{
    DP_ViewModeBuffer *vmb = DP_malloc(sizeof(*vmb));
    *vmb = (DP_ViewModeBuffer){0, 0, NULL};
    return vmb;
}

void DP_view_mode_buffer_free(DP_ViewModeBuffer *vmb)
{
    if (vmb) {
        int track_count = vmb->capacity; // Not count, we want to clean em all.
        for (int i = 0; i < track_count; ++i) {
            DP_vector_dispose(&vmb->tracks[i].hidden_layer_ids);
        }
        DP_free(vmb->tracks);
        DP_free(vmb);
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

static DP_ViewModeFilter make_manual_frame_filter(DP_ViewModeBuffer *vmb)
{
    return (DP_ViewModeFilter){TYPE_FRAME_MANUAL, {.vmb = vmb}};
}

static DP_ViewModeContext make_manual_frame_context(int track_index,
                                                    DP_ViewModeBuffer *vmb)
{
    return (DP_ViewModeContext){TYPE_FRAME_MANUAL,
                                {.frame = {track_index, vmb}}};
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

static bool build_frame_hidden_layer_ids(DP_Vector *hidden_layer_ids,
                                         DP_LayerProps *lp, int count,
                                         const DP_KeyFrameLayer *kfls,
                                         bool parent_hidden)
{
    int layer_id = DP_layer_props_id(lp);
    DP_KeyFrameLayer kfl = get_key_frame_layer(layer_id, count, kfls);
    bool hidden = DP_key_frame_layer_hidden(&kfl)
               || (parent_hidden && !DP_key_frame_layer_revealed(&kfl));

    bool all_children_hidden = true;
    DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
    if (child_lpl) {
        int child_count = DP_layer_props_list_count(child_lpl);
        for (int i = 0; i < child_count; ++i) {
            DP_LayerProps *child_lp =
                DP_layer_props_list_at_noinc(child_lpl, i);
            bool child_hidden = build_frame_hidden_layer_ids(
                hidden_layer_ids, child_lp, count, kfls, hidden);
            if (!child_hidden) {
                all_children_hidden = false;
            }
        }
    }

    if (hidden && all_children_hidden) {
        DP_VECTOR_PUSH_TYPE(hidden_layer_ids, int, layer_id);
        return true;
    }
    else {
        return false;
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
        bool all_hidden = build_frame_hidden_layer_ids(hidden_layer_ids, lp,
                                                       count, kfls, false);
        if (all_hidden) {
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

static void build_track_onion_skins(DP_ViewModeBuffer *vmb, DP_CanvasState *cs,
                                    DP_Track *t, const DP_OnionSkins *oss,
                                    int target)
{
    int key_frame_count = DP_track_key_frame_count(t);

    int count_below = DP_onion_skins_count_below(oss);
    for (int i = 0; i < count_below; ++i) {
        int skin_index = target - count_below + i;
        const DP_OnionSkin *os = DP_onion_skins_skin_below_at(oss, i);
        bool is_visible =
            skin_index >= 0 && skin_index < key_frame_count && os->opacity != 0;
        if (is_visible) {
            build_key_frame(vmb, cs, t, skin_index, os);
        }
    }

    int count_above = DP_onion_skins_count_above(oss);
    for (int i = 0; i < count_above; ++i) {
        int skin_index = target + i + 1;
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

DP_ViewModeFilter DP_view_mode_filter_make_frame(DP_ViewModeBuffer *vmb,
                                                 DP_CanvasState *cs,
                                                 int frame_index,
                                                 const DP_OnionSkins *oss)
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
    return make_manual_frame_filter(vmb);
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
    case DP_VIEW_MODE_FRAME:
        return DP_view_mode_filter_make_frame(vmb, cs, frame_index, oss);
    default:
        DP_UNREACHABLE();
    }
}

bool DP_view_mode_filter_excludes_everything(const DP_ViewModeFilter *vmf)
{
    DP_ASSERT(vmf);
    return vmf->internal_type == TYPE_NOTHING;
}


static DP_ViewModeResult make_result(bool hidden_by_view_mode,
                                     DP_ViewModeContext child_vmc)
{
    return (DP_ViewModeResult){hidden_by_view_mode, child_vmc};
}

static DP_ViewModeResult apply_layer(int layer_id, DP_LayerProps *lp)
{
    if (DP_layer_props_id(lp) == layer_id) {
        return make_result(false, make_normal_context());
    }
    else if (DP_layer_props_children_noinc(lp)) {
        return make_result(false, make_layer_context(layer_id));
    }
    else {
        return make_result(true, make_nothing_context());
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

static DP_ViewModeResult
apply_frame_manual(int track_index, DP_ViewModeBuffer *vmb, DP_LayerProps *lp)
{
    DP_ASSERT(track_index >= 0);
    DP_ASSERT(track_index < vmb->count);
    DP_ViewModeTrack *vmt = &vmb->tracks[track_index];
    if (vmt->layer_id == 0 || is_hidden_in_frame(lp, vmt)) {
        return make_result(true, make_nothing_context());
    }
    else {
        return make_result(false, make_manual_frame_context(track_index, vmb));
    }
}

DP_ViewModeContextRoot
DP_view_mode_context_root_init(const DP_ViewModeFilter *vmf, DP_CanvasState *cs)
{
    DP_ASSERT(vmf);
    DP_ASSERT(cs);
    return (DP_ViewModeContextRoot){
        *vmf, vmf->internal_type == TYPE_FRAME_MANUAL
                  ? vmf->vmb->count
                  : DP_layer_list_count(DP_canvas_state_layers_noinc(cs))};
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
    const DP_OnionSkin **out_os)
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
    if (internal_type == TYPE_FRAME_MANUAL) {
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
                return make_manual_frame_context(index, vmb);
            }
            else {
                DP_warn("Track %d layer id %d not found", index, layer_id);
            }
        }
        *out_lle = NULL;
        *out_lp = NULL;
        *out_os = NULL;
        return make_nothing_context();
    }
    else {
        *out_lle = DP_layer_list_at_noinc(ll, index);
        *out_lp = DP_layer_props_list_at_noinc(lpl, index);
        *out_os = NULL;
        return (DP_ViewModeContext){internal_type, {.layer_id = vmf->layer_id}};
    }
}

DP_ViewModeResult DP_view_mode_context_apply(const DP_ViewModeContext *vmc,
                                             DP_LayerProps *lp)
{
    DP_ASSERT(vmc);
    DP_ASSERT(lp);
    switch (vmc->internal_type) {
    case TYPE_NORMAL:
        return make_result(false, make_normal_context());
    case TYPE_NOTHING:
        return make_result(true, make_nothing_context());
    case TYPE_LAYER:
        return apply_layer(vmc->layer_id, lp);
    case TYPE_FRAME_MANUAL:
        return apply_frame_manual(vmc->frame.track_index, vmc->frame.vmb, lp);
    default:
        DP_UNREACHABLE();
    }
}

bool DP_view_mode_context_should_flatten(const DP_ViewModeContext *vmc,
                                         DP_LayerProps *lp,
                                         uint16_t parent_opacity)
{
    return DP_layer_props_visible(lp) && parent_opacity != 0
        && !DP_view_mode_context_apply(vmc, lp).hidden_by_view_mode;
}


DP_OnionSkins *DP_onion_skins_new(int count_below, int count_above)
{
    DP_ASSERT(count_below >= 0);
    DP_ASSERT(count_above >= 0);
    DP_OnionSkins *oss = DP_malloc_zeroed(DP_FLEX_SIZEOF(
        DP_OnionSkins, skins, DP_int_to_size(count_below + count_above)));
    oss->count_below = count_below;
    oss->count_above = count_above;
    return oss;
}

void DP_onion_skins_free(DP_OnionSkins *oss)
{
    DP_free(oss);
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
