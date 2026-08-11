// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PLAYBACK_H
#define DPENGINE_PLAYBACK_H
#include <dpcommon/common.h>

typedef struct DP_CanvasHistory DP_CanvasHistory;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_LocalState DP_LocalState;
typedef struct DP_Message DP_Message;
typedef struct DP_Project DP_Project;


typedef struct DP_Playback DP_Playback;
typedef void (*DP_PlaybackHandleFn)(DP_Playback *pb, DP_Message *msg);

DP_Playback *DP_playback_new(DP_DrawContext *dc);

void DP_playback_free(DP_Playback *pb);

void DP_playback_reset(DP_Playback *pb);

DP_DrawContext *DP_playback_draw_context(DP_Playback *pb);
DP_CanvasHistory *DP_playback_canvas_history(DP_Playback *pb);
DP_LocalState *DP_playback_local_state(DP_Playback *pb);

// Call DP_playback_flush_multidab before these if you need the current state!
// Otherwise there may still be dabs pending.
DP_CanvasState *DP_playback_local_canvas_inc(DP_Playback *pb);
DP_CanvasState *DP_playback_history_canvas_noinc(DP_Playback *pb);

bool DP_playback_local_state_dirty(DP_Playback *pb);
void DP_playback_local_state_dirty_set(DP_Playback *pb, bool local_state_dirty);

void DP_playback_canvas_history_reset(DP_Playback *pb);
void DP_playback_canvas_history_soft_reset(DP_Playback *pb);
void DP_playback_canvas_history_reset_to_state_noinc(DP_Playback *pb,
                                                     DP_CanvasState *cs);

long long DP_playback_canvas_history_project_player_snapshot(
    DP_Playback *pb, DP_Project *prj, long long session_id,
    long long sequence_id, double recorded_at);

void DP_playback_flush_multidab(DP_Playback *pb);
void DP_playback_clear_multidab(DP_Playback *pb);

DP_PlaybackHandleFn DP_playback_get_handle_fn(int type);

void DP_playback_handle_message_dec(DP_Playback *pb, DP_Message *msg,
                                    DP_PlaybackHandleFn fn);

void DP_playback_push_message_inc(DP_Playback *pb, DP_Message *msg);

bool DP_playback_local_state_get_reset(DP_Playback *pb,
                                       bool (*fn)(void *, DP_Message *),
                                       void *user);


#endif
