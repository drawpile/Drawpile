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
#include "text_reader.h"
#include "blend_mode.h"
#include "message.h"
#include <dpcommon/base64.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/vector.h>
#include <ctype.h>
#include <math.h>


#define MIN_READ_CAPACITY   4096
#define MIN_VECTOR_CAPACITY 16

typedef struct DP_TextReaderField {
    size_t key_offset;
    size_t value_offset;
} DP_TextReaderField;

typedef struct DP_TextReaderTupleRow {
    size_t count;
    size_t offset_index;
} DP_TextReaderTupleRow;

struct DP_TextReader {
    DP_Input *input;
    size_t input_length;
    size_t input_offset;
    size_t body_offset;
    size_t line_end;
    struct {
        size_t capacity;
        size_t used;
        char *buffer;
    } read;
    struct {
        size_t capacity;
        size_t used;
        char *buffer;
        DP_Vector offsets;
    } field;
    struct {
        DP_Vector rows;
        DP_Vector offsets;
    } tuple;
};

DP_TextReader *DP_text_reader_new(DP_Input *input)
{
    DP_ASSERT(input);
    bool error;
    size_t input_length = DP_input_length(input, &error);
    if (error) {
        return NULL;
    }

    DP_TextReader *reader = DP_malloc(sizeof(*reader));
    *reader = (DP_TextReader){input,
                              input_length,
                              0,
                              0,
                              0,
                              {0, 0, NULL},
                              {0, 0, NULL, DP_VECTOR_NULL},
                              {DP_VECTOR_NULL, DP_VECTOR_NULL}};
    DP_VECTOR_INIT_TYPE(&reader->field.offsets, DP_TextReaderField,
                        MIN_VECTOR_CAPACITY);
    DP_VECTOR_INIT_TYPE(&reader->tuple.rows, DP_TextReaderTupleRow,
                        MIN_VECTOR_CAPACITY);
    DP_VECTOR_INIT_TYPE(&reader->tuple.offsets, size_t, MIN_VECTOR_CAPACITY);
    return reader;
}

void DP_text_reader_free(DP_TextReader *reader)
{
    if (reader) {
        DP_vector_dispose(&reader->tuple.offsets);
        DP_vector_dispose(&reader->tuple.rows);
        DP_vector_dispose(&reader->field.offsets);
        DP_free(reader->field.buffer);
        DP_free(reader->read.buffer);
        DP_input_free(reader->input);
        DP_free(reader);
    }
}

size_t DP_text_reader_body_offset(DP_TextReader *reader)
{
    return reader->body_offset;
}

size_t DP_text_reader_tell(DP_TextReader *reader)
{
    DP_ASSERT(reader);
    return reader->input_offset + reader->line_end;
}

bool DP_text_reader_seek(DP_TextReader *reader, size_t offset)
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
        reader->read.used = 0;
        reader->line_end = 0;
        return true;
    }
}

double DP_text_reader_progress(DP_TextReader *reader)
{
    DP_ASSERT(reader);
    double length = DP_size_to_double(reader->input_length);
    double offset = DP_size_to_double(reader->input_offset);
    return length == 0 ? 1.0 : offset / length;
}

static bool buffer_more(DP_TextReader *reader)
{
    DP_Input *input = reader->input;
    if (input) {
        size_t capacity = reader->read.capacity;
        size_t used = reader->read.used;
        size_t capacity_left = capacity - used;
        if (capacity_left == 0) {
            size_t new_capacity = DP_max_size(MIN_READ_CAPACITY, capacity * 2);
            // Allocate an extra byte so we can always append a null terminator.
            reader->read.buffer =
                DP_realloc(reader->read.buffer, new_capacity + 1);
            reader->read.capacity = new_capacity;
            capacity_left = new_capacity - used;
        }

        bool error;
        size_t read = DP_input_read(input, reader->read.buffer + used,
                                    capacity_left, &error);
        if (error) {
            DP_input_free(input);
            reader->read.used = 0;
            return false;
        }

        reader->read.used = used + read;
        reader->read.buffer[reader->read.used] = '\0';
        return true;
    }
    else {
        return false;
    }
}

static bool buffer_line(DP_TextReader *reader, size_t *out_len)
{
    size_t i = 0;
    while (true) {
        if (i == reader->read.used) {
            if (!buffer_more(reader)) {
                return false;
            }
            if (i == reader->read.used) {
                break;
            }
        }
        if (reader->read.buffer[i] == '\n') {
            ++i;
            break;
        }
        ++i;
    }
    *out_len = reader->line_end = i;
    return true;
}

