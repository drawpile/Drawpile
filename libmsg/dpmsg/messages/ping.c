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
#include "ping.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>


#define PAYLOAD_LENGTH 1

struct DP_MsgPing {
    bool is_pong;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgPing *mp = DP_msg_ping_cast(msg);
    DP_ASSERT(mp);
    return DP_write_bigendian_uint8(mp->is_pong ? 1 : 0, data);
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgPing *mp = DP_msg_ping_cast(msg);
    DP_ASSERT(mp);
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "pong", mp->is_pong));
    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgPing *a = DP_msg_ping_cast(msg);
    DP_MsgPing *b = DP_msg_ping_cast(other);
    return a->is_pong == b->is_pong;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_ping_new(unsigned int context_id, bool is_pong)
{
    DP_MsgPing *mp;
    DP_Message *msg =
        DP_message_new(DP_MSG_PING, context_id, &methods, sizeof(*mp));
    mp = DP_message_internal(msg);
    mp->is_pong = is_pong;
    return msg;
}

DP_Message *DP_msg_ping_deserialize(unsigned int context_id,
                                    const unsigned char *buffer, size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_ping_new(context_id, buffer[0] != 0);
    }
    else {
        DP_error_set("Wrong length for PING message: %zu", length);
        return NULL;
    }
}


DP_MsgPing *DP_msg_ping_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_PING);
}


bool DP_msg_ping_is_pong(DP_MsgPing *mp)
{
    DP_ASSERT(mp);
    return mp->is_pong;
}
