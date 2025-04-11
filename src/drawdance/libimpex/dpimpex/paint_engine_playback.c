// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_PAINT_ENGINE_PLAYBACK_H
#define DPIMPEX_PAINT_ENGINE_PLAYBACK_H
#include "paint_engine_playback.h"
#include "player_index.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpengine/paint_engine.h>
#include <dpmsg/message.h>
#include <dpmsg/msg_internal.h>


#define PLAYBACK_STEP_MESSAGES    0
#define PLAYBACK_STEP_UNDO_POINTS 1
#define PLAYBACK_STEP_MSECS       2


static void push_playback(DP_PaintEnginePushMessageFn push_message, void *user,
                          long long position)
{
    push_message(user, DP_msg_internal_playback_new(0, position));
}

static long long guess_message_msecs(DP_Message *msg, DP_MessageType type,
                                     bool *out_next_has_time)
{
    // The recording format doesn't contain proper timing information, so we
    // just make some wild guesses as to how long each message takes to try to
    // get some vaguely okayly timed playback out of it.
    switch (type) {
    case DP_MSG_DRAW_DABS_CLASSIC:
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
    case DP_MSG_DRAW_DABS_MYPAINT:
    case DP_MSG_DRAW_DABS_MYPAINT_BLEND:
        return 5;
    case DP_MSG_PUT_IMAGE:
    case DP_MSG_MOVE_POINTER:
        return 10;
    case DP_MSG_LAYER_ATTRIBUTES:
    case DP_MSG_LAYER_RETITLE:
    case DP_MSG_ANNOTATION_RESHAPE:
    case DP_MSG_ANNOTATION_EDIT:
        return 20;
    case DP_MSG_CANVAS_RESIZE:
        return 60;
    case DP_MSG_UNDO:
        // An undo for user 0 means that the next message is actually an undone
        // entry, which happens at the start of recordings. We treat those
        // messages as having a length of 0, since they just get appended to the
        // history and aren't visible.
        if (DP_message_context_id(msg) == 0
            && DP_msg_undo_override_user(DP_message_internal(msg)) == 0) {
            *out_next_has_time = false;
            return 0;
        }
        else {
            return 20;
        }
    default:
        return 0;
    }
}

