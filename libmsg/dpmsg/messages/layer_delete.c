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
#include "layer_delete.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 3

struct DP_MsgLayerDelete {
    uint16_t layer_id;
    bool merge;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgLayerDelete *mld = DP_msg_layer_delete_cast(msg);
    DP_ASSERT(mld);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mld->layer_id, data + written);
    written += DP_write_bigendian_uint8(mld->merge ? 1 : 0, data + written);
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgLayerDelete *mld = DP_msg_layer_delete_cast(msg);
    DP_ASSERT(mld);

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mld->layer_id));

    if (mld->merge) {
        DP_RETURN_UNLESS(DP_text_writer_write_string(writer, "merge", "true"));
    }

    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgLayerDelete *a = DP_msg_layer_delete_cast(msg);
    DP_MsgLayerDelete *b = DP_msg_layer_delete_cast(other);
    return a->layer_id == b->layer_id && a->merge == b->merge;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_layer_delete_new(unsigned int context_id, int layer_id,
                                    bool visible)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_MsgLayerDelete *mld;
    DP_Message *msg =
        DP_message_new(DP_MSG_LAYER_DELETE, context_id, &methods, sizeof(*mld));
    mld = DP_message_internal(msg);
    *mld = (DP_MsgLayerDelete){DP_int_to_uint16(layer_id), visible};
    return msg;
}

DP_Message *DP_msg_layer_delete_deserialize(unsigned int context_id,
                                            const unsigned char *buffer,
                                            size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_layer_delete_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_uint8(buffer + 2) != 0);
    }
    else {
        DP_error_set("Wrong length for LAYER_DELETE message: %zu", length);
        return NULL;
    }
}


DP_MsgLayerDelete *DP_msg_layer_delete_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_LAYER_DELETE);
}


int DP_msg_layer_delete_layer_id(DP_MsgLayerDelete *mld)
{
    DP_ASSERT(mld);
    return mld->layer_id;
}

bool DP_msg_layer_delete_merge(DP_MsgLayerDelete *mld)
{
    DP_ASSERT(mld);
    return mld->merge;
}
