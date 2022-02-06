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
#include "put_image.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PREFIX_LENGTH      19
#define MAX_IMAGE_SIZE     (0xffff - PREFIX_LENGTH)
#define MIN_PAYLOAD_LENGTH PREFIX_LENGTH

struct DP_MsgPutImage {
    uint16_t layer_id;
    uint8_t blend_mode;
    int32_t x, y;
    int32_t width, height;
    size_t image_size;
    unsigned char image[];
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    DP_MsgPutImage *mpi = DP_msg_put_image_cast(msg);
    DP_ASSERT(mpi);
    return PREFIX_LENGTH + mpi->image_size;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgPutImage *mpi = DP_msg_put_image_cast(msg);
    DP_ASSERT(mpi);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mpi->layer_id, data + written);
    written += DP_write_bigendian_uint8(mpi->blend_mode, data + written);
    written += DP_write_bigendian_int32(mpi->x, data + written);
    written += DP_write_bigendian_int32(mpi->y, data + written);
    written += DP_write_bigendian_int32(mpi->width, data + written);
    written += DP_write_bigendian_int32(mpi->height, data + written);
    size_t image_size = mpi->image_size;
    memcpy(data + written, mpi->image, image_size);
    return written + image_size;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgPutImage *mpi = DP_msg_put_image_cast(msg);
    DP_ASSERT(mpi);
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "h", mpi->height));
    DP_RETURN_UNLESS(DP_text_writer_write_base64(writer, "img", mpi->image,
                                                 mpi->image_size));
    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mpi->layer_id));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "mode", mpi->blend_mode));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "w", mpi->width));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "x", mpi->x));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "y", mpi->y));
    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgPutImage *a = DP_msg_put_image_cast(msg);
    DP_MsgPutImage *b = DP_msg_put_image_cast(other);
    return a->layer_id == b->layer_id && a->x == b->x && a->y == b->y
        && a->width == b->width && a->height == b->height
        && a->image_size == b->image_size
        && memcmp(a->image, b->image, a->image_size) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_put_image_new(unsigned int context_id, int layer_id,
                                 int blend_mode, int x, int y, int width,
                                 int height, const unsigned char *image,
                                 size_t image_size)
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
    DP_ASSERT(image_size <= MAX_IMAGE_SIZE);
    DP_Message *msg =
        DP_message_new(DP_MSG_PUT_IMAGE, context_id, &methods,
                       DP_FLEX_SIZEOF(DP_MsgPutImage, image, image_size));
    DP_MsgPutImage *mpi = DP_message_internal(msg);
    mpi->layer_id = DP_int_to_uint16(layer_id);
    mpi->blend_mode = DP_int_to_uint8(blend_mode);
    mpi->x = DP_int_to_int32(x);
    mpi->y = DP_int_to_int32(y);
    mpi->width = DP_int_to_int32(width);
    mpi->height = DP_int_to_int32(height);
    mpi->image_size = image_size;
    memcpy(mpi->image, image, image_size);
    return msg;
}

DP_Message *DP_msg_put_image_deserialize(unsigned int context_id,
                                         const unsigned char *buffer,
                                         size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        return DP_msg_put_image_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_uint8(buffer + 2),
            DP_read_bigendian_int32(buffer + 3),
            DP_read_bigendian_int32(buffer + 7),
            DP_read_bigendian_int32(buffer + 11),
            DP_read_bigendian_int32(buffer + 15), buffer + PREFIX_LENGTH,
            length - PREFIX_LENGTH);
    }
    else {
        DP_error_set("Wrong length for PUT_IMAGE message: %zu", length);
        return NULL;
    }
}


DP_MsgPutImage *DP_msg_put_image_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_PUT_IMAGE);
}


int DP_msg_put_image_layer_id(DP_MsgPutImage *mpi)
{
    DP_ASSERT(mpi);
    return mpi->layer_id;
}

int DP_msg_put_image_blend_mode(DP_MsgPutImage *mpi)
{
    DP_ASSERT(mpi);
    return mpi->blend_mode;
}

int DP_msg_put_image_x(DP_MsgPutImage *mpi)
{
    DP_ASSERT(mpi);
    return mpi->x;
}

int DP_msg_put_image_y(DP_MsgPutImage *mpi)
{
    DP_ASSERT(mpi);
    return mpi->y;
}

int DP_msg_put_image_width(DP_MsgPutImage *mpi)
{
    DP_ASSERT(mpi);
    return mpi->width;
}

int DP_msg_put_image_height(DP_MsgPutImage *mpi)
{
    DP_ASSERT(mpi);
    return mpi->height;
}

const unsigned char *DP_msg_put_image_image(DP_MsgPutImage *mpi,
                                            size_t *out_image_size)
{
    DP_ASSERT(mpi);
    if (out_image_size) {
        *out_image_size = mpi->image_size;
    }
    return mpi->image;
}
