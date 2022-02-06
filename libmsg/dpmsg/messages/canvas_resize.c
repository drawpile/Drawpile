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
#include "canvas_resize.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 16

struct DP_MsgCanvasResize {
    int32_t top, right, bottom, left;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgCanvasResize *mcr = DP_msg_canvas_resize_cast(msg);
    DP_ASSERT(mcr);
    size_t written = 0;
    written += DP_write_bigendian_int32(mcr->top, data + written);
    written += DP_write_bigendian_int32(mcr->right, data + written);
    written += DP_write_bigendian_int32(mcr->bottom, data + written);
    written += DP_write_bigendian_int32(mcr->left, data + written);
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgCanvasResize *mcr = DP_msg_canvas_resize_cast(msg);
    DP_ASSERT(mcr);
    if (mcr->bottom != 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_int(writer, "bottom", mcr->bottom));
    }
    if (mcr->left != 0) {
        DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "left", mcr->left));
    }
    if (mcr->right != 0) {
        DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "right", mcr->right));
    }
    if (mcr->top != 0) {
        DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "top", mcr->top));
    }
    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgCanvasResize *a = DP_msg_canvas_resize_cast(msg);
    DP_MsgCanvasResize *b = DP_msg_canvas_resize_cast(other);
    return a->top == b->top && a->right == b->right && a->bottom == b->bottom
        && a->left == b->left;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_canvas_resize_new(unsigned int context_id, int top,
                                     int right, int bottom, int left)
{
    DP_MsgCanvasResize *mcr;
    DP_Message *msg = DP_message_new(DP_MSG_CANVAS_RESIZE, context_id, &methods,
                                     sizeof(*mcr));
    mcr = DP_message_internal(msg);
    mcr->top = DP_int_to_int32(top);
    mcr->right = DP_int_to_int32(right);
    mcr->bottom = DP_int_to_int32(bottom);
    mcr->left = DP_int_to_int32(left);
    return msg;
}

DP_Message *DP_msg_canvas_resize_deserialize(unsigned int context_id,
                                             const unsigned char *buffer,
                                             size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_canvas_resize_new(context_id,
                                        DP_read_bigendian_int32(buffer),
                                        DP_read_bigendian_int32(buffer + 4),
                                        DP_read_bigendian_int32(buffer + 8),
                                        DP_read_bigendian_int32(buffer + 12));
    }
    else {
        DP_error_set("Wrong length for CANVAS_RESIZE message: %zu", length);
        return NULL;
    }
}


DP_MsgCanvasResize *DP_msg_canvas_resize_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_CANVAS_RESIZE);
}

int DP_msg_canvas_resize_top(DP_MsgCanvasResize *mcr)
{
    DP_ASSERT(mcr);
    return mcr->top;
}

int DP_msg_canvas_resize_right(DP_MsgCanvasResize *mcr)
{
    DP_ASSERT(mcr);
    return mcr->right;
}

int DP_msg_canvas_resize_bottom(DP_MsgCanvasResize *mcr)
{
    DP_ASSERT(mcr);
    return mcr->bottom;
}

int DP_msg_canvas_resize_left(DP_MsgCanvasResize *mcr)
{
    DP_ASSERT(mcr);
    return mcr->left;
}

void DP_msg_canvas_resize_dimensions(DP_MsgCanvasResize *mcr, int *out_top,
                                     int *out_right, int *out_bottom,
                                     int *out_left)
{
    DP_ASSERT(mcr);
    if (out_top) {
        *out_top = mcr->top;
    }
    if (out_right) {
        *out_right = mcr->right;
    }
    if (out_bottom) {
        *out_bottom = mcr->bottom;
    }
    if (out_left) {
        *out_left = mcr->left;
    }
}
