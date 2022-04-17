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
#include "annotation_delete.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 2

struct DP_MsgAnnotationDelete {
    uint16_t annotation_id;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgAnnotationDelete *mad = DP_msg_annotation_delete_cast(msg);
    DP_ASSERT(mad);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mad->annotation_id, data + written);
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgAnnotationDelete *mad = DP_msg_annotation_delete_cast(msg);
    DP_ASSERT(mad);
    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "id", mad->annotation_id));
    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgAnnotationDelete *a = DP_msg_annotation_delete_cast(msg);
    DP_MsgAnnotationDelete *b = DP_msg_annotation_delete_cast(other);
    return a->annotation_id == b->annotation_id;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_annotation_delete_new(unsigned int context_id,
                                         int annotation_id)
{
    DP_ASSERT(annotation_id >= 0);
    DP_ASSERT(annotation_id <= UINT16_MAX);
    DP_MsgAnnotationDelete *mad;
    DP_Message *msg = DP_message_new(DP_MSG_ANNOTATION_DELETE, context_id,
                                     &methods, sizeof(*mad));
    mad = DP_message_internal(msg);
    (*mad) = (DP_MsgAnnotationDelete){DP_int_to_uint16(annotation_id)};
    return msg;
}

DP_Message *DP_msg_annotation_delete_deserialize(unsigned int context_id,
                                                 const unsigned char *buffer,
                                                 size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_annotation_delete_new(context_id,
                                            DP_read_bigendian_uint16(buffer));
    }
    else {
        DP_error_set("Wrong length for ANNOTATION_DELETE message: %zu", length);
        return NULL;
    }
}


DP_MsgAnnotationDelete *DP_msg_annotation_delete_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_ANNOTATION_DELETE);
}


int DP_msg_annotation_delete_annotation_id(DP_MsgAnnotationDelete *mad)
{
    DP_ASSERT(mad);
    return mad->annotation_id;
}
