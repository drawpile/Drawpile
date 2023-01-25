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
#include "dump_reader.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/vector.h>
#include <dpmsg/message.h>
#include <limits.h>


struct DP_DumpReader {
    DP_BufferedInput input;
    long long position;
    size_t offset;
    long long max_position;
    DP_Vector reset_offsets;
    DP_Vector messages;
};

DP_DumpReader *DP_dump_reader_new(DP_Input *input)
{
    DP_ASSERT(input);
    DP_DumpReader *dr = DP_malloc(sizeof(*dr));
    *dr = (DP_DumpReader){
        DP_buffered_input_init(input), 0, 0, 0, DP_VECTOR_NULL, DP_VECTOR_NULL};
    DP_VECTOR_INIT_TYPE(&dr->reset_offsets, DP_DumpReaderEntry, 64);
    DP_VECTOR_INIT_TYPE(&dr->messages, DP_Message *, 64);
    return dr;
}

static void dispose_message(void *user)
{
    DP_message_decref(*(DP_Message **)user);
}

void DP_dump_reader_free(DP_DumpReader *dr)
{
    if (dr) {
        DP_VECTOR_CLEAR_DISPOSE_TYPE(&dr->messages, DP_Message *,
                                     dispose_message);
        DP_vector_dispose(&dr->reset_offsets);
        DP_buffered_input_dispose(&dr->input);
        DP_free(dr);
    }
}


static bool read_exactly(DP_DumpReader *dr, size_t size)
{
    bool error;
    size_t read = DP_buffered_input_read(&dr->input, size, &error);
    dr->offset += read;
    if (error) {
        return false;
    }
    else if (read != size) {
        DP_error_set("Expected to read %zu byte(s), but got %zu", size, read);
        return false;
    }
    else {
        return true;
    }
}

static DP_DumpReaderResult handle_message(DP_DumpReader *dr)
{
    if (!read_exactly(dr, sizeof(uint32_t))) {
        return DP_DUMP_READER_ERROR_INPUT;
    }

    size_t size = DP_read_bigendian_uint32(dr->input.buffer);
    if (!read_exactly(dr, size)) {
        return DP_DUMP_READER_ERROR_INPUT;
    }

    DP_Message *msg = DP_message_deserialize(dr->input.buffer, size);
    if (msg) {
        DP_VECTOR_PUSH_TYPE(&dr->messages, DP_Message *, msg);
        return DP_DUMP_READER_SUCCESS;
    }
    else {
        return DP_DUMP_READER_ERROR_PARSE;
    }
}

static DP_DumpReaderResult handle_multidab(DP_DumpReader *dr)
{
    if (!read_exactly(dr, sizeof(uint32_t))) {
        return DP_DUMP_READER_ERROR_INPUT;
    }

    uint32_t count = DP_read_bigendian_uint32(dr->input.buffer);
    for (uint32_t i = 0; i < count; ++i) {
        DP_DumpReaderResult result = handle_message(dr);
        if (result != DP_DUMP_READER_SUCCESS) {
            return result;
        }
    }
    return DP_DUMP_READER_SUCCESS;
}

static DP_DumpReaderResult handle_reset(DP_DumpReader *dr)
{
    DP_DumpReaderEntry entry = {dr->position, dr->offset};
    DP_VECTOR_PUSH_TYPE(&dr->reset_offsets, DP_DumpReaderEntry, entry);
    return DP_DUMP_READER_SUCCESS;
}

