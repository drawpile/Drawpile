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
#ifndef DPENGINE_CANVAS_STATE_H
#define DPENGINE_CANVAS_STATE_H
#include <dpcommon/common.h>

typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_Message DP_Message;
typedef struct DP_Tile DP_Tile;


#define DP_FLAT_IMAGE_INCLUDE_BACKGROUND   (1 << 0)
#define DP_FLAT_IMAGE_INCLUDE_FIXED_LAYERS (1 << 1)
#define DP_FLAT_IMAGE_INCLUDE_SUBLAYERS    (1 << 2)

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_TransientCanvasState DP_TransientCanvasState;
typedef struct DP_TransientLayer DP_TransientLayer;
#else
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_CanvasState DP_TransientCanvasState;
typedef struct DP_Layer DP_TransientLayer;
#endif

DP_CanvasState *DP_canvas_state_new(void);

DP_CanvasState *DP_canvas_state_incref(DP_CanvasState *cs);

void DP_canvas_state_decref(DP_CanvasState *cs);

int DP_canvas_state_refcount(DP_CanvasState *cs);

bool DP_canvas_state_transient(DP_CanvasState *cs);

DP_CanvasState *DP_canvas_state_handle(DP_CanvasState *cs, DP_DrawContext *dc,
                                       DP_Message *msg);

DP_Image *DP_canvas_state_to_flat_image(DP_CanvasState *cs, unsigned int flags);

DP_Tile *DP_canvas_state_flatten_tile(DP_CanvasState *cs, int tile_index);

void DP_canvas_state_diff(DP_CanvasState *cs, DP_CanvasState *prev_or_null,
                          DP_CanvasDiff *diff);

void DP_canvas_state_render(DP_CanvasState *cs, DP_TransientLayer *target,
                            DP_CanvasDiff *diff);

DP_LayerList *DP_canvas_state_layers_noinc(DP_CanvasState *cs);


DP_TransientCanvasState *DP_transient_canvas_state_new(DP_CanvasState *cs);

DP_TransientCanvasState *
DP_transient_canvas_state_incref(DP_TransientCanvasState *cs);

void DP_transient_canvas_state_decref(DP_TransientCanvasState *cs);

int DP_transient_canvas_state_refcount(DP_TransientCanvasState *cs);

DP_CanvasState *DP_transient_canvas_state_persist(DP_TransientCanvasState *tcs);


bool DP_transient_canvas_state_resize(DP_TransientCanvasState *tcs,
                                      unsigned int context_id, int top,
                                      int right, int bottom, int left);


#endif
