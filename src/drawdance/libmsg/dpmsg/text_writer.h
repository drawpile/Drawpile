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
#ifndef DPMSG_TEXT_WRITER_H
#define DPMSG_TEXT_WRITER_H
#include <dpcommon/common.h>
#include <parson.h>

typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;


typedef struct DP_TextWriter DP_TextWriter;

DP_TextWriter *DP_text_writer_new(DP_Output *output);

void DP_text_writer_free(DP_TextWriter *writer);


bool DP_text_writer_write_header(DP_TextWriter *writer,
                                 JSON_Object *header) DP_MUST_CHECK;


bool DP_text_writer_start_message(DP_TextWriter *writer, DP_Message *msg);

bool DP_text_writer_finish_message(DP_TextWriter *writer);

bool DP_text_writer_write_bool(DP_TextWriter *writer, const char *key,
                               bool value) DP_MUST_CHECK;

bool DP_text_writer_write_int(DP_TextWriter *writer, const char *key,
                              int value) DP_MUST_CHECK;

bool DP_text_writer_write_uint(DP_TextWriter *writer, const char *key,
                               unsigned int value) DP_MUST_CHECK;

bool DP_text_writer_write_decimal(DP_TextWriter *writer, const char *key,
                                  double value) DP_MUST_CHECK;

bool DP_text_writer_write_string(DP_TextWriter *writer, const char *key,
                                 const char *value) DP_MUST_CHECK;

bool DP_text_writer_write_argb_color(DP_TextWriter *writer, const char *key,
                                     uint32_t bgra) DP_MUST_CHECK;

bool DP_text_writer_write_blend_mode(DP_TextWriter *writer, const char *key,
                                     int blend_mode) DP_MUST_CHECK;

bool DP_text_writer_write_base64(DP_TextWriter *writer, const char *key,
                                 const unsigned char *value,
                                 int length) DP_MUST_CHECK;

bool DP_text_writer_write_flags(DP_TextWriter *writer, const char *key,
                                unsigned int value, int count,
                                const char **names,
                                unsigned int *values) DP_MUST_CHECK;

bool DP_text_writer_write_uint8_list(DP_TextWriter *writer, const char *key,
                                     const uint8_t *value,
                                     int count) DP_MUST_CHECK;

bool DP_text_writer_write_uint16_list(DP_TextWriter *writer, const char *key,
                                      const uint16_t *value,
                                      int count) DP_MUST_CHECK;

bool DP_text_writer_write_uint24_list(DP_TextWriter *writer, const char *key,
                                      const uint32_t *value,
                                      int count) DP_MUST_CHECK;

bool DP_text_writer_write_uint32_list(DP_TextWriter *writer, const char *key,
                                      const uint32_t *value,
                                      int count) DP_MUST_CHECK;

bool DP_text_writer_write_int32_list(DP_TextWriter *writer, const char *key,
                                     const int32_t *value,
                                     int count) DP_MUST_CHECK;

bool DP_text_writer_start_subs(DP_TextWriter *writer) DP_MUST_CHECK;

bool DP_text_writer_finish_subs(DP_TextWriter *writer) DP_MUST_CHECK;

bool DP_text_writer_start_subobject(DP_TextWriter *writer) DP_MUST_CHECK;

bool DP_text_writer_finish_subobject(DP_TextWriter *writer) DP_MUST_CHECK;

bool DP_text_writer_write_subfield_int(DP_TextWriter *writer, const char *key,
                                       int value) DP_MUST_CHECK;

bool DP_text_writer_write_subfield_uint(DP_TextWriter *writer, const char *key,
                                        unsigned int value) DP_MUST_CHECK;

bool DP_text_writer_write_subfield_decimal(DP_TextWriter *writer,
                                           const char *key,
                                           double value) DP_MUST_CHECK;

bool DP_text_writer_write_subfield_rgb_color(DP_TextWriter *writer,
                                             const char *key,
                                             uint32_t bgr) DP_MUST_CHECK;


#endif
