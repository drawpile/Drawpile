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
#ifndef DPENGINE_VIEW_MODE_H
#define DPENGINE_VIEW_MODE_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_LayerListEntry DP_LayerListEntry;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_LayerPropsList DP_LayerPropsList;


typedef enum DP_ViewMode {
    DP_VIEW_MODE_NORMAL,
    DP_VIEW_MODE_LAYER,
    DP_VIEW_MODE_FRAME,
} DP_ViewMode;

typedef struct DP_ViewModeBuffer DP_ViewModeBuffer;

typedef struct DP_ViewModeFilter {
    int internal_type;
    union {
        int layer_id;
        DP_ViewModeBuffer *vmb;
    } DP_ANONYMOUS(data);
} DP_ViewModeFilter;

typedef struct DP_ViewModeContext {
    int internal_type;
    union {
        int layer_id;
        struct {
            int track_index;
            DP_ViewModeBuffer *vmb;
        } frame;
    } DP_ANONYMOUS(data);
} DP_ViewModeContext;

typedef struct DP_ViewModeContextRoot {
    DP_ViewModeFilter vmf;
    int count;
} DP_ViewModeContextRoot;

typedef struct DP_ViewModeResult {
    bool hidden_by_view_mode;
    DP_ViewModeContext child_vmc;
} DP_ViewModeResult;

typedef struct DP_OnionSkin {
    uint16_t opacity;
    DP_UPixel15 tint;
} DP_OnionSkin;

typedef struct DP_OnionSkins DP_OnionSkins;

typedef void (*AddVisibleLayerFn)(void *user, int layer_id, bool visible);


DP_ViewModeBuffer *DP_view_mode_buffer_new(void);

void DP_view_mode_buffer_free(DP_ViewModeBuffer *vmb);


DP_ViewModeFilter DP_view_mode_filter_make_default(void);

DP_ViewModeFilter DP_view_mode_filter_make_frame(DP_ViewModeBuffer *vmb,
                                                 DP_CanvasState *cs,
                                                 int frame_index,
                                                 const DP_OnionSkins *oss);

DP_ViewModeFilter DP_view_mode_filter_make(DP_ViewModeBuffer *vmb,
                                           DP_ViewMode vm, DP_CanvasState *cs,
                                           int layer_id, int frame_index,
                                           const DP_OnionSkins *oss);

bool DP_view_mode_filter_excludes_everything(const DP_ViewModeFilter *vmf);


DP_ViewModeContextRoot
DP_view_mode_context_root_init(const DP_ViewModeFilter *vmf,
                               DP_CanvasState *cs);

DP_ViewModeContext DP_view_mode_context_make_default(void);

bool DP_view_mode_context_excludes_everything(const DP_ViewModeContext *vmc);

DP_ViewModeContext DP_view_mode_context_root_at(
    const DP_ViewModeContextRoot *vmcr, DP_CanvasState *cs, int index,
    DP_LayerListEntry **out_lle, DP_LayerProps **out_lp,
    const DP_OnionSkin **out_os);

DP_ViewModeResult DP_view_mode_context_apply(const DP_ViewModeContext *vmc,
                                             DP_LayerProps *lp);

bool DP_view_mode_context_should_flatten(const DP_ViewModeContext *vmc,
                                         DP_LayerProps *lp,
                                         uint16_t parent_opacity);


void DP_view_mode_get_layers_visible_in_frame(DP_CanvasState *cs,
                                              int frame_index,
                                              AddVisibleLayerFn fn, void *user);

void DP_view_mode_get_layers_visible_in_track_frame(DP_CanvasState *cs,
                                                    int track_id,
                                                    int frame_index,
                                                    AddVisibleLayerFn fn,
                                                    void *user);


DP_OnionSkins *DP_onion_skins_new(int count_below, int count_above);

void DP_onion_skins_free(DP_OnionSkins *oss);

int DP_onion_skins_count_below(const DP_OnionSkins *oss);

int DP_onion_skins_count_above(const DP_OnionSkins *oss);

const DP_OnionSkin *DP_onion_skins_skin_below_at(const DP_OnionSkins *oss,
                                                 int index);

const DP_OnionSkin *DP_onion_skins_skin_above_at(const DP_OnionSkins *oss,
                                                 int index);

void DP_onion_skins_skin_below_at_set(DP_OnionSkins *oss, int index,
                                      uint16_t opacity, DP_UPixel15 tint);

void DP_onion_skins_skin_above_at_set(DP_OnionSkins *oss, int index,
                                      uint16_t opacity, DP_UPixel15 tint);


#endif
