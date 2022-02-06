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
#ifndef DPMSG_UNDO_H
#define DPMSG_UNDO_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_MsgUndo DP_MsgUndo;

DP_Message *DP_msg_undo_new(unsigned int context_id, unsigned int override_id,
                            bool is_redo);

DP_Message *DP_msg_undo_deserialize(unsigned int context_id,
                                    const unsigned char *buffer, size_t length);

DP_MsgUndo *DP_msg_undo_cast(DP_Message *msg);

const char *DP_msg_undo_message_name(DP_Message *msg);


unsigned int DP_msg_undo_override_id(DP_MsgUndo *mu);

bool DP_msg_undo_is_redo(DP_MsgUndo *mu);


#endif
