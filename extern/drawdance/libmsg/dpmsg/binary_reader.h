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
#ifndef DPMSG_BINARY_READER_H
#define DPMSG_BINARY_READER_H
#include <dpcommon/common.h>

typedef struct DP_Input DP_Input;
typedef struct DP_Message DP_Message;
typedef struct json_object_t JSON_Object;

#define DP_DPREC_MAGIC        "DPREC"
#define DP_DPREC_MAGIC_LENGTH 6

typedef struct DP_BinaryReader DP_BinaryReader;

typedef enum DP_BinaryReaderResult {
    DP_BINARY_READER_SUCCESS,
    DP_BINARY_READER_INPUT_END,
    DP_BINARY_READER_ERROR_INPUT,
    DP_BINARY_READER_ERROR_PARSE,
} DP_BinaryReaderResult;

DP_BinaryReader *DP_binary_reader_new(DP_Input *input);

void DP_binary_reader_free(DP_BinaryReader *reader);


JSON_Object *DP_binary_reader_header(DP_BinaryReader *reader);

size_t DP_binary_reader_body_offset(DP_BinaryReader *reader);

size_t DP_binary_reader_tell(DP_BinaryReader *reader);

bool DP_binary_reader_seek(DP_BinaryReader *reader, size_t offset);

double DP_binary_reader_progress(DP_BinaryReader *reader);

DP_BinaryReaderResult DP_binary_reader_read_message(DP_BinaryReader *reader,
                                                    DP_Message **out_msg);


#endif
