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
typedef struct DP_Frame DP_Frame;
typedef struct DP_Timeline DP_Timeline;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_LayerPropsList DP_LayerPropsList;


typedef enum DP_ViewMode {
    DP_VIEW_MODE_NORMAL,
    DP_VIEW_MODE_LAYER,
    DP_VIEW_MODE_FRAME,
} DP_ViewMode;

typedef struct DP_ViewModeFilter {
    int internal_type;
    union {
        int layer_id;
        DP_Frame *frame;
    };
} DP_ViewModeFilter;

typedef struct DP_ViewModeFilterResult {
    bool hidden_by_view_mode;
    DP_ViewModeFilter child_vmf;
} DP_ViewModeFilterResult;

DP_ViewModeFilter DP_view_mode_filter_make_default(void);

DP_ViewModeFilter DP_view_mode_filter_make_frame(DP_CanvasState *cs,
                                                 int frame_index);

DP_ViewModeFilter DP_view_mode_filter_make(DP_ViewMode vm, DP_CanvasState *cs,
                                           int layer_id, int frame_index);

bool DP_view_mode_filter_excludes_everything(const DP_ViewModeFilter *vmf);

DP_ViewModeFilterResult DP_view_mode_filter_apply(const DP_ViewModeFilter *vmf,
                                                  DP_LayerProps *lp);


typedef struct DP_OnionSkin {
    uint16_t opacity;
    DP_UPixel15 tint;
} DP_OnionSkin;

typedef struct DP_OnionSkins DP_OnionSkins;

DP_OnionSkins *DP_onion_skins_new(int count_above, int count_below);

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