static void regurgitate_line(DP_TextReader *reader)
{
    reader->line_end = 0;
}

static void consume_line(DP_TextReader *reader)
{
    DP_ASSERT(reader->line_end <= reader->read.used);
    size_t consume = reader->line_end;
    size_t new_used = reader->read.used - consume;
    reader->input_offset += consume;
    reader->line_end = 0;
    reader->read.used = new_used;
    memmove(reader->read.buffer, reader->read.buffer + consume, new_used);
}

static bool buffer_relevant_line(DP_TextReader *reader, size_t *out_start,
                                 size_t *out_end)
{
    while (true) {
        consume_line(reader);

        size_t len;
        if (!buffer_line(reader, &len)) {
            return false;
        }

        if (len == 0) {
            *out_start = 0;
            *out_end = 0;
            return true;
        }

        char *buffer = reader->read.buffer;
        for (size_t i = 0; i < len; ++i) {
            char c = buffer[i];
            if (c == '#') {
                break;
            }
            else if (!isspace(c)) {
                *out_start = i;
                *out_end = len;
                return true;
            }
        }
    }
}

static size_t search_char_index(const char *buffer, size_t start, size_t end,
                                char c)
{
    for (size_t i = start; i < end; ++i) {
        if (buffer[i] == c) {
            return i;
        }
    }
    return end;
}

static size_t skip_ws(const char *buffer, size_t start, size_t end)
{
    for (size_t i = start; i < end; ++i) {
        if (!isspace(buffer[i])) {
            return i;
        }
    }
    return end;
}

static size_t skip_non_ws(const char *buffer, size_t start, size_t end)
{
    for (size_t i = start; i < end; ++i) {
        if (isspace(buffer[i])) {
            return i;
        }
    }
    return end;
}

static size_t skip_ws_reverse(const char *buffer, size_t end, size_t start)
{
    for (size_t i = end; i > start; --i) {
        if (!isspace(buffer[i - 1])) {
            return i;
        }
    }
    return start;
}

static size_t skip_newline_reverse(const char *buffer, size_t end, size_t start)
{
    for (size_t i = end; i > start; --i) {
        char c = buffer[i - 1];
        if (c != '\n' && c != '\r') {
            return i;
        }
    }
    return start;
}


static bool parse_uint(const char *buffer, size_t start, size_t end,
                       unsigned int min, unsigned int max,
                       unsigned int *out_value)
{
    char *last;
    unsigned long value = strtoul(buffer + start, &last, 10);
    if (last == buffer + end && value >= min && value <= max) {
        *out_value = DP_ulong_to_uint(value);
        return true;
    }
    else {
        return false;
    }
}


DP_TextReaderResult DP_text_reader_read_header_field(DP_TextReader *reader,
                                                     const char **out_key,
                                                     const char **out_value)
{
    DP_ASSERT(reader);
    DP_ASSERT(out_key);
    DP_ASSERT(out_value);

    size_t start, end;
    if (!buffer_relevant_line(reader, &start, &end)) {
        return DP_TEXT_READER_ERROR_INPUT;
    }

    char *buffer = reader->read.buffer;
    char *key, *value;
    if (end == 0 || buffer[start] != '!') {
        regurgitate_line(reader); // This isn't a header line, don't consume it.
        reader->body_offset = reader->input_offset;
        return DP_TEXT_READER_HEADER_END;
    }

    size_t equals_index = search_char_index(buffer, start + 1, end, '=');
    size_t key_start = skip_ws(buffer, start + 1, end);
    size_t key_end = skip_ws_reverse(buffer, equals_index, key_start);
    DP_ASSERT(key_start > start);
    DP_ASSERT(key_start <= end);
    DP_ASSERT(key_end <= end);
    key = buffer + key_start;
    buffer[key_end] = '\0';

    size_t value_start = skip_ws(buffer, equals_index + 1, end);
    size_t value_end = skip_ws_reverse(buffer, end, value_start);
    DP_ASSERT(value_start >= key_start);
    DP_ASSERT(value_start <= end);
    DP_ASSERT(value_end <= end);
    value = buffer + value_start;
    buffer[value_end] = '\0';

    *out_key = key;
    *out_value = value;
    return DP_TEXT_READER_SUCCESS;
}


