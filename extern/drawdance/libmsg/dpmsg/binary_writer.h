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
#ifndef DPMSG_BINARY_WRITER_H
#define DPMSG_BINARY_WRITER_H
#include <dpcommon/common.h>
#ifdef DP_BUNDLED_PARSON
#include "parson/parson.h"
#else
#include <parson.h>
#endif

typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;


typedef struct DP_BinaryWriter DP_BinaryWriter;

DP_BinaryWriter *DP_binary_writer_new(DP_Output *output);

void DP_binary_writer_free(DP_BinaryWriter *writer);


bool DP_binary_writer_write_header(DP_BinaryWriter *writer,
                                   JSON_Object *header) DP_MUST_CHECK;


bool DP_binary_writer_write_message(DP_BinaryWriter *writer,
                                    DP_Message *msg) DP_MUST_CHECK;


#endif
