// SPDX-License-Identifier: MIT
#ifndef DPENGINE_LOCAL_STATE_H
#define DPENGINE_LOCAL_STATE_H

#include "view_mode.h"
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

typedef void (*DP_LocalStateViewInvalidatedFn)(void *user, bool check_all,
                                               int layer_id);
typedef bool (*DP_LocalStateAcceptResetMessageFn)(void *user, DP_Message *msg);

DP_LocalState *
DP_local_state_new(DP_CanvasState *cs_or_null,
                   DP_LocalStateViewInvalidatedFn view_invalidated, void *user);

void DP_local_state_free(DP_LocalState *ls);

const int *DP_local_state_hidden_layer_ids(DP_LocalState *ls, int *out_count);

int DP_local_state_hidden_layer_id_count(DP_LocalState *ls);

DP_Tile *DP_local_state_background_tile_noinc(DP_LocalState *ls);

bool DP_local_state_background_opaque(DP_LocalState *ls);

DP_ViewMode DP_local_state_view_mode(DP_LocalState *ls);

int DP_local_state_active_layer_id(DP_LocalState *ls);

int DP_local_state_active_frame_index(DP_LocalState *ls);

DP_ViewModeFilter DP_local_state_view_mode_filter_make(DP_LocalState *ls,
                                                       DP_ViewModeBuffer *vmb,
                                                       DP_CanvasState *cs);

const DP_LocalTrackState *DP_local_state_track_states(DP_LocalState *ls,
                                                      int *out_count);

int DP_local_state_track_state_count(DP_LocalState *ls);

bool DP_local_state_track_visible(DP_LocalState *ls, int track_id);

void DP_local_state_handle(DP_LocalState *ls, DP_DrawContext *dc,
                           DP_Message *msg);

bool DP_local_state_reset_image_build(DP_LocalState *ls, DP_DrawContext *dc,
                                      DP_LocalStateAcceptResetMessageFn fn,
                                      void *user);


DP_Message *DP_local_state_msg_layer_visibility_new(int layer_id, bool hidden);

// May return NULL if compressing the tile fails.
DP_Message *DP_local_state_msg_background_tile_new(DP_DrawContext *dc,
                                                   DP_Tile *tile_or_null);

DP_Message *DP_local_state_msg_view_mode_new(DP_ViewMode view_mode);

DP_Message *DP_local_state_msg_active_layer_new(int layer_id);

DP_Message *DP_local_state_msg_active_frame_new(int frame_index);

DP_Message *DP_local_state_msg_onion_skins_new(const DP_OnionSkins *oss);

DP_Message *DP_local_state_msg_track_visibility_new(int track_id, bool hidden);

DP_Message *DP_local_state_msg_track_onion_skin_new(int track_id,
                                                    bool onion_skin);


#endif
