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
#include "text_writer.h"
#include "blend_mode.h"
#include "dpcommon/output.h"
#include "message.h"
#include <dpcommon/base64.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <inttypes.h>
#include <parson.h>

#define DECIMAL_BUFFER_SIZE 1024


struct DP_TextWriter {
    DP_Output *output;
    bool payload_open;
    bool first_subobject;
    bool first_subfield;
    char decimal_buffer[DECIMAL_BUFFER_SIZE];
};

DP_TextWriter *DP_text_writer_new(DP_Output *output)
{
    DP_ASSERT(output);
    DP_TextWriter *writer = DP_malloc(sizeof(*writer));
    writer->output = output;
    writer->payload_open = false;
    writer->first_subobject = false;
    writer->first_subfield = false;
    return writer;
}

void DP_text_writer_free(DP_TextWriter *writer)
{
    if (writer) {
        DP_output_free(writer->output);
        DP_free(writer);
    }
}


static bool print_header_field_prefix(DP_Output *output, const char *key)
{
    return DP_output_write(output, "!", 1) && DP_output_print(output, key)
        && DP_output_write(output, "=", 1);
}

static bool print_header_field_suffix(DP_Output *output)
{
    return DP_output_write(output, "\n", 1);
}

static bool print_header_field(DP_Output *output, const char *key,
                               const char *value)
{
    return print_header_field_prefix(output, key)
        && DP_output_print(output, value) && print_header_field_suffix(output);
}

static bool write_header_field(DP_Output *output, const char *key,
                               JSON_Value *value)
{
    switch (json_type(value)) {
    case JSONNull:
        return print_header_field(output, key, "null");
    case JSONString:
        return print_header_field(output, key, json_string(value));
    case JSONNumber:
        return print_header_field_prefix(output, key)
            && DP_output_format(output, "%f", json_number(value))
            && print_header_field_suffix(output);
    case JSONBoolean:
        return print_header_field(output, key,
                                  json_boolean(value) ? "true" : "false");
    default:
        DP_error_set("Header field '%s' cannot be represented as text", key);
        return false;
    }
}

bool DP_text_writer_write_header(DP_TextWriter *writer, JSON_Object *header)
{
    DP_ASSERT(writer);
    DP_ASSERT(header);

    DP_Output *output = writer->output;
    size_t count = json_object_get_count(header);
    for (size_t i = 0; i < count; ++i) {
        const char *key = json_object_get_name(header, i);
        JSON_Value *value = json_object_get_value_at(header, i);
        if (!write_header_field(output, key, value)) {
            return false;
        }
    }
    return true;
}


bool DP_text_writer_start_message(DP_TextWriter *writer, DP_Message *msg)
{
    DP_ASSERT(writer);
    DP_ASSERT(msg);
    writer->payload_open = false;
    writer->first_subobject = false;
    return DP_output_format(writer->output, "%d %s", DP_message_context_id(msg),
                            DP_message_name(msg));
}

bool DP_text_writer_finish_message(DP_TextWriter *writer)
{
    DP_ASSERT(writer);
    if (writer->payload_open) {
        return DP_OUTPUT_PRINT_LITERAL(writer->output, "}\n");
    }
    else {
        return DP_OUTPUT_PRINT_LITERAL(writer->output, "\n");
    }
}


static bool open_payload(DP_TextWriter *writer)
{
    if (writer->payload_open) {
        return DP_OUTPUT_PRINT_LITERAL(writer->output, ",");
    }
    else {
        writer->payload_open = true;
        return DP_OUTPUT_PRINT_LITERAL(writer->output, " {");
    }
}

static bool format_argument(DP_TextWriter *writer, const char *fmt, ...)
    DP_FORMAT(2, 3);

static bool format_argument(DP_TextWriter *writer, const char *fmt, ...)
{
    if (open_payload(writer)) {
        va_list ap;
        va_start(ap, fmt);
        bool ok = DP_output_vformat(writer->output, fmt, ap);
        va_end(ap);
        return ok;
    }
    else {
        return false;
    }
}


bool DP_text_writer_write_bool(DP_TextWriter *writer, const char *key,
                               bool value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, "\"%s\":%s", key, value ? "true" : "false");
}

bool DP_text_writer_write_int(DP_TextWriter *writer, const char *key, int value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, "\"%s\":%d", key, value);
}

bool DP_text_writer_write_uint(DP_TextWriter *writer, const char *key,
                               unsigned int value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, "\"%s\":%u", key, value);
}

