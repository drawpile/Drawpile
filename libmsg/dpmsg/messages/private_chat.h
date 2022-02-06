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
#ifndef DPMSG_PRIVATE_CHAT_H
#define DPMSG_PRIVATE_CHAT_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


#define DP_MSG_PRIVATE_CHAT_ACTION (1 << 1)


typedef struct DP_MsgPrivateChat DP_MsgPrivateChat;

DP_Message *DP_msg_private_chat_new(unsigned int context_id,
                                    unsigned int target,
                                    unsigned int opaque_flags, const char *text,
                                    size_t text_length);

DP_Message *DP_msg_private_chat_deserialize(unsigned int context_id,
                                            const unsigned char *buffer,
                                            size_t length);

DP_MsgPrivateChat *DP_msg_private_chat_cast(DP_Message *msg);


const char *DP_msg_private_chat_text(DP_MsgPrivateChat *mpc,
                                     size_t *out_length);

unsigned int DP_msg_private_chat_target(DP_MsgPrivateChat *mpc);

bool DP_msg_private_chat_is_action(DP_MsgPrivateChat *mpc);


#endif