static void discard_message_body(DP_TextReader *reader)
{
    while (true) {
        size_t start, end;
        if (!buffer_relevant_line(reader, &start, &end)) {
            DP_warn("Input error during message discard: %s", DP_error());
            break;
        }

        if (end == 0) {
            DP_warn("End of input during message discard");
            break;
        }

        char *buffer = reader->read.buffer;
        size_t field_start = skip_ws(buffer, start, end);
        if (buffer[field_start] == '}') {
            if (skip_ws(buffer, field_start + 1, end) != end) {
                DP_warn("Garbage after '}' at offset %zu",
                        reader->input_offset);
            }
            break;
        }
    }
}

static void discard_message(DP_TextReader *reader, size_t field_offset,
                            size_t end)
{
    while (true) {
        char *buffer = reader->read.buffer;
        size_t field_start = skip_ws(buffer, field_offset, end);
        size_t field_end = skip_non_ws(buffer, field_start, end);
        if (field_start == field_end) {
            consume_line(reader);
            break;
        }
        else if (buffer[field_start] == '{') {
            discard_message_body(reader);
            break;
        }
        else {
            field_offset = field_end + 1;
        }
    }
}

static size_t expand_field_buffer(DP_TextReader *reader, size_t size)
{
    size_t used = reader->field.used;
    size_t required_capacity = used + size;
    reader->field.used = required_capacity;
    if (reader->field.capacity < required_capacity) {
        reader->field.buffer =
            DP_realloc(reader->field.buffer, required_capacity);
        reader->field.capacity = required_capacity;
    }
    return used;
}

static void push_field(DP_TextReader *reader, const char *key, size_t key_len,
                       const char *value, size_t value_len)
{
    size_t key_start = expand_field_buffer(reader, key_len + value_len + 2);
    size_t key_end = key_start + key_len;
    char *buffer = reader->field.buffer;
    memcpy(buffer + key_start, key, key_len);
    buffer[key_end] = '\0';

    size_t value_start = key_end + 1;
    size_t value_end = value_start + value_len;
    memcpy(buffer + value_start, value, value_len);
    buffer[value_end] = '\0';

    DP_TextReaderField trf = {key_start, value_start};
    DP_VECTOR_PUSH_TYPE(&reader->field.offsets, DP_TextReaderField, trf);
}

static size_t parse_tuple_value(DP_TextReader *reader, size_t offset,
                                size_t end)
{
    char *read_buffer = reader->read.buffer;
    size_t value_start = skip_ws(read_buffer, offset, end);
    size_t value_end = skip_non_ws(read_buffer, value_start, end);
    if (value_start == value_end) {
        return 0;
    }
    else {
        char *value = read_buffer + value_start;
        size_t value_len = value_end - value_start;
        size_t start = expand_field_buffer(reader, value_len + 1);
        char *field_buffer = reader->field.buffer;
        memcpy(field_buffer + start, value, value_len);
        field_buffer[start + value_len] = '\0';
        DP_VECTOR_PUSH_TYPE(&reader->tuple.offsets, size_t, start);
        return value_len;
    }
}

static void parse_tuple(DP_TextReader *reader, size_t field_start, size_t end)
{
    size_t offset = field_start;
    size_t count = 0;
    size_t offset_index = reader->tuple.offsets.used;
    size_t len;
    while ((len = parse_tuple_value(reader, offset, end)) != 0) {
        offset += len + 1;
        ++count;
    }
    DP_TextReaderTupleRow trtr = {count, offset_index};
    DP_VECTOR_PUSH_TYPE(&reader->tuple.rows, DP_TextReaderTupleRow, trtr);
}

static bool is_multiline_field_continuation(DP_TextReader *reader,
                                            const char *key, size_t key_len)
{
    DP_TextReaderField trf =
        DP_VECTOR_LAST_TYPE(&reader->field.offsets, DP_TextReaderField);
    const char *last_key = reader->field.buffer + trf.key_offset;
    return key_len == strlen(last_key) && memcmp(key, last_key, key_len) == 0;
}

static void append_multiline_field(DP_TextReader *reader, const char *value,
                                   size_t value_len)
{
    size_t start = expand_field_buffer(reader, value_len + 1);
    char *buffer = reader->field.buffer;
    buffer[start - 1] = '\n';
    memcpy(buffer + start, value, value_len);
    buffer[start + value_len] = '\0';
}

