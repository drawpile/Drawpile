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
#include "layer_visibility.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 3

struct DP_MsgLayerVisibility {
    uint16_t layer_id;
    bool visible;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgLayerVisibility *mlv = DP_msg_layer_visibility_cast(msg);
    DP_ASSERT(mlv);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mlv->layer_id, data + written);
    written += DP_write_bigendian_uint8(mlv->visible ? 1 : 0, data + written);
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgLayerVisibility *mlv = DP_msg_layer_visibility_cast(msg);
    DP_ASSERT(mlv);

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mlv->layer_id));
    DP_RETURN_UNLESS(DP_text_writer_write_string(
        writer, "visible", mlv->visible ? "true" : "false"));

    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgLayerVisibility *a = DP_msg_layer_visibility_cast(msg);
    DP_MsgLayerVisibility *b = DP_msg_layer_visibility_cast(other);
    return a->layer_id == b->layer_id && a->visible == b->visible;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_layer_visibility_new(unsigned int context_id, int layer_id,
                                        bool visible)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_MsgLayerVisibility *mlv;
    DP_Message *msg = DP_message_new(DP_MSG_LAYER_VISIBILITY, context_id,
                                     &methods, sizeof(*mlv));
    mlv = DP_message_internal(msg);
    *mlv = (DP_MsgLayerVisibility){DP_int_to_uint16(layer_id), visible};
    return msg;
}

DP_Message *DP_msg_layer_visibility_deserialize(unsigned int context_id,
                                                const unsigned char *buffer,
                                                size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_layer_visibility_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_uint8(buffer + 2) != 0);
    }
    else {
        DP_error_set("Wrong length for LAYER_VISIBILITY message: %zu", length);
        return NULL;
    }
}


DP_MsgLayerVisibility *DP_msg_layer_visibility_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_LAYER_VISIBILITY);
}


int DP_msg_layer_visibility_layer_id(DP_MsgLayerVisibility *mlv)
{
    DP_ASSERT(mlv);
    return mlv->layer_id;
}

bool DP_msg_layer_visibility_visible(DP_MsgLayerVisibility *mlv)
{
    DP_ASSERT(mlv);
    return mlv->visible;
}
