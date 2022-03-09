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
#include "layer_order.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <stdint.h>


struct DP_MsgLayerOrder {
    int count;
    int layer_ids[];
};

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    DP_MsgLayerOrder *mlo = DP_msg_layer_order_cast(msg);
    return DP_int_to_size(mlo->count) * 2;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgLayerOrder *mlo = DP_msg_layer_order_cast(msg);
    DP_ASSERT(mlo);
    size_t written = 0;
    int count = mlo->count;
    for (int i = 0; i < count; ++i) {
        uint16_t layer_id = DP_int_to_uint16(mlo->layer_ids[i]);
        written += DP_write_bigendian_uint16(layer_id, data + written);
    }
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgLayerOrder *mlo = DP_msg_layer_order_cast(msg);
    DP_ASSERT(mlo);

    DP_RETURN_UNLESS(DP_text_writer_raw_print(writer, " layers="));
    DP_RETURN_UNLESS(DP_text_writer_write_id_list(writer, "layers",
                                                  mlo->layer_ids, mlo->count));

    return true;
}

static bool layer_ids_equal(int *a, int *b, int count)
{
    for (int i = 0; i < count; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgLayerOrder *a = DP_msg_layer_order_cast(msg);
    DP_MsgLayerOrder *b = DP_msg_layer_order_cast(other);
    return a->count == b->count
        && layer_ids_equal(a->layer_ids, b->layer_ids, a->count);
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_layer_order_new(unsigned int context_id, int count,
                                   int (*get_layer_id)(void *user, int i),
                                   void *user)
{
    DP_ASSERT(count >= 0);
    DP_Message *msg = DP_message_new(
        DP_MSG_LAYER_ORDER, context_id, &methods,
        DP_FLEX_SIZEOF(DP_MsgLayerOrder, layer_ids, DP_int_to_size(count)));
    DP_MsgLayerOrder *mlo = DP_message_internal(msg);
    mlo->count = count;
    for (int i = 0; i < count; ++i) {
        int layer_id = get_layer_id(user, i);
        DP_ASSERT(layer_id >= 0);
        DP_ASSERT(layer_id <= UINT16_MAX);
        mlo->layer_ids[i] = layer_id;
    }
    return msg;
}

static int deserialize_layer_id(void *user, int i)
{
    const unsigned char *buffer = user;
    return DP_read_bigendian_uint16(buffer + i * 2);
}

DP_Message *DP_msg_layer_order_deserialize(unsigned int context_id,
                                           const unsigned char *buffer,
                                           size_t length)
{
    if (length % 2 == 0) {
        return DP_msg_layer_order_new(context_id, DP_size_to_int(length / 2),
                                      deserialize_layer_id, (void *)buffer);
    }
    else {
        DP_error_set("Wrong length for LAYER_ORDER message: %zu", length);
        return NULL;
    }
}


DP_MsgLayerOrder *DP_msg_layer_order_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_LAYER_ORDER);
}


int DP_msg_layer_order_layer_id_count(DP_MsgLayerOrder *mlo)
{
    DP_ASSERT(mlo);
    return mlo->count;
}

int DP_msg_layer_order_layer_id_at(DP_MsgLayerOrder *mlo, int index)
{
    DP_ASSERT(mlo);
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < mlo->count);
    return mlo->layer_ids[index];
}
