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
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_AnnotationList DP_AnnotationList;
typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_DocumentMetadata DP_DocumentMetadata;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_LayerRoutes DP_LayerRoutes;
typedef struct DP_Message DP_Message;
typedef struct DP_Rect DP_Rect;
typedef struct DP_Tile DP_Tile;
typedef struct DP_Timeline DP_Timeline;
typedef struct DP_ViewModeFilter DP_ViewModeFilter;


#define DP_FLAT_IMAGE_INCLUDE_BACKGROUND (1 << 0)
#define DP_FLAT_IMAGE_INCLUDE_SUBLAYERS  (1 << 1)
#define DP_FLAT_IMAGE_RENDER_FLAGS \
    (DP_FLAT_IMAGE_INCLUDE_BACKGROUND | DP_FLAT_IMAGE_INCLUDE_SUBLAYERS)

#define DP_USER_CURSOR_FLAG_NONE     0x0u
#define DP_USER_CURSOR_FLAG_VALID    0x1u
#define DP_USER_CURSOR_FLAG_MYPAINT  0x2u
#define DP_USER_CURSOR_FLAG_PEN_UP   0x4u
#define DP_USER_CURSOR_FLAG_PEN_DOWN 0x8u

typedef struct DP_CanvasState DP_CanvasState;

typedef struct DP_UserCursor {
    unsigned int flags;
    unsigned int context_id;
    int layer_id;
    int x, y;
} DP_UserCursor;

typedef struct DP_CanvasStateChange {
    DP_CanvasState *cs;
    DP_UserCursor user_cursor;
} DP_CanvasStateChange;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientCanvasState DP_TransientCanvasState;
typedef struct DP_TransientDocumentMetadata DP_TransientDocumentMetadata;
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
typedef struct DP_TransientLayerList DP_TransientLayerList;
typedef struct DP_TransientLayerPropsList DP_TransientLayerPropsList;
typedef struct DP_TransientLayerRoutes DP_TransientLayerRoutes;
typedef struct DP_TransientTile DP_TransientTile;
typedef struct DP_TransientTimeline DP_TransientTimeline;
#else
typedef struct DP_CanvasState DP_TransientCanvasState;
typedef struct DP_DocumentMetadata DP_TransientDocumentMetadata;
typedef struct DP_LayerContent DP_TransientLayerContent;
typedef struct DP_LayerList DP_TransientLayerList;
typedef struct DP_LayerPropsList DP_TransientLayerPropsList;
typedef struct DP_LayerRoutes DP_TransientLayerRoutes;
typedef struct DP_Tile DP_TransientTile;
typedef struct DP_Timeline DP_TransientTimeline;
#endif


DP_INLINE DP_CanvasStateChange DP_canvas_state_change_of(DP_CanvasState *cs)
{
    DP_CanvasStateChange change = {cs, {DP_USER_CURSOR_FLAG_NONE, 0, 0, 0, 0}};
    return change;
}

DP_INLINE DP_CanvasStateChange DP_canvas_state_change_null(void)
{
    return DP_canvas_state_change_of(DP_NULLPTR);
}


DP_CanvasState *DP_canvas_state_new(void);

DP_CanvasState *DP_canvas_state_incref(DP_CanvasState *cs);

DP_CanvasState *DP_canvas_state_incref_nullable(DP_CanvasState *cs_or_null);

void DP_canvas_state_decref(DP_CanvasState *cs);

void DP_canvas_state_decref_nullable(DP_CanvasState *cs_or_null);

int DP_canvas_state_refcount(DP_CanvasState *cs);

bool DP_canvas_state_transient(DP_CanvasState *cs);

int DP_canvas_state_width(DP_CanvasState *cs);

int DP_canvas_state_height(DP_CanvasState *cs);

int DP_canvas_state_offset_x(DP_CanvasState *cs);

int DP_canvas_state_offset_y(DP_CanvasState *cs);

DP_Tile *DP_canvas_state_background_tile_noinc(DP_CanvasState *cs);

bool DP_canvas_state_background_opaque(DP_CanvasState *cs);

DP_LayerList *DP_canvas_state_layers_noinc(DP_CanvasState *cs);

DP_LayerPropsList *DP_canvas_state_layer_props_noinc(DP_CanvasState *cs);

DP_LayerRoutes *DP_canvas_state_layer_routes_noinc(DP_CanvasState *cs);

DP_AnnotationList *DP_canvas_state_annotations_noinc(DP_CanvasState *cs);

DP_Timeline *DP_canvas_state_timeline_noinc(DP_CanvasState *cs);

DP_DocumentMetadata *DP_canvas_state_metadata_noinc(DP_CanvasState *cs);

int DP_canvas_state_frame_count(DP_CanvasState *cs);

int DP_canvas_state_framerate(DP_CanvasState *cs);

bool DP_canvas_state_same_frame(DP_CanvasState *cs, int frame_index_a,
                                int frame_index_b);

DP_CanvasStateChange
DP_canvas_state_handle(DP_CanvasState *cs, DP_DrawContext *dc, DP_Message *msg);

DP_CanvasStateChange DP_canvas_state_handle_multidab(DP_CanvasState *cs,
                                                     DP_DrawContext *dc,
                                                     int count,
                                                     DP_Message **msgs);

int DP_canvas_state_search_change_bounds(DP_CanvasState *cs,
                                         unsigned int context_id, int *out_x,
                                         int *out_y, int *out_width,
                                         int *out_height);

