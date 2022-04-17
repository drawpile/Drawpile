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
#include "annotation_edit.h"
#include "../message.h"
#include "../text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


#define MIN_PAYLOAD_LENGTH 8

struct DP_MsgAnnotationEdit {
    uint16_t annotation_id;
    uint32_t background_color;
    uint8_t flags;
    uint8_t border;
    size_t text_length;
    char text[];
};

static size_t payload_length(DP_Message *msg)
{
    DP_MsgAnnotationEdit *mae = DP_msg_annotation_edit_cast(msg);
    DP_ASSERT(mae);
    return 8 + mae->text_length;
}

static size_t serialize_payload(DP_Message *msg, unsigned char *data)
{
    DP_MsgAnnotationEdit *mae = DP_msg_annotation_edit_cast(msg);
    DP_ASSERT(mae);
    size_t written = 0;
    written += DP_write_bigendian_uint16(mae->annotation_id, data + written);
    written += DP_write_bigendian_uint32(mae->background_color, data + written);
    written += DP_write_bigendian_uint8(mae->flags, data + written);
    written += DP_write_bigendian_uint8(mae->border, data + written);
    size_t text_length = mae->text_length;
    memcpy(data, mae->text, text_length);
    written += text_length;
    return written;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgAnnotationEdit *mae = DP_msg_annotation_edit_cast(msg);
    DP_ASSERT(mae);

    uint32_t background_color = mae->background_color;
    if (background_color > 0) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_argb_color(writer, "bg", background_color));
    }

    uint8_t border = mae->border;
    if (border > 0) {
        DP_RETURN_UNLESS(DP_text_writer_write_int(writer, "border", border));
    }

    if (DP_msg_annotation_edit_protect(mae)) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_string(writer, "flags", "protect"));
    }

    DP_RETURN_UNLESS(DP_text_writer_write_id(writer, "id", mae->annotation_id));
    DP_RETURN_UNLESS(DP_text_writer_write_string(writer, "text", mae->text));

    if (DP_msg_annotation_edit_valign_bottom(mae)) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_string(writer, "valign", "bottom"));
    }
    else if (DP_msg_annotation_edit_valign_center(mae)) {
        DP_RETURN_UNLESS(
            DP_text_writer_write_string(writer, "valign", "center"));
    }

    return true;
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgAnnotationEdit *a = DP_msg_annotation_edit_cast(msg);
    DP_MsgAnnotationEdit *b = DP_msg_annotation_edit_cast(other);
    return a->annotation_id == b->annotation_id
        && a->background_color == b->background_color && a->flags == b->flags
        && a->border == b->border && a->text_length == b->text_length
        && memcmp(a->text, b->text, a->text_length) == 0;
}

static const DP_MessageMethods methods = {
    payload_length, serialize_payload, write_payload_text, equals, NULL,
};

DP_Message *DP_msg_annotation_edit_new(unsigned int context_id,
                                       int annotation_id,
                                       uint32_t background_color, int flags,
                                       int border, const char *text,
                                       size_t text_length)
{
    DP_ASSERT(annotation_id >= 0);
    DP_ASSERT(annotation_id <= UINT16_MAX);
    DP_ASSERT(flags >= 0);
    DP_ASSERT(flags <= UINT8_MAX);
    DP_ASSERT(border >= 0);
    DP_ASSERT(border <= UINT8_MAX);
    DP_ASSERT(text_length == 0 || text);
    DP_ASSERT(text_length <= UINT16_MAX - MIN_PAYLOAD_LENGTH);
    DP_MsgAnnotationEdit *mae;
    DP_Message *msg = DP_message_new(DP_MSG_ANNOTATION_EDIT, context_id,
                                     &methods, sizeof(*mae) + text_length + 1);
    mae = DP_message_internal(msg);
    mae->annotation_id = DP_int_to_uint16(annotation_id);
    mae->background_color = background_color;
    mae->flags = DP_int_to_uint8(flags);
    mae->border = DP_int_to_uint8(border);
    mae->text_length = text_length;
    memcpy(mae->text, text, text_length);
    mae->text[text_length] = '\0';
    return msg;
}

DP_Message *DP_msg_annotation_edit_deserialize(unsigned int context_id,
                                               const unsigned char *buffer,
                                               size_t length)
{
    if (length >= MIN_PAYLOAD_LENGTH) {
        return DP_msg_annotation_edit_new(
            context_id, DP_read_bigendian_uint16(buffer),
            DP_read_bigendian_uint32(buffer + 2), buffer[6], buffer[7],
            (const char *)buffer + 8, length - MIN_PAYLOAD_LENGTH);
    }
    else {
        DP_error_set("Wrong length for ANNOTATION_EDIT message: %zu", length);
        return NULL;
    }
}


DP_MsgAnnotationEdit *DP_msg_annotation_edit_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_ANNOTATION_EDIT);
}


int DP_msg_annotation_edit_annotation_id(DP_MsgAnnotationEdit *mae)
{
    DP_ASSERT(mae);
    return mae->annotation_id;
}

uint32_t DP_msg_annotation_edit_background_color(DP_MsgAnnotationEdit *mae)
{
    DP_ASSERT(mae);
    return mae->background_color;
}

int DP_msg_annotation_edit_flags(DP_MsgAnnotationEdit *mae)
{
    DP_ASSERT(mae);
    return mae->flags;
}

bool DP_msg_annotation_edit_protect(DP_MsgAnnotationEdit *mae)
{
    DP_ASSERT(mae);
    return mae->flags & DP_MSG_ANNOTATION_EDIT_PROTECT;
}

int DP_msg_annotation_edit_valign(DP_MsgAnnotationEdit *mae)
{
    DP_ASSERT(mae);
    return mae->flags & DP_MSG_ANNOTATION_EDIT_VALIGN_MASK;
}

bool DP_msg_annotation_edit_valign_center(DP_MsgAnnotationEdit *mae)
{
    DP_ASSERT(mae);
    return DP_msg_annotation_edit_valign(mae)
        == DP_MSG_ANNOTATION_EDIT_VALIGN_CENTER;
}

bool DP_msg_annotation_edit_valign_bottom(DP_MsgAnnotationEdit *mae)
{
    DP_ASSERT(mae);
    return DP_msg_annotation_edit_valign(mae)
        == DP_MSG_ANNOTATION_EDIT_VALIGN_BOTTOM;
}

int DP_msg_annotation_edit_border(DP_MsgAnnotationEdit *mae)
{
    DP_ASSERT(mae);
    return mae->border;
}

const char *DP_msg_annotation_edit_text(DP_MsgAnnotationEdit *mae,
                                        size_t *out_text_length)
{
    if (out_text_length) {
        *out_text_length = mae->text_length;
    }
    return mae->text;
}
