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
#include "chat.h"
#include "../message.h"
#include "../text_writer.h"
#include "dpcommon/binary.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PREFIX_LENGTH      2
#define MIN_PAYLOAD_LENGTH (PREFIX_LENGTH + 1)
#define MIN_TEXT_LENGTH    1

struct DP_MsgChat {
    uint8_t transparent_flags;
    uint8_t opaque_flags;
    size_t text_length;
    char text[];
};

static size_t payload_length(DP_Message *msg)
{
    DP_MsgChat *mc = DP_msg_chat_cast(msg);
    DP_ASSERT(mc);
    return PREFIX_LENGTH + mc->text_length;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgChat *mc = DP_msg_chat_cast(msg);
    DP_ASSERT(mc);
    size_t written = 0;
    written += DP_write_bigendian_uint8(mc->transparent_flags, data + written);
    written += DP_write_bigendian_uint8(mc->opaque_flags, data + written);

    size_t text_length = mc->text_length;
    memcpy(data + written, mc->text, text_length);
    written += mc->text_length;

    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgChat *mc = DP_msg_chat_cast(msg);
    DP_ASSERT(mc);
    DP_RETURN_UNLESS(DP_text_writer_write_string(writer, "message", mc->text));

    unsigned int transparent_flags = mc->transparent_flags;
    unsigned int opaque_flags = mc->opaque_flags;
    unsigned int combined_flags = transparent_flags << 8 | opaque_flags;
    DP_RETURN_UNLESS(DP_text_writer_write_flags(
        writer, "flags", combined_flags, "bypass", DP_MSG_CHAT_BYPASS << 8,
        "shout", DP_MSG_CHAT_SHOUT, "action", DP_MSG_CHAT_ACTION, "pin",
        DP_MSG_CHAT_PIN, (const char *)NULL));

    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgChat *a = DP_msg_chat_cast(msg);
    DP_MsgChat *b = DP_msg_chat_cast(other);
    return a->transparent_flags == b->transparent_flags
        && a->opaque_flags == b->opaque_flags
        && a->text_length == b->text_length
        && memcmp(a->text, b->text, a->text_length) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_chat_new(unsigned int context_id,
                            unsigned int transparent_flags,
                            unsigned int opaque_flags, const char *text,
                            size_t text_length)
{
    DP_ASSERT(transparent_flags <= UINT8_MAX);
    DP_ASSERT(opaque_flags <= UINT8_MAX);
    DP_ASSERT(text);
    DP_ASSERT(text_length > 0);
    DP_Message *msg =
        DP_message_new(DP_MSG_CHAT, context_id, &methods,
                       DP_FLEX_SIZEOF(DP_MsgChat, text, text_length + 1));
    DP_MsgChat *mc = DP_message_internal(msg);
    mc->transparent_flags = DP_uint_to_uint8(transparent_flags);
    mc->opaque_flags = DP_uint_to_uint8(opaque_flags);
    mc->text_length = DP_size_to_uint8(text_length);
    memcpy(mc->text, text, text_length);
    mc->text[text_length] = '\0';
    return msg;
}

DP_Message *DP_msg_chat_deserialize(unsigned int context_id,
                                    const unsigned char *buffer, size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        return DP_msg_chat_new(context_id, DP_read_bigendian_uint8(buffer),
                               DP_read_bigendian_uint8(buffer + 1),
                               (const char *)buffer + PREFIX_LENGTH,
                               length - PREFIX_LENGTH);
    }
    else {
        DP_error_set("Wrong length for CHAT message: %zu", length);
        return NULL;
    }
}


DP_MsgChat *DP_msg_chat_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_CHAT);
}


const char *DP_msg_chat_text(DP_MsgChat *mc, size_t *out_length)
{
    DP_ASSERT(mc);
    if (out_length) {
        *out_length = mc->text_length;
    }
    return mc->text;
}

bool DP_msg_chat_is_bypass(DP_MsgChat *mc)
{
    DP_ASSERT(mc);
    return mc->transparent_flags & DP_MSG_CHAT_BYPASS;
}

bool DP_msg_chat_is_shout(DP_MsgChat *mc)
{
    DP_ASSERT(mc);
    return mc->opaque_flags & DP_MSG_CHAT_SHOUT;
}

bool DP_msg_chat_is_action(DP_MsgChat *mc)
{
    DP_ASSERT(mc);
    return mc->opaque_flags & DP_MSG_CHAT_ACTION;
}

bool DP_msg_chat_is_pin(DP_MsgChat *mc)
{
    DP_ASSERT(mc);
    return mc->opaque_flags & DP_MSG_CHAT_PIN;
}