DP_TransientLayerContent *DP_canvas_state_to_flat_layer(DP_CanvasState *cs,
                                                        unsigned int flags);

DP_Image *DP_canvas_state_to_flat_image(DP_CanvasState *cs, unsigned int flags,
                                        const DP_Rect *area_or_null,
                                        const DP_ViewModeFilter *vmf_or_null);

DP_TransientTile *DP_canvas_state_flatten_tile_to(DP_CanvasState *cs,
                                                  int tile_index,
                                                  DP_TransientTile *tt_or_null,
                                                  bool include_sublayers,
                                                  const DP_ViewModeFilter *vmf);

DP_TransientTile *
DP_canvas_state_flatten_tile(DP_CanvasState *cs, int tile_index,
                             unsigned int flags,
                             const DP_ViewModeFilter *vmf_or_null);

DP_TransientTile *
DP_canvas_state_flatten_tile_at(DP_CanvasState *cs, int x, int y,
                                unsigned int flags,
                                const DP_ViewModeFilter *vmf_or_null);

void DP_canvas_state_diff(DP_CanvasState *cs, DP_CanvasState *prev_or_null,
                          DP_CanvasDiff *diff);

DP_TransientLayerContent *DP_canvas_state_render(DP_CanvasState *cs,
                                                 DP_TransientLayerContent *lc,
                                                 DP_CanvasDiff *diff);


DP_TransientCanvasState *DP_transient_canvas_state_new_init(void);

DP_TransientCanvasState *DP_transient_canvas_state_new(DP_CanvasState *cs);

// Doesn't reindex layer routes, since the given layers are still transient.
// Reindex them before you persist the state.
DP_TransientCanvasState *DP_transient_canvas_state_new_with_layers_noinc(
    DP_CanvasState *cs, DP_TransientLayerList *tll,
    DP_TransientLayerPropsList *tlpl);

DP_TransientCanvasState *
DP_transient_canvas_state_new_with_timeline_noinc(DP_CanvasState *cs,
                                                  DP_TransientTimeline *ttl);

DP_TransientCanvasState *
DP_transient_canvas_state_incref(DP_TransientCanvasState *cs);

DP_TransientCanvasState *
DP_transient_canvas_state_incref_nullable(DP_TransientCanvasState *tcs_or_null);

void DP_transient_canvas_state_decref(DP_TransientCanvasState *cs);

void DP_transient_canvas_state_decref_nullable(
    DP_TransientCanvasState *tcs_or_null);

int DP_transient_canvas_state_refcount(DP_TransientCanvasState *cs);

DP_CanvasState *DP_transient_canvas_state_persist(DP_TransientCanvasState *tcs);

int DP_transient_canvas_state_width(DP_TransientCanvasState *tcs);

int DP_transient_canvas_state_height(DP_TransientCanvasState *tcs);

void DP_transient_canvas_state_width_set(DP_TransientCanvasState *tcs,
                                         int width);

void DP_transient_canvas_state_height_set(DP_TransientCanvasState *tcs,
                                          int height);

void DP_transient_canvas_state_offsets_add(DP_TransientCanvasState *tcs,
                                           int offset_x, int offset_y);

void DP_transient_canvas_state_background_tile_set_noinc(
    DP_TransientCanvasState *tcs, DP_Tile *tile, bool opaque);

void DP_transient_canvas_state_layer_routes_reindex(
    DP_TransientCanvasState *tcs, DP_DrawContext *dc);

void DP_transient_canvas_state_timeline_cleanup(DP_TransientCanvasState *tcs);

DP_LayerList *
DP_transient_canvas_state_layers_noinc(DP_TransientCanvasState *tcs);

DP_LayerPropsList *
DP_transient_canvas_state_layer_props_noinc(DP_TransientCanvasState *tcs);

DP_LayerRoutes *
DP_transient_canvas_state_layer_routes_noinc(DP_TransientCanvasState *tcs);

DP_DocumentMetadata *
DP_transient_canvas_state_metadata_noinc(DP_TransientCanvasState *tcs);

void DP_transient_canvas_state_layers_set_inc(DP_TransientCanvasState *tcs,
                                              DP_LayerList *ll);

void DP_transient_canvas_state_transient_layers_set_noinc(
    DP_TransientCanvasState *tcs, DP_TransientLayerList *tll);

DP_TransientLayerList *
DP_transient_canvas_state_transient_layers(DP_TransientCanvasState *tcs,
                                           int reserve);

DP_TransientLayerPropsList *
DP_transient_canvas_state_transient_layer_props(DP_TransientCanvasState *tcs,
                                                int reserve);

void DP_transient_canvas_state_layer_props_set_inc(DP_TransientCanvasState *tcs,
                                                   DP_LayerPropsList *lpl);

void DP_transient_canvas_state_transient_layer_props_set_noinc(
    DP_TransientCanvasState *tcs, DP_TransientLayerPropsList *tlpl);

DP_TransientAnnotationList *
DP_transient_canvas_state_transient_annotations(DP_TransientCanvasState *tcs,
                                                int reserve);

DP_TransientTimeline *
DP_transient_canvas_state_transient_timeline(DP_TransientCanvasState *tcs,
                                             int reserve);

void DP_transient_canvas_state_timeline_set_inc(DP_TransientCanvasState *tcs,
                                                DP_Timeline *tl);

DP_TransientDocumentMetadata *
DP_transient_canvas_state_transient_metadata(DP_TransientCanvasState *tcs);


#endif
