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

#define DP_MYPAINT_BRUSH_PIGMENT_FLAG     0x1
#define DP_MYPAINT_BRUSH_MODE_FLAG        0x80
#define DP_MYPAINT_BRUSH_MODE_INCREMENTAL 0x0
#define DP_MYPAINT_BRUSH_MODE_NORMAL      0x1
#define DP_MYPAINT_BRUSH_MODE_RECOLOR     0x2
#define DP_MYPAINT_BRUSH_MODE_ERASE       0x3
#define DP_MYPAINT_BRUSH_MODE_MASK        0x3

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

bool DP_message_compat(DP_Message *msg);

void DP_message_compat_set(DP_Message *msg);

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

#ifdef DP_PROTOCOL_COMPAT_VERSION
size_t DP_message_serialize_compat(DP_Message *msg, bool write_body_length,
                                   DP_GetMessageBufferFn get_buffer,
                                   void *user) DP_MUST_CHECK;
#endif

size_t DP_message_serialize_body(DP_Message *msg,
                                 DP_GetMessageBufferFn get_buffer,
                                 void *user) DP_MUST_CHECK;

bool DP_message_write_text(DP_Message *msg,
                           DP_TextWriter *writer) DP_MUST_CHECK;

bool DP_message_equals(DP_Message *msg, DP_Message *other);

bool DP_message_payload_equals(DP_Message *msg, DP_Message *other);


DP_Message *DP_message_deserialize_length(const unsigned char *buf,
                                          size_t bufsize, size_t body_length,
                                          bool decode_opaque);

DP_Message *DP_message_deserialize(const unsigned char *buf, size_t bufsize,
                                   bool decode_opaque);

#ifdef DP_PROTOCOL_COMPAT_VERSION
DP_Message *DP_message_deserialize_length_compat(const unsigned char *buf,
                                                 size_t bufsize,
                                                 size_t body_length,
                                                 bool decode_opaque);

DP_Message *DP_message_deserialize_compat(const unsigned char *buf,
                                          size_t bufsize, bool decode_opaque);
#endif


int DP_msg_draw_dabs_classic_paint_mode(DP_MsgDrawDabsClassic *mddc);

int DP_msg_draw_dabs_pixel_paint_mode(DP_MsgDrawDabsPixel *mddp);

int DP_msg_draw_dabs_mypaint_blend_paint_mode(
    DP_MsgDrawDabsMyPaintBlend *mddmpb);


int DP_msg_draw_dabs_classic_mask_selection_id(DP_MsgDrawDabsClassic *mddc);

int DP_msg_draw_dabs_pixel_mask_selection_id(DP_MsgDrawDabsPixel *mddp);

int DP_msg_draw_dabs_mypaint_mask_selection_id(DP_MsgDrawDabsMyPaint *mddmp);

int DP_msg_draw_dabs_mypaint_blend_mask_selection_id(
    DP_MsgDrawDabsMyPaintBlend *mddmpb);


void DP_msg_draw_dabs_mypaint_mode_extract(DP_MsgDrawDabsMyPaint *mddmp,
                                           int *out_blend_mode,
                                           int *out_paint_mode,
                                           uint8_t *out_posterize_num);

int DP_msg_draw_dabs_mypaint_paint_mode(DP_MsgDrawDabsMyPaint *mddmp);


#endif
