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
#include "messages/annotation_create.h"
#include "messages/annotation_delete.h"
#include "messages/annotation_edit.h"
#include "messages/annotation_reshape.h"
#include "messages/canvas_background.h"
#include "messages/canvas_resize.h"
#include "messages/chat.h"
#include "messages/command.h"
#include "messages/disconnect.h"
#include "messages/draw_dabs.h"
#include "messages/feature_levels.h"
#include "messages/fill_rect.h"
#include "messages/interval.h"
#include "messages/layer_acl.h"
#include "messages/layer_attr.h"
#include "messages/layer_create.h"
#include "messages/layer_delete.h"
#include "messages/layer_order.h"
#include "messages/layer_retitle.h"
#include "messages/layer_visibility.h"
#include "messages/pen_up.h"
#include "messages/ping.h"
#include "messages/private_chat.h"
#include "messages/put_image.h"
#include "messages/put_tile.h"
#include "messages/region_move.h"
#include "messages/session_owner.h"
#include "messages/soft_reset.h"
#include "messages/trusted_users.h"
#include "messages/undo.h"
#include "messages/undo_point.h"
#include "messages/user_acl.h"
#include "messages/user_join.h"
#include "messages/user_leave.h"
#include "text_writer.h"
#include <dpcommon/atomic.h>
#include <dpcommon/binary.h>
#include <dpcommon/common.h>


#define CONTROL      (1 << 0)
#define META         (1 << 1)
#define COMMAND      (1 << 2)
#define OPAQUE       (1 << 3)
#define RECORDABLE   (1 << 4)
#define DYNAMIC_NAME (1 << 5)

typedef DP_Message *(*DP_MessageDeserializeFn)(unsigned int context_id,
                                               const unsigned char *buffer,
                                               size_t length);

typedef struct DP_MessageTypeAttributes {
    int flags;
    const char *enum_name;
    union {
        const char *name;
        const char *(*get_name)(DP_Message *msg);
    };
    DP_Message *(*deserialize)(unsigned int context_id,
                               const unsigned char *buffer, size_t length);
} DP_MessageTypeAttributes;

static DP_Message *invalid_deserialize(DP_UNUSED unsigned int context_id,
                                       DP_UNUSED const unsigned char *buffer,
                                       DP_UNUSED size_t length)
{
    return NULL;
}

static const DP_MessageTypeAttributes invalid_attributes = {
    0,
    "DP_MSG_UNKNOWN",
    {"unknown"},
    invalid_deserialize,
};

