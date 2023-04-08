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
#ifndef DPMSG_MSG_INTERNAL_H
#define DPMSG_MSG_INTERNAL_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;

typedef enum DP_MsgInternalType {
    DP_MSG_INTERNAL_TYPE_RESET,
    DP_MSG_INTERNAL_TYPE_SOFT_RESET,
    DP_MSG_INTERNAL_TYPE_RESET_TO_STATE,
    DP_MSG_INTERNAL_TYPE_SNAPSHOT,
    DP_MSG_INTERNAL_TYPE_CATCHUP,
    DP_MSG_INTERNAL_TYPE_CLEANUP,
    DP_MSG_INTERNAL_TYPE_PREVIEW,
    DP_MSG_INTERNAL_TYPE_RECORDER_START,
    DP_MSG_INTERNAL_TYPE_PLAYBACK,
    DP_MSG_INTERNAL_TYPE_DUMP_PLAYBACK,
    DP_MSG_INTERNAL_TYPE_DUMP_COMMAND,
    DP_MSG_INTERNAL_TYPE_COUNT,
} DP_MsgInternalType;


typedef struct DP_MsgInternal DP_MsgInternal;

DP_Message *DP_msg_internal_reset_new(unsigned int context_id);

DP_Message *DP_msg_internal_soft_reset_new(unsigned int context_id);

DP_Message *DP_msg_internal_reset_to_state_new(unsigned int context_id,
                                               void *data);

DP_Message *DP_msg_internal_snapshot_new(unsigned int context_id);

DP_Message *DP_msg_internal_catchup_new(unsigned int context_id, int progress);

DP_Message *DP_msg_internal_cleanup_new(unsigned int context_id);

DP_Message *DP_msg_internal_preview_new(unsigned int context_id, void *data);

DP_Message *DP_msg_internal_recorder_start_new(unsigned int context_id);

DP_Message *DP_msg_internal_playback_new(unsigned int context_id,
                                         long long position, int interval);

DP_Message *DP_msg_internal_dump_playback_new(unsigned int context_id,
                                              long long position);

DP_Message *DP_msg_internal_dump_command_new_inc(unsigned int context_id,
                                                 int type, int count,
                                                 DP_Message **messages);

DP_MsgInternal *DP_msg_internal_cast(DP_Message *msg);


DP_MsgInternalType DP_msg_internal_type(DP_MsgInternal *mi);

void *DP_msg_internal_reset_to_state_data(DP_MsgInternal *mi);

int DP_msg_internal_catchup_progress(DP_MsgInternal *mi);

void *DP_msg_internal_preview_data(DP_MsgInternal *mi);

long long DP_msg_internal_playback_position(DP_MsgInternal *mi);

int DP_msg_internal_playback_interval(DP_MsgInternal *mi);

long long DP_msg_internal_dump_playback_position(DP_MsgInternal *mi);

int DP_msg_internal_dump_command_type(DP_MsgInternal *mi);

DP_Message **DP_msg_internal_dump_command_messages(DP_MsgInternal *mi,
                                                   int *out_count);


#endif
