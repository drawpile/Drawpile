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
#include "binary_reader.h"
#include "message.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/input.h>
#include <parson.h>

#define MIN_BUFFER_SIZE   128
#define MESSAGE_SIZE_DONE (DP_MESSAGE_HEADER_LENGTH - 1)

static_assert(MESSAGE_SIZE_DONE > 0, "Valid message size sentinel value");

struct DP_BinaryReader {
    DP_Input *input;
    JSON_Value *header;
    unsigned char *buffer;
    size_t buffer_size;
    size_t message_size;
};


static bool read_magic(DP_Input *input)
{
    DP_ASSERT(strlen(DP_DPREC_MAGIC) + 1 == DP_DPREC_MAGIC_LENGTH);

    char buffer[DP_DPREC_MAGIC_LENGTH];
    bool error;
    size_t read = DP_input_read(input, buffer, DP_DPREC_MAGIC_LENGTH, &error);
    if (error) {
        return false;
    }
    else if (read != DP_DPREC_MAGIC_LENGTH) {
        DP_error_set("Invalid recording header prefix size: %zu != %d", read,
                     DP_DPREC_MAGIC_LENGTH);
        return false;
    }
    else if (memcmp(buffer, DP_DPREC_MAGIC, DP_DPREC_MAGIC_LENGTH) != 0) {
        DP_error_set("Invalid recording header prefix value");
        return false;
    }
    else {
        return true;
    }
}

static size_t read_metadata_length(DP_Input *input)
{
    unsigned char buffer[2];
    bool error;
    size_t read = DP_input_read(input, buffer, 2, &error);
    if (error) {
        return 0;
    }
    else if (read != 2) {
        DP_error_set("Invalid recording metadata length size: %zu != 2", read);
        return 0;
    }

    size_t length = DP_read_bigendian_uint16(buffer);
    if (length == 0) {
        DP_error_set("Recording metadata length is 0");
        return 0;
    }

    return length;
}

static char *read_metadata(DP_Input *input, size_t length)
{
    char *buffer = DP_malloc(length + 1);
    bool error;
    size_t read = DP_input_read(input, buffer, length, &error);
    if (error) {
        DP_free(buffer);
        return NULL;
    }
    else if (read != length) {
        DP_error_set("Got only %zu bytes of %zu of recording metadata", read,
                     length);
        DP_free(buffer);
        return NULL;
    }
    buffer[length] = '\0';
    return buffer;
}

static JSON_Value *parse_metadata(const char *buffer)
{
    JSON_Value *value = json_parse_string(buffer);
    if (!value) {
        DP_error_set("Invalid metadata format");
        return NULL;
    }

    if (json_value_get_type(value) != JSONObject) {
        DP_error_set("Metadata is not an object");
        json_value_free(value);
        return NULL;
    }

    return value;
}

static JSON_Value *read_header(DP_Input *input)
{
    if (!read_magic(input)) {
        return NULL;
    }

    size_t length = read_metadata_length(input);
    if (length == 0) {
        return NULL;
    }

    char *buffer = read_metadata(input, length);
    if (!buffer) {
        return NULL;
    }

    JSON_Value *value = parse_metadata(buffer);
    DP_free(buffer);
    return value;
}


DP_BinaryReader *DP_binary_reader_new(DP_Input *input)
{
    DP_ASSERT(input);
    JSON_Value *header = read_header(input);
    if (!header) {
        DP_input_free(input);
        return NULL;
    }

    DP_BinaryReader *reader = DP_malloc(sizeof(*reader));
    *reader = (DP_BinaryReader){input, header, NULL, 0, 0};
    return reader;
}

void DP_binary_reader_free(DP_BinaryReader *reader)
{
    if (reader) {
        DP_free(reader->buffer);
        json_value_free(reader->header);
        DP_input_free(reader->input);
        DP_free(reader);
    }
}


JSON_Object *DP_binary_reader_header(DP_BinaryReader *reader)
{
    DP_ASSERT(reader);
    return json_value_get_object(reader->header);
}


static void ensure_buffer_size(DP_BinaryReader *reader, size_t required_size)
{
    if (reader->buffer_size < required_size) {
        reader->buffer = DP_realloc(
            reader->buffer,
            required_size < MIN_BUFFER_SIZE ? MIN_BUFFER_SIZE : required_size);
    }
}

static size_t read_into(DP_BinaryReader *reader, size_t size, size_t offset,
                        bool *out_error)
{
    ensure_buffer_size(reader, size + offset);
    return DP_input_read(reader->input, reader->buffer + offset, size,
                         out_error);
}

static bool read_message_length(DP_BinaryReader *reader)
{
    bool error;
    size_t read = read_into(reader, DP_MESSAGE_HEADER_LENGTH, 0, &error);

    if (error) {
        return false;
    }
    else if (read != 0 && read < DP_MESSAGE_HEADER_LENGTH) {
        DP_error_set("Expected message length of %d bytes, but got %zu",
                     DP_MESSAGE_HEADER_LENGTH, read);
    }

    return read == DP_MESSAGE_HEADER_LENGTH;
}

static bool read_message_body(DP_BinaryReader *reader, size_t body_length)
{
    bool error;
    size_t read =
        read_into(reader, body_length, DP_MESSAGE_HEADER_LENGTH, &error);

    if (error) {
        return false;
    }
    else if (read != body_length) {
        DP_error_set("Expected message body of %zu bytes, but got %zu",
                     body_length, read);
        return false;
    }
    else {
        return true;
    }
}

static bool check_next(DP_BinaryReader *reader)
{
    if (read_message_length(reader)) {
        size_t body_length = DP_read_bigendian_uint16(reader->buffer);
        if (read_message_body(reader, body_length)) {
            reader->message_size = DP_MESSAGE_HEADER_LENGTH + body_length;
            return true;
        }
    }
    reader->message_size = MESSAGE_SIZE_DONE;
    return false;
}

bool DP_binary_reader_has_next(DP_BinaryReader *reader)
{
    DP_ASSERT(reader);
    switch (reader->message_size) {
    case 0:
        return check_next(reader);
    case MESSAGE_SIZE_DONE:
        return false;
    default:
        return true;
    }
}

DP_Message *DP_binary_reader_read_next(DP_BinaryReader *reader)
{
    DP_ASSERT(reader);
    if (DP_binary_reader_has_next(reader)) {
        DP_Message *message =
            DP_message_deserialize(reader->buffer, reader->message_size);
        reader->message_size = 0;
        return message;
    }
    else {
        return NULL;
    }
}
