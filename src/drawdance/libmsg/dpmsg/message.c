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
#include "blend_mode.h"
#include "ids.h"
#include "text_writer.h"
#include <dpcommon/atomic.h>
#include <dpcommon/binary.h>
#include <dpcommon/common.h>

#define FLAG_NONE   0x0
#define FLAG_OPAQUE 0x1
#define FLAG_COMPAT 0x2

typedef DP_Message *(*DP_MessageDeserializeFn)(unsigned int context_id,
                                               const unsigned char *buffer,
                                               size_t length);

struct DP_Message {
    DP_Atomic refcount;
    uint8_t type;
    uint8_t flags;
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
    DP_Message *msg =
        DP_malloc_zeroed(DP_FLEX_SIZEOF(DP_Message, internal, internal_size));
    DP_atomic_set(&msg->refcount, 1);
    msg->type = (uint8_t)type;
    msg->flags = FLAG_NONE;
    msg->context_id = context_id;
    msg->methods = methods;
    return msg;
}

typedef struct DP_OpaqueMessage {
    size_t length;
    unsigned char body[];
} DP_OpaqueMessage;

static size_t opaque_payload_length(DP_Message *msg)
{
    DP_OpaqueMessage *om = (void *)msg->internal;
    return om->length;
}

static size_t opaque_serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_OpaqueMessage *om = (void *)msg->internal;
    size_t length = om->length;
    if (length != 0) {
        // Some GCC versions raise a bogus warning here.
#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Warray-bounds"
#endif
        memcpy(data, om->body, length);
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif
    }
    return length;
}

static bool opaque_write_payload_text(DP_UNUSED DP_Message *msg,
                                      DP_UNUSED DP_TextWriter *writer)
{
    DP_error_set("Can't write payload text of opaque message");
    return false;
}

static bool opaque_equals(DP_Message *DP_RESTRICT msg,
                          DP_Message *DP_RESTRICT other)
{
    DP_OpaqueMessage *a = (void *)msg->internal;
    DP_OpaqueMessage *b = (void *)other->internal;
    return a->length == b->length && memcmp(a->body, b->body, a->length) == 0;
}

static const DP_MessageMethods opaque_methods = {
    opaque_payload_length,     opaque_serialize_payload,
#ifdef DP_PROTOCOL_COMPAT_VERSION
    opaque_payload_length,     opaque_serialize_payload,
#endif
    opaque_write_payload_text, opaque_equals,
};

DP_Message *DP_message_new_opaque(DP_MessageType type, unsigned int context_id,
                                  const unsigned char *body, size_t length)
{
    DP_ASSERT(type >= 0);
    DP_ASSERT(type <= DP_MESSAGE_MAX);
    DP_ASSERT(context_id <= UINT8_MAX);
    DP_ASSERT(length <= SIZE_MAX - sizeof(DP_Message));
    DP_Message *msg = DP_malloc_zeroed(DP_FLEX_SIZEOF(
        DP_Message, internal, DP_FLEX_SIZEOF(DP_OpaqueMessage, body, length)));
    DP_atomic_set(&msg->refcount, 1);
    msg->type = (uint8_t)type;
    msg->flags = FLAG_OPAQUE;
    msg->context_id = context_id;
    msg->methods = &opaque_methods;
    DP_OpaqueMessage *om = (void *)msg->internal;
    om->length = length;
    if (length != 0) {
        memcpy(om->body, body, length);
    }
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
    return (DP_MessageType)msg->type;
}

bool DP_message_opaque(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    return msg->flags & FLAG_OPAQUE;
}

bool DP_message_compat(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    return msg->flags & FLAG_COMPAT;
}

void DP_message_compat_set(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    msg->flags |= FLAG_COMPAT;
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
    DP_ASSERT(!DP_message_opaque(msg));
    return msg->internal;
}

DP_Message *DP_message_from_internal(void *internal)
{
    DP_ASSERT(internal);
    unsigned char *bytes = internal;
    return (void *)(bytes - offsetof(DP_Message, internal));
}

void *DP_message_cast(DP_Message *msg, DP_MessageType type)
{
    if (msg) {
        DP_MessageType msg_type = (DP_MessageType)msg->type;
        if (msg_type == type) {
            return DP_message_internal(msg);
        }
        else {
            DP_error_set("Wrong message type %d, wanted %d", msg_type, type);
        }
    }
    return NULL;
}

static size_t message_length(DP_Message *msg, size_t header)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    return header + msg->methods->payload_length(msg);
}

size_t DP_message_length(DP_Message *msg)
{
    return message_length(msg, DP_MESSAGE_HEADER_LENGTH);
}

size_t DP_message_ws_length(DP_Message *msg)
{
    return message_length(msg, DP_MESSAGE_WS_HEADER_LENGTH);
}

