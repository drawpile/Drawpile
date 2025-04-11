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
#include <parson.h>


#define MIN_READ_CAPACITY 4096

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
    JSON_Object *body;
};

DP_TextReader *DP_text_reader_new(DP_Input *input)
{
    DP_ASSERT(input);
    bool error;
    size_t input_length = DP_input_length(input, &error);
    if (error) {
        DP_input_free(input);
        return NULL;
    }

    DP_TextReader *reader = DP_malloc(sizeof(*reader));
    *reader = (DP_TextReader){input, input_length, 0, 0, 0, {0, 0, NULL}, NULL};
    return reader;
}

void DP_text_reader_free(DP_TextReader *reader)
{
    if (reader) {
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
    if (new_used != 0) {
        memmove(reader->read.buffer, reader->read.buffer + consume, new_used);
    }
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

static void skip_body(const char *buffer, size_t start, size_t end,
                      size_t *out_body_start, size_t *out_body_end)
{
    size_t i = start;
    while (i < end) {
        size_t pos = i++;
        char c = buffer[pos];
        if (c == '\n') {
            *out_body_start = pos;
            *out_body_end = pos;
            return;
        }
        else if (!isspace(c)) {
            start = pos;
            break;
        }
    }

    while (i < end) {
        size_t pos = i++;
        if (buffer[pos] == '\n') {
            *out_body_start = start;
            *out_body_end = pos;
            return;
        }
    }

    *out_body_start = start;
    *out_body_end = end;
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


DP_TextReaderResult DP_text_reader_read_message(DP_TextReader *reader,
                                                DP_Message **out_msg)
{
    DP_ASSERT(reader);
    DP_ASSERT(out_msg);

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

        DP_MessageType type =
            DP_message_type_from_name(buffer + type_start, DP_MSG_TYPE_COUNT);
        if (type == DP_MSG_TYPE_COUNT) {
            DP_error_set("Unknown message type '%s'", buffer + type_start);
            return DP_TEXT_READER_ERROR_PARSE;
        }

        size_t body_start, body_end;
        skip_body(buffer, type_end + 1, end, &body_start, &body_end);

        JSON_Value *json;
        if (body_start == body_end) {
            json = NULL;
        }
        else {
            buffer[body_end] = '\0';
            json = json_parse_string(buffer + body_start);
            if (!json) {
                DP_error_set("Failed to parse message body");
                return DP_TEXT_READER_ERROR_PARSE;
            }

            reader->body = json_value_get_object(json);
            if (!reader->body) {
                json_value_free(json);
                DP_error_set("Message body is not an object");
                return DP_TEXT_READER_ERROR_PARSE;
            }
        }

        DP_Message *msg = DP_message_parse_body(type, context_id, reader);
        json_value_free(json);
        reader->body = NULL;

        if (msg) {
            *out_msg = msg;
            return DP_TEXT_READER_SUCCESS;
        }
        else {
            return DP_TEXT_READER_ERROR_PARSE;
        }
    }
}


static const char *stringify_json_type(JSON_Value_Type type)
{
    switch (type) {
    case JSONError:
        return "error";
    case JSONNull:
        return "null";
    case JSONString:
        return "string";
    case JSONNumber:
        return "number";
    case JSONObject:
        return "object";
    case JSONArray:
        return "array";
    case JSONBoolean:
        return "boolean";
    }
    return "unknown";
}

static bool check_json_type(const char *key, JSON_Value *value,
                            JSON_Value_Type type)
{
    JSON_Value_Type value_type = json_value_get_type(value);
    if (value_type == type) {
        return true;
    }
    else {
        DP_warn("Expected field '%s' to be %s, but is %s", key,
                stringify_json_type(type), stringify_json_type(value_type));
        return false;
    }
}

static bool get_field(DP_TextReader *reader, const char *key,
                      JSON_Value **out_value)
{
    JSON_Object *body = reader->body;
    if (!body) {
        return false;
    }

    JSON_Value *value = json_object_get_value(body, key);
    if (!value) {
        return false;
    }

    *out_value = value;
    return true;
}

static bool get_string_field(DP_TextReader *reader, const char *key,
                             const char **out_s, size_t *out_len)
{
    JSON_Value *value;
    if (!get_field(reader, key, &value)) {
        return false;
    }

    if (!check_json_type(key, value, JSONString)) {
        return false;
    }

    const char *s = json_value_get_string(value);
    size_t len;
    if (s && s[0] != '\0') {
        *out_s = s;
        len = strlen(s);
    }
    else {
        *out_s = "";
        len = 0;
    }

    if (out_len) {
        *out_len = len;
    }
    return true;
}

bool DP_text_reader_get_bool(DP_TextReader *reader, const char *key)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    JSON_Value *value;
    return get_field(reader, key, &value)
        && check_json_type(key, value, JSONBoolean)
        && json_value_get_boolean(value);
}

static long get_long(const char *key, long min, long max, JSON_Value *value)
{
    if (check_json_type(key, value, JSONNumber)) {
        double d = json_value_get_number(value);
        long l = lround(d);
        if (l >= min && l <= max) {
            return l;
        }
        else {
            DP_warn("Signed field '%s' value %f not in bounds [%ld, %ld]", key,
                    d, min, max);
            return 0L;
        }
    }
    else {
        return 0L;
    }
}

long DP_text_reader_get_long(DP_TextReader *reader, const char *key, long min,
                             long max)

{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    JSON_Value *value;
    if (get_field(reader, key, &value)) {
        return get_long(key, min, max, value);
    }
    else {
        return 0;
    }
}

static unsigned long get_ulong(const char *key, unsigned long max,
                               JSON_Value *value)
{
    if (check_json_type(key, value, JSONNumber)) {
        double d = json_value_get_number(value);
        unsigned long ul;
        if (d >= 0.0 && (ul = DP_double_to_ulong(d + 0.5)) <= max) {
            return ul;
        }
        else {
            DP_warn("Unsigned field '%s' value %f not in bounds [0, %lu]", key,
                    d, max);
            return 0UL;
        }
    }
    else {
        return 0UL;
    }
}

unsigned long DP_text_reader_get_ulong(DP_TextReader *reader, const char *key,
                                       unsigned long max)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    JSON_Value *value;
    if (get_field(reader, key, &value)) {
        return get_ulong(key, max, value);
    }
    else {
        return 0;
    }
}

