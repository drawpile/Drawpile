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
#include "put_tile.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PREFIX_LENGTH      9
#define MAX_IMAGE_SIZE     (0xffff - 8)
#define MIN_PAYLOAD_LENGTH (PREFIX_LENGTH + 4)

struct DP_MsgPutTile {
    uint16_t layer_id;
    uint16_t x, y;
    uint16_t repeat;
    uint8_t sublayer_id;
    size_t image_size;
    unsigned char image[];
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    DP_MsgPutTile *mpt = DP_msg_put_tile_cast(msg);
    DP_ASSERT(mpt);
    return PREFIX_LENGTH + mpt->image_size;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgPutTile *mpt = DP_msg_put_tile_cast(msg);
    DP_ASSERT(mpt);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mpt->layer_id, data + written);
    written += DP_write_bigendian_uint8(mpt->sublayer_id, data + written);
    written += DP_write_bigendian_uint16(mpt->x, data + written);
    written += DP_write_bigendian_uint16(mpt->y, data + written);
    written += DP_write_bigendian_uint16(mpt->repeat, data + written);
    size_t image_size = mpt->image_size;
    memcpy(data + written, mpt->image, image_size);
    return written + image_size;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgPutTile *mpt = DP_msg_put_tile_cast(msg);
    DP_ASSERT(mpt);

    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "col", mpt->x));

    uint32_t color;
    if (DP_msg_put_tile_color(mpt, &color)) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_argb_color(writer, "color", color));
    }
    else {
        DP_RETURN_UNLESS(DP_text_writer_write_base64(writer, "img", mpt->image,
                                                     mpt->image_size));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mpt->layer_id));

    int repeat = mpt->repeat;
    if (repeat != 0) {
        DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "repeat", repeat));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "row", mpt->y));

    int sublayer_id = mpt->sublayer_id;
    if (sublayer_id != 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_int(writer, "sublayer", sublayer_id));
    }

    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgPutTile *a = DP_msg_put_tile_cast(msg);
    DP_MsgPutTile *b = DP_msg_put_tile_cast(other);
    return a->layer_id == b->layer_id && a->x == b->x && a->y == b->y
        && a->repeat == b->repeat && a->sublayer_id == b->sublayer_id
        && a->image_size == b->image_size
        && memcmp(a->image, b->image, a->image_size) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_put_tile_new(unsigned int context_id, int layer_id,
                                int sublayer_id, int x, int y, int repeat,
                                const unsigned char *image, size_t image_size)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_ASSERT(sublayer_id >= 0);
    DP_ASSERT(sublayer_id <= UINT8_MAX);
    DP_ASSERT(x >= 0);
    DP_ASSERT(x <= UINT16_MAX);
    DP_ASSERT(y >= 0);
    DP_ASSERT(y <= UINT16_MAX);
    DP_ASSERT(repeat >= 0);
    DP_ASSERT(repeat <= UINT16_MAX);
    DP_ASSERT(image_size >= 4);
    DP_ASSERT(image_size <= MAX_IMAGE_SIZE);
    DP_Message *msg =
        DP_message_new(DP_MSG_PUT_TILE, context_id, &methods,
                       DP_FLEX_SIZEOF(DP_MsgPutTile, image, image_size));
    DP_MsgPutTile *mpt = DP_message_internal(msg);
    mpt->layer_id = DP_int_to_uint16(layer_id);
    mpt->sublayer_id = DP_int_to_uint8(sublayer_id);
    mpt->x = DP_int_to_uint16(x);
    mpt->y = DP_int_to_uint16(y);
    mpt->repeat = DP_int_to_uint16(repeat);
    mpt->image_size = image_size;
    memcpy(mpt->image, image, image_size);
    return msg;
}

DP_Message *DP_msg_put_tile_deserialize(unsigned int context_id,
                                        const unsigned char *buffer,
                                        size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        return DP_msg_put_tile_new(context_id, DP_read_bigendian_uint16(buffer),
                                   DP_read_bigendian_uint8(buffer + 2),
                                   DP_read_bigendian_uint16(buffer + 3),
                                   DP_read_bigendian_uint16(buffer + 5),
                                   DP_read_bigendian_uint16(buffer + 7),
                                   buffer + 9, length - 9);
    }
    else {
        DP_error_set("Wrong length for PUT_TILE message: %zu", length);
        return NULL;
    }
}


DP_MsgPutTile *DP_msg_put_tile_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_PUT_TILE);
}


int DP_msg_put_tile_layer_id(DP_MsgPutTile *mpt)
{
    DP_ASSERT(mpt);
    return mpt->layer_id;
}

int DP_msg_put_tile_sublayer_id(DP_MsgPutTile *mpt)
{
    DP_ASSERT(mpt);
    return mpt->sublayer_id;
}

int DP_msg_put_tile_x(DP_MsgPutTile *mpt)
{
    DP_ASSERT(mpt);
    return mpt->x;
}

int DP_msg_put_tile_y(DP_MsgPutTile *mpt)
{
    DP_ASSERT(mpt);
    return mpt->y;
}

int DP_msg_put_tile_repeat(DP_MsgPutTile *mpt)
{
    DP_ASSERT(mpt);
    return mpt->repeat;
}

bool DP_msg_put_tile_color(DP_MsgPutTile *mpt, uint32_t *out_color)
{
    DP_ASSERT(mpt);
    if (mpt->image_size == 4) {
        if (out_color) {
            *out_color = DP_read_bigendian_uint32(mpt->image);
        }
        return true;
    }
    else {
        return false;
    }
}

const unsigned char *DP_msg_put_tile_image(DP_MsgPutTile *mpt,
                                           size_t *out_image_size)
{
    DP_ASSERT(mpt);
    if (out_image_size) {
        *out_image_size = mpt->image_size;
    }
    return mpt->image;
}
