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
#include "layer_attr.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 6

struct DP_MsgLayerAttr {
    uint16_t layer_id;
    uint8_t sublayer_id;
    uint8_t flags;
    uint8_t opacity;
    uint8_t blend_mode;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgLayerAttr *mla = DP_msg_layer_attr_cast(msg);
    DP_ASSERT(mla);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mla->layer_id, data + written);
    written += DP_write_bigendian_uint8(mla->sublayer_id, data + written);
    written += DP_write_bigendian_uint8(mla->flags, data + written);
    written += DP_write_bigendian_uint8(mla->opacity, data + written);
    written += DP_write_bigendian_uint8(mla->blend_mode, data + written);
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgLayerAttr *mla = DP_msg_layer_attr_cast(msg);
    DP_ASSERT(mla);

    DP_RETURN_UNLESS(
        DP_text_writer_write_uint(writer, "blend", mla->blend_mode));

    unsigned int flags = mla->flags;
    if (flags & DP_MSG_LAYER_ATTR_FLAG_MASK) {
        DP_RETURN_UNLESS(DP_text_writer_write_flags(
            writer, "flags", flags, "censor", DP_MSG_LAYER_ATTR_FLAG_CENSORED,
            "fixed", DP_MSG_LAYER_ATTR_FLAG_FIXED, (const char *)NULL));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mla->layer_id));
    DP_RETURN_UNLESS(
        DP_text_writer_write_decimal(writer, "opacity", mla->opacity));

    if (mla->sublayer_id != 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_uint(writer, "sublayer", mla->sublayer_id));
    }

    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgLayerAttr *a = DP_msg_layer_attr_cast(msg);
    DP_MsgLayerAttr *b = DP_msg_layer_attr_cast(other);
    return a->layer_id == b->layer_id && a->sublayer_id == b->sublayer_id
        && a->flags == b->flags && a->opacity == b->opacity
        && a->blend_mode == b->blend_mode;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_layer_attr_new(unsigned int context_id, int layer_id,
                                  int sublayer_id, unsigned int flags,
                                  uint8_t opacity, int blend_mode)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_ASSERT(sublayer_id >= 0);
    DP_ASSERT(sublayer_id <= UINT8_MAX);
    DP_ASSERT(flags <= UINT8_MAX);
    DP_ASSERT(blend_mode >= 0);
    DP_ASSERT(blend_mode <= UINT8_MAX);
    DP_MsgLayerAttr *mla;
    DP_Message *msg =
        DP_message_new(DP_MSG_LAYER_ATTR, context_id, &methods, sizeof(*mla));
    mla = DP_message_internal(msg);
    *mla = (DP_MsgLayerAttr){DP_int_to_uint16(layer_id),
                             DP_int_to_uint8(sublayer_id),
                             DP_uint_to_uint8(flags), DP_uint_to_uint8(opacity),
                             DP_int_to_uint8(blend_mode)};
    return msg;
}

DP_Message *DP_msg_layer_attr_deserialize(unsigned int context_id,
                                          const unsigned char *buffer,
                                          size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_layer_attr_new(context_id,
                                     DP_read_bigendian_uint16(buffer),
                                     DP_read_bigendian_uint8(buffer + 2),
                                     DP_read_bigendian_uint8(buffer + 3),
                                     DP_read_bigendian_uint8(buffer + 4),
                                     DP_read_bigendian_uint8(buffer + 5));
    }
    else {
        DP_error_set("Wrong length for LAYER_ATTR message: %zu", length);
        return NULL;
    }
}


DP_MsgLayerAttr *DP_msg_layer_attr_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_LAYER_ATTR);
}


int DP_msg_layer_attr_layer_id(DP_MsgLayerAttr *mla)
{
    DP_ASSERT(mla);
    return mla->layer_id;
}

int DP_msg_layer_attr_sublayer_id(DP_MsgLayerAttr *mla)
{
    DP_ASSERT(mla);
    return mla->sublayer_id;
}

unsigned int DP_msg_layer_attr_flags(DP_MsgLayerAttr *mla)
{
    DP_ASSERT(mla);
    return mla->flags;
}

uint8_t DP_msg_layer_attr_opacity(DP_MsgLayerAttr *mla)
{
    DP_ASSERT(mla);
    return mla->opacity;
}

int DP_msg_layer_attr_blend_mode(DP_MsgLayerAttr *mla)
{
    DP_ASSERT(mla);
    return mla->blend_mode;
}
