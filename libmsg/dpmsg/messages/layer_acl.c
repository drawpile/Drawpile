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
#include "layer_acl.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define MIN_PAYLOAD_LENGTH 3
#define MAX_PAYLOAD_LENGTH 258
#define LOCKED_MASK        0x80
#define TIER_MASK          0x07

struct DP_MsgLayerAcl {
    uint16_t layer_id;
    uint8_t flags;
    int exclusive_id_count;
    unsigned int exclusive_ids[];
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    DP_MsgLayerAcl *mla = DP_msg_layer_acl_cast(msg);
    DP_ASSERT(mla);
    return MIN_PAYLOAD_LENGTH + DP_int_to_size(mla->exclusive_id_count);
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgLayerAcl *mla = DP_msg_layer_acl_cast(msg);
    DP_ASSERT(mla);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mla->layer_id, data + written);
    written += DP_write_bigendian_uint8(mla->flags, data + written);

    int exclusive_id_count = mla->exclusive_id_count;
    for (int i = 0; i < exclusive_id_count; ++i) {
        data[MIN_PAYLOAD_LENGTH + i] = DP_uint_to_uint8(mla->exclusive_ids[i]);
    }
    return written + DP_int_to_size(exclusive_id_count);
}

static const char *get_tier_name(int tier)
{
    switch (tier) {
    case 0:
        return "op";
    case 1:
        return "trusted";
    case 2:
        return "auth";
    default:
        return "guest";
    }
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgLayerAcl *mla = DP_msg_layer_acl_cast(msg);
    DP_ASSERT(mla);

    int layer_id = mla->layer_id;
    int exclusive_id_count = mla->exclusive_id_count;
    if (layer_id > 0 && exclusive_id_count > 0) {
        DP_RETURN_UNLESS(DP_text_writer_write_uint_list(
            writer, "exclusive", mla->exclusive_ids, exclusive_id_count));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "id", layer_id));
    DP_RETURN_UNLESS(DP_text_writer_write_string(
        writer, "locked", DP_msg_layer_acl_locked(mla) ? "true" : "false"));

    if (layer_id > 0) {
        const char *tier_name = get_tier_name(DP_msg_layer_acl_tier(mla));
        DP_RETURN_UNLESS(
            DP_text_writer_write_string(writer, "tier", tier_name));
    }

    return true;
}

static bool exclusive_ids_equal(unsigned int *a, unsigned int *b, int count)
{
    for (int i = 0; i < count; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgLayerAcl *a = DP_msg_layer_acl_cast(msg);
    DP_MsgLayerAcl *b = DP_msg_layer_acl_cast(other);
    return a->layer_id == b->layer_id && a->flags == b->flags
        && a->exclusive_id_count == b->exclusive_id_count
        && exclusive_ids_equal(a->exclusive_ids, b->exclusive_ids,
                               a->exclusive_id_count);
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_layer_acl_new(unsigned int context_id, int layer_id,
                                 unsigned int flags, int exclusive_id_count,
                                 unsigned int (*get_exclusive_id)(void *user,
                                                                  int i),
                                 void *user)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_ASSERT(flags <= UINT8_MAX);
    DP_ASSERT(exclusive_id_count <= UINT8_MAX);
    DP_Message *msg =
        DP_message_new(DP_MSG_LAYER_ACL, context_id, &methods,
                       DP_FLEX_SIZEOF(DP_MsgLayerAcl, exclusive_ids,
                                      DP_int_to_size(exclusive_id_count)));
    DP_MsgLayerAcl *mla = DP_message_internal(msg);
    mla->layer_id = DP_int_to_uint16(layer_id);
    mla->flags = DP_uint_to_uint8(flags);
    mla->exclusive_id_count = exclusive_id_count;
    for (int i = 0; i < exclusive_id_count; ++i) {
        unsigned int exclusive_id = get_exclusive_id(user, i);
        DP_ASSERT(exclusive_id <= UINT8_MAX);
        mla->exclusive_ids[i] = exclusive_id;
    }
    return msg;
}

static unsigned int deserialize_id(void *user, int i)
{
    const unsigned char *buffer = user;
    return buffer[i];
}

DP_Message *DP_msg_layer_acl_deserialize(unsigned int context_id,
                                         const unsigned char *buffer,
                                         size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH && length <= MAX_PAYLOAD_LENGTH) {
        return DP_msg_layer_acl_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_uint8(buffer + 2), DP_size_to_int(length - 3),
            deserialize_id, (void *)(buffer + 3));
    }
    else {
        DP_error_set("Wrong length for LAYER_ACL message: %zu", length);
        return NULL;
    }
}


DP_MsgLayerAcl *DP_msg_layer_acl_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_LAYER_ACL);
}


int DP_msg_layer_acl_layer_id(DP_MsgLayerAcl *mla)
{
    DP_ASSERT(mla);
    return mla->layer_id;
}

bool DP_msg_layer_acl_locked(DP_MsgLayerAcl *mla)
{
    DP_ASSERT(mla);
    return mla->flags & LOCKED_MASK;
}

int DP_msg_layer_acl_tier(DP_MsgLayerAcl *mla)
{
    DP_ASSERT(mla);
    return mla->flags & TIER_MASK;
}
