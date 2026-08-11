// SPDX-License-Identifier: GPL-3.0-or-later
#include "playback.h"
#include "canvas_history.h"
#include "draw_context.h"
#include "local_state.h"
#include <dpcommon/common.h>
#include <dpmsg/message.h>

#define MAX_MULTIDAB_COUNT 8192


typedef void (*DP_PlaybackHandleFn)(DP_Playback *pb, DP_Message *msg);

struct DP_Playback {
    DP_DrawContext *dc;
    DP_CanvasHistory *ch;
    DP_LocalState *ls;
    DP_Message **multidab_msgs;
    int multidab_count;
    bool local_state_dirty;
};

DP_Playback *DP_playback_new(DP_DrawContext *dc)
{
    DP_Playback *pb = DP_malloc(sizeof(*pb));
    *pb = (DP_Playback){
        dc,
        DP_canvas_history_new_no_mutex(),
        DP_local_state_new(NULL, NULL, NULL),
        DP_malloc(sizeof(*pb->multidab_msgs) * (size_t)MAX_MULTIDAB_COUNT),
        0,
        true,
    };
    return pb;
}

void DP_playback_free(DP_Playback *pb)
{
    if (pb) {
        DP_playback_clear_multidab(pb);
        DP_free(pb->multidab_msgs);
        DP_local_state_free(pb->ls);
        DP_canvas_history_free(pb->ch);
        DP_free(pb);
    }
}

void DP_playback_reset(DP_Playback *pb)
{
    DP_ASSERT(pb);
    DP_canvas_history_reset(pb->ch);
    DP_local_state_reset(pb->ls);
    DP_playback_clear_multidab(pb);
    pb->local_state_dirty = true;
}

DP_DrawContext *DP_playback_draw_context(DP_Playback *pb)
{
    DP_ASSERT(pb);
    return pb->dc;
}

DP_CanvasHistory *DP_playback_canvas_history(DP_Playback *pb)
{
    DP_ASSERT(pb);
    return pb->ch;
}

DP_LocalState *DP_playback_local_state(DP_Playback *pb)
{
    DP_ASSERT(pb);
    return pb->ls;
}

DP_CanvasState *DP_playback_local_canvas_inc(DP_Playback *pb)
{
    return DP_local_state_apply_nodec(
        pb->ls, DP_canvas_history_get_noinc_nolock(pb->ch), pb->dc);
}

DP_CanvasState *DP_playback_history_canvas_noinc(DP_Playback *pb)
{
    DP_ASSERT(pb);
    return DP_canvas_history_get_noinc_nolock(pb->ch);
}

bool DP_playback_local_state_dirty(DP_Playback *pb)
{
    DP_ASSERT(pb);
    return pb->local_state_dirty;
}

void DP_playback_local_state_dirty_set(DP_Playback *pb, bool local_state_dirty)
{
    DP_ASSERT(pb);
    pb->local_state_dirty = local_state_dirty;
}

void DP_playback_canvas_history_reset(DP_Playback *pb)
{
    DP_ASSERT(pb);
    DP_canvas_history_reset(pb->ch);
}

void DP_playback_canvas_history_soft_reset(DP_Playback *pb)
{
    DP_ASSERT(pb);
    DP_canvas_history_soft_reset(pb->ch, pb->dc, 0u, NULL, NULL);
}

void DP_playback_canvas_history_reset_to_state_noinc(DP_Playback *pb,
                                                     DP_CanvasState *cs)
{
    DP_ASSERT(pb);
    DP_canvas_history_reset_to_state_noinc(pb->ch, cs);
}

long long DP_playback_canvas_history_project_player_snapshot(
    DP_Playback *pb, DP_Project *prj, long long session_id,
    long long sequence_id, double recorded_at)
{
    DP_ASSERT(pb);
    DP_ASSERT(prj);
    return DP_canvas_history_project_player_snapshot(
        pb->ch, prj, pb->ls, session_id, sequence_id, recorded_at);
}

