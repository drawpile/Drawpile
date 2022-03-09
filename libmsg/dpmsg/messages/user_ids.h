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
#ifndef DPMSG_USER_IDS_H
#define DPMSG_USER_IDS_H
#include "../message.h"
#include "../text_writer.h"
#include "dpcommon/binary.h"
#include "dpcommon/conversions.h"
#include <dpcommon/common.h>


// SESSION_OWNER, TRUSTED_USERS and USER_ACL messages have exactly the same
// structure, so this generates identical code for them. The reason this is
// in macros and not in functions is because that flexible array member in the
// structure prevents it from being embedded into other structs in ISO C.
#define DP_MSG_USER_IDS_DEFINE(TYPE, STRUCT, NAME)                             \
    struct STRUCT {                                                            \
        int id_count;                                                          \
        unsigned int ids[];                                                    \
    };                                                                         \
                                                                               \
    static size_t payload_length(DP_Message *msg)                              \
    {                                                                          \
        STRUCT *mui = DP_msg_##NAME##_cast(msg);                               \
        DP_ASSERT(mui);                                                        \
        return DP_int_to_size(mui->id_count);                                  \
    }                                                                          \
                                                                               \
    static size_t serialize_payload(DP_Message *msg, unsigned char *data)      \
    {                                                                          \
        STRUCT *mui = DP_msg_##NAME##_cast(msg);                               \
        DP_ASSERT(mui);                                                        \
        size_t written = 0;                                                    \
        int count = mui->id_count;                                             \
        for (int i = 0; i < count; ++i) {                                      \
            uint8_t id = DP_uint_to_uint8(mui->ids[i]);                        \
            written += DP_write_bigendian_uint8(id, data);                     \
        }                                                                      \
        return written;                                                        \
    }                                                                          \
                                                                               \
    static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)     \
    {                                                                          \
        STRUCT *mui = DP_msg_##NAME##_cast(msg);                               \
        DP_ASSERT(mui);                                                        \
        if (mui->id_count > 0) {                                               \
            DP_RETURN_UNLESS(DP_text_writer_write_uint_list(                   \
                writer, "users", mui->ids, mui->id_count));                    \
        }                                                                      \
        return true;                                                           \
    }                                                                          \
                                                                               \
    static bool ids_equal(unsigned int *a, unsigned int *b, int count)         \
    {                                                                          \
        for (int i = 0; i < count; ++i) {                                      \
            if (a[i] != b[i]) {                                                \
                return false;                                                  \
            }                                                                  \
        }                                                                      \
        return true;                                                           \
    }                                                                          \
                                                                               \
    static bool equals(DP_Message *DP_RESTRICT msg,                            \
                       DP_Message *DP_RESTRICT other)                          \
    {                                                                          \
        STRUCT *a = DP_msg_##NAME##_cast(msg);                                 \
        STRUCT *b = DP_msg_##NAME##_cast(other);                               \
        return a->id_count == b->id_count                                      \
            && ids_equal(a->ids, b->ids, a->id_count);                         \
    }                                                                          \
                                                                               \
    static const DP_MessageMethods methods = {                                 \
        payload_length, serialize_payload, write_payload_text, equals, NULL,   \
    };                                                                         \
                                                                               \
    DP_Message *DP_msg_##NAME##_new(unsigned int context_id, int count,        \
                                    unsigned int (*get_id)(void *user, int i), \
                                    void *user)                                \
    {                                                                          \
        DP_Message *msg = DP_message_new(                                      \
            TYPE, context_id, &methods,                                        \
            DP_FLEX_SIZEOF(STRUCT, ids, DP_int_to_size(count)));               \
        STRUCT *mui = DP_message_internal(msg);                                \
        mui->id_count = count;                                                 \
        for (int i = 0; i < count; ++i) {                                      \
            unsigned int id = get_id(user, i);                                 \
            DP_ASSERT(id <= UINT8_MAX);                                        \
            mui->ids[i] = id;                                                  \
        }                                                                      \
        return msg;                                                            \
    }                                                                          \
                                                                               \
    static unsigned int deserialize_id(void *user, int i)                      \
    {                                                                          \
        const unsigned char *buffer = user;                                    \
        return buffer[i];                                                      \
    }                                                                          \
                                                                               \
    DP_Message *DP_msg_##NAME##_deserialize(                                   \
        unsigned int context_id, const unsigned char *buffer, size_t length)   \
    {                                                                          \
        return DP_msg_##NAME##_new(context_id, DP_size_to_int(length),         \
                                   deserialize_id, (void *)buffer);            \
    }                                                                          \
                                                                               \
                                                                               \
    STRUCT *DP_msg_##NAME##_cast(DP_Message *msg)                              \
    {                                                                          \
        return DP_message_cast(msg, TYPE);                                     \
    }                                                                          \
                                                                               \
                                                                               \
    const unsigned int *DP_msg_##NAME##_ids(STRUCT *mui, int *out_count)       \
    {                                                                          \
        DP_ASSERT(mui);                                                        \
        if (out_count) {                                                       \
            *out_count = mui->id_count;                                        \
        }                                                                      \
        return mui->ids;                                                       \
    }

#endif