static bool parse_multiline_field(DP_TextReader *reader, size_t field_start,
                                  size_t end, bool can_append)
{
    char *buffer = reader->read.buffer;
    size_t equals_index = search_char_index(buffer, field_start, end, '=');
    if (equals_index == end) {
        DP_error_set("Missing '=' in multiline field '%.*s' at offset %zu",
                     DP_size_to_int(end - field_start), buffer + field_start,
                     reader->input_offset);
        return can_append;
    }

    char *key = buffer + field_start;
    size_t key_len = equals_index - field_start;

    size_t value_start = equals_index + 1;
    size_t value_end = skip_newline_reverse(buffer, end, value_start);
    size_t value_len = value_end - value_start;
    char *value = buffer + value_start;

    if (can_append && is_multiline_field_continuation(reader, key, key_len)) {
        append_multiline_field(reader, buffer + value_start, value_len);
    }
    else {
        push_field(reader, key, key_len, value, value_len);
    }
    return true;
}

static DP_TextReaderResult parse_multiline(DP_TextReader *reader,
                                           bool parse_tuples)
{
    bool can_append = false;
    while (true) {
        size_t start, end;
        if (!buffer_relevant_line(reader, &start, &end)) {
            return DP_TEXT_READER_ERROR_INPUT;
        }

        if (end == 0) {
            DP_error_set("Expected }, but got end of input");
            return DP_TEXT_READER_ERROR_INPUT;
        }

        char *buffer = reader->read.buffer;
        size_t field_start = skip_ws(buffer, start, end);
        if (buffer[field_start] == '}') {
            if (skip_ws(buffer, field_start + 1, end) != end) {
                DP_warn("Garbage after '}' at offset %zu",
                        reader->input_offset);
            }
            return DP_TEXT_READER_SUCCESS;
        }

        if (parse_tuples) {
            parse_tuple(reader, field_start, end);
        }
        else {
            can_append =
                parse_multiline_field(reader, field_start, end, can_append);
        }
    }
}

static DP_TextReaderResult parse_fields(DP_TextReader *reader,
                                        size_t field_offset, size_t end,
                                        DP_MessageType type)
{
    while (true) {
        char *buffer = reader->read.buffer;
        size_t field_start = skip_ws(buffer, field_offset, end);
        size_t field_end = skip_non_ws(buffer, field_start, end);

        if (field_start == field_end) {
            return DP_TEXT_READER_SUCCESS;
        }

        if (buffer[field_start] == '{') {
            if (skip_ws(buffer, field_start + 1, end) != end) {
                DP_warn("Garbage after '{' at offset %zu",
                        reader->input_offset);
            }
            bool parse_tuples = DP_message_type_parse_multiline_tuples(type);
            return parse_multiline(reader, parse_tuples);
        }

        field_offset = field_end + 1;

        size_t equals_index =
            search_char_index(buffer, field_start, field_end, '=');
        if (equals_index == field_end) {
            DP_warn("Missing '=' in field '%s' at offset %zu",
                    buffer + field_start, reader->input_offset);
            continue;
        }

        size_t value_start = equals_index + 1;
        push_field(reader, buffer + field_start, equals_index - field_start,
                   buffer + value_start, field_end - value_start);
    }
}

