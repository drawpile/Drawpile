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
#ifndef DPENGINE_LAYER_CONTENT_LIST_H
#define DPENGINE_LAYER_CONTENT_LIST_H
#include "canvas_state.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasDiff DP_CanvasDiff;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_LayerContent DP_LayerContent;
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
typedef struct DP_LayerContentList DP_LayerContentList;
typedef struct DP_TransientLayerContentList DP_TransientLayerContentList;
typedef struct DP_TransientTile DP_TransientTile;
#else
typedef struct DP_LayerContent DP_LayerContent;
typedef struct DP_LayerContent DP_TransientLayerContent;
typedef struct DP_LayerContentList DP_LayerContentList;
typedef struct DP_LayerContentList DP_TransientLayerContentList;
typedef struct DP_Tile DP_TransientTile;
#endif


DP_LayerContentList *DP_layer_content_list_new(void);

DP_LayerContentList *DP_layer_content_list_incref(DP_LayerContentList *lcl);

DP_LayerContentList *
DP_layer_content_list_incref_nullable(DP_LayerContentList *lcl_or_null);

void DP_layer_content_list_decref(DP_LayerContentList *lcl);

void DP_layer_content_list_decref_nullable(DP_LayerContentList *lcl_or_null);

int DP_layer_content_list_refcount(DP_LayerContentList *lcl);

bool DP_layer_content_list_transient(DP_LayerContentList *lcl);

void DP_layer_content_list_diff(DP_LayerContentList *lcl,
                                DP_LayerPropsList *lpl,
                                DP_LayerContentList *prev_lcl,
                                DP_LayerPropsList *prev_lpl,
                                DP_CanvasDiff *diff);

void DP_layer_content_list_diff_mark(DP_LayerContentList *lcl,
                                     DP_CanvasDiff *diff);

int DP_layer_content_list_count(DP_LayerContentList *lcl);

DP_LayerContent *DP_layer_content_list_at_noinc(DP_LayerContentList *lcl,
                                                int index);

DP_TransientLayerContentList *
DP_layer_content_list_resize(DP_LayerContentList *lcl, unsigned int context_id,
                             int top, int right, int bottom, int left);


void DP_layer_content_list_merge_to_flat_image(DP_LayerContentList *lcl,
                                               DP_LayerPropsList *lpl,
                                               DP_TransientLayerContent *tlc,
                                               unsigned int flags);

void DP_layer_content_list_flatten_tile_to(DP_LayerContentList *lcl,
                                           DP_LayerPropsList *lpl,
                                           int tile_index,
                                           DP_TransientTile *tt);


DP_TransientLayerContentList *
DP_transient_layer_content_list_new_init(int reserve);

DP_TransientLayerContentList *
DP_transient_layer_content_list_new(DP_LayerContentList *lcl, int reserve);

DP_TransientLayerContentList *
DP_transient_layer_content_list_reserve(DP_TransientLayerContentList *tlcl,
                                        int reserve);

DP_TransientLayerContentList *
DP_transient_layer_content_list_incref(DP_TransientLayerContentList *tlcl);

void DP_transient_layer_content_list_decref(DP_TransientLayerContentList *tlcl);

int DP_transient_layer_content_list_refcount(
    DP_TransientLayerContentList *tlcl);

DP_LayerContentList *
DP_transient_layer_content_list_persist(DP_TransientLayerContentList *tlcl);

int DP_transient_layer_content_list_count(DP_TransientLayerContentList *tlcl);

DP_LayerContent *
DP_transient_layer_content_list_at_noinc(DP_TransientLayerContentList *tlcl,
                                         int index);

DP_TransientLayerContent *DP_transient_layer_content_list_transient_at_noinc(
    DP_TransientLayerContentList *tlcl, int index);

void DP_transient_layer_content_list_insert_inc(
    DP_TransientLayerContentList *tlcl, DP_LayerContent *lc, int index);

void DP_transient_layer_content_list_insert_transient_noinc(
    DP_TransientLayerContentList *tlcl, DP_TransientLayerContent *tlc,
    int index);

void DP_transient_layer_content_list_delete_at(
    DP_TransientLayerContentList *tlcl, int index);


#endif
