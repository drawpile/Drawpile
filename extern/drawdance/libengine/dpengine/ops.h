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
#ifndef DPENGINE_OPS_H
#define DPENGINE_OPS_H
#include "canvas_state.h"
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_KeyFrameLayer DP_KeyFrameLayer;
typedef struct DP_PaintDrawDabsParams DP_PaintDrawDabsParams;
typedef struct DP_Quad DP_Quad;
typedef struct DP_Rect DP_Rect;
typedef struct DP_Tile DP_Tile;

struct DP_LayerOrderPair {
    int child_count;
    int layer_id;
};


DP_CanvasState *DP_ops_canvas_resize(DP_CanvasState *cs,
                                     unsigned int context_id, int top,
                                     int right, int bottom, int left);

DP_CanvasState *
DP_ops_layer_tree_create(DP_CanvasState *cs, DP_DrawContext *dc, int layer_id,
                         int source_id, int target_id, DP_Tile *tile, bool into,
                         bool group, const char *title, size_t title_length);

DP_CanvasState *DP_ops_layer_attributes(DP_CanvasState *cs, int layer_id,
                                        int sublayer_id, uint16_t opacity,
                                        int blend_mode, bool censored,
                                        bool isolated);

DP_CanvasState *DP_ops_layer_order(DP_CanvasState *cs, DP_DrawContext *dc,
                                   int layer_id_count,
                                   int (*get_layer_id)(void *, int),
                                   void *user);

DP_CanvasState *DP_ops_layer_tree_move(DP_CanvasState *cs, DP_DrawContext *dc,
                                       int layer_id, int parent_id,
                                       int sibling_id);

DP_CanvasState *DP_ops_layer_retitle(DP_CanvasState *cs, int layer_id,
                                     const char *title, size_t title_length);

DP_CanvasState *DP_ops_layer_delete(DP_CanvasState *cs, DP_DrawContext *dc,
                                    unsigned int context_id, int layer_id,
                                    bool merge);

DP_CanvasState *DP_ops_layer_tree_delete(DP_CanvasState *cs, DP_DrawContext *dc,
                                         unsigned int context_id, int layer_id,
                                         int merge_layer_id);

DP_CanvasState *DP_ops_layer_visibility(DP_CanvasState *cs, int layer_id,
                                        bool visible);

DP_CanvasStateChange DP_ops_put_image(DP_CanvasState *cs,
                                      unsigned int context_id, int layer_id,
                                      int blend_mode, int x, int y, int width,
                                      int height, const unsigned char *image,
                                      size_t image_size);

DP_CanvasStateChange DP_ops_move_region(DP_CanvasState *cs, DP_DrawContext *dc,
                                        unsigned int context_id,
                                        int src_layer_id, int dst_layer_id,
                                        const DP_Rect *src_rect,
                                        const DP_Quad *dst_quad,
                                        int interpolation, DP_Image *mask);

DP_CanvasStateChange DP_ops_move_rect(DP_CanvasState *cs,
                                      unsigned int context_id, int src_layer_id,
                                      int dst_layer_id, const DP_Rect *src_rect,
                                      int dst_x, int dst_y, DP_Image *mask);

DP_CanvasStateChange DP_ops_fill_rect(DP_CanvasState *cs,
                                      unsigned int context_id, int layer_id,
                                      int blend_mode, int left, int top,
                                      int right, int bottom, DP_UPixel15 pixel);

DP_CanvasState *DP_ops_put_tile(DP_CanvasState *cs, DP_Tile *tile, int layer_id,
                                int sublayer_id, int x, int y, int repeat);

DP_CanvasState *DP_ops_pen_up(DP_CanvasState *cs, DP_DrawContext *dc,
                              unsigned int context_id);

DP_CanvasState *DP_ops_annotation_create(DP_CanvasState *cs, int annotation_id,
                                         int x, int y, int width, int height);

DP_CanvasState *DP_ops_annotation_reshape(DP_CanvasState *cs, int annotation_id,
                                          int x, int y, int width, int height);

DP_CanvasState *DP_ops_annotation_edit(DP_CanvasState *cs, int annotation_id,
                                       uint32_t background_color, bool protect,
                                       int valign, const char *text,
                                       size_t text_length);

DP_CanvasState *DP_ops_annotation_delete(DP_CanvasState *cs, int annotation_id);

DP_CanvasStateChange
DP_ops_draw_dabs(DP_CanvasState *cs, DP_DrawContext *dc,
                 bool (*next)(void *, DP_PaintDrawDabsParams *), void *user);

DP_CanvasState *DP_ops_track_create(DP_CanvasState *cs, int new_id,
                                    int insert_id, int source_id,
                                    const char *title, size_t title_length);

DP_CanvasState *DP_ops_track_retitle(DP_CanvasState *cs, int track_id,
                                     const char *title, size_t title_length);

DP_CanvasState *DP_ops_track_delete(DP_CanvasState *cs, int track_id);

DP_CanvasState *DP_ops_track_order(DP_CanvasState *cs, int track_id_count,
                                   int (*get_track_id)(void *, int),
                                   void *user);

DP_CanvasState *DP_ops_key_frame_set(DP_CanvasState *cs, int track_id,
                                     int frame_index, int layer_id);

DP_CanvasState *DP_ops_key_frame_copy(DP_CanvasState *cs, int track_id,
                                      int frame_index, int source_track_id,
                                      int source_frame_index);

DP_CanvasState *DP_ops_key_frame_retitle(DP_CanvasState *cs, int track_id,
                                         int frame_index, const char *title,
                                         size_t title_length);

DP_CanvasState *DP_ops_key_frame_layer_attributes(
    DP_CanvasState *cs, DP_DrawContext *dc, int track_id, int frame_index,
    int count, DP_KeyFrameLayer (*get_layer)(void *, int), void *user);

DP_CanvasState *DP_ops_key_frame_delete(DP_CanvasState *cs, int track_id,
                                        int frame_index, int move_track_id,
                                        int move_frame_index);


#endif
