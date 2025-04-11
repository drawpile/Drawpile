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
#ifndef DPMSG_TEXT_READER_H
#define DPMSG_TEXT_READER_H
#include "message.h"
#include <dpcommon/common.h>

typedef struct DP_Input DP_Input;
typedef struct json_array_t JSON_Array;


typedef struct DP_TextReader DP_TextReader;

typedef enum DP_TextReaderResult {
    DP_TEXT_READER_SUCCESS,
    DP_TEXT_READER_HEADER_END,
    DP_TEXT_READER_INPUT_END,
    DP_TEXT_READER_ERROR_INPUT,
    DP_TEXT_READER_ERROR_PARSE,
} DP_TextReaderResult;

typedef struct DP_TextReaderParseParams {
    union {
        struct {
            const char *value;
            size_t length;
        };
        JSON_Array *array;
    };
} DP_TextReaderParseParams;

DP_TextReader *DP_text_reader_new(DP_Input *input);

void DP_text_reader_free(DP_TextReader *reader);

size_t DP_text_reader_body_offset(DP_TextReader *reader);

size_t DP_text_reader_tell(DP_TextReader *reader);

bool DP_text_reader_seek(DP_TextReader *reader, size_t offset);

double DP_text_reader_progress(DP_TextReader *reader);

DP_TextReaderResult DP_text_reader_read_header_field(DP_TextReader *reader,
                                                     const char **out_key,
                                                     const char **out_value);

DP_TextReaderResult DP_text_reader_read_message(DP_TextReader *reader,
                                                DP_Message **out_msg);

bool DP_text_reader_get_bool(DP_TextReader *reader, const char *key);

long DP_text_reader_get_long(DP_TextReader *reader, const char *key, long min,
                             long max);

unsigned long DP_text_reader_get_ulong(DP_TextReader *reader, const char *key,
                                       unsigned long max);

double DP_text_reader_get_decimal(DP_TextReader *reader, const char *key,
                                  double multiplier, double min, double max);

const char *DP_text_reader_get_string(DP_TextReader *reader, const char *key,
                                      uint16_t *out_len);

uint32_t DP_text_reader_get_argb_color(DP_TextReader *reader, const char *key);

uint8_t DP_text_reader_get_blend_mode(DP_TextReader *reader, const char *key);

unsigned int DP_text_reader_get_flags(DP_TextReader *reader, const char *key,
                                      int count, const char **keys,
                                      unsigned int *values);

DP_TextReaderParseParams
DP_text_reader_get_base64_string(DP_TextReader *reader, const char *key,
                                 size_t *out_decoded_len);

void DP_text_reader_parse_base64(size_t size, unsigned char *out, void *user);

DP_TextReaderParseParams DP_text_reader_get_array(DP_TextReader *reader,
                                                  const char *key,
                                                  int *out_count);

void DP_text_reader_parse_uint8_array(int count, uint8_t *out, void *user);

void DP_text_reader_parse_uint16_array(int count, uint16_t *out, void *user);

void DP_text_reader_parse_int32_array(int count, int32_t *out, void *user);

int DP_text_reader_get_sub_count(DP_TextReader *reader);

long DP_text_reader_get_subfield_long(DP_TextReader *reader, int i,
                                      const char *subkey, long min, long max);

unsigned long DP_text_reader_get_subfield_ulong(DP_TextReader *reader, int i,
                                                const char *subkey,
                                                unsigned long max);

double DP_text_reader_get_subfield_decimal(DP_TextReader *reader, int i,
                                           const char *subkey,
                                           double multiplier, double min,
                                           double max);

uint32_t DP_text_reader_get_subfield_rgb_color(DP_TextReader *reader, int i,
                                               const char *subkey);

#endif
