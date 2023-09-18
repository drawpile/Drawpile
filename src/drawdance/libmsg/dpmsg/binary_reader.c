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
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <parson.h>

#define MIN_BUFFER_SIZE   128
#define MESSAGE_SIZE_DONE (DP_MESSAGE_HEADER_LENGTH - 1)

static_assert(MESSAGE_SIZE_DONE > 0, "Valid message size sentinel value");

struct DP_BinaryReader {
    DP_Input *input;
    size_t input_length;
    size_t input_offset;
    size_t body_offset;
    JSON_Value *header;
    unsigned char *buffer;
    size_t buffer_size;
};


static bool read_magic(DP_Input *input, size_t *input_offset)
{
    DP_ASSERT(strlen(DP_DPREC_MAGIC) + 1 == DP_DPREC_MAGIC_LENGTH);

    char buffer[DP_DPREC_MAGIC_LENGTH];
    bool error;
    size_t read = DP_input_read(input, buffer, DP_DPREC_MAGIC_LENGTH, &error);
    *input_offset += read;
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

static size_t read_metadata_length(DP_Input *input, size_t *input_offset)
{
    unsigned char buffer[2];
    bool error;
    size_t read = DP_input_read(input, buffer, 2, &error);
    *input_offset += read;
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

static char *read_metadata(DP_Input *input, size_t length, size_t *input_offset)
{
    char *buffer = DP_malloc(length + 1);
    bool error;
    size_t read = DP_input_read(input, buffer, length, &error);
    *input_offset += read;
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

static JSON_Value *read_header(DP_Input *input, size_t *input_offset)
{
    if (!read_magic(input, input_offset)) {
        return NULL;
    }

    size_t length = read_metadata_length(input, input_offset);
    if (length == 0) {
        return NULL;
    }

    char *buffer = read_metadata(input, length, input_offset);
    if (!buffer) {
        return NULL;
    }

    JSON_Value *value = parse_metadata(buffer);
    DP_free(buffer);
    return value;
}


DP_BinaryReader *DP_binary_reader_new(DP_Input *input, unsigned int flags)
{
    DP_ASSERT(input);

    size_t input_length;
    if (flags & DP_BINARY_READER_FLAG_NO_LENGTH) {
        input_length = 0;
    }
    else {
        bool error;
        input_length = DP_input_length(input, &error);
        if (error) {
            DP_input_free(input);
            return NULL;
        }
    }

    size_t input_offset = 0;
    JSON_Value *header;
    if (flags & DP_BINARY_READER_FLAG_NO_HEADER) {
        header = NULL;
    }
    else {
        header = read_header(input, &input_offset);
        if (!header) {
            DP_input_free(input);
            return NULL;
        }
    }

    DP_BinaryReader *reader = DP_malloc(sizeof(*reader));
    *reader = (DP_BinaryReader){
        input, input_length, input_offset, input_offset, header, NULL, 0};
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


JSON_Value *DP_binary_reader_header(DP_BinaryReader *reader)
{
    DP_ASSERT(reader);
    return reader->header;
}

size_t DP_binary_reader_body_offset(DP_BinaryReader *reader)
{
    DP_ASSERT(reader);
    return reader->body_offset;
}

size_t DP_binary_reader_tell(DP_BinaryReader *reader)
{
    DP_ASSERT(reader);
    return reader->input_offset;
}

bool DP_binary_reader_seek(DP_BinaryReader *reader, size_t offset)
{
    DP_ASSERT(reader);
    size_t input_length = reader->input_length;
    if (offset > input_length) {
        DP_error_set("Seek offset %zu beyond end %zu", offset, input_length);
        return false;
    }
    else if (!DP_input_seek(reader->input, offset)) {
        return false;
    }
    else {
        reader->input_offset = offset;
        return true;
    }
}

double DP_binary_reader_progress(DP_BinaryReader *reader)
{
    DP_ASSERT(reader);
    double length = DP_size_to_double(reader->input_length);
    double offset = DP_size_to_double(reader->input_offset);
    return length == 0 ? 1.0 : offset / length;
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
    size_t read =
        DP_input_read(reader->input, reader->buffer + offset, size, out_error);
    reader->input_offset += read;
    return read;
}

static DP_BinaryReaderResult read_message_header(DP_BinaryReader *reader,
                                                 size_t *out_body_length)
{
    bool error;
    size_t read = read_into(reader, DP_MESSAGE_HEADER_LENGTH, 0, &error);
    if (error) {
        return DP_BINARY_READER_ERROR_INPUT;
    }
    else if (read == 0) {
        return DP_BINARY_READER_INPUT_END;
    }
    else if (read != DP_MESSAGE_HEADER_LENGTH) {
        DP_error_set("Tried to read message header of %d bytes, but got %zu",
                     DP_MESSAGE_HEADER_LENGTH, read);
        return DP_BINARY_READER_ERROR_INPUT;
    }
    else {
        *out_body_length = DP_read_bigendian_uint16(reader->buffer);
        return DP_BINARY_READER_SUCCESS;
    }
}

DP_BinaryReaderResult DP_binary_reader_read_message(DP_BinaryReader *reader,
                                                    bool decode_opaque,
                                                    DP_Message **out_msg)
{
    DP_ASSERT(reader);
    DP_ASSERT(out_msg);

    size_t body_length;
    DP_BinaryReaderResult result = read_message_header(reader, &body_length);
    if (result != DP_BINARY_READER_SUCCESS) {
        return result;
    }

    bool error;
    size_t read =
        read_into(reader, body_length, DP_MESSAGE_HEADER_LENGTH, &error);
    if (error) {
        return DP_BINARY_READER_ERROR_INPUT;
    }
    else if (read != body_length) {
        DP_error_set("Tried to read message body of %zu bytes, but got %zu",
                     body_length, read);
        return DP_BINARY_READER_ERROR_INPUT;
    }

    DP_Message *msg = DP_message_deserialize(
        reader->buffer, DP_MESSAGE_HEADER_LENGTH + body_length, decode_opaque);
    if (msg) {
        *out_msg = msg;
        return DP_BINARY_READER_SUCCESS;
    }
    else {
        return DP_BINARY_READER_ERROR_PARSE;
    }
}

int DP_binary_reader_skip_message(DP_BinaryReader *reader, uint8_t *out_type,
                                  uint8_t *out_context_id)
{
    DP_ASSERT(reader);

    size_t body_length;
    DP_BinaryReaderResult result = read_message_header(reader, &body_length);
    if (result != DP_BINARY_READER_SUCCESS) {
        return -1;
    }

    DP_Input *input = reader->input;
    if (!DP_input_seek_by(input, body_length)) {
        return -1;
    }

    if (out_type) {
        *out_type = reader->buffer[2];
    }
    if (out_context_id) {
        *out_context_id = reader->buffer[3];
    }
    return DP_size_to_int(DP_MESSAGE_HEADER_LENGTH + body_length);
}
