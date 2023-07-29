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
#ifndef DPENGINE_LAYER_CONTENT_H
#define DPENGINE_LAYER_CONTENT_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_BrushStamp DP_BrushStamp;
typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_Image DP_Image;
typedef struct DP_Rect DP_Rect;
typedef struct DP_Tile DP_Tile;
typedef struct DP_ViewModeFilter DP_ViewModeFilter;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_LayerContent DP_LayerContent;
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_TransientLayerList DP_TransientLayerList;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_TransientLayerProps DP_TransientLayerProps;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_TransientLayerPropsList DP_TransientLayerPropsList;
typedef struct DP_TransientTile DP_TransientTile;
#else
typedef struct DP_LayerContent DP_LayerContent;
typedef struct DP_LayerContent DP_TransientLayerContent;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_LayerList DP_TransientLayerList;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_LayerProps DP_TransientLayerProps;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_LayerPropsList DP_TransientLayerPropsList;
typedef struct DP_Tile DP_TransientTile;
#endif


DP_LayerContent *DP_layer_content_incref(DP_LayerContent *lc);

DP_LayerContent *DP_layer_content_incref_nullable(DP_LayerContent *lc_or_null);

void DP_layer_content_decref(DP_LayerContent *lc);

void DP_layer_content_decref_nullable(DP_LayerContent *lc_or_null);

int DP_layer_content_refcount(DP_LayerContent *lc);

bool DP_layer_content_transient(DP_LayerContent *lc);

void DP_layer_content_diff(DP_LayerContent *lc, DP_LayerProps *lp,
                           DP_LayerContent *prev_lc, DP_LayerProps *prev_lp,
                           DP_CanvasDiff *diff);

void DP_layer_content_diff_mark(DP_LayerContent *lc, DP_CanvasDiff *diff);

int DP_layer_content_width(DP_LayerContent *lc);

int DP_layer_content_height(DP_LayerContent *lc);

DP_Tile *DP_layer_content_tile_at_noinc(DP_LayerContent *lc, int x, int y);

DP_Pixel15 DP_layer_content_pixel_at(DP_LayerContent *lc, int x, int y);

bool DP_layer_content_pick_at(DP_LayerContent *lc, int x, int y,
                              unsigned int *out_context_id);

DP_UPixelFloat DP_layer_content_sample_color_at(DP_LayerContent *lc,
                                                uint16_t *stamp_buffer, int x,
                                                int y, int diameter,
                                                int *in_out_last_diameter);

DP_LayerList *DP_layer_content_sub_contents_noinc(DP_LayerContent *lc);

DP_LayerPropsList *DP_layer_content_sub_props_noinc(DP_LayerContent *lc);

bool DP_layer_content_search_change_bounds(DP_LayerContent *lc,
                                           unsigned int context_id, int *out_x,
                                           int *out_y, int *out_width,
                                           int *out_height);

DP_Image *DP_layer_content_to_image(DP_LayerContent *lc);

DP_UPixel8 *DP_layer_content_to_upixels8_cropped(DP_LayerContent *lc,
                                                 int *out_offset_x,
                                                 int *out_offset_y,
                                                 int *out_width,
                                                 int *out_height);

DP_Pixel8 *DP_layer_content_to_pixels8(DP_LayerContent *lc, int x, int y,
                                       int width, int height);

DP_Image *DP_layer_content_select(DP_LayerContent *lc, const DP_Rect *rect,
                                  DP_Image *mask);

DP_TransientLayerContent *DP_layer_content_resize(DP_LayerContent *lc,
                                                  unsigned int context_id,
                                                  int top, int right,
                                                  int bottom, int left);

DP_LayerContent *DP_layer_content_merge_sublayers(DP_LayerContent *lc);

DP_TransientTile *DP_layer_content_flatten_tile_to(
    DP_LayerContent *lc, int tile_index, DP_TransientTile *tt_or_null,
    uint16_t opacity, int blend_mode, bool censored, bool include_sublayers);


DP_TransientLayerContent *DP_transient_layer_content_new(DP_LayerContent *lc);

DP_TransientLayerContent *
DP_transient_layer_content_new_init(int width, int height, DP_Tile *tile);

DP_TransientLayerContent *
DP_transient_layer_content_new_init_with_transient_sublayers_noinc(
    int width, int height, DP_Tile *tile, DP_TransientLayerList *sub_tll,
    DP_TransientLayerPropsList *sub_tlpl);

