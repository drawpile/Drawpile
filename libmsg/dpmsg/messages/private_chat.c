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
#include "private_chat.h"
#include "../message.h"
#include "../text_writer.h"
#include "dpcommon/binary.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PREFIX_LENGTH      2
#define MIN_PAYLOAD_LENGTH (PREFIX_LENGTH + 1)
#define MIN_TEXT_LENGTH    1

struct DP_MsgPrivateChat {
    uint8_t target;
    uint8_t opaque_flags;
    size_t text_length;
    char text[];
};

static size_t payload_length(DP_Message *msg)
{
    DP_MsgPrivateChat *mpc = DP_msg_private_chat_cast(msg);
    DP_ASSERT(mpc);
    return PREFIX_LENGTH + mpc->text_length;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgPrivateChat *mpc = DP_msg_private_chat_cast(msg);
    DP_ASSERT(mpc);
    size_t written = 0;
    written += DP_write_bigendian_uint8(mpc->target, data);
    written += DP_write_bigendian_uint8(mpc->opaque_flags, data);

    size_t text_length = mpc->text_length;
    memcpy(data + written, mpc->text, text_length);
    written += mpc->text_length;

    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgPrivateChat *mpc = DP_msg_private_chat_cast(msg);
    DP_ASSERT(mpc);
    DP_RETURN_UNLESS(DP_text_writer_write_uint(writer, "target", mpc->target));
    DP_RETURN_UNLESS(DP_text_writer_write_string(writer, "message", mpc->text));

    DP_RETURN_UNLESS(DP_text_writer_write_flags(
        writer, "flags", mpc->opaque_flags, "action",
        DP_MSG_PRIVATE_CHAT_ACTION, (const char *)NULL));

    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgPrivateChat *a = DP_msg_private_chat_cast(msg);
    DP_MsgPrivateChat *b = DP_msg_private_chat_cast(other);
    return a->target == b->target && a->opaque_flags == b->opaque_flags
        && a->text_length == b->text_length
        && memcmp(a->text, b->text, a->text_length) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_private_chat_new(unsigned int context_id,
                                    unsigned int target,
                                    unsigned int opaque_flags, const char *text,
                                    size_t text_length)
{
    DP_ASSERT(target <= UINT8_MAX);
    DP_ASSERT(opaque_flags <= UINT8_MAX);
    DP_ASSERT(text);
    DP_ASSERT(text_length > 0);
    DP_Message *msg = DP_message_new(
        DP_MSG_PRIVATE_CHAT, context_id, &methods,
        DP_FLEX_SIZEOF(DP_MsgPrivateChat, text, text_length + 1));
    DP_MsgPrivateChat *mpc = DP_message_internal(msg);
    mpc->target = DP_uint_to_uint8(target);
    mpc->opaque_flags = DP_uint_to_uint8(opaque_flags);
    mpc->text_length = DP_size_to_uint8(text_length);
    memcpy(mpc->text, text, text_length);
    mpc->text[text_length] = '\0';
    return msg;
}

DP_Message *DP_msg_private_chat_deserialize(unsigned int context_id,
                                            const unsigned char *buffer,
                                            size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        return DP_msg_private_chat_new(
            context_id, DP_read_bigendian_uint8(buffer),
            DP_read_bigendian_uint8(buffer + 1),
            (const char *)buffer + PREFIX_LENGTH, length - PREFIX_LENGTH);
    }
    else {
        DP_error_set("Wrong length for PRIVATE_CHAT message: %zu", length);
        return NULL;
    }
}


DP_MsgPrivateChat *DP_msg_private_chat_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_PRIVATE_CHAT);
}


const char *DP_msg_private_chat_text(DP_MsgPrivateChat *mpc, size_t *out_length)
{
    DP_ASSERT(mpc);
    if (out_length) {
        *out_length = mpc->text_length;
    }
    return mpc->text;
}

unsigned int DP_msg_private_chat_target(DP_MsgPrivateChat *mpc)
{
    DP_ASSERT(mpc);
    return mpc->target;
}

bool DP_msg_private_chat_is_action(DP_MsgPrivateChat *mpc)
{
    DP_ASSERT(mpc);
    return mpc->opaque_flags & DP_MSG_PRIVATE_CHAT_ACTION;
}
