// SPDX-License-Identifier: MIT
#ifndef DPENGINE_LOCAL_STATE_H
#define DPENGINE_LOCAL_STATE_H

#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Message DP_Message;
typedef struct DP_MsgLocalChange DP_MsgLocalChange;
typedef struct DP_Tile DP_Tile;


typedef struct DP_LocalState DP_LocalState;

typedef struct DP_LocalTrackState {
    int track_id;
    bool hidden;
    bool onion_skin;
} DP_LocalTrackState;

typedef void (*DP_LocalStateLayerVisibilityChangedFn)(void *user, int layer_id);
typedef void (*DP_LocalStateBackgroundTileChangedFn)(void *user);
typedef void (*DP_LocalStateTrackStateChangedFn)(void *user, int track_id);
typedef bool (*DP_LocalStateAcceptResetMessageFn)(void *user, DP_Message *msg);

DP_LocalState *DP_local_state_new(
    DP_CanvasState *cs_or_null,
    DP_LocalStateLayerVisibilityChangedFn layer_visibility_changed,
    DP_LocalStateBackgroundTileChangedFn background_tile_changed,
    DP_LocalStateTrackStateChangedFn track_state_changed, void *user);

void DP_local_state_free(DP_LocalState *ls);

const int *DP_local_state_hidden_layer_ids(DP_LocalState *ls, int *out_count);

int DP_local_state_hidden_layer_id_count(DP_LocalState *ls);

DP_Tile *DP_local_state_background_tile_noinc(DP_LocalState *ls);

bool DP_local_state_background_opaque(DP_LocalState *ls);

const DP_LocalTrackState *DP_local_state_track_states(DP_LocalState *ls,
                                                      int *out_count);

int DP_local_state_track_state_count(DP_LocalState *ls);

void DP_local_state_handle(DP_LocalState *ls, DP_DrawContext *dc,
                           DP_Message *msg);

bool DP_local_state_reset_image_build(DP_LocalState *ls, DP_DrawContext *dc,
                                      DP_LocalStateAcceptResetMessageFn fn,
                                      void *user);


DP_Message *DP_local_state_msg_layer_visibility_new(int layer_id, bool hidden);

// May return NULL if compressing the tile fails.
DP_Message *DP_local_state_msg_background_tile_new(DP_DrawContext *dc,
                                                   DP_Tile *tile_or_null);

DP_Message *DP_local_state_msg_track_visibility_new(int track_id, bool hidden);

DP_Message *DP_local_state_msg_track_onion_skin_new(int track_id,
                                                    bool onion_skin);


#endif
