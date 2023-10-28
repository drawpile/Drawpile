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
#ifndef DPENGINE_LAYER_PROPS_LIST_H
#define DPENGINE_LAYER_PROPS_LIST_H
#include <dpcommon/common.h>


#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_TransientLayerProps DP_TransientLayerProps;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_TransientLayerPropsList DP_TransientLayerPropsList;
#else
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_LayerProps DP_TransientLayerProps;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_LayerPropsList DP_TransientLayerPropsList;
#endif


DP_LayerPropsList *DP_layer_props_list_new(void);

DP_LayerPropsList *DP_layer_props_list_incref(DP_LayerPropsList *lpl);

DP_LayerPropsList *
DP_layer_props_list_incref_nullable(DP_LayerPropsList *lpl_or_null);

void DP_layer_props_list_decref(DP_LayerPropsList *lpl);

void DP_layer_props_list_decref_nullable(DP_LayerPropsList *lpl_or_null);

int DP_layer_props_list_refcount(DP_LayerPropsList *lpl);

bool DP_layer_props_list_transient(DP_LayerPropsList *lpl);

int DP_layer_props_list_count(DP_LayerPropsList *lpl);

DP_LayerProps *DP_layer_props_list_at_noinc(DP_LayerPropsList *lpl, int index);

int DP_layer_props_list_index_by_id(DP_LayerPropsList *lpl, int layer_id);

bool DP_layer_props_list_can_decrease_opacity(DP_LayerPropsList *lpl);


DP_TransientLayerPropsList *DP_transient_layer_props_list_new_init(int reserve);

DP_TransientLayerPropsList *
DP_transient_layer_props_list_new(DP_LayerPropsList *lpl, int reserve);

DP_TransientLayerPropsList *
DP_transient_layer_props_list_reserve(DP_TransientLayerPropsList *tlpl,
                                      int reserve);

DP_TransientLayerPropsList *
DP_transient_layer_props_list_incref(DP_TransientLayerPropsList *tlpl);

void DP_transient_layer_props_list_decref(DP_TransientLayerPropsList *tlpl);

int DP_transient_layer_props_list_refcount(DP_TransientLayerPropsList *tlpl);

DP_LayerPropsList *
DP_transient_layer_props_list_persist(DP_TransientLayerPropsList *tlpl);

int DP_transient_layer_props_list_count(DP_TransientLayerPropsList *tlpl);

DP_LayerProps *
DP_transient_layer_props_list_at_noinc(DP_TransientLayerPropsList *tlpl,
                                       int index);

DP_TransientLayerProps *DP_transient_layer_props_list_transient_at_noinc(
    DP_TransientLayerPropsList *tlpl, int index);

DP_TransientLayerProps *
DP_transient_layer_props_list_transient_at_with_children_noinc(
    DP_TransientLayerPropsList *tlpl, int index,
    DP_TransientLayerPropsList *transient_children);

int DP_transient_layer_props_list_index_by_id(DP_TransientLayerPropsList *tlpl,
                                              int layer_id);

void DP_transient_layer_props_list_set_noinc(DP_TransientLayerPropsList *tlpl,
                                             DP_LayerProps *lp, int index);

void DP_transient_layer_props_list_set_inc(DP_TransientLayerPropsList *tlpl,
                                           DP_LayerProps *lp, int index);

void DP_transient_layer_props_list_insert_inc(DP_TransientLayerPropsList *tlpl,
                                              DP_LayerProps *lp, int index);

void DP_transient_layer_props_list_set_transient_noinc(
    DP_TransientLayerPropsList *tlpl, DP_TransientLayerProps *tlp, int index);

void DP_transient_layer_props_list_insert_transient_noinc(
    DP_TransientLayerPropsList *tlpl, DP_TransientLayerProps *tlp, int index);

void DP_transient_layer_props_list_delete_at(DP_TransientLayerPropsList *tlpl,
                                             int index);

void DP_transient_layer_props_list_merge_at(DP_TransientLayerPropsList *tlpl,
                                            int index);


#endif
