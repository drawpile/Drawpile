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
#include "binary_writer.h"
#include "binary_reader.h"
#include "message.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/output.h>
#include <parson.h>

#define MIN_BUFFER_SIZE 128

struct DP_BinaryWriter {
    DP_Output *output;
    void *buffer;
    size_t size;
};

DP_BinaryWriter *DP_binary_writer_new(DP_Output *output)
{
    DP_ASSERT(output);
    DP_BinaryWriter *writer = DP_malloc(sizeof(*writer));
    *writer = (DP_BinaryWriter){output, NULL, 0};
    return writer;
}

void DP_binary_writer_free(DP_BinaryWriter *writer)
{
    if (writer) {
        DP_output_free(writer->output);
        DP_free(writer->buffer);
        DP_free(writer);
    }
}


static void *reserve(DP_BinaryWriter *writer, size_t size)
{
    if (writer->size < size) {
        size_t actual = size < MIN_BUFFER_SIZE ? MIN_BUFFER_SIZE : size;
        writer->buffer = DP_realloc(writer->buffer, actual);
        writer->size = actual;
    }
    return writer->buffer;
}

bool DP_binary_writer_write_header(DP_BinaryWriter *writer, JSON_Object *header)
{
    DP_ASSERT(writer);
    DP_ASSERT(header);

    DP_Output *output = writer->output;
    if (!DP_output_write(output, DP_DPREC_MAGIC, DP_DPREC_MAGIC_LENGTH)) {
        return false;
    }

    JSON_Value *value = json_object_get_wrapping_value(header);
    size_t size = json_serialization_size(value);
    if (size == 0) {
        DP_error_set("Can't calculate binary header size");
        return false;
    }

    size_t length = size - 1; // Without the null terminator.
    size_t max_length = (size_t)UINT16_MAX;
    if (length > max_length) {
        DP_error_set("Binary metadata too long: %zu > %zu", length, max_length);
        return false;
    }

    void *buffer = reserve(writer, size);
    size_t written = DP_write_bigendian_uint16((uint16_t)length, buffer);
    if (!DP_output_write(output, buffer, written)) {
        return false;
    }

    if (json_serialize_to_buffer(value, buffer, size) == JSONFailure) {
        DP_error_set("Can't serialize binary metadata");
        return false;
    }

    return DP_output_write(output, writer->buffer, length);
}


static unsigned char *get_buffer(void *user, size_t size)
{
    return reserve(user, size);
}

size_t DP_binary_writer_write_message(DP_BinaryWriter *writer, DP_Message *msg)
{
    DP_ASSERT(writer);
    DP_ASSERT(msg);
    size_t length = DP_message_serialize(msg, true, get_buffer, writer);
    return length != 0
                && DP_output_write(writer->output, writer->buffer, length)
             ? length
             : 0;
}
