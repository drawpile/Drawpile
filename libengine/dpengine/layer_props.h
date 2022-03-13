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
#ifndef DPENGINE_LAYER_PROPS_H
#define DPENGINE_LAYER_PROPS_H
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


DP_LayerProps *DP_layer_props_incref(DP_LayerProps *lp);

DP_LayerProps *DP_layer_props_incref_nullable(DP_LayerProps *lp_or_null);

void DP_layer_props_decref(DP_LayerProps *lp);

void DP_layer_props_decref_nullable(DP_LayerProps *lp_or_null);

int DP_layer_props_refcount(DP_LayerProps *lp);

bool DP_layer_props_transient(DP_LayerProps *lp);

int DP_layer_props_id(DP_LayerProps *lp);

uint8_t DP_layer_props_opacity(DP_LayerProps *lp);

int DP_layer_props_blend_mode(DP_LayerProps *lp);

bool DP_layer_props_hidden(DP_LayerProps *lp);

bool DP_layer_props_censored(DP_LayerProps *lp);

bool DP_layer_props_fixed(DP_LayerProps *lp);

bool DP_layer_props_visible(DP_LayerProps *lp);

const char *DP_layer_props_title(DP_LayerProps *lp, size_t *out_length);


DP_TransientLayerProps *DP_transient_layer_props_new(DP_LayerProps *lp);

DP_TransientLayerProps *DP_transient_layer_props_new_init(int layer_id);

DP_TransientLayerProps *
DP_transient_layer_props_incref(DP_TransientLayerProps *tlp);

void DP_transient_layer_props_decref(DP_TransientLayerProps *tlp);

int DP_transient_layer_props_refcount(DP_TransientLayerProps *tlp);

DP_LayerProps *DP_transient_layer_props_persist(DP_TransientLayerProps *tlp);

int DP_transient_layer_props_id(DP_TransientLayerProps *tlp);

uint8_t DP_transient_layer_props_opacity(DP_TransientLayerProps *tlp);

int DP_transient_layer_props_blend_mode(DP_TransientLayerProps *tlp);

bool DP_transient_layer_props_hidden(DP_TransientLayerProps *tlp);

bool DP_transient_layer_props_censored(DP_TransientLayerProps *tlp);

bool DP_transient_layer_props_fixed(DP_TransientLayerProps *tlp);

bool DP_transient_layer_props_visible(DP_TransientLayerProps *tlp);

const char *DP_transient_layer_props_title(DP_TransientLayerProps *tlp,
                                           size_t *out_length);

void DP_transient_layer_props_id_set(DP_TransientLayerProps *tlp, int layer_id);

void DP_transient_layer_props_title_set(DP_TransientLayerProps *tlp,
                                        const char *title, size_t length);

void DP_transient_layer_props_opacity_set(DP_TransientLayerProps *tlp,
                                          uint8_t opacity);

void DP_transient_layer_props_blend_mode_set(DP_TransientLayerProps *tlp,
                                             int blend_mode);

void DP_transient_layer_props_censored_set(DP_TransientLayerProps *tlp,
                                           bool censored);

void DP_transient_layer_props_hidden_set(DP_TransientLayerProps *tlp,
                                         bool hidden);

void DP_transient_layer_props_fixed_set(DP_TransientLayerProps *tlp,
                                        bool fixed);


#endif