static const char *format_decimal(DP_TextWriter *writer, double value)
{
    // Get rid of pointless trailing zeroes and decimal point.
    char *buffer = writer->decimal_buffer;
    snprintf(buffer, DECIMAL_BUFFER_SIZE, "%.8f", value);
    for (size_t i = 0; buffer[i] != '\0'; ++i) {
        if (buffer[i] == '.') {
            size_t end = i;
            for (size_t j = i + 1; buffer[j] != '\0'; ++j) {
                if (buffer[j] != '0') {
                    end = j + 1;
                }
            }
            buffer[end] = '\0';
            break;
        }
    }
    return buffer;
}

bool DP_text_writer_write_decimal(DP_TextWriter *writer, const char *key,
                                  double value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return format_argument(writer, "\"%s\":%s", key,
                           format_decimal(writer, value));
}

static bool write_string(DP_Output *output, const char *value, size_t len)
{
    bool ok = DP_OUTPUT_PRINT_LITERAL(output, "\"");
    for (size_t i = 0; ok && i < len; ++i) {
        switch (value[i]) {
        case '\"':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\\"");
            break;
        case '\\':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\\\");
            break;
        case '\b':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\b");
            break;
        case '\f':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\f");
            break;
        case '\n':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\n");
            break;
        case '\r':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\r");
            break;
        case '\t':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\t");
            break;
        case '\x00':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0000");
            break;
        case '\x01':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0001");
            break;
        case '\x02':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0002");
            break;
        case '\x03':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0003");
            break;
        case '\x04':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0004");
            break;
        case '\x05':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0005");
            break;
        case '\x06':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0006");
            break;
        case '\x07':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0007");
            break;
        case '\x0b':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u000b");
            break;
        case '\x0e':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u000e");
            break;
        case '\x0f':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u000f");
            break;
        case '\x10':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0010");
            break;
        case '\x11':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0011");
            break;
        case '\x12':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0012");
            break;
        case '\x13':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0013");
            break;
        case '\x14':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0014");
            break;
        case '\x15':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0015");
            break;
        case '\x16':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0016");
            break;
        case '\x17':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0017");
            break;
        case '\x18':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0018");
            break;
        case '\x19':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u0019");
            break;
        case '\x1a':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u001a");
            break;
        case '\x1b':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u001b");
            break;
        case '\x1c':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u001c");
            break;
        case '\x1d':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u001d");
            break;
        case '\x1e':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u001e");
            break;
        case '\x1f':
            ok = DP_OUTPUT_PRINT_LITERAL(output, "\\u001f");
            break;
        default:
            ok = DP_output_write(output, value + i, 1);
            break;
        }
    }
    return ok && DP_OUTPUT_PRINT_LITERAL(output, "\"");
}

bool DP_text_writer_write_string(DP_TextWriter *writer, const char *key,
                                 const char *value)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    DP_ASSERT(value);
    size_t len = strlen(value);
    if (len == 0) {
        return format_argument(writer, "\"%s\":\"\"", key);
    }
    else {
        return format_argument(writer, "\"%s\":", key)
            && write_string(writer->output, value, len);
    }
}

#define ALPHA_MASK 0xff000000
#define RGB_MASK   0x00ffffff

bool DP_text_writer_write_argb_color(DP_TextWriter *writer, const char *key,
                                     uint32_t bgra)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    if ((bgra & ALPHA_MASK) == ALPHA_MASK) {
        return format_argument(writer, "\"%s\":\"#%06" PRIx32 "\"", key,
                               bgra & RGB_MASK);
    }
    else {
        return format_argument(writer, "\"%s\":\"#%08" PRIx32 "\"", key, bgra);
    }
}

bool DP_text_writer_write_blend_mode(DP_TextWriter *writer, const char *key,
                                     int blend_mode)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    return DP_text_writer_write_string(writer, key,
                                       DP_blend_mode_dptxt_name(blend_mode));
}

bool DP_text_writer_write_base64(DP_TextWriter *writer, const char *key,
                                 const unsigned char *value, int length)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);

    if (length <= 0) {
        return true;
    }
    else {
        DP_ASSERT(value);
        char *base64 = DP_base64_encode(value, DP_int_to_size(length), NULL);
        bool ok = format_argument(writer, "\"%s\":\"%s\"", key, base64);
        DP_free(base64);
        return ok;
    }
}

