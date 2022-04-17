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
#include "annotation_reshape.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 14

struct DP_MsgAnnotationReshape {
    uint16_t annotation_id;
    int32_t x, y;
    uint16_t width, height;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgAnnotationReshape *mar = DP_msg_annotation_reshape_cast(msg);
    DP_ASSERT(mar);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mar->annotation_id, data + written);
    written += DP_write_bigendian_int32(mar->x, data + written);
    written += DP_write_bigendian_int32(mar->y, data + written);
    written += DP_write_bigendian_uint16(mar->width, data + written);
    written += DP_write_bigendian_uint16(mar->height, data + written);
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgAnnotationReshape *mar = DP_msg_annotation_reshape_cast(msg);
    DP_ASSERT(mar);
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "height", mar->height));
    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "id", mar->annotation_id));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "width", mar->width));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "x", mar->x));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "y", mar->y));
    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgAnnotationReshape *a = DP_msg_annotation_reshape_cast(msg);
    DP_MsgAnnotationReshape *b = DP_msg_annotation_reshape_cast(other);
    return a->annotation_id == b->annotation_id && a->x == b->x && a->y == b->y
        && a->width == b->width && a->height == b->height;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_annotation_reshape_new(unsigned int context_id,
                                          int annotation_id, int x, int y,
                                          int width, int height)
{
    DP_ASSERT(annotation_id >= 0);
    DP_ASSERT(annotation_id <= UINT16_MAX);
    DP_ASSERT(x >= INT32_MIN);
    DP_ASSERT(x <= INT32_MAX);
    DP_ASSERT(y >= INT32_MIN);
    DP_ASSERT(y <= INT32_MAX);
    DP_ASSERT(width >= 0);
    DP_ASSERT(width <= UINT16_MAX);
    DP_ASSERT(height >= 0);
    DP_ASSERT(height <= UINT16_MAX);
    DP_MsgAnnotationReshape *mar;
    DP_Message *msg = DP_message_new(DP_MSG_ANNOTATION_RESHAPE, context_id,
                                     &methods, sizeof(*mar));
    mar = DP_message_internal(msg);
    (*mar) = (DP_MsgAnnotationReshape){
        DP_int_to_uint16(annotation_id), DP_int_to_int32(x), DP_int_to_int32(y),
        DP_int_to_uint16(width), DP_int_to_uint16(height)};
    return msg;
}

DP_Message *DP_msg_annotation_reshape_deserialize(unsigned int context_id,
                                                  const unsigned char *buffer,
                                                  size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_annotation_reshape_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_uint16(buffer + 2),
            DP_read_bigendian_uint16(buffer + 6),
            DP_read_bigendian_uint16(buffer + 10),
            DP_read_bigendian_uint16(buffer + 12));
    }
    else {
        DP_error_set("Wrong length for ANNOTATION_RESHAPE message: %zu",
                     length);
        return NULL;
    }
}


DP_MsgAnnotationReshape *DP_msg_annotation_reshape_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_ANNOTATION_RESHAPE);
}


int DP_msg_annotation_reshape_annotation_id(DP_MsgAnnotationReshape *mar)
{
    DP_ASSERT(mar);
    return mar->annotation_id;
}

int DP_msg_annotation_reshape_x(DP_MsgAnnotationReshape *mar)
{
    DP_ASSERT(mar);
    return mar->x;
}

int DP_msg_annotation_reshape_y(DP_MsgAnnotationReshape *mar)
{
    DP_ASSERT(mar);
    return mar->y;
}

int DP_msg_annotation_reshape_width(DP_MsgAnnotationReshape *mar)
{
    DP_ASSERT(mar);
    return mar->width;
}

int DP_msg_annotation_reshape_height(DP_MsgAnnotationReshape *mar)
{
    DP_ASSERT(mar);
    return mar->height;
}
