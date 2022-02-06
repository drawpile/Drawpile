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
#include "interval.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define PAYLOAD_LENGTH 2

struct DP_MsgInterval {
    uint16_t msecs;
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return PAYLOAD_LENGTH;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgInterval *ms = DP_msg_interval_cast(msg);
    DP_ASSERT(ms);
    return DP_write_bigendian_uint16(ms->msecs, data);
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgInterval *ms = DP_msg_interval_cast(msg);
    DP_ASSERT(ms);
    DP_RETURN_UNLESS(DP_text_writer_write_uint(writer, "msecs", ms->msecs));
    return true;
}

static bool equals(DP_Message *restrict msg, DP_Message *restrict other)
{
    DP_MsgInterval *a = DP_msg_interval_cast(msg);
    DP_MsgInterval *b = DP_msg_interval_cast(other);
    return a->msecs == b->msecs;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_interval_new(unsigned int context_id, unsigned int msecs)
{
    DP_ASSERT(msecs <= UINT16_MAX);
    DP_MsgInterval *ms;
    DP_Message *msg =
        DP_message_new(DP_MSG_INTERVAL, context_id, &methods, sizeof(*ms));
    ms = DP_message_internal(msg);
    *ms = (DP_MsgInterval){DP_uint_to_uint16(msecs)};
    return msg;
}

DP_Message *DP_msg_interval_deserialize(unsigned int context_id,
                                        const unsigned char *buffer,
                                        size_t length)
{
    if (length == PAYLOAD_LENGTH) {
        return DP_msg_interval_new(context_id,
                                   DP_read_bigendian_uint16(buffer));
    }
    else {
        DP_error_set("Wrong length for INTERVAL message: %zu", length);
        return NULL;
    }
}


DP_MsgInterval *DP_msg_interval_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_INTERVAL);
}


unsigned int DP_msg_interval_msecs(DP_MsgInterval *mi)
{
    DP_ASSERT(mi);
    return mi->msecs;
}
