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
#include "canvas_background.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>


#define MIN_PAYLOAD_LENGTH 4
#define MAX_IMAGE_SIZE     (0xffff - 8)

struct DP_MsgCanvasBackground {
    size_t image_size;
    unsigned char image[];
};

static size_t payload_length(DP_Message *msg)
{
    DP_MsgCanvasBackground *mcb = DP_msg_canvas_background_cast(msg);
    DP_ASSERT(mcb);
    return mcb->image_size;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgCanvasBackground *mcb = DP_msg_canvas_background_cast(msg);
    DP_ASSERT(mcb);
    memcpy(data, mcb->image, mcb->image_size);
    return mcb->image_size;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgCanvasBackground *mcb = DP_msg_canvas_background_cast(msg);
    DP_ASSERT(mcb);
    uint32_t color;
    if (DP_msg_canvas_background_color(mcb, &color)) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_argb_color(writer, "color", color));
    }
    else {
        DP_RETURN_UNLESS(DP_text_writer_write_base64(writer, "img", mcb->image,
                                                     mcb->image_size));
    }
    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgCanvasBackground *a = DP_msg_canvas_background_cast(msg);
    DP_MsgCanvasBackground *b = DP_msg_canvas_background_cast(other);
    return a->image_size == b->image_size
        && memcmp(a->image, b->image, a->image_size) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_canvas_background_new(unsigned int context_id,
                                         const unsigned char *image,
                                         size_t image_size)
{
    DP_ASSERT(image);
    DP_ASSERT(image_size >= MIN_PAYLOAD_LENGTH);
    DP_ASSERT(image_size <= MAX_IMAGE_SIZE);
    DP_MsgCanvasBackground *mcb;
    DP_Message *msg = DP_message_new(DP_MSG_CANVAS_BACKGROUND, context_id,
                                     &methods, sizeof(*mcb) + image_size);
    mcb = DP_message_internal(msg);
    mcb->image_size = image_size;
    memcpy(mcb->image, image, image_size);
    return msg;
}

DP_Message *DP_msg_canvas_background_deserialize(unsigned int context_id,
                                                 const unsigned char *buffer,
                                                 size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH && length <= MAX_IMAGE_SIZE) {
        return DP_msg_canvas_background_new(context_id, buffer, length);
    }
    else {
        DP_error_set("Wrong length for CANVAS_BACKGROUND message: %zu", length);
        return NULL;
    }
}


DP_MsgCanvasBackground *DP_msg_canvas_background_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_CANVAS_BACKGROUND);
}


bool DP_msg_canvas_background_color(DP_MsgCanvasBackground *mcb,
                                    uint32_t *out_color)
{
    DP_ASSERT(mcb);
    if (mcb->image_size == 4) {
        if (out_color) {
            *out_color = DP_read_bigendian_uint32(mcb->image);
        }
        return true;
    }
    else {
        return false;
    }
}

const unsigned char *DP_msg_canvas_background_image(DP_MsgCanvasBackground *mcb,
                                                    size_t *out_image_size)
{
    DP_ASSERT(mcb);
    if (out_image_size) {
        *out_image_size = mcb->image_size;
    }
    return mcb->image;
}
