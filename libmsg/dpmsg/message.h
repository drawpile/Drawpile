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
#ifndef DPMSG_MESSAGE_H
#define DPMSG_MESSAGE_H
#include <dpcommon/common.h>

typedef struct DP_TextWriter DP_TextWriter;


#define DP_MESSAGE_MAX           255
#define DP_MESSAGE_HEADER_LENGTH 4

typedef enum DP_MessageType
{
    DP_MSG_COMMAND = 0,
    DP_MSG_DISCONNECT,
    DP_MSG_PING,
    DP_MSG_INTERNAL = 31,
    DP_MSG_USER_JOIN = 32,
    DP_MSG_USER_LEAVE,
    DP_MSG_SESSION_OWNER,
    DP_MSG_CHAT,
    DP_MSG_TRUSTED_USERS,
    DP_MSG_SOFT_RESET,
    DP_MSG_PRIVATE_CHAT,
    DP_MSG_INTERVAL = 64,
    DP_MSG_LASER_TRAIL,
    DP_MSG_MOVE_POINTER,
    DP_MSG_MARKER,
    DP_MSG_USER_ACL,
    DP_MSG_LAYER_ACL,
    DP_MSG_FEATURE_LEVELS,
    DP_MSG_LAYER_DEFAULT,
    DP_MSG_FILTERED,
    DP_MSG_EXTENSION,
    DP_MSG_UNDO_POINT = 128,
    DP_MSG_CANVAS_RESIZE,
    DP_MSG_LAYER_CREATE,
    DP_MSG_LAYER_ATTR,
    DP_MSG_LAYER_RETITLE,
    DP_MSG_LAYER_ORDER,
    DP_MSG_LAYER_DELETE,
    DP_MSG_LAYER_VISIBILITY,
    DP_MSG_PUT_IMAGE,
    DP_MSG_FILL_RECT,
    DP_MSG_REMOVED_138,
    DP_MSG_REMOVED_139,
    DP_MSG_PEN_UP,
    DP_MSG_ANNOTATION_CREATE,
    DP_MSG_ANNOTATION_RESHAPE,
    DP_MSG_ANNOTATION_EDIT,
    DP_MSG_ANNOTATION_DELETE,
    DP_MSG_REGION_MOVE,
    DP_MSG_PUT_TILE,
    DP_MSG_CANVAS_BACKGROUND,
    DP_MSG_DRAW_DABS_CLASSIC,
    DP_MSG_DRAW_DABS_PIXEL,
    DP_MSG_DRAW_DABS_PIXEL_SQUARE,
    DP_MSG_UNDO = DP_MESSAGE_MAX,
    DP_MSG_COUNT,
} DP_MessageType;

bool DP_message_type_control(DP_MessageType type);

bool DP_message_type_meta(DP_MessageType type);

bool DP_message_type_command(DP_MessageType type);

bool DP_message_type_opaque(DP_MessageType type);

bool DP_message_type_recordable(DP_MessageType type);

const char *DP_message_type_enum_name(DP_MessageType type);

const char *DP_message_type_enum_name_unprefixed(DP_MessageType type);


typedef struct DP_Message DP_Message;

typedef struct DP_MessageMethods {
    size_t (*payload_length)(DP_Message *msg);
    size_t (*serialize_payload)(DP_Message *msg, unsigned char *data);
    bool (*write_payload_text)(DP_Message *msg, DP_TextWriter *writer);
    bool (*equals)(DP_Message *restrict msg, DP_Message *restrict other);
    void (*dispose)(DP_Message *msg);
} DP_MessageMethods;

typedef unsigned char *(*DP_GetMessageBufferFn)(void *user, size_t length);


DP_Message *DP_message_new(DP_MessageType type, unsigned int context_id,
                           const DP_MessageMethods *methods,
                           size_t internal_size);

DP_Message *DP_message_incref(DP_Message *msg);

void DP_message_decref(DP_Message *msg);

int DP_message_refcount(DP_Message *msg);


DP_MessageType DP_message_type(DP_Message *msg);

const char *DP_message_name(DP_Message *msg);

unsigned int DP_message_context_id(DP_Message *msg);

void *DP_message_internal(DP_Message *msg);

DP_Message *DP_message_from_internal(void *internal);

void *DP_message_cast(DP_Message *msg, DP_MessageType type);

void *DP_message_cast2(DP_Message *msg, DP_MessageType type1,
                       DP_MessageType type2);

void *DP_message_cast3(DP_Message *msg, DP_MessageType type1,
                       DP_MessageType type2, DP_MessageType type3);

size_t DP_message_serialize(DP_Message *msg, bool write_body_length,
                            DP_GetMessageBufferFn get_buffer,
                            void *user) DP_MUST_CHECK;

bool DP_message_write_text(DP_Message *msg,
                           DP_TextWriter *writer) DP_MUST_CHECK;

bool DP_message_equals(DP_Message *msg, DP_Message *other);


DP_Message *DP_message_deserialize_length(const unsigned char *buf,
                                          size_t bufsize, size_t body_length);

DP_Message *DP_message_deserialize(const unsigned char *buf, size_t bufsize);


#endif
