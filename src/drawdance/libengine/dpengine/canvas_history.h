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
#ifndef DPENGINE_CANVAS_HISTORY_H
#define DPENGINE_CANVAS_HISTORY_H
#include "affected_area.h"
#include "canvas_state.h"
#include "recorder.h"
#include <dpcommon/common.h>

typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Message DP_Message;
typedef struct json_value_t JSON_Value;


#define DP_CANVAS_HISTORY_UNDO_DEPTH_MIN 3
#define DP_CANVAS_HISTORY_UNDO_DEPTH_MAX 255

#define DP_USER_CURSOR_COUNT 256

typedef struct DP_CanvasHistory DP_CanvasHistory;

typedef struct DP_CanvasHistorySnapshot DP_CanvasHistorySnapshot;

typedef enum DP_Undo {
    DP_UNDO_DONE,
    DP_UNDO_UNDONE,
    DP_UNDO_GONE,
} DP_Undo;

typedef struct DP_CanvasHistoryEntry {
    DP_Undo undo;
    DP_Message *msg;
    DP_CanvasState *state;
} DP_CanvasHistoryEntry;

typedef struct DP_ForkEntry {
    DP_Message *msg;
    DP_AffectedArea aa;
} DP_ForkEntry;

typedef struct DP_UserCursorBuffer {
    int count;
    DP_UserCursor cursors[DP_USER_CURSOR_COUNT];
} DP_UserCursorBuffer;

typedef void (*DP_CanvasHistorySavePointFn)(void *user, DP_CanvasState *cs,
                                            bool snapshot_requested);
typedef bool (*DP_CanvasHistoryAcceptResetStateFn)(void *user,
                                                   DP_CanvasState *cs);
typedef bool (*DP_CanvasHistoryAcceptResetMessageFn)(void *user,
                                                     DP_Message *msg);

typedef enum DP_DumpType {
    DP_DUMP_REMOTE_MESSAGE,
    DP_DUMP_REMOTE_MESSAGE_LOCAL_DRAWING_IN_PROGRESS,
    DP_DUMP_LOCAL_MESSAGE,
    DP_DUMP_REMOTE_MULTIDAB,
    DP_DUMP_REMOTE_MULTIDAB_LOCAL_DRAWING_IN_PROGRESS,
    DP_DUMP_LOCAL_MULTIDAB,
    DP_DUMP_RESET,
    DP_DUMP_SOFT_RESET,
    DP_DUMP_CLEANUP,
    DP_DUMP_UNDO_DEPTH_LIMIT,
} DP_DumpType;

DP_CanvasHistory *
DP_canvas_history_new(DP_CanvasHistorySavePointFn save_point_fn,
                      void *save_point_user, bool want_dump,
                      const char *dump_dir);

DP_CanvasHistory *DP_canvas_history_new_inc(
    DP_CanvasState *cs_or_null, DP_CanvasHistorySavePointFn save_point_fn,
    void *save_point_user, bool want_dump, const char *dump_dir);

void DP_canvas_history_free(DP_CanvasHistory *ch);

void DP_canvas_history_local_drawing_in_progress_set(
    DP_CanvasHistory *ch, bool local_drawing_in_progress);

bool DP_canvas_history_want_dump(DP_CanvasHistory *ch);

void DP_canvas_history_want_dump_set(DP_CanvasHistory *ch, bool want_dump);

DP_CanvasState *DP_canvas_history_get(DP_CanvasHistory *ch);

DP_CanvasState *
DP_canvas_history_compare_and_get(DP_CanvasHistory *ch, DP_CanvasState *prev,
                                  DP_UserCursorBuffer *out_user_cursors);

void DP_canvas_history_reset(DP_CanvasHistory *ch);

void DP_canvas_history_reset_to_state_noinc(DP_CanvasHistory *ch,
                                            DP_CanvasState *cs);

void DP_canvas_history_soft_reset(DP_CanvasHistory *ch);

int DP_canvas_history_undo_depth_limit(DP_CanvasHistory *ch);

void DP_canvas_history_undo_depth_limit_set(DP_CanvasHistory *ch,
                                            int undo_depth_limit);

bool DP_canvas_history_save_point_make(DP_CanvasHistory *ch);

// Cleans up after disconnecting from a remote session: the local fork is merged
// into the mainline history and all sublayers are merged into their parents.
// The messages are appended to the remote queue so they can be recorded.
void DP_canvas_history_cleanup(DP_CanvasHistory *ch, DP_DrawContext *dc,
                               void (*push_message)(void *, DP_Message *),
                               void *user);

bool DP_canvas_history_local_fork_clear(DP_CanvasHistory *ch,
                                        DP_DrawContext *dc);

bool DP_canvas_history_handle(DP_CanvasHistory *ch, DP_DrawContext *dc,
                              DP_Message *msg);

bool DP_canvas_history_handle_local(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                    DP_Message *msg);

void DP_canvas_history_handle_multidab_dec(DP_CanvasHistory *ch,
                                           DP_DrawContext *dc, int count,
                                           DP_Message **msgs);

void DP_canvas_history_handle_local_multidab_dec(DP_CanvasHistory *ch,
                                                 DP_DrawContext *dc, int count,
                                                 DP_Message **msgs);

bool DP_canvas_history_reset_image_new(
    DP_CanvasHistory *ch, DP_CanvasHistoryAcceptResetStateFn accept_state,
    DP_CanvasHistoryAcceptResetMessageFn accept_message, void *user);

// May return NULL if something goes wrong. Takes ownership of the output and
// header, so no matter the return value, the caller must not free it.
DP_Recorder *DP_canvas_history_recorder_new(
    DP_CanvasHistory *ch, DP_RecorderType type, JSON_Value *header,
    DP_RecorderGetTimeMsFn get_time_fn, void *get_time_user, DP_Output *output);


DP_CanvasHistorySnapshot *DP_canvas_history_snapshot_new(DP_CanvasHistory *ch);

DP_CanvasHistorySnapshot *
DP_canvas_history_snapshot_incref(DP_CanvasHistorySnapshot *chs);

DP_CanvasHistorySnapshot *DP_canvas_history_snapshot_incref_nullable(
    DP_CanvasHistorySnapshot *chs_or_null);

void DP_canvas_history_snapshot_decref(DP_CanvasHistorySnapshot *chs);

void DP_canvas_history_snapshot_decref_nullable(
    DP_CanvasHistorySnapshot *chs_or_null);

int DP_canvas_history_snapshot_refs(DP_CanvasHistorySnapshot *chs);

int DP_canvas_history_snapshot_history_undo_depth_limit(
    DP_CanvasHistorySnapshot *chs);

int DP_canvas_history_snapshot_history_offset(DP_CanvasHistorySnapshot *chs);

int DP_canvas_history_snapshot_history_count(DP_CanvasHistorySnapshot *chs);

const DP_CanvasHistoryEntry *
DP_canvas_history_snapshot_history_entry_at(DP_CanvasHistorySnapshot *chs,
                                            int index);

int DP_canvas_history_snapshot_fork_start(DP_CanvasHistorySnapshot *chs);

int DP_canvas_history_snapshot_fork_fallbehind(DP_CanvasHistorySnapshot *chs);

int DP_canvas_history_snapshot_fork_count(DP_CanvasHistorySnapshot *chs);

const DP_ForkEntry *
DP_canvas_history_snapshot_fork_entry_at(DP_CanvasHistorySnapshot *chs,
                                         int index);


#endif