DP_TextReaderResult DP_text_reader_read_message(DP_TextReader *reader,
                                                DP_Message **out_msg)
{
    DP_ASSERT(reader);
    DP_ASSERT(out_msg);
    reader->field.used = 0;
    reader->field.offsets.used = 0;
    reader->tuple.rows.used = 0;
    reader->tuple.offsets.used = 0;

    while (true) {
        size_t start, end;
        if (!buffer_relevant_line(reader, &start, &end)) {
            return DP_TEXT_READER_ERROR_INPUT;
        }

        if (end == 0) {
            return DP_TEXT_READER_INPUT_END;
        }

        char *buffer = reader->read.buffer;
        size_t context_id_end = skip_non_ws(buffer, start, end);
        buffer[context_id_end] = '\0';
        unsigned int context_id;
        if (!parse_uint(buffer, start, context_id_end, 0, 255, &context_id)) {
            DP_error_set("Error parsing context id in %s", buffer);
            return DP_TEXT_READER_ERROR_PARSE;
        }

        size_t type_start = skip_ws(buffer, context_id_end + 1, end);
        size_t type_end = skip_non_ws(buffer, type_start, end);
        buffer[type_end] = '\0';

        size_t field_offset = type_end + 1;
        DP_MessageType type =
            DP_message_type_from_name(buffer + type_start, DP_MSG_TYPE_COUNT);
        if (type == DP_MSG_TYPE_COUNT) {
            DP_error_set("Unknown message type '%s'", buffer + type_start);
            discard_message(reader, field_offset, end);
            return DP_TEXT_READER_ERROR_PARSE;
        }

        DP_TextReaderResult parse_fields_result =
            parse_fields(reader, field_offset, end, type);
        if (parse_fields_result != DP_TEXT_READER_SUCCESS) {
            return parse_fields_result;
        }

        DP_Message *msg = DP_message_parse_body(type, context_id, reader);
        if (msg) {
            *out_msg = msg;
            return DP_TEXT_READER_SUCCESS;
        }
        else {
            return DP_TEXT_READER_ERROR_PARSE;
        }
    }
}


static const char *search_field(DP_TextReader *reader, const char *key)
{
    size_t used = reader->field.offsets.used;
    const char *buffer = reader->field.buffer;
    for (size_t i = 0; i < used; ++i) {
        DP_TextReaderField trf =
            DP_VECTOR_AT_TYPE(&reader->field.offsets, DP_TextReaderField, i);
        if (DP_str_equal(buffer + trf.key_offset, key)) {
            return buffer + trf.value_offset;
        }
    }
    return NULL;
}

bool DP_text_reader_get_bool(DP_TextReader *reader, const char *key)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    const char *value = search_field(reader, key);
    if (value) {
        if (DP_str_equal(value, "true")) {
            return true;
        }
        else if (DP_str_equal(value, "false")) {
            return false;
        }
        else {
            DP_warn("Error parsing bool field '%s' = '%s'", key, value);
        }
    }
    return false;
}

static long parse_long(const char *value, long min, long max)
{
    if (!value || *value == '\0') {
        return 0;
    }

    char *end;
    long result = strtol(value, &end, 10);
    if (*end != '\0') {
        DP_warn("Error parsing signed field '%s'", value);
        return 0;
    }

    if (result >= min && result <= max) {
        return result;
    }
    else {
        DP_warn("Signed field value %ld not in bounds [%ld, %ld]", result, min,
                max);
        return 0;
    }
}

long DP_text_reader_get_long(DP_TextReader *reader, const char *key, long min,
                             long max)

{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    const char *value = search_field(reader, key);
    return parse_long(value, min, max);
}

static unsigned long parse_ulong(int base, const char *value, unsigned long max)
{
    if (!value || *value == '\0') {
        return 0;
    }

    char *end;
    unsigned long result = strtoul(value, &end, base);
    if (*end != '\0') {
        DP_warn("Error parsing unsigned field '%s'", value);
        return 0;
    }

    if (result <= max) {
        return result;
    }
    else {
        DP_warn("Unsigned field value %lu exceeds maximum %lu", result, max);
        return 0;
    }
}

unsigned long DP_text_reader_get_ulong(DP_TextReader *reader, const char *key,
                                       unsigned long max)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    const char *value = search_field(reader, key);
    return parse_ulong(10, value, max);
}

unsigned long DP_text_reader_get_ulong_hex(DP_TextReader *reader,
                                           const char *key, unsigned long max)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    const char *value = search_field(reader, key);
    if (value && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        return parse_ulong(16, value + 2, max);
    }
    return 0;
}

static double parse_decimal(const char *value, double multiplier, double min,
                            double max)
{
    if (!value || *value == '\0') {
        return 0.0;
    }

    char *end;
    double result = strtof(value, &end);
    if (*end != '\0' || isnan(result)) {
        DP_warn("Error parsing decimal field '%s'", value);
        return 0.0;
    }

    double scaled_result = result * multiplier;
    if (scaled_result >= min && scaled_result <= max) {
        return scaled_result >= 0.0 ? scaled_result + 0.5 : scaled_result - 0.5;
    }
    else {
        DP_warn("Decimal field value %f not in bounds [%f, %f]", scaled_result,
                min, max);
        return 0.0;
    }
}