static DP_PlayerResult
skip_playback_forward(DP_PaintEngine *pe, long long steps, int what,
                      DP_PaintEngineFilterMessageFn filter_message_or_null,
                      DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(steps >= 0);
    DP_ASSERT(push_message);
    DP_ASSERT(what == PLAYBACK_STEP_MESSAGES
              || what == PLAYBACK_STEP_UNDO_POINTS
              || what == PLAYBACK_STEP_MSECS);
    DP_debug("Skip playback forward by %lld %s", steps,
             what == PLAYBACK_STEP_MESSAGES      ? "step(s)"
             : what == PLAYBACK_STEP_UNDO_POINTS ? "undo point(s)"
                                                 : "millisecond(s)");

    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (!player) {
        DP_error_set("No player set");
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    long long done = 0;
    if (what == PLAYBACK_STEP_MSECS) {
        done += DP_paint_engine_playback(pe)->msecs;
    }

    while (done < steps) {
        DP_Message *msg;
        DP_PlayerResult result = DP_player_step(player, &msg);
        if (result == DP_PLAYER_SUCCESS) {
            bool should_time = true;
            if (filter_message_or_null) {
                unsigned int flags = filter_message_or_null(user, msg);
                if (flags & DP_PAINT_ENGINE_FILTER_MESSAGE_FLAG_NO_TIME) {
                    should_time = false;
                }
            }

            DP_MessageType type = DP_message_type(msg);
            if (type == DP_MSG_INTERVAL) {
                if (what == PLAYBACK_STEP_MESSAGES) {
                    ++done;
                }
                else if (what == PLAYBACK_STEP_MSECS && should_time) {
                    DP_MsgInterval *mi = DP_message_internal(msg);
                    // Really no point in delaying playback for ages, it just
                    // makes the user wonder if there's something broken.
                    done += DP_min_int(1000, DP_msg_interval_msecs(mi));
                }
                DP_message_decref(msg);
            }
            else {
                if (what == PLAYBACK_STEP_MESSAGES
                    || (what == PLAYBACK_STEP_UNDO_POINTS
                        && type == DP_MSG_UNDO_POINT)) {
                    ++done;
                }
                else if (what == PLAYBACK_STEP_MSECS) {
                    if (DP_paint_engine_playback(pe)->next_has_time) {
                        long long msecs = guess_message_msecs(
                            msg, type,
                            &DP_paint_engine_playback(pe)->next_has_time);
                        if (should_time) {
                            done += msecs;
                        }
                    }
                    else {
                        DP_paint_engine_playback(pe)->next_has_time = true;
                    }
                }
                push_message(user, msg);
            }
        }
        else if (result == DP_PLAYER_ERROR_PARSE) {
            if (what == PLAYBACK_STEP_MESSAGES) {
                ++done;
            }
            DP_warn("Can't play back message: %s", DP_error());
        }
        else {
            // We're either at the end of the recording or encountered an input
            // error. In either case, we're done playing this recording, report
            // a negative value as the position to indicate that.
            push_playback(push_message, user, -1);
            return result;
        }
    }

    if (what == PLAYBACK_STEP_MSECS) {
        DP_paint_engine_playback(pe)->msecs = done - steps;
    }

    // If we don't have an index, report the position as a percentage
    // completion based on the amount of bytes read from the recording.
    long long position =
        DP_player_index_loaded(player)
            ? DP_player_position(player)
            : DP_double_to_llong(DP_player_progress(player) * 100.0 + 0.5);
    push_playback(push_message, user, position);
    return DP_PLAYER_SUCCESS;
}

DP_PlayerResult
DP_paint_engine_playback_step(DP_PaintEngine *pe, long long steps,
                              DP_PaintEnginePushMessageFn push_message,
                              void *user)
{
    DP_ASSERT(pe);
    DP_ASSERT(steps >= 0);
    DP_ASSERT(push_message);
    return skip_playback_forward(pe, steps, PLAYBACK_STEP_MESSAGES, NULL,
                                 push_message, user);
}

static DP_PlayerResult rewind_playback(DP_PaintEngine *pe,
                                       DP_PaintEnginePushMessageFn push_message,
                                       void *user)
{
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (!player) {
        DP_error_set("No player set");
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    if (DP_player_rewind(player)) {
        push_message(user, DP_msg_internal_reset_new(0));
        push_playback(push_message, user, 0);
        return DP_PLAYER_SUCCESS;
    }
    else {
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_INPUT;
    }
}

static DP_PlayerResult
jump_playback_to(DP_PaintEngine *pe, DP_DrawContext *dc, long long to,
                 bool relative, bool exact,
                 DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (!player) {
        DP_error_set("No player set");
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    if (!DP_player_index_loaded(player)) {
        DP_error_set("No index loaded");
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    long long player_position = DP_player_position(player);
    long long target_position = relative ? player_position + to : to;
    DP_debug("Jump playback from %lld to %s %lld", player_position,
             exact ? "exactly" : "snapshot nearest", target_position);

    long long message_count =
        DP_uint_to_llong(DP_player_index_message_count(player));
    if (message_count == 0) {
        DP_error_set("Recording contains no messages");
        push_playback(push_message, user, player_position);
        return DP_PLAYER_ERROR_INPUT;
    }
    else if (target_position < 0) {
        target_position = 0;
    }
    else if (target_position >= message_count) {
        target_position = message_count - 1;
    }
    DP_debug("Clamped position to %lld (message count %lld)", target_position,
             message_count);

    DP_PlayerIndexEntry entry = DP_player_index_entry_search(
        player, target_position, relative && !exact && to > 0);
    DP_debug("Loaded entry with message index %lld, message offset %zu, "
             "snapshot offset %zu, thumbnail offset %zu",
             entry.message_index, entry.message_offset, entry.snapshot_offset,
             entry.thumbnail_offset);

    bool inside_snapshot = player_position >= entry.message_index
                        && player_position < target_position;
    if (inside_snapshot) {
        long long steps = target_position - player_position;
        DP_debug("Already inside snapshot, stepping forward by %lld", steps);
        DP_PlayerResult result = skip_playback_forward(
            pe, steps, PLAYBACK_STEP_MESSAGES, NULL, push_message, user);
        // If skipping playback forward doesn't work, e.g. because we're
        // in an input error state, punt to reloading the snapshot.
        if (result == DP_PLAYER_SUCCESS) {
            return result;
        }
        else {
            DP_warn("Skipping inside snapshot failed with result %d: %s",
                    (int)result, DP_error());
        }
    }

    DP_PlayerIndexEntrySnapshot *snapshot =
        DP_player_index_entry_load(player, dc, entry);
    if (!snapshot) {
        DP_debug("Reading snapshot failed: %s", DP_error());
        push_playback(push_message, user, player_position);
        return DP_PLAYER_ERROR_INPUT;
    }

    if (!DP_player_seek(player, entry.message_index, entry.message_offset)) {
        DP_player_index_entry_snapshot_free(snapshot);
        push_playback(push_message, user, player_position);
        return DP_PLAYER_ERROR_INPUT;
    }

    DP_CanvasState *cs =
        DP_player_index_entry_snapshot_canvas_state_inc(snapshot);
    push_message(user, DP_msg_internal_reset_to_state_new(0, cs));

    int count = DP_player_index_entry_snapshot_message_count(snapshot);
    for (int i = 0; i < count; ++i) {
        DP_Message *msg =
            DP_player_index_entry_snapshot_message_at_inc(snapshot, i);
        if (msg) {
            push_message(user, msg);
        }
    }

    DP_player_index_entry_snapshot_free(snapshot);
    return skip_playback_forward(
        pe, exact ? target_position - entry.message_index : 0,
        PLAYBACK_STEP_MESSAGES, NULL, push_message, user);
}

DP_PlayerResult DP_paint_engine_playback_skip_by(
    DP_PaintEngine *pe, DP_DrawContext *dc, long long steps, bool by_snapshots,
    DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    if (steps < 0 || by_snapshots) {
        return jump_playback_to(pe, dc, steps, true, !by_snapshots,
                                push_message, user);
    }
    else {
        return skip_playback_forward(pe, steps, PLAYBACK_STEP_UNDO_POINTS, NULL,
                                     push_message, user);
    }
}

DP_PlayerResult DP_paint_engine_playback_jump_to(
    DP_PaintEngine *pe, DP_DrawContext *dc, long long position,
    DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    if (position <= 0) {
        return rewind_playback(pe, push_message, user);
    }
    else {
        return jump_playback_to(pe, dc, position, false, true, push_message,
                                user);
    }
}

DP_PlayerResult DP_paint_engine_playback_begin(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (player) {
        DP_paint_engine_playback(pe)->msecs = 0;
        return DP_PLAYER_SUCCESS;
    }
    else {
        DP_error_set("No player set");
        return DP_PLAYER_ERROR_OPERATION;
    }
}

DP_PlayerResult DP_paint_engine_playback_play(
    DP_PaintEngine *pe, long long msecs,
    DP_PaintEngineFilterMessageFn filter_message_or_null,
    DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    return skip_playback_forward(pe, msecs, PLAYBACK_STEP_MSECS,
                                 filter_message_or_null, push_message, user);
}

bool DP_paint_engine_playback_index_build(
    DP_PaintEngine *pe, DP_DrawContext *dc,
    DP_PlayerIndexShouldSnapshotFn should_snapshot_fn,
    DP_PlayerIndexProgressFn progress_fn, void *user)
{
    DP_ASSERT(pe);
    DP_ASSERT(dc);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (player) {
        return DP_player_index_build(player, dc, should_snapshot_fn,
                                     progress_fn, user);
    }
    else {
        DP_error_set("No player set");
        return false;
    }
}

bool DP_paint_engine_playback_index_load(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (player) {
        return DP_player_index_load(player);
    }
    else {
        DP_error_set("No player set");
        return false;
    }
}

unsigned int DP_paint_engine_playback_index_message_count(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (player) {
        return DP_player_index_message_count(player);
    }
    else {
        DP_error_set("No player set");
        return 0;
    }
}

size_t DP_paint_engine_playback_index_entry_count(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (player) {
        return DP_player_index_entry_count(player);
    }
    else {
        DP_error_set("No player set");
        return 0;
    }
}

DP_Image *DP_paint_engine_playback_index_thumbnail_at(DP_PaintEngine *pe,
                                                      size_t index,
                                                      bool *out_error)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (player) {
        return DP_player_index_thumbnail_at(player, index, out_error);
    }
    else {
        DP_error_set("No player set");
        if (out_error) {
            *out_error = true;
        }
        return NULL;
    }
}

static DP_PlayerResult step_dump(DP_Player *player,
                                 DP_PaintEnginePushMessageFn push_message,
                                 void *user)
{
    DP_DumpType type;
    int count;
    DP_Message **msgs;
    DP_PlayerResult result = DP_player_step_dump(player, &type, &count, &msgs);
    if (result == DP_PLAYER_SUCCESS) {
        push_message(user, DP_msg_internal_dump_command_new_inc(0, (int)type,
                                                                count, msgs));
    }
    return result;
}

static void push_dump_playback(DP_PaintEnginePushMessageFn push_message,
                               void *user, long long position)
{
    push_message(user, DP_msg_internal_dump_playback_new(0, position));
}

DP_PlayerResult DP_paint_engine_playback_dump_step(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (!player) {
        DP_error_set("No player set");
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    DP_PlayerResult result = step_dump(player, push_message, user);
    long long position_to_report =
        result == DP_PLAYER_SUCCESS ? DP_player_position(player) : -1;
    push_dump_playback(push_message, user, position_to_report);
    return result;
}

DP_PlayerResult DP_paint_engine_playback_dump_jump_previous_reset(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (!player) {
        DP_error_set("No player set");
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    if (!DP_player_seek_dump(player, DP_player_position(player) - 1)) {
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_INPUT;
    }

    push_message(user, DP_msg_internal_reset_new(0));
    push_dump_playback(push_message, user, DP_player_position(player));
    return DP_PLAYER_SUCCESS;
}

static int step_toward_next_reset(DP_Player *player, bool stop_at_reset,
                                  DP_PaintEnginePushMessageFn push_message,
                                  void *user)
{
    long long position = DP_player_position(player);
    size_t offset = DP_player_tell(player);
    DP_DumpType type;
    int count;
    DP_Message **msgs;
    DP_PlayerResult result = DP_player_step_dump(player, &type, &count, &msgs);
    if (result == DP_PLAYER_SUCCESS) {
        if (type == DP_DUMP_RESET && stop_at_reset) {
            if (DP_player_seek(player, position, offset)) {
                return -1;
            }
            else {
                return DP_PLAYER_ERROR_INPUT;
            }
        }
        else {
            push_message(user, DP_msg_internal_dump_command_new_inc(
                                   0, (int)type, count, msgs));
        }
    }
    return (int)result;
}

DP_PlayerResult DP_paint_engine_playback_dump_jump_next_reset(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (!player) {
        DP_error_set("No player set");
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    // We want to jump up to, but excluding the next reset. So we keep walking
    // through the dump until we hit a reset and then seek to before it. If we
    // hit a reset as our very first message, we push through it, since
    // otherwise we'd be doing nothing at all until manually stepping forward.
    int result = step_toward_next_reset(player, false, push_message, user);
    while (result == DP_PLAYER_SUCCESS) {
        result = step_toward_next_reset(player, true, push_message, user);
    }

    push_dump_playback(push_message, user, DP_player_position(player));
    return result == -1 ? DP_PLAYER_SUCCESS : (DP_PlayerResult)result;
}

DP_PlayerResult
DP_paint_engine_playback_dump_jump(DP_PaintEngine *pe, long long position,
                                   DP_PaintEnginePushMessageFn push_message,
                                   void *user)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (!player) {
        DP_error_set("No player set");
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    if (!DP_player_seek_dump(player, position)) {
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_INPUT;
    }

    if (DP_player_position(player) == 0) {
        push_message(user, DP_msg_internal_reset_new(0));
    }

    DP_PlayerResult result = DP_PLAYER_SUCCESS;
    while (DP_player_position(player) < position) {
        result = step_dump(player, push_message, user);
        if (result != DP_PLAYER_SUCCESS) {
            break;
        }
    }
    push_dump_playback(push_message, user, DP_player_position(player));
    return result;
}

bool DP_paint_engine_playback_flush(DP_PaintEngine *pe,
                                    DP_PaintEnginePushMessageFn push_message,
                                    void *user)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (player) {
        while (true) {
            DP_Message *msg;
            DP_PlayerResult result = DP_player_step(player, &msg);
            if (result == DP_PLAYER_SUCCESS) {
                DP_MessageType type = DP_message_type(msg);
                if (type == DP_MSG_INTERVAL) {
                    DP_message_decref(msg);
                }
                else {
                    push_message(user, msg);
                }
            }
            else if (result == DP_PLAYER_ERROR_PARSE) {
                DP_warn("Can't play back message: %s", DP_error());
            }
            else if (result == DP_PLAYER_RECORDING_END) {
                return true;
            }
            else {
                return false; // Some kind of input error.
            }
        }
    }
    else {
        return false;
    }
}

bool DP_paint_engine_playback_close(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = DP_paint_engine_playback(pe)->player;
    if (player) {
        DP_player_free(player);
        DP_paint_engine_playback(pe)->player = NULL;
        return true;
    }
    else {
        return false;
    }
}


#endif
