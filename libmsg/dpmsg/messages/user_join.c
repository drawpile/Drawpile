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
#include "user_join.h"
#include "../message.h"
#include "../text_writer.h"
#include "dpcommon/binary.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define MIN_PAYLOAD_LENGTH 2
#define FLAG_AUTHENTICATED (1 << 0)
#define FLAG_MODERATOR     (1 << 1)
#define FLAG_BOT           (1 << 1)

struct DP_MsgUserJoin {
    uint8_t flags;
    uint8_t name_length;
    size_t avatar_size;
    char data[]; // contains null-terminated name and avatar data
};

static size_t payload_length(DP_Message *msg)
{
    DP_MsgUserJoin *muj = DP_msg_user_join_cast(msg);
    DP_ASSERT(muj);
    return MIN_PAYLOAD_LENGTH + (size_t)muj->name_length + 1 + muj->avatar_size;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgUserJoin *muj = DP_msg_user_join_cast(msg);
    DP_ASSERT(muj);
    size_t written = 0;
    written += DP_write_bigendian_uint8(muj->flags, data);

    size_t name_length;
    const char *name = DP_msg_user_join_name(muj, &name_length);
    written += DP_write_bigendian_uint8(DP_size_to_uint8(name_length), data);
    memcpy(data + written, name, name_length);
    written += muj->name_length;

    size_t avatar_size;
    const unsigned char *avatar = DP_msg_user_join_avatar(muj, &avatar_size);
    memcpy(data + written, avatar, avatar_size);
    written += avatar_size;

    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgUserJoin *muj = DP_msg_user_join_cast(msg);
    DP_ASSERT(muj);
    DP_RETURN_UNLESS(DP_text_writer_write_string(writer, "name", muj->data));

    size_t avatar_size;
    const unsigned char *avatar = DP_msg_user_join_avatar(muj, &avatar_size);
    if (muj->avatar_size > 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_base64(writer, "avatar", avatar, avatar_size));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_flags(
        writer, "flags", muj->flags, "mod", FLAG_MODERATOR, "auth",
        FLAG_AUTHENTICATED, "bot", FLAG_BOT, (const char *)NULL));

    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgUserJoin *a = DP_msg_user_join_cast(msg);
    DP_MsgUserJoin *b = DP_msg_user_join_cast(other);
    return a->flags == b->flags && a->name_length == b->name_length
        && a->avatar_size == b->avatar_size
        && memcmp(a->data, b->data, a->name_length + 1 + a->avatar_size) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_user_join_new(unsigned int context_id, unsigned int flags,
                                 const char *name, size_t name_length,
                                 const unsigned char *avatar,
                                 size_t avatar_size)
{
    DP_ASSERT(flags <= UINT8_MAX);
    DP_ASSERT(name_length <= UINT8_MAX);
    DP_ASSERT(name);
    DP_ASSERT(avatar || avatar_size == 0);
    DP_MsgUserJoin *muj;
    DP_Message *msg =
        DP_message_new(DP_MSG_USER_JOIN, context_id, &methods,
                       sizeof(*muj) + name_length + 1 + avatar_size);
    muj = DP_message_internal(msg);
    muj->flags = DP_uint_to_uint8(flags);
    muj->name_length = DP_size_to_uint8(name_length);
    muj->avatar_size = avatar_size;
    memcpy(muj->data, name, name_length);
    muj->data[name_length] = '\0';
    if (avatar_size > 0) {
        memcpy(muj->data + name_length + 1, avatar, avatar_size);
    }
    return msg;
}

DP_Message *DP_msg_user_join_deserialize(unsigned int context_id,
                                         const unsigned char *buffer,
                                         size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        size_t name_length = DP_read_bigendian_uint8(buffer + 1);
        if (name_length == 0) {
            DP_error_set("USER_JOIN message name length is 0");
            return NULL;
        }
        else if (name_length > length - 1) {
            DP_error_set("USER_JOIN message name length out of bounds");
            return NULL;
        }
        else {
            return DP_msg_user_join_new(
                context_id, DP_read_bigendian_uint8(buffer),
                (const char *)buffer + 2, name_length, buffer + 2 + name_length,
                length - name_length - 1);
        }
    }
    else {
        DP_error_set("Wrong length for USER_JOIN message: %zu", length);
        return NULL;
    }
}


DP_MsgUserJoin *DP_msg_user_join_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_USER_JOIN);
}


const char *DP_msg_user_join_name(DP_MsgUserJoin *muj, size_t *out_length)
{
    DP_ASSERT(muj);
    if (out_length) {
        *out_length = muj->name_length;
    }
    return muj->data;
}

const unsigned char *DP_msg_user_join_avatar(DP_MsgUserJoin *muj,
                                             size_t *out_size)
{
    DP_ASSERT(muj);
    size_t avatar_size = muj->avatar_size;
    if (out_size) {
        *out_size = avatar_size;
    }
    if (avatar_size == 0) {
        return NULL;
    }
    else {
        return (const unsigned char *)muj->data + muj->name_length + 1;
    }
}

bool DP_msg_user_join_is_authenticated(DP_MsgUserJoin *muj)
{
    DP_ASSERT(muj);
    return muj->flags & FLAG_AUTHENTICATED;
}

bool DP_msg_user_join_is_moderator(DP_MsgUserJoin *muj)
{
    DP_ASSERT(muj);
    return muj->flags & FLAG_MODERATOR;
}

bool DP_msg_user_join_is_bot(DP_MsgUserJoin *muj)
{
    DP_ASSERT(muj);
    return muj->flags & FLAG_BOT;
}
