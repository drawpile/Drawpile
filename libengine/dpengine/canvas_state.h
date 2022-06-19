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
#include "annotation_list.h"
#include <dpcommon/common.h>

typedef struct DP_AnnotationList DP_AnnotationList;
typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_LayerContentList DP_LayerContentList;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_Message DP_Message;
typedef struct DP_Tile DP_Tile;


#define DP_FLAT_IMAGE_INCLUDE_BACKGROUND   (1 << 0)
#define DP_FLAT_IMAGE_INCLUDE_FIXED_LAYERS (1 << 1)
#define DP_FLAT_IMAGE_INCLUDE_SUBLAYERS    (1 << 2)

typedef struct DP_CanvasState DP_CanvasState;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientCanvasState DP_TransientCanvasState;
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
typedef struct DP_TransientLayerContentList DP_TransientLayerContentList;
typedef struct DP_TransientLayerPropsList DP_TransientLayerPropsList;
typedef struct DP_TransientTile DP_TransientTile;
#else
typedef struct DP_CanvasState DP_TransientCanvasState;
typedef struct DP_LayerContent DP_TransientLayerContent;
typedef struct DP_LayerContentList DP_TransientLayerContentList;
typedef struct DP_LayerPropsList DP_TransientLayerPropsList;
typedef struct DP_Tile DP_TransientTile;
#endif

DP_CanvasState *DP_canvas_state_new(void);

DP_CanvasState *DP_canvas_state_incref(DP_CanvasState *cs);

DP_CanvasState *DP_canvas_state_incref_nullable(DP_CanvasState *cs_or_null);

void DP_canvas_state_decref(DP_CanvasState *cs);

void DP_canvas_state_decref_nullable(DP_CanvasState *cs_or_null);

int DP_canvas_state_refcount(DP_CanvasState *cs);

bool DP_canvas_state_transient(DP_CanvasState *cs);

int DP_canvas_state_width(DP_CanvasState *cs);

int DP_canvas_state_height(DP_CanvasState *cs);

DP_Tile *DP_canvas_state_background_tile_noinc(DP_CanvasState *cs);

DP_LayerContentList *DP_canvas_state_layer_contents_noinc(DP_CanvasState *cs);

DP_LayerPropsList *DP_canvas_state_layer_props_noinc(DP_CanvasState *cs);

DP_AnnotationList *DP_canvas_state_annotations_noinc(DP_CanvasState *cs);

DP_CanvasState *DP_canvas_state_handle(DP_CanvasState *cs, DP_DrawContext *dc,
                                       DP_Message *msg);

int DP_canvas_state_search_change_bounds(DP_CanvasState *cs,
                                         unsigned int context_id, int *out_x,
                                         int *out_y, int *out_width,
                                         int *out_height);

DP_Image *DP_canvas_state_to_flat_image(DP_CanvasState *cs, unsigned int flags);

DP_TransientTile *DP_canvas_state_flatten_tile(DP_CanvasState *cs,
                                               int tile_index);

DP_TransientTile *DP_canvas_state_flatten_tile_at(DP_CanvasState *cs, int x,
                                                  int y);

void DP_canvas_state_diff(DP_CanvasState *cs, DP_CanvasState *prev_or_null,
                          DP_CanvasDiff *diff);

DP_TransientLayerContent *DP_canvas_state_render(DP_CanvasState *cs,
                                                 DP_TransientLayerContent *lc,
                                                 DP_CanvasDiff *diff);


DP_TransientCanvasState *DP_transient_canvas_state_new_init(void);

DP_TransientCanvasState *DP_transient_canvas_state_new(DP_CanvasState *cs);

DP_TransientCanvasState *DP_transient_canvas_state_new_with_layers_noinc(
    DP_CanvasState *cs, DP_TransientLayerContentList *tlcl,
    DP_TransientLayerPropsList *tlpl);

DP_TransientCanvasState *
DP_transient_canvas_state_incref(DP_TransientCanvasState *cs);

void DP_transient_canvas_state_decref(DP_TransientCanvasState *cs);

int DP_transient_canvas_state_refcount(DP_TransientCanvasState *cs);

DP_CanvasState *DP_transient_canvas_state_persist(DP_TransientCanvasState *tcs);

void DP_transient_canvas_state_width_set(DP_TransientCanvasState *tcs,
                                         int width);

void DP_transient_canvas_state_height_set(DP_TransientCanvasState *tcs,
                                          int height);

void DP_transient_canvas_state_background_tile_set_noinc(
    DP_TransientCanvasState *tcs, DP_Tile *tile);

DP_LayerContentList *
DP_transient_canvas_state_layer_contents_noinc(DP_TransientCanvasState *tcs);

DP_LayerPropsList *
DP_transient_canvas_state_layer_props_noinc(DP_TransientCanvasState *tcs);

void DP_transient_canvas_state_transient_layer_contents_set_noinc(
    DP_TransientCanvasState *tcs, DP_TransientLayerContentList *tlcl);

DP_TransientLayerContentList *
DP_transient_canvas_state_transient_layer_contents(DP_TransientCanvasState *tcs,
                                                   int reserve);

DP_TransientLayerPropsList *
DP_transient_canvas_state_transient_layer_props(DP_TransientCanvasState *tcs,
                                                int reserve);

DP_TransientAnnotationList *
DP_transient_canvas_state_transient_annotations(DP_TransientCanvasState *tcs,
                                                int reserve);


#endif
