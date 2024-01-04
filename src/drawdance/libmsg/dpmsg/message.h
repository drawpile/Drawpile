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
#include "messages.h"
#include <dpcommon/common.h>

typedef struct DP_TextWriter DP_TextWriter;


#define DP_MESSAGE_MAX                255
#define DP_MESSAGE_HEADER_LENGTH      4
#define DP_MESSAGE_WS_HEADER_LENGTH   2
#define DP_MESSAGE_MAX_PAYLOAD_LENGTH 65535

#define DP_MESSAGE_TYPE_RANGE_START_CLIENT  64
#define DP_MESSAGE_TYPE_RANGE_START_COMMAND 128

typedef struct DP_Message DP_Message;

typedef unsigned char *(*DP_GetMessageBufferFn)(void *user, size_t length);


DP_Message *DP_message_new(DP_MessageType type, unsigned int context_id,
                           const DP_MessageMethods *methods,
                           size_t internal_size);

DP_Message *DP_message_new_opaque(DP_MessageType type, unsigned int context_id,
                                  const unsigned char *body, size_t length);

DP_Message *DP_message_incref(DP_Message *msg);

DP_Message *DP_message_incref_nullable(DP_Message *msg_or_null);

void DP_message_decref(DP_Message *msg);

void DP_message_decref_nullable(DP_Message *msg_or_null);

int DP_message_refcount(DP_Message *msg);


DP_MessageType DP_message_type(DP_Message *msg);

bool DP_message_opaque(DP_Message *msg);

const char *DP_message_name(DP_Message *msg);

unsigned int DP_message_context_id(DP_Message *msg);

void DP_message_context_id_set(DP_Message *msg, unsigned int context_id);

void *DP_message_internal(DP_Message *msg);

DP_Message *DP_message_from_internal(void *internal);

void *DP_message_cast(DP_Message *msg, DP_MessageType type);

size_t DP_message_length(DP_Message *msg);
size_t DP_message_ws_length(DP_Message *msg);

size_t DP_message_serialize(DP_Message *msg, bool write_body_length,
                            DP_GetMessageBufferFn get_buffer,
                            void *user) DP_MUST_CHECK;

bool DP_message_write_text(DP_Message *msg,
                           DP_TextWriter *writer) DP_MUST_CHECK;

bool DP_message_equals(DP_Message *msg, DP_Message *other);


DP_Message *DP_message_deserialize_length(const unsigned char *buf,
                                          size_t bufsize, size_t body_length,
                                          bool decode_opaque);

DP_Message *DP_message_deserialize(const unsigned char *buf, size_t bufsize,
                                   bool decode_opaque);


bool DP_message_compat_flag_indirect(DP_Message *msg);
void DP_message_compat_flag_indirect_set(DP_Message *msg);


bool DP_msg_draw_dabs_classic_indirect(DP_MsgDrawDabsClassic *mddc);

bool DP_msg_draw_dabs_pixel_indirect(DP_MsgDrawDabsPixel *mddp);


#endif