static const DP_MessageTypeAttributes type_attributes[DP_MSG_COUNT] = {
    [DP_MSG_COMMAND] =
        {
            CONTROL,
            "DP_MSG_COMMAND",
            {"command"},
            DP_msg_command_deserialize,
        },
    [DP_MSG_DISCONNECT] =
        {
            CONTROL,
            "DP_MSG_DISCONNECT",
            {"disconnect"},
            DP_msg_disconnect_deserialize,
        },
    [DP_MSG_PING] =
        {
            CONTROL,
            "DP_MSG_PING",
            {"ping"},
            DP_msg_ping_deserialize,
        },
    [DP_MSG_INTERNAL] =
        {
            CONTROL,
            "DP_MSG_INTERNAL",
            {"internal"},
            NULL,
        },
    [DP_MSG_USER_JOIN] =
        {
            META | RECORDABLE,
            "DP_MSG_USER_JOIN",
            {"join"},
            DP_msg_user_join_deserialize,
        },
    [DP_MSG_USER_LEAVE] =
        {
            META | RECORDABLE,
            "DP_MSG_USER_LEAVE",
            {"leave"},
            DP_msg_user_leave_deserialize,
        },
    [DP_MSG_SESSION_OWNER] =
        {
            META | RECORDABLE,
            "DP_MSG_SESSION_OWNER",
            {"owner"},
            DP_msg_session_owner_deserialize,
        },
    [DP_MSG_CHAT] =
        {
            META | RECORDABLE,
            "DP_MSG_CHAT",
            {"chat"},
            DP_msg_chat_deserialize,
        },
    [DP_MSG_TRUSTED_USERS] =
        {
            META | RECORDABLE,
            "DP_MSG_TRUSTED_USERS",
            {"trusted"},
            DP_msg_trusted_users_deserialize,
        },
    [DP_MSG_SOFT_RESET] =
        {
            META | RECORDABLE,
            "DP_MSG_SOFT_RESET",
            {"softreset"},
            DP_msg_soft_reset_deserialize,
        },
    [DP_MSG_PRIVATE_CHAT] =
        {
            META | RECORDABLE,
            "DP_MSG_PRIVATE_CHAT",
            {"pm"},
            DP_msg_private_chat_deserialize,
        },
    [DP_MSG_INTERVAL] =
        {
            CONTROL | META | OPAQUE | RECORDABLE,
            "DP_MSG_INTERVAL",
            {"interval"},
            DP_msg_interval_deserialize,
        },
    [DP_MSG_USER_ACL] =
        {
            CONTROL | META | OPAQUE | RECORDABLE,
            "DP_MSG_USER_ACL",
            {"useracl"},
            DP_msg_user_acl_deserialize,
        },
    [DP_MSG_LAYER_ACL] =
        {
            CONTROL | META | OPAQUE | RECORDABLE,
            "DP_MSG_LAYER_ACL",
            {"layeracl"},
            DP_msg_layer_acl_deserialize,
        },
    [DP_MSG_FEATURE_LEVELS] =
        {
            CONTROL | META | OPAQUE | RECORDABLE,
            "DP_MSG_FEATURE_LEVELS",
            {"featureaccess"},
            DP_msg_feature_levels_deserialize,
        },
    [DP_MSG_UNDO_POINT] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_UNDO_POINT",
            {"undopoint"},
            DP_msg_undo_point_deserialize,
        },
    [DP_MSG_CANVAS_RESIZE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_CANVAS_RESIZE",
            {"resize"},
            DP_msg_canvas_resize_deserialize,
        },
    [DP_MSG_LAYER_CREATE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_LAYER_CREATE",
            {"newlayer"},
            DP_msg_layer_create_deserialize,
        },
    [DP_MSG_LAYER_DELETE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_LAYER_DELETE",
            {"deletelayer"},
            DP_msg_layer_delete_deserialize,
        },
    [DP_MSG_LAYER_ATTR] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_LAYER_ATTR",
            {"layerattr"},
            DP_msg_layer_attr_deserialize,
        },
    [DP_MSG_LAYER_ORDER] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_LAYER_ORDER",
            {"layerorder"},
            DP_msg_layer_order_deserialize,
        },
    [DP_MSG_LAYER_RETITLE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_LAYER_RETITLE",
            {"retitlelayer"},
            DP_msg_layer_retitle_deserialize,
        },
    [DP_MSG_LAYER_VISIBILITY] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_LAYER_VISIBILITY",
            {"layervisibility"},
            DP_msg_layer_visibility_deserialize,
        },
    [DP_MSG_PUT_IMAGE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_PUT_IMAGE",
            {"putimage"},
            DP_msg_put_image_deserialize,
        },
    [DP_MSG_FILL_RECT] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_FILL_RECT",
            {"fillrect"},
            DP_msg_fill_rect_deserialize,
        },
    [DP_MSG_PEN_UP] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_PEN_UP",
            {"penup"},
            DP_msg_pen_up_deserialize,
        },
    [DP_MSG_ANNOTATION_CREATE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_ANNOTATION_CREATE",
            {"newannotation"},
            DP_msg_annotation_create_deserialize,
        },
    [DP_MSG_ANNOTATION_RESHAPE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_ANNOTATION_RESHAPE",
            {"reshapeannotation"},
            DP_msg_annotation_reshape_deserialize,
        },
    [DP_MSG_ANNOTATION_EDIT] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_ANNOTATION_EDIT",
            {"editannotation"},
            DP_msg_annotation_edit_deserialize,
        },
    [DP_MSG_ANNOTATION_DELETE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_ANNOTATION_DELETE",
            {"deleteannotation"},
            DP_msg_annotation_delete_deserialize,
        },
    [DP_MSG_REGION_MOVE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_REGION_MOVE",
            {"moveregion"},
            DP_msg_region_move_deserialize,
        },
    [DP_MSG_PUT_TILE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_PUT_TILE",
            {"puttile"},
            DP_msg_put_tile_deserialize,
        },
    [DP_MSG_CANVAS_BACKGROUND] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_CANVAS_BACKGROUND",
            {"background"},
            DP_msg_canvas_background_deserialize,
        },
    [DP_MSG_DRAW_DABS_CLASSIC] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_DRAW_DABS_CLASSIC",
            {"classicdabs"},
            DP_msg_draw_dabs_classic_deserialize,
        },
    [DP_MSG_DRAW_DABS_PIXEL] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_DRAW_DABS_PIXEL",
            {"pixeldabs"},
            DP_msg_draw_dabs_pixel_deserialize,
        },
    [DP_MSG_DRAW_DABS_PIXEL_SQUARE] =
        {
            COMMAND | OPAQUE | RECORDABLE,
            "DP_MSG_DRAW_DABS_PIXEL_SQUARE",
            {"squarepixeldabs"},
            DP_msg_draw_dabs_pixel_square_deserialize,
        },
    [DP_MSG_UNDO] =
        {
            COMMAND | OPAQUE | RECORDABLE | DYNAMIC_NAME,
            "DP_MSG_UNDO",
            {.get_name = DP_msg_undo_message_name},
            DP_msg_undo_deserialize,
        },
};

