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
#include "undo.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 2

struct DP_MsgUndo {
    uint8_t override_id;
    bool is_redo;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgUndo *mu = DP_msg_undo_cast(msg);
    DP_ASSERT(mu);
    size_t written = 0;
    written += DP_write_bigendian_uint8(mu->override_id, data + written);
    written += DP_write_bigendian_uint8(mu->is_redo ? 1 : 0, data + written);
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgUndo *mu = DP_msg_undo_cast(msg);
    DP_ASSERT(mu);

    if (mu->override_id != 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_uint(writer, "override", mu->override_id));
    }

    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgUndo *a = DP_msg_undo_cast(msg);
    DP_MsgUndo *b = DP_msg_undo_cast(other);
    return a->override_id == b->override_id && a->is_redo == b->is_redo;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_undo_new(unsigned int context_id, unsigned int override_id,
                            bool is_redo)
{
    DP_ASSERT(override_id <= 255);
    DP_MsgUndo *mu;
    DP_Message *msg =
        DP_message_new(DP_MSG_UNDO, context_id, &methods, sizeof(*mu));
    mu = DP_message_internal(msg);
    mu->override_id = DP_uint_to_uint8(override_id);
    mu->is_redo = is_redo;
    return msg;
}

DP_Message *DP_msg_undo_deserialize(unsigned int context_id,
                                    const unsigned char *buffer, size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_undo_new(context_id, DP_read_bigendian_uint8(buffer),
                               DP_read_bigendian_uint8(buffer + 1));
    }
    else {
        DP_error_set("Wrong length for UNDO message: %zu", length);
        return NULL;
    }
}


DP_MsgUndo *DP_msg_undo_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_UNDO);
}

const char *DP_msg_undo_message_name(DP_Message *msg)
{
    DP_MsgUndo *mu = DP_msg_undo_cast(msg);
    DP_ASSERT(mu);
    return mu->is_redo ? "redo" : "undo";
}


unsigned int DP_msg_undo_override_id(DP_MsgUndo *mu)
{
    DP_ASSERT(mu);
    return mu->override_id;
}

bool DP_msg_undo_is_redo(DP_MsgUndo *mu)
{
    DP_ASSERT(mu);
    return mu->is_redo;
}
