/*
 * Copyright (C) 2022 - 2023 askmeaboutloom
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
#ifndef DPENGINE_FLOOD_FILL_H
#define DPENGINE_FLOOD_FILL_H
#include "pixels.h"
#include "view_mode.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Image DP_Image;

typedef enum DP_FloodFillResult {
    DP_FLOOD_FILL_SUCCESS,
    DP_FLOOD_FILL_OUT_OF_BOUNDS,
    DP_FLOOD_FILL_INVALID_LAYER,
    DP_FLOOD_FILL_NOTHING_TO_FILL,
    DP_FLOOD_FILL_CANCELLED,
} DP_FloodFillResult;

typedef bool (*DP_FloodFillShouldCancelFn)(void *user);

DP_FloodFillResult
DP_flood_fill(DP_CanvasState *cs, unsigned int context_id, int selection_id,
              int x, int y, DP_UPixelFloat fill_color, double tolerance,
              int layer_id, int size, int gap, int expand, int feather_radius,
              bool from_edge, bool continuous, DP_ViewMode view_mode,
              int active_layer_id, int active_frame_index, DP_Image **out_img,
              int *out_x, int *out_y, DP_FloodFillShouldCancelFn should_cancel,
              void *user);

DP_FloodFillResult
DP_selection_fill(DP_CanvasState *cs, unsigned int context_id, int selection_id,
                  DP_UPixelFloat fill_color, int expand, int feather_radius,
                  bool from_edge, DP_Image **out_img, int *out_x, int *out_y,
                  DP_FloodFillShouldCancelFn should_cancel, void *user);


#endif
