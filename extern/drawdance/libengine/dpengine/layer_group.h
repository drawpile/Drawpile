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
#ifndef DPENGINE_LAYER_GROUP_H
#define DPENGINE_LAYER_GROUP_H
#include <dpcommon/common.h>

typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_ViewModeFilter DP_ViewModeFilter;
typedef union DP_Pixel8 DP_Pixel8;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_LayerGroup DP_LayerGroup;
typedef struct DP_TransientLayerGroup DP_TransientLayerGroup;
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_TransientLayerList DP_TransientLayerList;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_TransientTile DP_TransientTile;
#else
typedef struct DP_LayerGroup DP_LayerGroup;
typedef struct DP_LayerGroup DP_TransientLayerGroup;
typedef struct DP_LayerContent DP_TransientLayerContent;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_LayerList DP_TransientLayerList;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_Tile DP_TransientTile;
#endif


DP_LayerGroup *DP_layer_group_incref(DP_LayerGroup *lg);

DP_LayerGroup *DP_layer_group_incref_nullable(DP_LayerGroup *lg_or_null);

void DP_layer_group_decref(DP_LayerGroup *lg);

void DP_layer_group_decref_nullable(DP_LayerGroup *lg_or_null);

int DP_layer_group_refcount(DP_LayerGroup *lg);

bool DP_layer_group_transient(DP_LayerGroup *lg);

void DP_layer_group_diff(DP_LayerGroup *lg, DP_LayerProps *lp,
                         DP_LayerGroup *prev_lg, DP_LayerProps *prev_lp,
                         DP_CanvasDiff *diff);

void DP_layer_group_diff_mark(DP_LayerGroup *lg, DP_CanvasDiff *diff);

int DP_layer_group_width(DP_LayerGroup *lg);

int DP_layer_group_height(DP_LayerGroup *lg);

DP_LayerList *DP_layer_group_children_noinc(DP_LayerGroup *lg);

int DP_layer_group_search_change_bounds(DP_LayerGroup *lg, DP_LayerProps *lp,
                                        unsigned int context_id, int *out_x,
                                        int *out_y, int *out_width,
                                        int *out_height);

DP_TransientLayerGroup *DP_layer_group_resize(DP_LayerGroup *lg,
                                              unsigned int context_id, int top,
                                              int right, int bottom, int left);

DP_Pixel8 *DP_layer_group_to_pixels8(DP_LayerGroup *lg, DP_LayerProps *lp,
                                     int x, int y, int width, int height);

DP_TransientLayerContent *DP_layer_group_merge(DP_LayerGroup *lg,
                                               DP_LayerProps *lp);

void DP_layer_group_merge_to_flat_image(DP_LayerGroup *lg, DP_LayerProps *lp,
                                        DP_TransientLayerContent *tlc,
                                        uint16_t parent_opacity,
                                        bool include_sublayers);

DP_TransientTile *
DP_layer_group_flatten_tile_to(DP_LayerGroup *lg, DP_LayerProps *lp,
                               int tile_index, DP_TransientTile *tt_or_null,
                               uint16_t parent_opacity, bool include_sublayers,
                               const DP_ViewModeFilter *vmf);


DP_TransientLayerGroup *DP_transient_layer_group_new(DP_LayerGroup *lg);

DP_TransientLayerGroup *
DP_transient_layer_group_new_with_children_noinc(DP_LayerGroup *lg,
                                                 DP_TransientLayerList *tll);

DP_TransientLayerGroup *DP_transient_layer_group_new_init(int width, int height,
                                                          int reserve);

DP_TransientLayerGroup *
DP_transient_layer_group_new_init_with_transient_children_noinc(
    int width, int height, DP_TransientLayerList *tll);

DP_TransientLayerGroup *
DP_transient_layer_group_incref(DP_TransientLayerGroup *tlg);

void DP_transient_layer_group_decref(DP_TransientLayerGroup *tlg);

int DP_transient_layer_group_refcount(DP_TransientLayerGroup *tlg);

DP_LayerGroup *DP_transient_layer_group_persist(DP_TransientLayerGroup *tlg);

int DP_transient_layer_group_width(DP_TransientLayerGroup *tlg);

int DP_transient_layer_group_height(DP_TransientLayerGroup *tlg);

DP_LayerList *
DP_transient_layer_group_children_noinc(DP_TransientLayerGroup *tlg);

DP_TransientLayerList *
DP_transient_layer_group_transient_children(DP_TransientLayerGroup *tlg,
                                            int reserve);


#endif
