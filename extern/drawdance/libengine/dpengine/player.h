/*
 * Copyright (C) 2023 askmeaboutloom
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
#ifndef DPMSG_PLAYER_H
#define DPMSG_PLAYER_H
#include "canvas_history.h"
#include "load.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_Input DP_Input;
typedef struct DP_Message DP_Message;
typedef struct json_object_t JSON_Object;


typedef struct DP_Player DP_Player;

typedef enum DP_PlayerType {
    DP_PLAYER_TYPE_GUESS,
    DP_PLAYER_TYPE_BINARY,
    DP_PLAYER_TYPE_TEXT,
    DP_PLAYER_TYPE_DEBUG_DUMP,
} DP_PlayerType;

typedef enum DP_PlayerResult {
    DP_PLAYER_SUCCESS,
    DP_PLAYER_RECORDING_END,
    DP_PLAYER_ERROR_PARSE,
    DP_PLAYER_ERROR_INPUT,
    DP_PLAYER_ERROR_OPERATION,
} DP_PlayerResult;

typedef struct DP_PlayerIndexEntry {
    long long message_index;
    size_t message_offset;
    size_t snapshot_offset;
    size_t thumbnail_offset;
} DP_PlayerIndexEntry;

typedef struct DP_PlayerIndexEntrySnapshot DP_PlayerIndexEntrySnapshot;

typedef bool (*DP_PlayerIndexShouldSnapshotFn)(void *user);
typedef void (*DP_PlayerIndexProgressFn)(void *user, int percent);


DP_Player *DP_player_new(DP_PlayerType type, const char *path_or_null,
                         DP_Input *input, DP_LoadResult *out_result);

void DP_player_free(DP_Player *player);

JSON_Object *DP_player_header(DP_Player *player);

bool DP_player_compatible(DP_Player *player);

bool DP_player_index_loaded(DP_Player *player);

size_t DP_player_tell(DP_Player *player);

double DP_player_progress(DP_Player *player);

long long DP_player_position(DP_Player *player);


DP_PlayerResult DP_player_step(DP_Player *player, DP_Message **out_msg);

DP_PlayerResult DP_player_step_dump(DP_Player *player, DP_DumpType *out_type,
                                    int *out_count, DP_Message ***out_msgs);

bool DP_player_seek(DP_Player *player, long long position, size_t offset);

bool DP_player_seek_dump(DP_Player *player, long long position);


bool DP_player_index_build(DP_Player *player, DP_DrawContext *dc,
                           DP_PlayerIndexShouldSnapshotFn should_snapshot_fn,
                           DP_PlayerIndexProgressFn progress_fn, void *user);


bool DP_player_index_load(DP_Player *player);

unsigned int DP_player_index_message_count(DP_Player *player);

size_t DP_player_index_entry_count(DP_Player *player);

DP_PlayerIndexEntry
DP_player_index_entry_search(DP_Player *player, long long position, bool after);

DP_PlayerIndexEntrySnapshot *
DP_player_index_entry_load(DP_Player *player, DP_DrawContext *dc,
                           DP_PlayerIndexEntry entry);

DP_CanvasState *DP_player_index_entry_snapshot_canvas_state_inc(
    DP_PlayerIndexEntrySnapshot *snapshot);

int DP_player_index_entry_snapshot_message_count(
    DP_PlayerIndexEntrySnapshot *snapshot);

DP_Message *DP_player_index_entry_snapshot_message_at_inc(
    DP_PlayerIndexEntrySnapshot *snapshot, int i);

void DP_player_index_entry_snapshot_free(DP_PlayerIndexEntrySnapshot *snapshot);

DP_Image *DP_player_index_thumbnail_at(DP_Player *player, size_t index,
                                       bool *out_error);


#endif