static const DP_MessageTypeAttributes *get_attributes(DP_MessageType type)
{
    DP_ASSERT(type >= 0);
    DP_ASSERT(type < DP_MSG_COUNT);
    const DP_MessageTypeAttributes *attrs = &type_attributes[type];
    if (attrs->flags) {
        return attrs;
    }
    else {
        DP_error_set("Unknown message type %d", (int)type);
        return &invalid_attributes;
    }
}

bool DP_message_type_control(DP_MessageType type)
{
    return get_attributes(type)->flags & CONTROL;
}

bool DP_message_type_meta(DP_MessageType type)
{
    return get_attributes(type)->flags & META;
}

bool DP_message_type_command(DP_MessageType type)
{
    return get_attributes(type)->flags & COMMAND;
}

bool DP_message_type_opaque(DP_MessageType type)
{
    return get_attributes(type)->flags & OPAQUE;
}

bool DP_message_type_recordable(DP_MessageType type)
{
    return get_attributes(type)->flags & RECORDABLE;
}

const char *DP_message_type_enum_name(DP_MessageType type)
{
    return get_attributes(type)->enum_name;
}

const char *DP_message_type_enum_name_unprefixed(DP_MessageType type)
{
    return get_attributes(type)->enum_name + 7;
}


struct DP_Message {
    DP_Atomic refcount;
    DP_MessageType type;
    unsigned int context_id;
    const DP_MessageMethods *methods;
    alignas(max_align_t) unsigned char internal[];
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
    DP_ASSERT(internal_size <= SIZE_MAX - sizeof(DP_Message));
    DP_Message *msg = DP_malloc(sizeof(*msg) + internal_size);
    DP_atomic_set(&msg->refcount, 1);
    msg->type = type;
    msg->context_id = context_id;
    msg->methods = methods;
    memset(msg->internal, 0, internal_size);
    return msg;
}

DP_Message *DP_message_incref(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    DP_atomic_inc(&msg->refcount);
    return msg;
}

void DP_message_decref(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    if (DP_atomic_dec(&msg->refcount)) {
        void (*dispose)(DP_Message *) = msg->methods->dispose;
        if (dispose) {
            dispose(msg);
        }
        DP_free(msg);
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
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    const DP_MessageTypeAttributes *attrs = get_attributes(msg->type);
    return attrs->flags & DYNAMIC_NAME ? attrs->get_name(msg) : attrs->name;
}

unsigned int DP_message_context_id(DP_Message *msg)
{
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&msg->refcount) > 0);
    return msg->context_id;
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
        && DP_text_writer_finish_message(writer, msg);
}

bool DP_message_equals(DP_Message *msg, DP_Message *other)
{
    return (msg == other)
        || (msg && other && msg->type == other->type
            && msg->methods->equals(msg, other));
}


static DP_Message *decode_message_body(int type, unsigned int context_id,
                                       const unsigned char *buf, size_t length)
{
    if (type < 0 || type > DP_MESSAGE_MAX) {
        DP_error_set("Message type out of bounds: %d", type);
        return NULL;
    }

    const DP_MessageTypeAttributes *attrs =
        get_attributes((DP_MessageType)type);
    DP_debug("Decode message with type %d (%s), context_id %u, length %zu",
             type, attrs->enum_name, context_id, length);
    DP_Message *msg = attrs->deserialize(context_id, buf, length);
    DP_ASSERT(!msg || (int)msg->type == type);
    return msg;
}

DP_Message *DP_message_deserialize_length(const unsigned char *buf,
                                          size_t bufsize, size_t body_length)
{
    DP_ASSERT(buf);
    size_t total_length = 2 + body_length;
    if (bufsize >= total_length) {
        return decode_message_body(buf[0], buf[1], buf + 2, body_length);
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