bool DP_text_writer_write_flags(DP_TextWriter *writer, const char *key,
                                unsigned int value, int count,
                                const char **names, unsigned int *values)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    DP_ASSERT(count >= 0);
    DP_ASSERT(names);
    DP_ASSERT(values);
    bool first = true;
    DP_Output *output = writer->output;
    for (int i = 0; i < count; ++i) {
        if (value & values[i]) {
            const char *name = names[i];
            if (first) {
                first = false;
                if (!format_argument(writer, "\"%s\":[\"%s\"", key, name)) {
                    return false;
                }
            }
            else if (!DP_output_format(output, ",\"%s\"", name)) {
                return false;
            }
        }
    }
    return first ? true : DP_OUTPUT_PRINT_LITERAL(output, "]");
}


#define WRITE_LIST(WRITER, KEY, VALUE, COUNT, TYPE, FMT)         \
    do {                                                         \
        if (COUNT == 0) {                                        \
            return true;                                         \
        }                                                        \
        else {                                                   \
            if (!format_argument(WRITER, "\"%s\":[" FMT, KEY,    \
                                 (TYPE)VALUE[0])) {              \
                return false;                                    \
            }                                                    \
            for (int _i = 1; _i < COUNT; ++_i) {                 \
                if (!DP_output_format(writer->output, "," FMT,   \
                                      (TYPE)VALUE[_i])) {        \
                    return false;                                \
                }                                                \
            }                                                    \
            return DP_OUTPUT_PRINT_LITERAL(writer->output, "]"); \
        }                                                        \
    } while (0)

bool DP_text_writer_write_uint8_list(DP_TextWriter *writer, const char *key,
                                     const uint8_t *value, int count)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    DP_ASSERT(count >= 0);
    DP_ASSERT(value || count == 0);
    WRITE_LIST(writer, key, value, count, unsigned int, "%u");
}

bool DP_text_writer_write_uint16_list(DP_TextWriter *writer, const char *key,
                                      const uint16_t *value, int count)
{
    DP_ASSERT(writer);
    DP_ASSERT(key);
    DP_ASSERT(count >= 0);
    DP_ASSERT(value || count == 0);
    WRITE_LIST(writer, key, value, count, unsigned int, "%u");
}

bool DP_text_writer_start_subs(DP_TextWriter *writer)
{
    DP_ASSERT(writer);
    writer->first_subobject = true;
    return format_argument(writer, "\"_\":[");
}

bool DP_text_writer_finish_subs(DP_TextWriter *writer)
{
    DP_ASSERT(writer);
    return DP_OUTPUT_PRINT_LITERAL(writer->output, "]");
}

bool DP_text_writer_start_subobject(DP_TextWriter *writer)
{
    DP_ASSERT(writer);
    writer->first_subfield = true;
    return writer->first_subobject
             ? DP_OUTPUT_PRINT_LITERAL(writer->output, "{")
             : DP_OUTPUT_PRINT_LITERAL(writer->output, ",{");
}

bool DP_text_writer_finish_subobject(DP_TextWriter *writer)
{
    writer->first_subobject = false;
    return DP_OUTPUT_PRINT_LITERAL(writer->output, "}");
}

static bool format_subfield(DP_TextWriter *writer, const char *fmt, ...)
    DP_FORMAT(2, 3);

static bool format_subfield(DP_TextWriter *writer, const char *fmt, ...)
{
    if (writer->first_subfield) {
        writer->first_subfield = false;
    }
    else if (!DP_OUTPUT_PRINT_LITERAL(writer->output, ",")) {
        return false;
    }
    va_list ap;
    va_start(ap, fmt);
    bool ok = DP_output_vformat(writer->output, fmt, ap);
    va_end(ap);
    return ok;
}

bool DP_text_writer_write_subfield_int(DP_TextWriter *writer, const char *key,
                                       int value)
{
    DP_ASSERT(writer);
    return format_subfield(writer, "\"%s\":%d", key, value);
}

bool DP_text_writer_write_subfield_uint(DP_TextWriter *writer, const char *key,
                                        unsigned int value)
{
    DP_ASSERT(writer);
    return format_subfield(writer, "\"%s\":%u", key, value);
}

bool DP_text_writer_write_subfield_decimal(DP_TextWriter *writer,
                                           const char *key, double value)
{
    DP_ASSERT(writer);
    return format_subfield(writer, "\"%s\":%s", key,
                           format_decimal(writer, value));
}