static DP_DumpReaderResult read_type(DP_DumpReader *dr, int type)
{
    switch (type) {
    case DP_DUMP_REMOTE_MESSAGE:
    case DP_DUMP_REMOTE_MESSAGE_LOCAL_DRAWING_IN_PROGRESS:
    case DP_DUMP_LOCAL_MESSAGE:
        return handle_message(dr);
    case DP_DUMP_REMOTE_MULTIDAB:
    case DP_DUMP_REMOTE_MULTIDAB_LOCAL_DRAWING_IN_PROGRESS:
    case DP_DUMP_LOCAL_MULTIDAB:
        return handle_multidab(dr);
    case DP_DUMP_RESET:
        return handle_reset(dr);
    case DP_DUMP_SOFT_RESET:
    case DP_DUMP_CLEANUP:
        return DP_DUMP_READER_SUCCESS;
    default:
        DP_error_set("Invalid dump type: %d", type);
        return DP_DUMP_READER_ERROR_PARSE;
    }
}

DP_DumpReaderResult DP_dump_reader_read(DP_DumpReader *dr,
                                        DP_DumpType *out_type, int *out_count,
                                        DP_Message ***out_msgs)
{
    DP_ASSERT(dr);
    bool error;
    size_t read = DP_buffered_input_read(&dr->input, 1, &error);
    if (error) {
        return DP_DUMP_READER_ERROR_INPUT;
    }
    else if (read == 0) {
        return DP_DUMP_READER_INPUT_END;
    }

    DP_VECTOR_CLEAR_TYPE(&dr->messages, DP_Message *, dispose_message);
    int type = DP_read_bigendian_uint8(dr->input.buffer);
    DP_DumpReaderResult result = read_type(dr, type);
    ++dr->position;
    ++dr->offset;
    if (dr->position > dr->max_position) {
        dr->max_position = dr->position;
    }

    if (out_type) {
        *out_type = (DP_DumpType)type;
    }
    if (out_count) {
        *out_count = DP_size_to_int(dr->messages.used);
    }
    if (out_msgs) {
        *out_msgs = dr->messages.elements;
    }
    return result;
}

size_t DP_dump_reader_tell(DP_DumpReader *dr)
{
    DP_ASSERT(dr);
    return dr->offset;
}

long long DP_dump_reader_position(DP_DumpReader *dr)
{
    DP_ASSERT(dr);
    return dr->position;
}

static DP_DumpReaderEntry search_best_entry(DP_DumpReader *dr,
                                            long long position)
{
    DP_DumpReaderEntry best_entry = {0, 0};
    size_t count = dr->reset_offsets.used;
    for (size_t i = 0; i < count; ++i) {
        DP_DumpReaderEntry entry =
            DP_VECTOR_AT_TYPE(&dr->reset_offsets, DP_DumpReaderEntry, i);
        bool is_best =
            entry.position > best_entry.position && entry.position <= position;
        if (is_best) {
            best_entry = entry;
        }
    }
    return best_entry;
}

static bool index_reset_points_for(DP_DumpReader *dr, long long position)
{
    DP_DumpReaderEntry entry = search_best_entry(dr, INT_MAX);
    if (DP_buffered_input_seek(&dr->input, entry.offset)) {
        return false;
    }

    while (dr->position <= position) {
        DP_DumpReaderResult result = DP_dump_reader_read(dr, NULL, NULL, NULL);
        if (result == DP_DUMP_READER_INPUT_END) {
            break;
        }
        else if (result != DP_DUMP_READER_SUCCESS) {
            return false;
        }
    }
    return true;
}

DP_DumpReaderEntry DP_dump_reader_search_reset_for(DP_DumpReader *dr,
                                                   long long position)
{
    DP_ASSERT(dr);
    if (dr->max_position <= position) {
        if (!index_reset_points_for(dr, position)) {
            DP_warn("Error indexing reset points for %lld: %s", position,
                    DP_error());
        }
    }
    return search_best_entry(dr, position);
}

bool DP_dump_reader_seek(DP_DumpReader *dr, DP_DumpReaderEntry entry)
{
    DP_ASSERT(dr);
    if (DP_buffered_input_seek(&dr->input, entry.offset)) {
        dr->position = entry.position;
        dr->offset = entry.offset;
        return true;
    }
    else {
        return false;
    }
}
