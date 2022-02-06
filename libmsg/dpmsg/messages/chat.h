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
#ifndef DPMSG_CHAT_H
#define DPMSG_CHAT_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


#define DP_MSG_CHAT_BYPASS (1 << 0)

#define DP_MSG_CHAT_SHOUT  (1 << 0)
#define DP_MSG_CHAT_ACTION (1 << 1)
#define DP_MSG_CHAT_PIN    (1 << 2)


typedef struct DP_MsgChat DP_MsgChat;

DP_Message *DP_msg_chat_new(unsigned int context_id,
                            unsigned int transparent_flags,
                            unsigned int opaque_flags, const char *text,
                            size_t text_length);

DP_Message *DP_msg_chat_deserialize(unsigned int context_id,
                                    const unsigned char *buffer, size_t length);

DP_MsgChat *DP_msg_chat_cast(DP_Message *msg);


const char *DP_msg_chat_text(DP_MsgChat *mc, size_t *out_length);

bool DP_msg_chat_is_bypass(DP_MsgChat *mc);

bool DP_msg_chat_is_shout(DP_MsgChat *mc);
bool DP_msg_chat_is_action(DP_MsgChat *mc);
bool DP_msg_chat_is_pin(DP_MsgChat *mc);


#endif
