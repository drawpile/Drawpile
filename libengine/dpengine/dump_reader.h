/*
 * Copyright (C) 2023 askmeaboutloom
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
#ifndef DPENGINE_DUMP_READER_H
#define DPENGINE_DUMP_READER_H
#include "canvas_history.h"
#include <dpcommon/common.h>

typedef struct DP_Input DP_Input;
typedef struct DP_Message DP_Message;


typedef struct DP_DumpReader DP_DumpReader;

typedef struct DP_DumpReaderEntry {
    long long position;
    size_t offset;
} DP_DumpReaderEntry;

typedef enum DP_DumpReaderResult {
    DP_DUMP_READER_SUCCESS,
    DP_DUMP_READER_INPUT_END,
    DP_DUMP_READER_ERROR_INPUT,
    DP_DUMP_READER_ERROR_PARSE,
} DP_DumpReaderResult;

DP_DumpReader *DP_dump_reader_new(DP_Input *input);

void DP_dump_reader_free(DP_DumpReader *dr);

DP_DumpReaderResult DP_dump_reader_read(DP_DumpReader *dr,
                                        DP_DumpType *out_type, int *out_count,
                                        DP_Message ***out_msgs);

size_t DP_dump_reader_tell(DP_DumpReader *dr);

long long DP_dump_reader_position(DP_DumpReader *dr);

DP_DumpReaderEntry DP_dump_reader_search_reset_for(DP_DumpReader *dr,
                                                   long long position);

bool DP_dump_reader_seek(DP_DumpReader *dr, DP_DumpReaderEntry entry);


#endif