static void playback_handle_single_dec(DP_Playback *pb, DP_Message *msg)
{
    if (!DP_canvas_history_handle(pb->ch, pb->dc, msg)) {
        DP_warn("Error playing back message: %s", DP_error());
    }
    DP_message_decref(msg);
}

void DP_playback_flush_multidab(DP_Playback *pb)
{
    int count = pb->multidab_count;
    switch (count) {
    case 0:
        break;
    case 1:
        pb->multidab_count = 0;
        playback_handle_single_dec(pb, pb->multidab_msgs[0]);
        break;
    default:
        pb->multidab_count = 0;
        DP_canvas_history_handle_multidab_dec(pb->ch, pb->dc, count,
                                              pb->multidab_msgs);
        break;
    }
}

void DP_playback_clear_multidab(DP_Playback *pb)
{
    int count = pb->multidab_count;
    pb->multidab_count = 0;
    for (int i = 0; i < count; ++i) {
        DP_message_decref(pb->multidab_msgs[i]);
    }
}

static void playback_handle_command(DP_Playback *pb, DP_Message *msg)
{
    DP_MessageType type = DP_message_type(msg);
    if (DP_message_type_is_draw_dabs(type)) {
        int index = pb->multidab_count++;
        pb->multidab_msgs[index] = msg;
        if (index == MAX_MULTIDAB_COUNT - 1) {
            DP_playback_flush_multidab(pb);
        }
    }
    else {
        DP_playback_flush_multidab(pb);
        if (DP_local_state_handle(pb->ls, pb->dc, msg, false)) {
            pb->local_state_dirty = true;
        }
        playback_handle_single_dec(pb, msg);
    }
}

static void playback_handle_soft_reset(DP_Playback *pb, DP_Message *msg)
{
    DP_playback_flush_multidab(pb);
    DP_canvas_history_soft_reset(pb->ch, pb->dc, DP_message_context_id(msg),
                                 NULL, 0);
    DP_message_decref(msg);
}

static void playback_handle_undo_depth(DP_Playback *pb, DP_Message *msg)
{
    DP_playback_flush_multidab(pb);
    DP_MsgUndoDepth *mud = DP_message_internal(msg);
    DP_canvas_history_undo_depth_limit_set(pb->ch, pb->dc,
                                           DP_msg_undo_depth_depth(mud));
    DP_canvas_history_soft_reset(pb->ch, pb->dc, DP_message_context_id(msg),
                                 NULL, 0);
    DP_message_decref(msg);
}

static void playback_handle_local_change(DP_Playback *pb, DP_Message *msg)
{
    if (DP_local_state_handle(pb->ls, pb->dc, msg, false)) {
        pb->local_state_dirty = true;
    }
    DP_message_decref(msg);
}

DP_PlaybackHandleFn DP_playback_get_handle_fn(int type)
{
    switch (type) {
    case DP_MSG_SOFT_RESET:
        return playback_handle_soft_reset;
    case DP_MSG_UNDO_DEPTH:
        return playback_handle_undo_depth;
    case DP_MSG_LOCAL_CHANGE:
        return playback_handle_local_change;
    default:
        if (DP_message_type_command((DP_MessageType)type)) {
            return playback_handle_command;
        }
        else {
            return NULL;
        }
    }
}

void DP_playback_handle_message_dec(DP_Playback *pb, DP_Message *msg,
                                    DP_PlaybackHandleFn fn)
{
    DP_ASSERT(pb);
    DP_ASSERT(msg);
    DP_ASSERT(fn);
    fn(pb, msg);
}

bool DP_playback_local_state_get_reset(DP_Playback *pb,
                                       bool (*fn)(void *, DP_Message *),
                                       void *user)
{
    DP_ASSERT(pb);
    DP_ASSERT(fn);
    if (pb->local_state_dirty) {
        pb->local_state_dirty = false;
        DP_local_state_playback_image_build(pb->ls, pb->dc, fn, user);
        return true;
    }
    else {
        return false;
    }
}
