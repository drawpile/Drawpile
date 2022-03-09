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
#include "region_move.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PREFIX_LENGTH      50
#define MAX_MASK_SIZE      (0xffff - PREFIX_LENGTH)
#define MIN_PAYLOAD_LENGTH PREFIX_LENGTH

struct DP_MsgRegionMove {
    uint16_t layer_id;
    int32_t src_x, src_y;
    int32_t src_width, src_height;
    int32_t x1, y1, x2, y2, x3, y3, x4, y4;
    size_t mask_size;
    unsigned char mask[];
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    DP_MsgRegionMove *mrm = DP_msg_region_move_cast(msg);
    DP_ASSERT(mrm);
    return PREFIX_LENGTH + mrm->mask_size;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgRegionMove *mrm = DP_msg_region_move_cast(msg);
    DP_ASSERT(mrm);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mrm->layer_id, data + written);
    written += DP_write_bigendian_int32(mrm->src_x, data + written);
    written += DP_write_bigendian_int32(mrm->src_y, data + written);
    written += DP_write_bigendian_int32(mrm->src_width, data + written);
    written += DP_write_bigendian_int32(mrm->src_height, data + written);
    written += DP_write_bigendian_int32(mrm->x1, data + written);
    written += DP_write_bigendian_int32(mrm->y1, data + written);
    written += DP_write_bigendian_int32(mrm->x2, data + written);
    written += DP_write_bigendian_int32(mrm->y2, data + written);
    written += DP_write_bigendian_int32(mrm->x3, data + written);
    written += DP_write_bigendian_int32(mrm->y3, data + written);
    written += DP_write_bigendian_int32(mrm->x4, data + written);
    written += DP_write_bigendian_int32(mrm->y4, data + written);
    size_t mask_size = mrm->mask_size;
    memcpy(data + written, mrm->mask, mask_size);
    return written + mask_size;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgRegionMove *mrm = DP_msg_region_move_cast(msg);
    DP_ASSERT(mrm);

    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "bh", mrm->src_height));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "bw", mrm->src_width));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "bx", mrm->src_x));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "by", mrm->src_y));
    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "layer", mrm->layer_id));

    size_t mask_size = mrm->mask_size;
    if (mask_size != 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_base64(writer, "mask", mrm->mask, mask_size));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "x1", mrm->x1));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "x2", mrm->x2));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "x3", mrm->x3));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "x4", mrm->x4));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "y1", mrm->y1));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "y2", mrm->y2));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "y3", mrm->y3));
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "y4", mrm->y4));

    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgRegionMove *a = DP_msg_region_move_cast(msg);
    DP_MsgRegionMove *b = DP_msg_region_move_cast(other);
    return a->layer_id == b->layer_id && a->src_x == b->src_x
        && a->src_y == b->src_y && a->src_width == b->src_width
        && a->src_height == b->src_height && a->x1 == b->x1 && a->y1 == b->y1
        && a->x2 == b->x2 && a->y2 == b->y2 && a->x3 == b->x3 && a->y3 == b->y3
        && a->x4 == b->x4 && a->y4 == b->y4 && a->mask_size == b->mask_size
        && memcmp(a->mask, b->mask, a->mask_size) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_region_move_new(unsigned int context_id, int layer_id,
                                   int src_x, int src_y, int src_width,
                                   int src_height, int x1, int y1, int x2,
                                   int y2, int x3, int y3, int x4, int y4,
                                   const unsigned char *mask, size_t mask_size)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_ASSERT(src_x >= INT32_MIN);
    DP_ASSERT(src_x <= INT32_MAX);
    DP_ASSERT(src_y >= INT32_MIN);
    DP_ASSERT(src_y <= INT32_MAX);
    DP_ASSERT(src_width >= INT32_MIN);
    DP_ASSERT(src_width <= INT32_MAX);
    DP_ASSERT(src_height >= INT32_MIN);
    DP_ASSERT(src_height <= INT32_MAX);
    DP_ASSERT(x1 >= INT32_MIN);
    DP_ASSERT(x1 <= INT32_MAX);
    DP_ASSERT(y1 >= INT32_MIN);
    DP_ASSERT(y1 <= INT32_MAX);
    DP_ASSERT(x2 >= INT32_MIN);
    DP_ASSERT(x2 <= INT32_MAX);
    DP_ASSERT(y2 >= INT32_MIN);
    DP_ASSERT(y2 <= INT32_MAX);
    DP_ASSERT(x3 >= INT32_MIN);
    DP_ASSERT(x3 <= INT32_MAX);
    DP_ASSERT(y3 >= INT32_MIN);
    DP_ASSERT(y3 <= INT32_MAX);
    DP_ASSERT(x4 >= INT32_MIN);
    DP_ASSERT(x4 <= INT32_MAX);
    DP_ASSERT(y4 >= INT32_MIN);
    DP_ASSERT(y4 <= INT32_MAX);
    DP_ASSERT(mask_size <= MAX_MASK_SIZE);
    DP_Message *msg =
        DP_message_new(DP_MSG_REGION_MOVE, context_id, &methods,
                       DP_FLEX_SIZEOF(DP_MsgRegionMove, mask, mask_size));
    DP_MsgRegionMove *mrm = DP_message_internal(msg);
    mrm->layer_id = DP_int_to_uint16(layer_id);
    mrm->src_x = DP_int_to_int32(src_x);
    mrm->src_y = DP_int_to_int32(src_y);
    mrm->src_width = DP_int_to_int32(src_width);
    mrm->src_height = DP_int_to_int32(src_height);
    mrm->x1 = DP_int_to_int32(x1);
    mrm->y1 = DP_int_to_int32(y1);
    mrm->x2 = DP_int_to_int32(x2);
    mrm->y2 = DP_int_to_int32(y2);
    mrm->x3 = DP_int_to_int32(x3);
    mrm->y3 = DP_int_to_int32(y3);
    mrm->x4 = DP_int_to_int32(x4);
    mrm->y4 = DP_int_to_int32(y4);
    mrm->mask_size = mask_size;
    if (mask_size != 0) {
        memcpy(mrm->mask, mask, mask_size);
    }
    return msg;
}