double DP_text_reader_get_decimal(DP_TextReader *reader, const char *key,
                                  double multiplier, double min, double max)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    DP_ASSERT(min <= max);
    const char *value = search_field(reader, key);
    return parse_decimal(value, multiplier, min, max);
}

const char *DP_text_reader_get_string(DP_TextReader *reader, const char *key,
                                      uint16_t *out_len)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    DP_ASSERT(out_len);
    const char *value = search_field(reader, key);
    if (value) {
        size_t len = strlen(value);
        if (len <= UINT16_MAX) {
            *out_len = DP_size_to_uint16(len);
            return value;
        }
        else {
            DP_warn("String length %zu out of bounds", len);
        }
    }
    *out_len = 0;
    return "";
}

static uint32_t parse_argb(const char *value, uint32_t base)
{
    if (value[0] != '#') {
        DP_warn("Invalid color field '%s'", value);
        return 0;
    }

    char *end;
    unsigned long result = strtoul(value + 1, &end, 16);
    if (*end != '\0') {
        DP_warn("Error parsing color '%s'", value);
        return 0;
    }

    return DP_ulong_to_uint32(result) | base;
}

uint32_t DP_text_reader_get_argb_color(DP_TextReader *reader, const char *key)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    const char *value = search_field(reader, key);
    if (value) {
        size_t len = strlen(value);
        if (len == 7) {
            return parse_argb(value, 0xff000000u);
        }
        else if (len == 9) {
            return parse_argb(value, 0x0u);
        }
        else {
            DP_warn("Invalid color of length %zu: '%s'", len, value);
        }
    }
    return 0;
}

uint8_t DP_text_reader_get_blend_mode(DP_TextReader *reader, const char *key)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    const char *value = search_field(reader, key);
    return value
             ? (uint8_t)DP_blend_mode_by_svg_name(value, DP_BLEND_MODE_NORMAL)
             : DP_BLEND_MODE_NORMAL;
}

static unsigned int get_flag_value(int count, const char **keys,
                                   unsigned int *values, const char *in,
                                   size_t len)
{
    for (int i = 0; i < count; ++i) {
        const char *key = keys[i];
        if (strlen(key) == len && memcmp(in, key, len) == 0) {
            return values[i];
        }
    }
    return 0;
}

unsigned int DP_text_reader_get_flags(DP_TextReader *reader, const char *key,
                                      int count, const char **keys,
                                      unsigned int *values)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    DP_ASSERT(count > 0);
    DP_ASSERT(keys);
    DP_ASSERT(values);
    unsigned int flags = 0;
    const char *value = search_field(reader, key);
    if (value) {
        size_t i = 0;
        size_t start = 0;
        while (true) {
            char c = value[i];
            if (c == ',' || c == '\0') {
                flags |= get_flag_value(count, keys, values, value + start,
                                        i - start);
                if (c == ',') {
                    start = i + 2;
                }
                else {
                    break;
                }
            }
            ++i;
        }
    }
    return flags;
}

DP_TextReaderParseParams
DP_text_reader_get_base64_string(DP_TextReader *reader, const char *key,
                                 size_t *out_decoded_len)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    DP_ASSERT(out_decoded_len);
    const char *value = search_field(reader, key);
    if (value) {
        size_t len = strlen(value);
        *out_decoded_len = DP_base64_decode_length(value, len);
        return (DP_TextReaderParseParams){value, len};
    }
    *out_decoded_len = 0;
    return (DP_TextReaderParseParams){"", 0};
}

void DP_text_reader_parse_base64(size_t size, unsigned char *out, void *user)
{
    if (size != 0) {
        DP_TextReaderParseParams *params = user;
        DP_base64_decode(params->value, params->length, out, size);
    }
}

DP_TextReaderParseParams
DP_text_reader_get_comma_separated(DP_TextReader *reader, const char *key,
                                   int *out_count)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    DP_ASSERT(out_count);
    const char *value = search_field(reader, key);
    if (value) {
        size_t len = strlen(value);
        bool content_found = false;
        int count = 0;
        for (size_t i = 0; i < len; ++i) {
            char c = value[i];
            if (c == ',') {
                content_found = false;
            }
            else if (!isspace(c) && !content_found) {
                content_found = true;
                ++count;
            }
        }
        *out_count = count;
        return (DP_TextReaderParseParams){value, len};
    }
    *out_count = 0;
    return (DP_TextReaderParseParams){"", 0};
}

