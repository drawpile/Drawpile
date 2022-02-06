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
#ifndef DPENGINE_LAYER_H
#define DPENGINE_LAYER_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_BrushStamp DP_BrushStamp;
typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_PaintDrawDabsParams DP_PaintDrawDabsParams;
typedef struct DP_Quad DP_Quad;
typedef struct DP_Rect DP_Rect;
typedef struct DP_Tile DP_Tile;


#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_Layer DP_Layer;
typedef struct DP_TransientLayer DP_TransientLayer;
typedef struct DP_TransientLayerData DP_TransientLayerData;
typedef struct DP_TransientTile DP_TransientTile;
#else
typedef struct DP_Layer DP_Layer;
typedef struct DP_Layer DP_TransientLayer;
typedef struct DP_LayerData DP_TransientLayerData;
typedef struct DP_Tile DP_TransientTile;
#endif


void DP_transient_layer_data_brush_stamp_apply(DP_TransientLayerData *tld,
                                               unsigned int context_id,
                                               DP_Pixel src, int blend_mode,
                                               DP_BrushStamp *stamp);


DP_Layer *DP_layer_incref(DP_Layer *l);

void DP_layer_decref(DP_Layer *l);

int DP_layer_refcount(DP_Layer *l);

bool DP_layer_transient(DP_Layer *l);

void DP_layer_diff(DP_Layer *l, DP_Layer *prev, DP_CanvasDiff *diff);

void DP_layer_diff_mark(DP_Layer *l, DP_CanvasDiff *diff);


int DP_layer_id(DP_Layer *l);

uint8_t DP_layer_opacity(DP_Layer *l);

bool DP_layer_hidden(DP_Layer *l);

bool DP_layer_fixed(DP_Layer *l);

DP_LayerList *DP_layer_sublayers_noinc(DP_Layer *l);

bool DP_layer_visible(DP_Layer *l);

int DP_layer_width(DP_Layer *l);

int DP_layer_height(DP_Layer *l);

const char *DP_layer_title(DP_Layer *l, size_t *out_length);


DP_Tile *DP_layer_tile_at(DP_Layer *l, int x, int y);


DP_Layer *DP_layer_merge_to_flat_image(DP_Layer *l);

void DP_layer_flatten_tile_to(DP_Layer *l, int tile_index,
                              DP_TransientTile *tt);

DP_Image *DP_layer_to_image(DP_Layer *l);


DP_TransientLayer *DP_transient_layer_new(DP_Layer *l);

DP_TransientLayer *DP_transient_layer_new_init(int id, int width, int height,
                                               DP_Tile *tile);

DP_TransientLayer *DP_transient_layer_incref(DP_TransientLayer *tl);

void DP_transient_layer_decref(DP_TransientLayer *tl);

int DP_transient_layer_refcount(DP_TransientLayer *tl);

DP_Layer *DP_transient_layer_persist(DP_TransientLayer *tl);


int DP_transient_layer_id(DP_TransientLayer *tl);

uint8_t DP_transient_layer_opacity(DP_TransientLayer *tl);

bool DP_transient_layer_hidden(DP_TransientLayer *tl);

bool DP_transient_layer_fixed(DP_TransientLayer *tl);

DP_LayerList *DP_transient_layer_sublayers(DP_TransientLayer *tl);

bool DP_transient_layer_visible(DP_TransientLayer *tl);

int DP_transient_layer_width(DP_TransientLayer *tl);

int DP_transient_layer_height(DP_TransientLayer *tl);


void DP_transient_layer_id_set(DP_TransientLayer *tl, int layer_id);

void DP_transient_layer_title_set(DP_TransientLayer *tl, const char *title,
                                  size_t length);

void DP_transient_layer_opacity_set(DP_TransientLayer *tl, uint8_t opacity);

void DP_transient_layer_blend_mode_set(DP_TransientLayer *tl, int blend_mode);

void DP_transient_layer_censored_set(DP_TransientLayer *tl, bool censored);

void DP_transient_layer_hidden_set(DP_TransientLayer *tl, bool hidden);

void DP_transient_layer_fixed_set(DP_TransientLayer *tl, bool fixed);


DP_TransientLayer *
DP_transient_layer_transient_sublayer_create(DP_TransientLayer *tl,
                                             int sublayer_id);

DP_TransientLayer *
DP_transient_layer_transient_sublayer_at(DP_TransientLayer *tl, int index);

void DP_transient_layer_merge_all_sublayers(DP_TransientLayer *tl,
                                            unsigned int context_id);

void DP_transient_layer_merge_sublayer_at(DP_TransientLayer *tl,
                                          unsigned int context_id, int index);


void DP_transient_layer_merge(DP_TransientLayer *tl, DP_Layer *l,
                              unsigned int context_id);

void DP_transient_layer_resize(DP_TransientLayer *tl, unsigned int context_id,
                               int top, int right, int bottom, int left);

void DP_transient_layer_layer_attr(DP_TransientLayer *tl, int sublayer_id,
                                   uint8_t opacity, int blend_mode,
                                   bool censored, bool fixed);

void DP_transient_layer_layer_retitle(DP_TransientLayer *tl, const char *title,
                                      size_t title_length);

bool DP_transient_layer_put_image(DP_TransientLayer *tl,
                                  unsigned int context_id, int blend_mode,
                                  int x, int y, int width, int height,
                                  const unsigned char *image,
                                  size_t image_size) DP_MUST_CHECK;

void DP_transient_layer_fill_rect(DP_TransientLayer *tl,
                                  unsigned int context_id, int blend_mode,
                                  int left, int top, int right, int bottom,
                                  uint32_t color);

bool DP_transient_layer_put_tile(DP_TransientLayer *tl, DP_Tile *tile,
                                 int sublayer_id, int x, int y,
                                 int repeat) DP_MUST_CHECK;

bool DP_transient_layer_region_move(DP_TransientLayer *tl, DP_DrawContext *dc,
                                    unsigned int context_id,
                                    const DP_Rect *src_rect,
                                    const DP_Quad *dst_quad,
                                    DP_Image *mask) DP_MUST_CHECK;

bool DP_transient_layer_draw_dabs(DP_TransientLayer *tl, int sublayer_id,
                                  int sublayer_blend_mode, int sublayer_opacity,
                                  DP_PaintDrawDabsParams *params) DP_MUST_CHECK;


bool DP_transient_layer_resize_to(DP_TransientLayer *tl,
                                  unsigned int context_id, int width,
                                  int height);

void DP_transient_layer_render_tile(DP_TransientLayer *tl, DP_CanvasState *cs,
                                    int tile_index);


#endif
