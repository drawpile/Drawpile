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
#ifndef DPENGINE_LAYER_LIST_H
#define DPENGINE_LAYER_LIST_H
#include "canvas_state.h"
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_LayerListEntry DP_LayerListEntry;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_ViewModeContext DP_ViewModeContext;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_LayerContent DP_LayerContent;
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
typedef struct DP_LayerGroup DP_LayerGroup;
typedef struct DP_TransientLayerGroup DP_TransientLayerGroup;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_TransientLayerList DP_TransientLayerList;
typedef struct DP_TransientTile DP_TransientTile;
#else
typedef struct DP_LayerContent DP_LayerContent;
typedef struct DP_LayerContent DP_TransientLayerContent;
typedef struct DP_LayerGroup DP_LayerGroup;
typedef struct DP_LayerGroup DP_TransientLayerGroup;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_LayerList DP_TransientLayerList;
typedef struct DP_Tile DP_TransientTile;
#endif


bool DP_layer_list_entry_is_group(DP_LayerListEntry *lle);

bool DP_layer_list_entry_transient(DP_LayerListEntry *lle);

DP_LayerContent *DP_layer_list_entry_content_noinc(DP_LayerListEntry *lle);

DP_TransientLayerContent *
DP_layer_list_entry_transient_content_noinc(DP_LayerListEntry *lle);

DP_LayerGroup *DP_layer_list_entry_group_noinc(DP_LayerListEntry *lle);

DP_TransientLayerGroup *
DP_layer_list_entry_transient_group_noinc(DP_LayerListEntry *lle);

int DP_layer_list_entry_width(DP_LayerListEntry *lle);
int DP_layer_list_entry_height(DP_LayerListEntry *lle);


DP_LayerList *DP_layer_list_new(void);

DP_LayerList *DP_layer_list_incref(DP_LayerList *ll);

DP_LayerList *DP_layer_list_incref_nullable(DP_LayerList *ll_or_null);

void DP_layer_list_decref(DP_LayerList *ll);

void DP_layer_list_decref_nullable(DP_LayerList *ll_or_null);

int DP_layer_list_refcount(DP_LayerList *ll);

bool DP_layer_list_transient(DP_LayerList *ll);

void DP_layer_list_diff(DP_LayerList *ll, DP_LayerPropsList *lpl,
                        DP_LayerList *prev_ll, DP_LayerPropsList *prev_lpl,
                        DP_CanvasDiff *diff, int only_layer_id);

void DP_layer_list_diff_mark(DP_LayerList *ll, DP_CanvasDiff *diff);

int DP_layer_list_count(DP_LayerList *ll);

DP_LayerListEntry *DP_layer_list_at_noinc(DP_LayerList *ll, int index);

DP_LayerContent *DP_layer_list_content_at_noinc(DP_LayerList *ll, int index);

DP_LayerGroup *DP_layer_list_group_at_noinc(DP_LayerList *ll, int index);

DP_TransientLayerList *DP_layer_list_resize(DP_LayerList *ll,
                                            unsigned int context_id, int top,
                                            int right, int bottom, int left);


void DP_layer_list_merge_to_flat_image(DP_LayerList *ll, DP_LayerPropsList *lpl,
                                       DP_TransientLayerContent *tlc,
                                       uint16_t parent_opacity,
                                       bool include_sublayers,
                                       bool reveal_censored,
                                       bool pass_through_censored, bool clip);

DP_TransientTile *DP_layer_list_entry_flatten_tile_to(
    DP_LayerListEntry *lle, DP_LayerProps *lp, int tile_index,
    DP_TransientTile *tt, uint16_t parent_opacity, DP_UPixel8 parent_tint,
    bool include_sublayers, bool pass_through_censored, bool clip,
    const DP_ViewModeContext *vmc);

DP_TransientTile *DP_layer_list_flatten_clipping_tile_to(
    void *user,
    DP_ViewModeContext (*fn)(void *, int, DP_LayerListEntry **,
                             DP_LayerProps **),
    int i, int clip_count, int tile_index, DP_TransientTile *tt_or_null,
    uint16_t parent_opacity, bool include_sublayers,
    const DP_ViewModeContext *vmc);

DP_TransientTile *DP_layer_list_flatten_tile_to(
    DP_LayerList *ll, DP_LayerPropsList *lpl, int tile_index,
    DP_TransientTile *tt_or_null, uint16_t parent_opacity,
    DP_UPixel8 parent_tint, bool include_sublayers, bool pass_through_censored,
    bool clip, const DP_ViewModeContext *vmc);


DP_TransientLayerList *DP_transient_layer_list_new_init(int reserve);

DP_TransientLayerList *DP_transient_layer_list_new(DP_LayerList *ll,
                                                   int reserve);

DP_TransientLayerList *
DP_transient_layer_list_reserve(DP_TransientLayerList *tll, int reserve);

DP_TransientLayerList *
DP_transient_layer_list_incref(DP_TransientLayerList *tll);

void DP_transient_layer_list_decref(DP_TransientLayerList *tll);

int DP_transient_layer_list_refcount(DP_TransientLayerList *tll);

DP_LayerList *DP_transient_layer_list_persist(DP_TransientLayerList *tll);

int DP_transient_layer_list_count(DP_TransientLayerList *tll);

DP_LayerListEntry *DP_transient_layer_list_at_noinc(DP_TransientLayerList *tll,
                                                    int index);


DP_TransientLayerContent *
DP_transient_layer_list_transient_content_at_noinc(DP_TransientLayerList *tll,
                                                   int index);

DP_TransientLayerGroup *
DP_transient_layer_list_transient_group_at_noinc(DP_TransientLayerList *tll,
                                                 int index);

DP_TransientLayerGroup *
DP_transient_layer_list_transient_group_at_with_children_noinc(
    DP_TransientLayerList *tll, int index,
    DP_TransientLayerList *transient_children);

void DP_transient_layer_list_set_content_noinc(DP_TransientLayerList *tll,
                                               DP_LayerContent *lc, int index);

void DP_transient_layer_list_set_content_inc(DP_TransientLayerList *tll,
                                             DP_LayerContent *lc, int index);

void DP_transient_layer_list_set_group_noinc(DP_TransientLayerList *tll,
                                             DP_LayerGroup *lg, int index);

void DP_transient_layer_list_set_group_inc(DP_TransientLayerList *tll,
                                           DP_LayerGroup *lg, int index);

void DP_transient_layer_list_set_inc(DP_TransientLayerList *tll,
                                     DP_LayerListEntry *lle, int index);

void DP_transient_layer_list_insert_content_inc(DP_TransientLayerList *tll,
                                                DP_LayerContent *lc, int index);

void DP_transient_layer_list_insert_group_inc(DP_TransientLayerList *tll,
                                              DP_LayerGroup *lg, int index);

void DP_transient_layer_list_set_transient_group_noinc(
    DP_TransientLayerList *tll, DP_TransientLayerGroup *tlg, int index);

void DP_transient_layer_list_insert_transient_content_noinc(
    DP_TransientLayerList *tll, DP_TransientLayerContent *tlc, int index);

void DP_transient_layer_list_insert_transient_group_noinc(
    DP_TransientLayerList *tll, DP_TransientLayerGroup *tlg, int index);

void DP_transient_layer_list_delete_at(DP_TransientLayerList *tll, int index);

void DP_transient_layer_list_merge_at(DP_TransientLayerList *tll,
                                      DP_LayerProps *lp, int index);


#endif