DP_TransientLayerContent *
DP_transient_layer_content_incref(DP_TransientLayerContent *tlc);

void DP_transient_layer_content_decref(DP_TransientLayerContent *tlc);

int DP_transient_layer_content_refcount(DP_TransientLayerContent *tlc);

DP_LayerContent *
DP_transient_layer_content_persist(DP_TransientLayerContent *tlc);

int DP_transient_layer_content_width(DP_TransientLayerContent *tlc);

int DP_transient_layer_content_height(DP_TransientLayerContent *tlc);

DP_Tile *DP_transient_layer_content_tile_at_noinc(DP_TransientLayerContent *tlc,
                                                  int x, int y);

void DP_transient_layer_content_transient_tile_at_set_noinc(
    DP_TransientLayerContent *tlc, int x, int y, DP_TransientTile *tt);

DP_LayerList *
DP_transient_layer_content_sub_contents_noinc(DP_TransientLayerContent *tlc);

DP_LayerPropsList *
DP_transient_layer_content_sub_props_noinc(DP_TransientLayerContent *tlc);

DP_TransientLayerContent *
DP_transient_layer_content_resize_to(DP_TransientLayerContent *tlc,
                                     unsigned int context_id, int width,
                                     int height);

void DP_transient_layer_content_merge(DP_TransientLayerContent *tlc,
                                      unsigned int context_id,
                                      DP_LayerContent *lc, uint16_t opacity,
                                      int blend_mode);

void DP_transient_layer_content_pixel_at_put(DP_TransientLayerContent *tlc,
                                             unsigned int context_id,
                                             int blend_mode, int x, int y,
                                             DP_Pixel15 pixel);

void DP_transient_layer_content_pixel_at_set(DP_TransientLayerContent *tlc,
                                             unsigned int context_id, int x,
                                             int y, DP_Pixel15 pixel);

void DP_transient_layer_content_put_image(DP_TransientLayerContent *tlc,
                                          unsigned int context_id,
                                          int blend_mode, int left, int top,
                                          DP_Image *img);

void DP_transient_layer_content_fill_rect(DP_TransientLayerContent *tlc,
                                          unsigned int context_id,
                                          int blend_mode, int left, int top,
                                          int right, int bottom,
                                          DP_UPixel15 pixel);

void DP_transient_layer_content_tile_set_noinc(DP_TransientLayerContent *tlc,
                                               DP_Tile *t, int i);

void DP_transient_layer_content_transient_tile_set_noinc(
    DP_TransientLayerContent *tlc, DP_TransientTile *tt, int i);

void DP_transient_layer_content_put_tile_inc(DP_TransientLayerContent *tlc,
                                             DP_Tile *tile, int x, int y,
                                             int repeat);

void DP_transient_layer_content_brush_stamp_apply(
    DP_TransientLayerContent *tlc, unsigned int context_id, DP_UPixel15 pixel,
    uint16_t opacity, int blend_mode, DP_BrushStamp *stamp);

void DP_transient_layer_content_brush_stamp_apply_posterize(
    DP_TransientLayerContent *tlc, unsigned int context_id, uint16_t opacity,
    int posterize_num, DP_BrushStamp *stamp);

void DP_transient_layer_content_transient_sublayer_at(
    DP_TransientLayerContent *tlc, int sublayer_index,
    DP_TransientLayerContent **out_tlc, DP_TransientLayerProps **out_tlp);

void DP_transient_layer_content_transient_sublayer(
    DP_TransientLayerContent *tlc, int sublayer_id,
    DP_TransientLayerContent **out_tlc, DP_TransientLayerProps **out_tlp);

void DP_transient_layer_content_sublayer_insert_inc(
    DP_TransientLayerContent *tlc, DP_LayerContent *sub_lc,
    DP_LayerProps *sub_lp);

void DP_transient_layer_content_merge_sublayer_at(DP_TransientLayerContent *tlc,
                                                  unsigned int context_id,
                                                  int index);

void DP_transient_layer_content_merge_all_sublayers(
    DP_TransientLayerContent *tl, unsigned int context_id);

DP_TransientTile *
DP_transient_layer_content_render_tile(DP_TransientLayerContent *tlc,
                                       DP_CanvasState *cs, int tile_index,
                                       const DP_ViewModeFilter *vmf_or_null);


#endif
