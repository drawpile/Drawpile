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
#include "layer_retitle.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define MIN_PAYLOAD_LENGTH 2

struct DP_MsgLayerRetitle {
    uint16_t layer_id;
    size_t title_length;
    char title[];
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    DP_MsgLayerRetitle *mlr = DP_msg_layer_retitle_cast(msg);
    DP_ASSERT(mlr);
    return MIN_PAYLOAD_LENGTH + mlr->title_length;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgLayerRetitle *mlr = DP_msg_layer_retitle_cast(msg);
    DP_ASSERT(mlr);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mlr->layer_id, data + written);
    size_t title_length = mlr->title_length;
    memcpy(data + written, mlr->title, title_length);
    return written + title_length;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgLayerRetitle *mlr = DP_msg_layer_retitle_cast(msg);
    DP_ASSERT(mlr);
    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "id", mlr->layer_id));
    DP_RETURN_UNLESS(DP_text_writer_write_string(writer, "title", mlr->title));
    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgLayerRetitle *a = DP_msg_layer_retitle_cast(msg);
    DP_MsgLayerRetitle *b = DP_msg_layer_retitle_cast(other);
    return a->layer_id == b->layer_id && a->title_length == b->title_length
        && memcmp(a->title, b->title, a->title_length) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_layer_retitle_new(unsigned int context_id, int layer_id,
                                     const char *title, size_t title_length)
{
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    DP_MsgLayerRetitle *mlr;
    DP_Message *msg = DP_message_new(DP_MSG_LAYER_RETITLE, context_id, &methods,
                                     sizeof(*mlr) + title_length + 1);
    mlr = DP_message_internal(msg);
    mlr->layer_id = DP_int_to_uint16(layer_id);
    mlr->title_length = title_length;
    if (mlr->title_length > 0) {
        memcpy(mlr->title, title, title_length);
    }
    mlr->title[title_length] = '\0';
    return msg;
}

DP_Message *DP_msg_layer_retitle_deserialize(unsigned int context_id,
                                             const unsigned char *buffer,
                                             size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        return DP_msg_layer_retitle_new(
            context_id, DP_read_bigendian_uint16(buffer),
            (const char *)buffer + 2, length - MIN_PAYLOAD_LENGTH);
    }
    else {
        DP_error_set("Wrong length for LAYER_RETITLE message: %zu", length);
        return NULL;
    }
}


DP_MsgLayerRetitle *DP_msg_layer_retitle_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_LAYER_RETITLE);
}


int DP_msg_layer_retitle_layer_id(DP_MsgLayerRetitle *mlr)
{
    DP_ASSERT(mlr);
    return mlr->layer_id;
}

const char *DP_msg_layer_retitle_title(DP_MsgLayerRetitle *mlr,
                                       size_t *out_length)
{
    DP_ASSERT(mlr);
    if (out_length) {
        *out_length = mlr->title_length;
    }
    return mlr->title;
}
