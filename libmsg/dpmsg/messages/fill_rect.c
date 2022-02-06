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
#include "fill_rect.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 23

struct DP_MsgFillRect {
    uint16_t layer_id;
    uint8_t blend_mode;
    int32_t x, y;
    int32_t width, height;
    uint32_t color;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgFillRect *mfr = DP_msg_fill_rect_cast(msg);
    DP_ASSERT(mfr);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mfr->layer_id, data + written);
    written += DP_write_bigendian_uint8(mfr->blend_mode, data + written);
    written += DP_write_bigendian_int32(mfr->x, data + written);
    written += DP_write_bigendian_int32(mfr->y, data + written);
    written += DP_write_bigendian_int32(mfr->width, data + written);
    written += DP_write_bigendian_int32(mfr->height, data + written);
    written += DP_write_bigendian_uint32(mfr->color, data + written);
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgFillRect *mfr = DP_msg_fill_rect_cast(msg);
    DP_ASSERT(mfr);
    DP_RETURN_UNLESS(
        DP_text_writer_write_int(writer, "blend", mfr->blend_mode));
    DP_RETURN_UNLESS(
        DP_text_writer_write_argb_color(writer, "color", mfr->color));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "h", mfr->height));
    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mfr->layer_id));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "w", mfr->width));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "x", mfr->x));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "y", mfr->y));
    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgFillRect *a = DP_msg_fill_rect_cast(msg);
    DP_MsgFillRect *b = DP_msg_fill_rect_cast(other);
    return a->layer_id == b->layer_id && a->blend_mode == b->blend_mode
        && a->x == b->x && a->y == b->y && a->width == b->width
        && a->height == b->height && a->color == b->color;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_fill_rect_new(unsigned int context_id, int layer_id,
                                 int blend_mode, int x, int y, int width,
                                 int height, uint32_t color)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_ASSERT(blend_mode >= 0);
    DP_ASSERT(blend_mode <= UINT8_MAX);
    DP_ASSERT(x >= 0);
    DP_ASSERT(x <= INT32_MAX);
    DP_ASSERT(y >= 0);
    DP_ASSERT(y <= INT32_MAX);
    DP_ASSERT(width >= 0);
    DP_ASSERT(width <= INT32_MAX);
    DP_ASSERT(height >= 0);
    DP_ASSERT(height <= INT32_MAX);
    DP_MsgFillRect *mfr;
    DP_Message *msg =
        DP_message_new(DP_MSG_FILL_RECT, context_id, &methods, sizeof(*mfr));
    mfr = DP_message_internal(msg);
    mfr->layer_id = DP_int_to_uint16(layer_id);
    mfr->blend_mode = DP_int_to_uint8(blend_mode);
    mfr->x = DP_int_to_int32(x);
    mfr->y = DP_int_to_int32(y);
    mfr->width = DP_int_to_int32(width);
    mfr->height = DP_int_to_int32(height);
    mfr->color = color;
    return msg;
}

DP_Message *DP_msg_fill_rect_deserialize(unsigned int context_id,
                                         const unsigned char *buffer,
                                         size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_fill_rect_new(context_id,
                                    DP_read_bigendian_uint16(buffer),
                                    DP_read_bigendian_uint8(buffer + 2),
                                    DP_read_bigendian_int32(buffer + 3),
                                    DP_read_bigendian_int32(buffer + 7),
                                    DP_read_bigendian_int32(buffer + 11),
                                    DP_read_bigendian_int32(buffer + 15),
                                    DP_read_bigendian_uint32(buffer + 19));
    }
    else {
        DP_error_set("Wrong length for FILL_RECT message: %zu", length);
        return NULL;
    }
}


DP_MsgFillRect *DP_msg_fill_rect_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_FILL_RECT);
}


int DP_msg_fill_rect_layer_id(DP_MsgFillRect *mfr)
{
    DP_ASSERT(mfr);
    return mfr->layer_id;
}

int DP_msg_fill_rect_blend_mode(DP_MsgFillRect *mfr)
{
    DP_ASSERT(mfr);
    return mfr->blend_mode;
}

int DP_msg_fill_rect_x(DP_MsgFillRect *mfr)
{
    DP_ASSERT(mfr);
    return mfr->x;
}

int DP_msg_fill_rect_y(DP_MsgFillRect *mfr)
{
    DP_ASSERT(mfr);
    return mfr->y;
}

int DP_msg_fill_rect_width(DP_MsgFillRect *mfr)
{
    DP_ASSERT(mfr);
    return mfr->width;
}

int DP_msg_fill_rect_height(DP_MsgFillRect *mfr)
{
    DP_ASSERT(mfr);
    return mfr->height;
}

uint32_t DP_msg_fill_rect_color(DP_MsgFillRect *mfr)
{
    DP_ASSERT(mfr);
    return mfr->color;
}
