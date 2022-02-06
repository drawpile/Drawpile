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
#include "internal.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>


struct DP_MsgInternal {
    DP_MsgInternalType type;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    DP_warn("DP_MsgInternal: payload_length called on internal message");
    return 0;
}

static size_t serialize_payload(DP_UNUSED DP_Message *msg,
                                DP_UNUSED unsigned char *data)
{
    DP_warn("DP_MsgInternal: serialize_payload called on internal message");
    return 0;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgInternal *mi = DP_msg_internal_cast(msg);
    DP_ASSERT(mi);
    DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "type", (int)mi->type));
    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgInternal *a = DP_msg_internal_cast(msg);
    DP_MsgInternal *b = DP_msg_internal_cast(other);
    return a->type == b->type;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_internal_reset_new(unsigned int context_id)
{
    DP_MsgInternal *mi;
    DP_Message *msg =
        DP_message_new(DP_MSG_INTERNAL, context_id, &methods, sizeof(*mi));
    mi = DP_message_internal(msg);
    mi->type = DP_MSG_INTERNAL_TYPE_RESET;
    return msg;
}


DP_MsgInternal *DP_msg_internal_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_INTERNAL);
}


DP_MsgInternalType DP_msg_internal_type(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    return mi->type;
}