static bool has_hex_prefix(const char *value)
{
    return value && value[0] == '0' && (value[1] == 'x' || value[1] == 'X');
}

static void
parse_comma_separated_array(int count, DP_TextReaderParseParams *params,
                            const char *title, unsigned long max,
                            void (*set_at)(void *, int, unsigned long),
                            void *user, bool hex)
{
    if (count != 0) {
        const char *value = params->value;
        size_t length = params->length;
        char buffer[64]; // plenty to hold the digits to a uint8 and uint16
        size_t buffer_fill = 0;
        int out_index = 0;

        for (size_t i = 0; i <= length; ++i) {
            char c = i == length ? ',' : value[i];
            if (c == ',') {
                if (buffer_fill != 0) {
                    buffer[buffer_fill] = '\0';

                    unsigned long result;
                    if (hex) {
                        if (has_hex_prefix(buffer)) {
                            result = parse_ulong(16, buffer + 2, max);
                        }
                        else {
                            DP_warn("Invalid %s value '%s'", title, buffer);
                            result = 0;
                        }
                    }
                    else {
                        result = parse_ulong(10, buffer, max);
                    }

                    set_at(user, out_index, result);
                    ++out_index;
                    buffer_fill = 0;
                }
            }
            else if (!isspace(c) && buffer_fill < sizeof(buffer) - 1) {
                buffer[buffer_fill++] = c;
            }
        }

        DP_ASSERT(out_index == count);
    }
}

static void set_uint8(void *user, int index, unsigned long result)
{
    DP_ASSERT(result <= UINT8_MAX);
    uint8_t *out = user;
    out[index] = DP_ulong_to_uint8(result);
}

void DP_text_reader_parse_uint8_array(int count, uint8_t *out, void *user)
{
    parse_comma_separated_array(count, user, "uint8", UINT8_MAX, set_uint8, out,
                                false);
}

static void set_uint16(void *user, int index, unsigned long result)
{
    DP_ASSERT(result <= UINT16_MAX);
    uint16_t *out = user;
    out[index] = DP_ulong_to_uint16(result);
}

void DP_text_reader_parse_uint16_array(int count, uint16_t *out, void *user)
{
    parse_comma_separated_array(count, user, "uint16", UINT16_MAX, set_uint16,
                                out, false);
}

void DP_text_reader_parse_uint16_array_hex(int count, uint16_t *out, void *user)
{
    parse_comma_separated_array(count, user, "uint16", UINT16_MAX, set_uint16,
                                out, true);
}

int DP_text_reader_get_tuple_count(DP_TextReader *reader)
{
    DP_ASSERT(reader);
    return DP_size_to_int(reader->tuple.rows.used);
}

static const char *get_tuple_field(DP_TextReader *reader, size_t row,
                                   size_t col)
{
    DP_TextReaderTupleRow trtr =
        DP_VECTOR_AT_TYPE(&reader->tuple.rows, DP_TextReaderTupleRow, row);
    if (col < trtr.count) {
        size_t offset = DP_VECTOR_AT_TYPE(&reader->tuple.offsets, size_t,
                                          trtr.offset_index + col);
        return reader->field.buffer + offset;
    }
    else {
        DP_warn("Tuple in row %zu has no column %zu", row, col);
        return NULL;
    }
}

long DP_text_reader_get_subfield_long(DP_TextReader *reader, int row, int col,
                                      long min, long max)
{
    DP_ASSERT(reader);
    DP_ASSERT(min < max);
    const char *value =
        get_tuple_field(reader, DP_int_to_size(row), DP_int_to_size(col));
    return parse_long(value, min, max);
}

unsigned long DP_text_reader_get_subfield_ulong(DP_TextReader *reader, int row,
                                                int col, unsigned long max)
{
    DP_ASSERT(reader);
    const char *value =
        get_tuple_field(reader, DP_int_to_size(row), DP_int_to_size(col));
    return parse_ulong(10, value, max);
}

double DP_text_reader_get_subfield_decimal(DP_TextReader *reader, int row,
                                           int col, double multiplier,
                                           double min, double max)
{
    DP_ASSERT(reader);
    DP_ASSERT(min < max);

    const char *value =
        get_tuple_field(reader, DP_int_to_size(row), DP_int_to_size(col));
    return parse_decimal(value, multiplier, min, max);
}
