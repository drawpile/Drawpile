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
#include "disconnect.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>


#define MIN_PAYLOAD_LENGTH 1
#define MAX_PAYLOAD_LENGTH (0xffff - DP_MESSAGE_HEADER_LENGTH - 1)

struct DP_MsgDisconnect {
    uint8_t reason;
    size_t message_length;
    char message[];
};

static size_t payload_length(DP_Message *msg)
{
    DP_MsgDisconnect *md = DP_msg_disconnect_cast(msg);
    DP_ASSERT(md);
    return MIN_PAYLOAD_LENGTH + md->message_length;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgDisconnect *md = DP_msg_disconnect_cast(msg);
    DP_ASSERT(md);
    size_t written = DP_write_bigendian_uint8(md->reason, data);
    memcpy(data + written, md->message, md->message_length);
    return written + md->message_length;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgDisconnect *md = DP_msg_disconnect_cast(msg);
    DP_ASSERT(md);
    DP_RETURN_UNLESS(
        DP_text_writer_write_string(writer, "message", md->message));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "reason", md->reason));
    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgDisconnect *a = DP_msg_disconnect_cast(msg);
    DP_MsgDisconnect *b = DP_msg_disconnect_cast(other);
    return a->reason == b->reason && a->message_length == b->message_length
        && memcmp(a->message, b->message, a->message_length) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_disconnect_new(unsigned int context_id,
                                  DP_DisconnectReason reason,
                                  const char *message, size_t message_len)
{
    DP_ASSERT(reason >= 0);
    DP_ASSERT(reason <= UINT8_MAX);
    DP_ASSERT(message_len <= MAX_PAYLOAD_LENGTH);
    DP_MsgDisconnect *md;
    DP_Message *msg = DP_message_new(DP_MSG_DISCONNECT, context_id, &methods,
                                     sizeof(*md) + message_len + 1);
    md = DP_message_internal(msg);
    md->reason = (uint8_t)reason;
    md->message_length = message_len;
    if (message_len != 0) {
        DP_ASSERT(message);
        memcpy(md->message, message, message_len);
    }
    md->message[message_len] = '\0';
    return msg;
}

DP_Message *DP_msg_disconnect_deserialize(unsigned int context_id,
                                          const unsigned char *buffer,
                                          size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH && length <= MAX_PAYLOAD_LENGTH) {
        return DP_msg_disconnect_new(context_id,
                                     DP_read_bigendian_uint8(buffer),
                                     (const char *)buffer + 1, length - 1);
    }
    else {
        DP_error_set("Wrong length for DISCONNECT message: %zu", length);
        return NULL;
    }
}


DP_MsgDisconnect *DP_msg_disconnect_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_DISCONNECT);
}


DP_DisconnectReason DP_msg_disconnect_reason(DP_MsgDisconnect *md)
{
    DP_ASSERT(md);
    return md->reason;
}

const char *DP_msg_disconnect_message(DP_MsgDisconnect *md, size_t *length)
{
    DP_ASSERT(md);
    if (length) {
        *length = md->message_length;
    }
    return md->message;
}
