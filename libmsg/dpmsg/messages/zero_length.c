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
#include "zero_length.h"
#include "../message.h"
#include <dpcommon/common.h>


static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    return 0;
}

static size_t serialize_payload(DP_UNUSED DP_Message *msg,
                                DP_UNUSED unsigned char *data)
{
    return 0;
}

static bool write_payload_text(DP_UNUSED DP_Message *msg,
                               DP_UNUSED DP_TextWriter *writer)
{
    return true;
}

static bool equals(DP_UNUSED DP_Message *restrict msg,
                   DP_UNUSED DP_Message *restrict other)
{
    return true;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_zero_length_new(DP_MessageType type, unsigned int context_id)
{
    return DP_message_new(type, context_id, &methods, 0);
}

DP_Message *DP_zero_length_deserialize(DP_MessageType type,
                                       unsigned int context_id,
                                       DP_UNUSED const unsigned char *buffer,
                                       size_t length)
{
    if (length == 0) {
        return DP_zero_length_new(type, context_id);
    }
    else {
        const char *name = DP_message_type_enum_name_unprefixed(type);
        DP_error_set("Wrong length for %s message: %zu", name, length);
        return NULL;
    }
}
