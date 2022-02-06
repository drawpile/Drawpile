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
#include "layer_create.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define MIN_PAYLOAD_LENGTH 9

struct DP_MsgLayerCreate {
    uint16_t layer_id;
    uint16_t source_id;
    uint32_t fill;
    uint8_t flags;
    size_t title_length;
    char title[];
};

static size_t payload_length(DP_Message *msg)
{
    DP_MsgLayerCreate *mlc = DP_msg_layer_create_cast(msg);
    DP_ASSERT(mlc);
    return MIN_PAYLOAD_LENGTH + mlc->title_length;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgLayerCreate *mlc = DP_msg_layer_create_cast(msg);
    DP_ASSERT(mlc);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mlc->layer_id, data + written);
    written += DP_write_bigendian_uint16(mlc->source_id, data + written);
    written += DP_write_bigendian_uint32(mlc->fill, data + written);
    written += DP_write_bigendian_uint8(mlc->flags, data + written);
    memcpy(data + written, mlc->title, mlc->title_length);
    return written + mlc->title_length;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgLayerCreate *mlc = DP_msg_layer_create_cast(msg);
    DP_ASSERT(mlc);

    if (mlc->fill != 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_argb_color(writer, "fill", mlc->fill));
    }

    unsigned int flags = mlc->flags;
    if (flags & DP_MSG_LAYER_CREATE_FLAG_MASK) {
        DP_RETURN_UNLESS(DP_text_writer_write_flags(
            writer, "flags", flags, "copy", DP_MSG_LAYER_CREATE_FLAG_COPY,
            "insert", DP_MSG_LAYER_CREATE_FLAG_INSERT, (const char *)NULL));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "id", mlc->layer_id));

    if (mlc->source_id != 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_id(writer, "source", mlc->source_id));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_string(writer, "title", mlc->title));

    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgLayerCreate *a = DP_msg_layer_create_cast(msg);
    DP_MsgLayerCreate *b = DP_msg_layer_create_cast(other);
    return a->layer_id == b->layer_id && a->source_id == b->source_id
        && a->fill == b->fill && a->flags == b->flags
        && a->title_length == b->title_length
        && memcmp(a->title, b->title, a->title_length) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_layer_create_new(unsigned int context_id, int layer_id,
                                    int source_id, unsigned int fill,
                                    unsigned int flags, const char *title,
                                    size_t title_length)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_ASSERT(source_id >= 0);
    DP_ASSERT(source_id <= UINT16_MAX);
    DP_ASSERT(fill <= UINT32_MAX);
    DP_ASSERT(flags <= UINT8_MAX);
    DP_ASSERT(title_length == 0 || title);
    DP_MsgLayerCreate *mlc;
    DP_Message *msg = DP_message_new(DP_MSG_LAYER_CREATE, context_id, &methods,
                                     sizeof(*mlc) + title_length + 1);
    mlc = DP_message_internal(msg);
    mlc->layer_id = DP_int_to_uint16(layer_id);
    mlc->source_id = DP_int_to_uint16(source_id);
    mlc->fill = DP_uint_to_uint32(fill);
    mlc->flags = DP_uint_to_uint8(flags);
    mlc->title_length = title_length;
    if (title_length != 0) {
        memcpy(mlc->title, title, title_length);
    }
    mlc->title[title_length] = '\0';
    return msg;
}

DP_Message *DP_msg_layer_create_deserialize(unsigned int context_id,
                                            const unsigned char *buffer,
                                            size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        return DP_msg_layer_create_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_uint16(buffer + 2),
            DP_read_bigendian_uint32(buffer + 4),
            DP_read_bigendian_uint8(buffer + 8),
            (const char *)(buffer + MIN_PAYLOAD_LENGTH),
            length - MIN_PAYLOAD_LENGTH);
    }
    else {
        DP_error_set("Wrong length for LAYER_CREATE message: %zu", length);
        return NULL;
    }
}


DP_MsgLayerCreate *DP_msg_layer_create_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_LAYER_CREATE);
}


int DP_msg_layer_create_layer_id(DP_MsgLayerCreate *mlc)
{
    DP_ASSERT(mlc);
    return mlc->layer_id;
}

int DP_msg_layer_create_source_id(DP_MsgLayerCreate *mlc)
{
    DP_ASSERT(mlc);
    return mlc->source_id;
}

uint32_t DP_msg_layer_create_fill(DP_MsgLayerCreate *mlc)
{
    DP_ASSERT(mlc);
    return mlc->fill;
}

unsigned int DP_msg_layer_create_flags(DP_MsgLayerCreate *mlc)
{
    DP_ASSERT(mlc);
    return mlc->flags;
}

const char *DP_msg_layer_create_title(DP_MsgLayerCreate *mlc,
                                      size_t *out_length)
{
    DP_ASSERT(mlc);
    if (out_length) {
        *out_length = mlc->title_length;
    }
    return mlc->title;
}

bool DP_msg_layer_create_id_valid(DP_MsgLayerCreate *mlc)
{
    DP_ASSERT(mlc);
    DP_Message *msg = DP_message_from_internal(mlc);
    return ((mlc->layer_id >> 8u) & 0xff) == DP_message_context_id(msg);
}