static size_t serialize_message(
    DP_Message *msg, bool write_body_length, DP_GetMessageBufferFn get_buffer,
    void *user, size_t (*payload_length_fn)(DP_Message *msg),
    size_t (*serialize_payload_fn)(DP_Message *msg, unsigned char *data))
{
    DP_MessageType type = (DP_MessageType)msg->type;
    if (type < 0 || type > UINT8_MAX) {
        DP_error_set("Message type out of bounds: %d", (int)type);
        return 0;
    }

    unsigned int context_id = msg->context_id;
    if (context_id > UINT8_MAX) {
        DP_error_set("Message context id out of bounds: %u", context_id);
        return 0;
    }

    size_t length = payload_length_fn(msg);
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
    written += serialize_payload_fn(msg, buffer + written);
    DP_ASSERT(written == length_with_header);
    return written;
}

size_t DP_message_serialize(DP_Message *msg, bool write_body_length,
                            DP_GetMessageBufferFn get_buffer, void *user)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    DP_ASSERT(get_buffer);
    return serialize_message(msg, write_body_length, get_buffer, user,
                             msg->methods->payload_length,
                             msg->methods->serialize_payload);
}

#ifdef DP_PROTOCOL_COMPAT_VERSION
size_t DP_message_serialize_compat(DP_Message *msg, bool write_body_length,
                                   DP_GetMessageBufferFn get_buffer, void *user)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    DP_ASSERT(get_buffer);
    if (DP_message_type_compatible(msg->type)) {
        return serialize_message(msg, write_body_length, get_buffer, user,
                                 msg->methods->payload_length_compat,
                                 msg->methods->serialize_payload_compat);
    }
    else {
        DP_error_set("Message type %d is not backward-compatible",
                     DP_message_type(msg));
        return 0;
    }
}
#endif

size_t DP_message_serialize_body(DP_Message *msg,
                                 DP_GetMessageBufferFn get_buffer, void *user)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    DP_ASSERT(get_buffer);

    size_t length = msg->methods->payload_length(msg);
    if (length > DP_MESSAGE_MAX_PAYLOAD_LENGTH) {
        DP_error_set("Message body length out of bounds: %zu", length);
        return 0;
    }

    unsigned char *buffer = get_buffer(user, length);
    if (!buffer) {
        return 0;
    }

    size_t written = msg->methods->serialize_payload(msg, buffer);
    DP_ASSERT(written == length);
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
    if (msg == other) {
        return true;
    }
    else if (msg && other && msg->type == other->type) {
        return DP_message_payload_equals(msg, other);
    }
    else {
        return false;
    }
}

bool DP_message_payload_equals(DP_Message *msg, DP_Message *other)
{
    DP_ASSERT(msg);
    DP_ASSERT(other);
    DP_ASSERT(msg->type == other->type);
    DP_ASSERT(DP_message_opaque(msg) == DP_message_opaque(other));
    return msg->methods->equals(msg, other);
}


DP_Message *DP_message_deserialize_length(const unsigned char *buf,
                                          size_t bufsize, size_t body_length,
                                          bool decode_opaque)
{
    DP_ASSERT(buf);
    size_t total_length = 2 + body_length;
    if (bufsize >= total_length) {
        return DP_message_deserialize_body(buf[0], buf[1], buf + 2, body_length,
                                           decode_opaque);
    }
    else {
        DP_error_set("Buffer size %zu shorter than message length %zu", bufsize,
                     total_length);
        return NULL;
    }
}

DP_Message *DP_message_deserialize(const unsigned char *buf, size_t bufsize,
                                   bool decode_opaque)
{
    if (bufsize >= DP_MESSAGE_HEADER_LENGTH) {
        DP_ASSERT(buf);
        size_t body_length = DP_read_bigendian_uint16(buf);
        return DP_message_deserialize_length(buf + 2, bufsize - 2, body_length,
                                             decode_opaque);
    }
    else {
        DP_error_set("Buffer size %zu too short for message header", bufsize);
        return NULL;
    }
}

#ifdef DP_PROTOCOL_COMPAT_VERSION
DP_Message *DP_message_deserialize_length_compat(const unsigned char *buf,
                                                 size_t bufsize,
                                                 size_t body_length,
                                                 bool decode_opaque)
{
    DP_ASSERT(buf);
    size_t total_length = 2 + body_length;
    if (bufsize >= total_length) {
        return DP_message_deserialize_body_compat(buf[0], buf[1], buf + 2,
                                                  body_length, decode_opaque);
    }
    else {
        DP_error_set("Buffer size %zu shorter than message length %zu", bufsize,
                     total_length);
        return NULL;
    }
}

DP_Message *DP_message_deserialize_compat(const unsigned char *buf,
                                          size_t bufsize, bool decode_opaque)
{
    if (bufsize >= DP_MESSAGE_HEADER_LENGTH) {
        DP_ASSERT(buf);
        size_t body_length = DP_read_bigendian_uint16(buf);
        return DP_message_deserialize_length_compat(buf + 2, bufsize - 2,
                                                    body_length, decode_opaque);
    }
    else {
        DP_error_set("Buffer size %zu too short for message header", bufsize);
        return NULL;
    }
}
#endif