static double get_decimal(const char *key, double multiplier, double min,
                          double max, JSON_Value *value)
{
    if (check_json_type(key, value, JSONNumber)) {
        double d = json_value_get_number(value);
        double scaled = d * multiplier;
        if (scaled >= min && scaled <= max) {
            return scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5;
        }
        else {
            DP_warn("Decimal field '%s' value %f not in bounds [%f, %f]", key,
                    d, min, max);
            return 0.0;
        }
    }
    else {
        return 0.0;
    }
}

double DP_text_reader_get_decimal(DP_TextReader *reader, const char *key,
                                  double multiplier, double min, double max)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    DP_ASSERT(min <= max);
    JSON_Value *value;
    if (get_field(reader, key, &value)) {
        return get_decimal(key, multiplier, min, max, value);
    }
    else {
        return 0.0;
    }
}

const char *DP_text_reader_get_string(DP_TextReader *reader, const char *key,
                                      uint16_t *out_len)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    DP_ASSERT(out_len);
    const char *s;
    size_t len;
    if (get_string_field(reader, key, &s, &len)) {
        if (len <= UINT16_MAX) {
            *out_len = DP_size_to_uint16(len);
            return s;
        }
        else {
            DP_warn("String field '%s' length %zu out of bounds", key, len);
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
    const char *s;
    size_t len;
    if (get_string_field(reader, key, &s, &len)) {
        if (len == 7) {
            return parse_argb(s, 0xff000000u);
        }
        else if (len == 9) {
            return parse_argb(s, 0x0u);
        }
        else {
            DP_warn("Invalid color of length %zu: '%s'", len, s);
        }
    }
    return 0;
}

uint8_t DP_text_reader_get_blend_mode(DP_TextReader *reader, const char *key)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    const char *s;
    return get_string_field(reader, key, &s, NULL)
             ? (uint8_t)DP_blend_mode_by_dptxt_name(s, DP_BLEND_MODE_NORMAL)
             : DP_BLEND_MODE_NORMAL;
}

