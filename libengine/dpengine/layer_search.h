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
 */
#ifndef DPENGINE_LAYER_SEARCH_H
#define DPENGINE_LAYER_SEARCH_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_LayerListEntry DP_LayerListEntry;
typedef struct DP_LayerProps DP_LayerProps;

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

#define DP_LAYER_SEARCH_FOUND(SEARCH) ((SEARCH).lle)


typedef struct DP_LayerSearch {
    int layer_id;
    int layer_index_list;
    DP_LayerListEntry *lle;
    DP_LayerProps *lp;
} DP_LayerSearch;


DP_LayerSearch DP_layer_search_make(int layer_id, int layer_list_index);

int DP_layer_search(DP_CanvasState *cs, DP_DrawContext *dc, int searches_count,
                    DP_LayerSearch *searches);

DP_TransientLayerContent *
DP_layer_search_transient_content(DP_TransientCanvasState *tcs,
                                  int layer_index_count, int *layer_indexes);

DP_TransientLayerProps *
DP_layer_search_transient_props(DP_TransientCanvasState *tcs,
                                  int layer_index_count, int *layer_indexes);

void DP_layer_search_transient_children(DP_TransientCanvasState *tcs,
                                        int layer_index_count,
                                        int *layer_indexes, int reserve,
                                        DP_TransientLayerList **out_tll,
                                        DP_TransientLayerPropsList **out_tlpl);


#endif