static int unpack_paint_mode(int flags)
{
    return flags & 0x3;
}

int DP_msg_draw_dabs_classic_paint_mode(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return unpack_paint_mode(DP_msg_draw_dabs_classic_flags(mddc));
}

int DP_msg_draw_dabs_pixel_paint_mode(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return unpack_paint_mode(DP_msg_draw_dabs_pixel_flags(mddp));
}

int DP_msg_draw_dabs_mypaint_blend_paint_mode(
    DP_MsgDrawDabsMyPaintBlend *mddmpb)
{
    DP_ASSERT(mddmpb);
    return unpack_paint_mode(DP_msg_draw_dabs_mypaint_blend_flags(mddmpb));
}


static int unpack_mask_selection_id(int flags)
{
    int n = (flags & 0xf8) >> 3;
    return n == 0 ? 0 : DP_SELECTION_ID_FIRST_REMOTE - 1 + n;
}

int DP_msg_draw_dabs_classic_mask_selection_id(DP_MsgDrawDabsClassic *mddc)
{
    DP_ASSERT(mddc);
    return unpack_mask_selection_id(DP_msg_draw_dabs_classic_flags(mddc));
}

int DP_msg_draw_dabs_pixel_mask_selection_id(DP_MsgDrawDabsPixel *mddp)
{
    DP_ASSERT(mddp);
    return unpack_mask_selection_id(DP_msg_draw_dabs_pixel_flags(mddp));
}

int DP_msg_draw_dabs_mypaint_mask_selection_id(DP_MsgDrawDabsMyPaint *mddmp)
{
    DP_ASSERT(mddmp);
    return unpack_mask_selection_id(DP_msg_draw_dabs_mypaint_flags(mddmp));
}

int DP_msg_draw_dabs_mypaint_blend_mask_selection_id(
    DP_MsgDrawDabsMyPaintBlend *mddmpb)
{
    DP_ASSERT(mddmpb);
    return unpack_mask_selection_id(
        DP_msg_draw_dabs_mypaint_blend_flags(mddmpb));
}


void DP_msg_draw_dabs_mypaint_mode_extract(DP_MsgDrawDabsMyPaint *mddmp,
                                           int *out_blend_mode,
                                           int *out_paint_mode,
                                           uint8_t *out_posterize_num)
{
    DP_ASSERT(mddmp);
    uint8_t flags = DP_msg_draw_dabs_mypaint_flags(mddmp);
    uint8_t mode = DP_msg_draw_dabs_mypaint_mode(mddmp);

    int blend_mode;
    int paint_mode;
    uint8_t posterize_num;
    if (mode & DP_MYPAINT_BRUSH_MODE_FLAG) {
        posterize_num = 0;
        switch (mode & DP_MYPAINT_BRUSH_MODE_MASK) {
        case DP_MYPAINT_BRUSH_MODE_NORMAL:
            blend_mode = DP_BLEND_MODE_NORMAL;
            paint_mode = DP_PAINT_MODE_INDIRECT_SOFT;
            break;
        case DP_MYPAINT_BRUSH_MODE_RECOLOR:
            blend_mode = DP_BLEND_MODE_RECOLOR;
            paint_mode = DP_PAINT_MODE_INDIRECT_SOFT;
            break;
        case DP_MYPAINT_BRUSH_MODE_ERASE:
            blend_mode = DP_BLEND_MODE_ERASE;
            paint_mode = DP_PAINT_MODE_INDIRECT_SOFT;
            break;
        default:
            blend_mode = DP_BLEND_MODE_NORMAL_AND_ERASER;
            paint_mode = DP_PAINT_MODE_DIRECT;
            break;
        }
    }
    else {
        switch (flags & DP_MYPAINT_BRUSH_SPACE_MASK) {
        case DP_MYPAINT_BRUSH_PIGMENT_FLAG:
            blend_mode = DP_BLEND_MODE_PIGMENT_AND_ERASER;
            break;
        case DP_MYPAINT_BRUSH_OKLAB_FLAG:
            blend_mode = DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER;
            break;
        default:
            blend_mode = DP_BLEND_MODE_NORMAL_AND_ERASER;
            break;
        }
        paint_mode = DP_PAINT_MODE_DIRECT;
        posterize_num = mode;
    }

    if (out_blend_mode) {
        *out_blend_mode = blend_mode;
    }
    if (out_paint_mode) {
        *out_paint_mode = paint_mode;
    }
    if (out_posterize_num) {
        *out_posterize_num = posterize_num;
    }
}

int DP_msg_draw_dabs_mypaint_paint_mode(DP_MsgDrawDabsMyPaint *mddmp)
{
    DP_ASSERT(mddmp);
    if (DP_msg_draw_dabs_mypaint_mode(mddmp) & DP_MYPAINT_BRUSH_MODE_FLAG) {
        return DP_PAINT_MODE_INDIRECT_SOFT;
    }
    else {
        return DP_PAINT_MODE_DIRECT;
    }
}