DP_Message *DP_msg_region_move_deserialize(unsigned int context_id,
                                           const unsigned char *buffer,
                                           size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        return DP_msg_region_move_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_int32(buffer + 2),
            DP_read_bigendian_int32(buffer + 6),
            DP_read_bigendian_int32(buffer + 10),
            DP_read_bigendian_int32(buffer + 14),
            DP_read_bigendian_int32(buffer + 18),
            DP_read_bigendian_int32(buffer + 22),
            DP_read_bigendian_int32(buffer + 26),
            DP_read_bigendian_int32(buffer + 30),
            DP_read_bigendian_int32(buffer + 34),
            DP_read_bigendian_int32(buffer + 38),
            DP_read_bigendian_int32(buffer + 42),
            DP_read_bigendian_int32(buffer + 46), buffer + PREFIX_LENGTH,
            length - PREFIX_LENGTH);
    }
    else {
        DP_error_set("Wrong length for REGION_MOVE message: %zu", length);
        return NULL;
    }
}


DP_MsgRegionMove *DP_msg_region_move_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_REGION_MOVE);
}


int DP_msg_region_move_layer_id(DP_MsgRegionMove *mrm)
{
    DP_ASSERT(mrm);
    return mrm->layer_id;
}

void DP_msg_region_move_src_rect(DP_MsgRegionMove *mrm, int *out_src_x,
                                 int *out_src_y, int *out_src_width,
                                 int *out_src_height)
{
    DP_ASSERT(mrm);
    if (out_src_x) {
        *out_src_x = mrm->src_x;
    }
    if (out_src_y) {
        *out_src_y = mrm->src_y;
    }
    if (out_src_width) {
        *out_src_width = mrm->src_width;
    }
    if (out_src_height) {
        *out_src_height = mrm->src_height;
    }
}

void DP_msg_region_move_dst_quad(DP_MsgRegionMove *mrm, int *out_x1,
                                 int *out_y1, int *out_x2, int *out_y2,
                                 int *out_x3, int *out_y3, int *out_x4,
                                 int *out_y4)
{
    DP_ASSERT(mrm);
    if (out_x1) {
        *out_x1 = mrm->x1;
    }
    if (out_y1) {
        *out_y1 = mrm->y1;
    }
    if (out_x2) {
        *out_x2 = mrm->x2;
    }
    if (out_y2) {
        *out_y2 = mrm->y2;
    }
    if (out_x3) {
        *out_x3 = mrm->x3;
    }
    if (out_y3) {
        *out_y3 = mrm->y3;
    }
    if (out_x4) {
        *out_x4 = mrm->x4;
    }
    if (out_y4) {
        *out_y4 = mrm->y4;
    }
}

const unsigned char *DP_msg_region_move_mask(DP_MsgRegionMove *mrm,
                                             size_t *out_mask_size)
{
    DP_ASSERT(mrm);
    size_t mask_size = mrm->mask_size;
    if (out_mask_size) {
        *out_mask_size = mask_size;
    }
    return mask_size == 0 ? NULL : mrm->mask;
}
