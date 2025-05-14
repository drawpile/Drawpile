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
#include "load_enums.h"
#include <dpcommon/common.h>
#include <dpcommon/input.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
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

typedef enum DP_PlayerCompatibility {
    DP_PLAYER_COMPATIBLE,
    DP_PLAYER_MINOR_INCOMPATIBILITY,
    DP_PLAYER_BACKWARD_COMPATIBLE,
    DP_PLAYER_INCOMPATIBLE,
} DP_PlayerCompatibility;

typedef enum DP_PlayerPass {
    // Client playback, doesn't pass any server or control messages through, as
    // well as excluding any messages that manipulate the ACL state.
    DP_PLAYER_PASS_CLIENT_PLAYBACK,
    // Also passes through feature access messages, for e.g. session templates.
    DP_PLAYER_PASS_FEATURE_ACCESS,
    // Passes everything through.
    DP_PLAYER_PASS_ALL,
} DP_PlayerPass;

typedef struct DP_PlayerIndexEntry {
    long long message_index;
    size_t message_offset;
    size_t snapshot_offset;
    size_t thumbnail_offset;
} DP_PlayerIndexEntry;

typedef struct DP_PlayerIndex {
    DP_BufferedInput input;
    unsigned int message_count;
    DP_PlayerIndexEntry *entries;
    size_t entry_count;
} DP_PlayerIndex;


DP_Player *DP_player_new(DP_PlayerType type, const char *path_or_null,
                         DP_Input *input, DP_LoadResult *out_result);

void DP_player_free(DP_Player *player);

DP_PlayerType DP_player_type(DP_Player *player);

JSON_Value *DP_player_header(DP_Player *player);

DP_PlayerCompatibility DP_player_compatibility(DP_Player *player);

bool DP_player_compatible(DP_Player *player);

void DP_player_acl_override_set(DP_Player *player, bool override);

void DP_player_pass_set(DP_Player *player, DP_PlayerPass pass);

const char *DP_player_recording_path(DP_Player *player);

const char *DP_player_index_path(DP_Player *player);

bool DP_player_index_loaded(DP_Player *player);

DP_PlayerIndex *DP_player_index(DP_Player *player);

void DP_player_index_set(DP_Player *player, DP_PlayerIndex index);

size_t DP_player_tell(DP_Player *player);

double DP_player_progress(DP_Player *player);

long long DP_player_position(DP_Player *player);


DP_PlayerResult DP_player_step(DP_Player *player, DP_Message **out_msg);

DP_PlayerResult DP_player_step_dump(DP_Player *player, DP_DumpType *out_type,
                                    int *out_count, DP_Message ***out_msgs);

bool DP_player_seek(DP_Player *player, long long position, size_t offset);

size_t DP_player_body_offset(DP_Player *player);

bool DP_player_rewind(DP_Player *player);

bool DP_player_seek_dump(DP_Player *player, long long position);


#endif