static unsigned int get_flag_value(int count, const char **keys,
                                   unsigned int *values, const char *s)
{
    size_t len = strlen(s);
    for (int i = 0; i < count; ++i) {
        const char *key = keys[i];
        if (strlen(key) == len && memcmp(s, key, len) == 0) {
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
    JSON_Value *value;
    if (get_field(reader, key, &value)
        && check_json_type(key, value, JSONArray)) {
        JSON_Array *array = json_value_get_array(value);
        size_t array_count = json_array_get_count(array);
        for (size_t i = 0; i < array_count; ++i) {
            const char *s =
                json_value_get_string(json_array_get_value(array, i));
            if (s) {
                unsigned int flag = get_flag_value(count, keys, values, s);
                if (flag == 0) {
                    DP_warn("Unknown '%s' flag '%s'", key, s);
                }
                else {
                    flags |= flag;
                }
            }
            else {
                DP_warn("Flag %zu value in %s is not a string", i, key);
            }
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
    const char *s;
    size_t len;
    if (get_string_field(reader, key, &s, &len)) {
        *out_decoded_len = DP_base64_decode_length(s, len);
        return (DP_TextReaderParseParams){.value = s, .length = len};
    }
    *out_decoded_len = 0;
    return (DP_TextReaderParseParams){.value = "", .length = 0};
}

void DP_text_reader_parse_base64(size_t size, unsigned char *out, void *user)
{
    if (size != 0) {
        DP_TextReaderParseParams *params = user;
        DP_base64_decode(params->value, params->length, out, size);
    }
}

DP_TextReaderParseParams
DP_text_reader_get_array(DP_TextReader *reader, const char *key, int *out_count)
{
    DP_ASSERT(reader);
    DP_ASSERT(key);
    DP_ASSERT(out_count);
    JSON_Value *value;
    if (get_field(reader, key, &value)
        && check_json_type(key, value, JSONArray)) {
        JSON_Array *array = json_value_get_array(value);
        *out_count = DP_size_to_int(json_array_get_count(array));
        return (DP_TextReaderParseParams){.array = array};
    }
    else {
        *out_count = 0;
        return (DP_TextReaderParseParams){.array = NULL};
    }
}

static long parse_iarray_value(JSON_Array *array, size_t array_count,
                               const char *title, long min, long max, int i)
{
    size_t si = DP_int_to_size(i);
    if (si >= array_count) {
        return 0UL;
    }

    JSON_Value *value = json_array_get_value(array, si);
    if (json_value_get_type(value) != JSONNumber) {
        DP_warn("Value %d in %s is not a string", i, title);
        return 0UL;
    }

    double d = json_value_get_number(value);
    long l = lround(d);
    if (l < min || l > max) {
        DP_warn("Value %d in %s not in bounds [%ld, %ld]", i, title, min, max);
        return 0UL;
    }

    return l;
}

static unsigned long parse_uarray_value(JSON_Array *array, size_t array_count,
                                        const char *title, unsigned long max,
                                        int i)
{
    size_t si = DP_int_to_size(i);
    if (si >= array_count) {
        return 0UL;
    }

    JSON_Value *value = json_array_get_value(array, si);
    if (json_value_get_type(value) != JSONNumber) {
        DP_warn("Value %d in %s is not a string", i, title);
        return 0UL;
    }

    double d = json_value_get_number(value);
    unsigned long ul;
    if (d < 0.0 || (ul = DP_double_to_ulong(d + 0.5)) > max) {
        DP_warn("Value %d in %s not in bounds [0, %lu]", i, title, max);
        return 0UL;
    }

    return ul;
}

static void parse_iarray(int count, DP_TextReaderParseParams *params,
                         const char *title, long min, long max,
                         void (*set_at)(void *, int, long), void *user)
{
    if (count != 0) {
        JSON_Array *array = params->array;
        size_t array_count = json_array_get_count(array);
        size_t scount = DP_int_to_size(count);
        if (array_count != scount) {
            DP_warn("Got %zu value(s) in array to fill %d %s target value(s)",
                    array_count, count, title);
        }

        for (int i = 0; i < count; ++i) {
            long value =
                parse_iarray_value(array, array_count, title, min, max, i);
            set_at(user, i, value);
        }
    }
}

static void parse_uarray(int count, DP_TextReaderParseParams *params,
                         const char *title, unsigned long max,
                         void (*set_at)(void *, int, unsigned long), void *user)
{
    if (count != 0) {
        JSON_Array *array = params->array;
        size_t array_count = json_array_get_count(array);
        size_t scount = DP_int_to_size(count);
        if (array_count != scount) {
            DP_warn("Got %zu value(s) in array to fill %d %s target value(s)",
                    array_count, count, title);
        }

        for (int i = 0; i < count; ++i) {
            unsigned long value =
                parse_uarray_value(array, array_count, title, max, i);
            set_at(user, i, value);
        }
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
    parse_uarray(count, user, "uint8", UINT8_MAX, set_uint8, out);
}

static void set_uint16(void *user, int index, unsigned long result)
{
    DP_ASSERT(result <= UINT16_MAX);
    uint16_t *out = user;
    out[index] = DP_ulong_to_uint16(result);
}

void DP_text_reader_parse_uint16_array(int count, uint16_t *out, void *user)
{
    parse_uarray(count, user, "uint16", UINT16_MAX, set_uint16, out);
}

static void set_int32(void *user, int index, long result)
{
    DP_ASSERT(result >= INT32_MIN);
    DP_ASSERT(result <= INT32_MAX);
    int32_t *out = user;
    out[index] = DP_long_to_int32(result);
}

void DP_text_reader_parse_int32_array(int count, int32_t *out, void *user)
{
    parse_iarray(count, user, "int32", INT32_MIN, INT32_MAX, set_int32, out);
}

static bool get_subfield_value(DP_TextReader *reader, int i, const char *subkey,
                               JSON_Value **out_value)
{
    JSON_Value *value;
    if (get_field(reader, "_", &value)
        && check_json_type("_", value, JSONArray)) {
        JSON_Array *array = json_value_get_array(value);
        size_t array_count = json_array_get_count(array);
        size_t si = DP_int_to_size(i);
        if (si < array_count) {
            JSON_Object *subobject =
                json_value_get_object(json_array_get_value(array, si));
            if (subobject) {
                JSON_Value *subvalue = json_object_get_value(subobject, subkey);
                if (subvalue) {
                    *out_value = subvalue;
                    return true;
                }
                else {
                    return false;
                }
            }
            else {
                DP_warn("Subvalue %d is not an object", i);
                return false;
            }
        }
        else {
            DP_warn("Subindex %d out of bounds", i);
            return false;
        }
    }
    else {
        return false;
    }
}

int DP_text_reader_get_sub_count(DP_TextReader *reader)
{
    DP_ASSERT(reader);
    JSON_Value *value;
    if (get_field(reader, "_", &value)
        && check_json_type("_", value, JSONArray)) {
        return DP_size_to_int(
            json_array_get_count(json_value_get_array(value)));
    }
    else {
        return 0;
    }
}

long DP_text_reader_get_subfield_long(DP_TextReader *reader, int i,
                                      const char *subkey, long min, long max)
{
    DP_ASSERT(reader);
    DP_ASSERT(i >= 0);
    DP_ASSERT(subkey);
    DP_ASSERT(min < max);
    JSON_Value *value;
    if (get_subfield_value(reader, i, subkey, &value)) {
        return get_long(subkey, min, max, value);
    }
    else {
        return 0L;
    }
}

unsigned long DP_text_reader_get_subfield_ulong(DP_TextReader *reader, int i,
                                                const char *subkey,
                                                unsigned long max)
{
    DP_ASSERT(reader);
    DP_ASSERT(i >= 0);
    DP_ASSERT(subkey);
    JSON_Value *value;
    if (get_subfield_value(reader, i, subkey, &value)) {
        return get_ulong(subkey, max, value);
    }
    else {
        return 0UL;
    }
}

double DP_text_reader_get_subfield_decimal(DP_TextReader *reader, int i,
                                           const char *subkey,
                                           double multiplier, double min,
                                           double max)
{
    DP_ASSERT(reader);
    DP_ASSERT(i >= 0);
    DP_ASSERT(subkey);
    DP_ASSERT(min < max);
    JSON_Value *value;
    if (get_subfield_value(reader, i, subkey, &value)) {
        return get_decimal(subkey, multiplier, min, max, value);
    }
    else {
        return 0UL;
    }
}

uint32_t DP_text_reader_get_subfield_rgb_color(DP_TextReader *reader, int i,
                                               const char *subkey)
{
    DP_ASSERT(reader);
    DP_ASSERT(i >= 0);
    DP_ASSERT(subkey);
    JSON_Value *value;
    if (get_subfield_value(reader, i, subkey, &value)
        && check_json_type(subkey, value, JSONString)) {
        const char *s = json_value_get_string(value);
        size_t len = s ? strlen(s) : 0;
        if (len == 7) {
            return parse_argb(s, 0xff000000u);
        }
        else {
            DP_warn("Invalid %s color of length %zu: '%s'", subkey, len,
                    s ? s : "");
            return 0u;
        }
    }
    else {
        return 0u;
    }
}
