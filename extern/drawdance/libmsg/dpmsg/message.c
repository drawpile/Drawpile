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
#include "message.h"
#include "text_writer.h"
#include <dpcommon/atomic.h>
#include <dpcommon/binary.h>
#include <dpcommon/common.h>


typedef DP_Message *(*DP_MessageDeserializeFn)(unsigned int context_id,
                                               const unsigned char *buffer,
                                               size_t length);

struct DP_Message {
    DP_Atomic refcount;
    DP_MessageType type;
    unsigned int context_id;
    const DP_MessageMethods *methods;
    alignas(DP_max_align_t) unsigned char internal[];
};

DP_Message *DP_message_new(DP_MessageType type, unsigned int context_id,
                           const DP_MessageMethods *methods,
                           size_t internal_size)
{
    DP_ASSERT(type >= 0);
    DP_ASSERT(type <= DP_MESSAGE_MAX);
    DP_ASSERT(context_id <= UINT8_MAX);
    DP_ASSERT(methods);
    DP_ASSERT(methods->payload_length);
    DP_ASSERT(methods->serialize_payload);
    DP_ASSERT(methods->equals);
    DP_ASSERT(methods->write_payload_text);
    DP_ASSERT(internal_size <= SIZE_MAX - sizeof(DP_Message));
    DP_Message *msg = DP_malloc_zeroed(sizeof(*msg) + internal_size);
    DP_atomic_set(&msg->refcount, 1);
    msg->type = type;
    msg->context_id = context_id;
    msg->methods = methods;
    return msg;
}

DP_Message *DP_message_incref(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    DP_atomic_inc(&msg->refcount);
    return msg;
}

DP_Message *DP_message_incref_nullable(DP_Message *msg_or_null)
{
    return msg_or_null ? DP_message_incref(msg_or_null) : NULL;
}

void DP_message_decref(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    if (DP_atomic_dec(&msg->refcount)) {
        DP_free(msg);
    }
}

void DP_message_decref_nullable(DP_Message *msg_or_null)
{
    if (msg_or_null) {
        DP_message_decref(msg_or_null);
    }
}

int DP_message_refcount(DP_Message *msg)
{
    DP_ASSERT(msg);
    int refcount = DP_atomic_get(&msg->refcount);
    DP_ASSERT(refcount > 0);
    return refcount;
}


DP_MessageType DP_message_type(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    return msg->type;
}

const char *DP_message_name(DP_Message *msg)
{
    return DP_message_type_name(DP_message_type(msg));
}

unsigned int DP_message_context_id(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    return msg->context_id;
}

void DP_message_context_id_set(DP_Message *msg, unsigned int context_id)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    msg->context_id = context_id;
}

void *DP_message_internal(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    return msg->internal;
}

DP_Message *DP_message_from_internal(void *internal)
{
    DP_ASSERT(internal);
    unsigned char *bytes = internal;
    return (DP_Message *)(bytes - offsetof(DP_Message, internal));
}

void *DP_message_cast(DP_Message *msg, DP_MessageType type)
{
    if (msg) {
        DP_MessageType msg_type = msg->type;
        if (msg_type == type) {
            return DP_message_internal(msg);
        }
        else {
            DP_error_set("Wrong message type %d, wanted %d", msg_type, type);
        }
    }
    return NULL;
}

void *DP_message_cast2(DP_Message *msg, DP_MessageType type1,
                       DP_MessageType type2)
{
    if (msg) {
        DP_MessageType msg_type = msg->type;
        if (msg_type == type1 || msg_type == type2) {
            return DP_message_internal(msg);
        }
        else {
            DP_error_set("Wrong message type %d, wanted %d or %d", msg_type,
                         type1, type2);
        }
    }
    return NULL;
}

void *DP_message_cast3(DP_Message *msg, DP_MessageType type1,
                       DP_MessageType type2, DP_MessageType type3)
{
    if (msg) {
        DP_MessageType msg_type = msg->type;
        if (msg_type == type1 || msg_type == type2 || msg_type == type3) {
            return DP_message_internal(msg);
        }
        else {
            DP_error_set("Wrong message type %d, wanted %d, %d or %d", msg_type,
                         type1, type2, type3);
        }
    }
    return NULL;
}

size_t DP_message_length(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    return DP_MESSAGE_HEADER_LENGTH + msg->methods->payload_length(msg);
}

size_t DP_message_serialize(DP_Message *msg, bool write_body_length,
                            DP_GetMessageBufferFn get_buffer, void *user)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    DP_ASSERT(get_buffer);

    DP_MessageType type = msg->type;
    if (type < 0 || type > UINT8_MAX) {
        DP_error_set("Message type out of bounds: %d", (int)type);
        return 0;
    }

    unsigned int context_id = msg->context_id;
    if (context_id > UINT8_MAX) {
        DP_error_set("Message context id out of bounds: %u", context_id);
        return 0;
    }

    size_t length = msg->methods->payload_length(msg);
    if (length > UINT16_MAX) {
        DP_error_set("Message body length out of bounds: %zu", length);
        return 0;
    }

    size_t header_length = write_body_length ? DP_MESSAGE_HEADER_LENGTH : 2;
    size_t length_with_header = header_length + length;
    unsigned char *buffer = get_buffer(user, length_with_header);
    if (!buffer) {
        return 0;
    }

    size_t written = 0;
    if (write_body_length) {
        written +=
            DP_write_bigendian_uint16((uint16_t)length, buffer + written);
    }
    written += DP_write_bigendian_uint8((uint8_t)type, buffer + written);
    written += DP_write_bigendian_uint8((uint8_t)context_id, buffer + written);
    written += msg->methods->serialize_payload(msg, buffer + written);
    DP_ASSERT(written == length_with_header);
    return written;
}

bool DP_message_write_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    DP_ASSERT(writer);
    return DP_text_writer_start_message(writer, msg)
        && msg->methods->write_payload_text(msg, writer)
        && DP_text_writer_finish_message(writer);
}

bool DP_message_equals(DP_Message *msg, DP_Message *other)
{
    return (msg == other)
        || (msg && other && msg->type == other->type
            && msg->methods->equals(msg, other));
}


DP_Message *DP_message_deserialize_length(const unsigned char *buf,
                                          size_t bufsize, size_t body_length)
{
    DP_ASSERT(buf);
    size_t total_length = 2 + body_length;
    if (bufsize >= total_length) {
        return DP_message_deserialize_body(buf[0], buf[1], buf + 2,
                                           body_length);
    }
    else {
        DP_error_set("Buffer size %zu shorter than message length %zu", bufsize,
                     total_length);
        return NULL;
    }
}

DP_Message *DP_message_deserialize(const unsigned char *buf, size_t bufsize)
{
    if (bufsize >= DP_MESSAGE_HEADER_LENGTH) {
        DP_ASSERT(buf);
        size_t body_length = DP_read_bigendian_uint16(buf);
        return DP_message_deserialize_length(buf + 2, bufsize - 2, body_length);
    }
    else {
        DP_error_set("Buffer size %zu too short for message header", bufsize);
        return NULL;
    }
}


static bool has_alpha(uint32_t color)
{
    return color & 0xff000000;
}

bool DP_msg_draw_dabs_classic_indirect(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return has_alpha(DP_msg_draw_dabs_classic_color(mddc));
}

bool DP_msg_draw_dabs_pixel_indirect(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return has_alpha(DP_msg_draw_dabs_pixel_color(mddp));
}
