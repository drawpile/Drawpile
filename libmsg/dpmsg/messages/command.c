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
#include "command.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>


#define MAX_PAYLOAD_LENGTH (0xffff - DP_MESSAGE_HEADER_LENGTH)

struct DP_MsgCommand {
    size_t message_length;
    char message[];
};

static size_t payload_length(DP_Message *msg)
{
    DP_MsgCommand *mc = DP_msg_command_cast(msg);
    DP_ASSERT(mc);
    return mc->message_length;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgCommand *mc = DP_msg_command_cast(msg);
    DP_ASSERT(mc);
    memcpy(data, mc->message, mc->message_length);
    return mc->message_length;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgCommand *mc = DP_msg_command_cast(msg);
    DP_ASSERT(mc);
    DP_RETURN_UNLESS(
        DP_text_writer_write_string(writer, "message", mc->message));
    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgCommand *a = DP_msg_command_cast(msg);
    DP_MsgCommand *b = DP_msg_command_cast(other);
    return a->message_length == b->message_length
        && memcmp(a->message, b->message, a->message_length) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_command_new(unsigned int context_id, const char *message,
                               size_t message_len)
{
    DP_ASSERT(message);
    DP_ASSERT(message_len <= MAX_PAYLOAD_LENGTH);
    DP_MsgCommand *mc;
    DP_Message *msg = DP_message_new(DP_MSG_COMMAND, context_id, &methods,
                                     sizeof(*mc) + message_len + 1);
    mc = DP_message_internal(msg);
    mc->message_length = message_len;
    memcpy(mc->message, message, message_len);
    mc->message[message_len] = '\0';
    return msg;
}

DP_Message *DP_msg_command_deserialize(unsigned int context_id,
                                       const unsigned char *buffer,
                                       size_t length)
{
    if (length <= MAX_PAYLOAD_LENGTH) {
        return DP_msg_command_new(context_id, (const char *)buffer, length);
    }
    else {
        DP_error_set("Wrong length for COMMAND message: %zu", length);
        return NULL;
    }
}


DP_MsgCommand *DP_msg_command_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_COMMAND);
}


const char *DP_msg_command_message(DP_MsgCommand *mc, size_t *length)
{
    DP_ASSERT(mc);
    if (length) {
        *length = mc->message_length;
    }
    return mc->message;
}
