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
#ifndef DPENGINE_LAYERLIST_H
#define DPENGINE_LAYERLIST_H
#include <dpcommon/common.h>

typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_Layer DP_Layer;
typedef struct DP_MsgLayerOrder DP_MsgLayerOrder;
typedef struct DP_PaintDrawDabsParams DP_PaintDrawDabsParams;
typedef struct DP_Quad DP_Quad;
typedef struct DP_Rect DP_Rect;
typedef struct DP_Tile DP_Tile;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientLayer DP_TransientLayer;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_TransientLayerList DP_TransientLayerList;
typedef struct DP_TransientTile DP_TransientTile;
#else
typedef struct DP_Layer DP_TransientLayer;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_LayerList DP_TransientLayerList;
typedef struct DP_Tile DP_TransientTile;
#endif

DP_LayerList *DP_layer_list_new(void);

DP_LayerList *DP_layer_list_incref(DP_LayerList *ll);

void DP_layer_list_decref(DP_LayerList *ll);

int DP_layer_list_refcount(DP_LayerList *ll);

bool DP_layer_list_transient(DP_LayerList *ll);

void DP_layer_list_diff(DP_LayerList *ll, DP_LayerList *prev,
                        DP_CanvasDiff *diff);

void DP_layer_list_diff_mark(DP_LayerList *ll, DP_CanvasDiff *diff);


int DP_layer_list_layer_count(DP_LayerList *ll);

int DP_layer_list_layer_index_by_id(DP_LayerList *ll, int layer_id);

DP_Layer *DP_layer_list_layer_by_id(DP_LayerList *ll, int layer_id);

DP_Layer *DP_layer_list_at_noinc(DP_LayerList *ll, int index);


void DP_layer_list_merge_to_flat_image(DP_LayerList *ll, DP_TransientLayer *tl,
                                       unsigned int flags);

void DP_layer_list_flatten_tile_to(DP_LayerList *ll, int tile_index,
                                   DP_TransientTile *tt);

DP_TransientLayerList *DP_layer_list_layer_reorder(DP_LayerList *ll,
                                                   const int *layer_ids,
                                                   int layer_id_count);


DP_TransientLayerList *DP_transient_layer_list_new(DP_LayerList *ll,
                                                   int reserve);

DP_TransientLayerList *DP_transient_layer_list_new_init(void);

DP_TransientLayerList *
DP_transient_layer_list_reserve(DP_TransientLayerList *tll, int reserve);

DP_TransientLayerList *
DP_transient_layer_list_incref(DP_TransientLayerList *tll);

void DP_transient_layer_list_decref(DP_TransientLayerList *tll);

int DP_transient_layer_list_refcount(DP_TransientLayerList *tll);

DP_LayerList *DP_transient_layer_list_persist(DP_TransientLayerList *tll);


int DP_transient_layer_list_layer_index_by_id(DP_TransientLayerList *tll,
                                              int layer_id);

DP_Layer *DP_transient_layer_list_layer_by_id(DP_TransientLayerList *tll,
                                              int layer_id);

DP_Layer *DP_transient_layer_list_at(DP_TransientLayerList *tll, int index);

DP_TransientLayer *
DP_transient_layer_list_transient_at(DP_TransientLayerList *tll, int index);

void DP_transient_layer_list_remove_at(DP_TransientLayerList *tll, int index);


void DP_transient_layer_list_resize(DP_TransientLayerList *tll,
                                    unsigned int context_id, int top, int right,
                                    int bottom, int left);

DP_TransientLayer *
DP_transient_layer_list_layer_create(DP_TransientLayerList *tll, int layer_id,
                                     int source_id, DP_Tile *tile, bool insert,
                                     bool copy, int width, int height,
                                     const char *title, size_t title_length);

bool DP_transient_layer_list_layer_attr(DP_TransientLayerList *tll,
                                        int layer_id, int sublayer_id,
                                        uint8_t opacity, int blend_mode,
                                        bool censored,
                                        bool fixed) DP_MUST_CHECK;

bool DP_transient_layer_list_layer_retitle(DP_TransientLayerList *tll,
                                           int layer_id, const char *title,
                                           size_t title_length) DP_MUST_CHECK;

bool DP_transient_layer_list_layer_delete(DP_TransientLayerList *tll,
                                          unsigned int context_id, int layer_id,
                                          bool merge) DP_MUST_CHECK;

bool DP_transient_layer_list_layer_visibility(DP_TransientLayerList *tll,
                                              int layer_id,
                                              bool visible) DP_MUST_CHECK;

bool DP_transient_layer_list_put_image(DP_TransientLayerList *tll,
                                       unsigned int context_id, int layer_id,
                                       int blend_mode, int x, int y, int width,
                                       int height, const unsigned char *image,
                                       size_t image_size) DP_MUST_CHECK;

bool DP_transient_layer_list_fill_rect(DP_TransientLayerList *tll,
                                       unsigned int context_id, int layer_id,
                                       int blend_mode, int left, int top,
                                       int right, int bottom,
                                       uint32_t color) DP_MUST_CHECK;

bool DP_transient_layer_list_region_move(DP_TransientLayerList *tll,
                                         DP_DrawContext *dc,
                                         unsigned int context_id, int layer_id,
                                         const DP_Rect *src_rect,
                                         const DP_Quad *dst_quad,
                                         DP_Image *mask) DP_MUST_CHECK;

bool DP_transient_layer_list_put_tile(DP_TransientLayerList *tll, DP_Tile *tile,
                                      int layer_id, int sublayer_id, int x,
                                      int y, int repeat) DP_MUST_CHECK;

bool DP_transient_layer_list_draw_dabs(
    DP_TransientLayerList *tll, int layer_id, int sublayer_id,
    int sublayer_blend_mode, int sublayer_opacity,
    DP_PaintDrawDabsParams *params) DP_MUST_CHECK;


#endif
