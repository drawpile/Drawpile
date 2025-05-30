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
#ifndef DPENGINE_LAYER_ROUTES_H
#define DPENGINE_LAYER_ROUTES_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_LayerContent DP_LayerContent;
typedef struct DP_LayerGroup DP_LayerGroup;
typedef struct DP_LayerList DP_LayerList;
typedef struct DP_LayerListEntry DP_LayerListEntry;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_Message DP_Message;
typedef union DP_UPixel8 DP_UPixel8;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientCanvasState DP_TransientCanvasState;
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
typedef struct DP_TransientLayerList DP_TransientLayerList;
typedef struct DP_TransientLayerProps DP_TransientLayerProps;
typedef struct DP_TransientLayerPropsList DP_TransientLayerPropsList;
#else
typedef struct DP_CanvasState DP_TransientCanvasState;
typedef struct DP_LayerContent DP_TransientLayerContent;
typedef struct DP_LayerList DP_TransientLayerList;
typedef struct DP_LayerProps DP_TransientLayerProps;
typedef struct DP_LayerPropsList DP_TransientLayerPropsList;
#endif


typedef struct DP_LayerRoutesEntry DP_LayerRoutesEntry;
typedef struct DP_LayerRoutes DP_LayerRoutes;

typedef struct DP_LayerRoutesSelEntry {
    bool exists;
    bool is_selection;
    union {
        DP_LayerRoutesEntry *lre;
        struct {
            unsigned int context_id;
            int selection_id;
            int index;
        } sel;
    } DP_ANONYMOUS(u);
} DP_LayerRoutesSelEntry;


DP_LayerRoutes *DP_layer_routes_new(void);

DP_LayerRoutes *DP_layer_routes_new_index(DP_LayerPropsList *lpl,
                                          DP_DrawContext *dc);

DP_LayerRoutes *DP_layer_routes_incref(DP_LayerRoutes *lr);

DP_LayerRoutes *DP_layer_routes_incref_nullable(DP_LayerRoutes *lr_or_null);

void DP_layer_routes_decref(DP_LayerRoutes *lr);

void DP_layer_routes_decref_nullable(DP_LayerRoutes *lr_or_null);

DP_LayerRoutesEntry *DP_layer_routes_search(DP_LayerRoutes *lr, int layer_id);

int DP_layer_routes_search_parent_id(DP_LayerRoutes *lr, int layer_id);

DP_Message *DP_layer_routes_layer_tree_move_make(DP_LayerRoutes *lr,
                                                 DP_CanvasState *cs,
                                                 unsigned int context_id,
                                                 int source_id, int target_id,
                                                 bool into, bool below);


int DP_layer_routes_entry_layer_id(DP_LayerRoutesEntry *lre);

bool DP_layer_routes_entry_is_group(DP_LayerRoutesEntry *lre);

int *DP_layer_routes_entry_indexes(DP_LayerRoutesEntry *lre, int *out_count);

int DP_layer_routes_entry_index_count(DP_LayerRoutesEntry *lre);

int DP_layer_routes_entry_index_at(DP_LayerRoutesEntry *lre, int index);

int DP_layer_routes_entry_index_last(DP_LayerRoutesEntry *lre);

DP_LayerListEntry *DP_layer_routes_entry_layer(DP_LayerRoutesEntry *lre,
                                               DP_CanvasState *cs);

DP_LayerContent *DP_layer_routes_entry_content(DP_LayerRoutesEntry *lre,
                                               DP_CanvasState *cs);

DP_LayerGroup *DP_layer_routes_entry_group(DP_LayerRoutesEntry *lre,
                                           DP_CanvasState *cs);

DP_LayerProps *DP_layer_routes_entry_props(DP_LayerRoutesEntry *lre,
                                           DP_CanvasState *cs);

void DP_layer_routes_entry_children(DP_LayerRoutesEntry *lre,
                                    DP_CanvasState *cs, DP_LayerList **out_ll,
                                    DP_LayerPropsList **out_lpl);

DP_LayerRoutesEntry *DP_layer_routes_entry_parent(DP_LayerRoutesEntry *lre);

void DP_layer_routes_entry_parent_opacity_tint(DP_LayerRoutesEntry *lre,
                                               DP_CanvasState *cs,
                                               uint16_t *out_parent_opacity,
                                               DP_UPixel8 *out_parent_tint);

DP_TransientLayerContent *
DP_layer_routes_entry_indexes_transient_content(int index_count, int *indexes,
                                                DP_TransientCanvasState *tcs);

DP_TransientLayerContent *
DP_layer_routes_entry_transient_content(DP_LayerRoutesEntry *lre,
                                        DP_TransientCanvasState *tcs);

DP_TransientLayerProps *
DP_layer_routes_entry_indexes_transient_props(int index_count, int *indexes,
                                              DP_TransientCanvasState *tcs);

DP_TransientLayerProps *
DP_layer_routes_entry_transient_props(DP_LayerRoutesEntry *lre,
                                      DP_TransientCanvasState *tcs);

void DP_layer_routes_entry_transient_children(
    DP_LayerRoutesEntry *lre, DP_TransientCanvasState *tcs, int offset,
    int reserve, DP_TransientLayerList **out_tll,
    DP_TransientLayerPropsList **out_tlpl);


DP_LayerRoutesSelEntry DP_layer_routes_search_sel(DP_LayerRoutes *lr,
                                                  DP_CanvasState *cs,
                                                  int layer_or_selection_id);

DP_LayerRoutesSelEntry DP_layer_routes_search_sel_only(DP_CanvasState *cs,
                                                       unsigned int context_id,
                                                       int selection_id);

DP_LayerRoutesSelEntry
DP_layer_routes_sel_entry_from_selection_index(DP_CanvasState *cs, int index);

// Layer or selection exists and is not a group.
bool DP_layer_routes_sel_entry_is_valid_source(DP_LayerRoutesSelEntry *lrse);

// Layer exists and is not a group, selection exists or can be auto-created.
bool DP_layer_routes_sel_entry_is_valid_target(DP_LayerRoutesSelEntry *lrse);

bool DP_layer_routes_sel_entry_is_group(DP_LayerRoutesSelEntry *lrse);

int *DP_layer_routes_sel_entry_indexes(DP_LayerRoutesSelEntry *lrse,
                                       int *out_count);

DP_LayerContent *DP_layer_routes_sel_entry_content(DP_LayerRoutesSelEntry *lrse,
                                                   DP_CanvasState *cs);

DP_TransientLayerContent *
DP_layer_routes_sel_entry_transient_content(DP_LayerRoutesSelEntry *lrse,
                                            DP_TransientCanvasState *tcs);


#endif
