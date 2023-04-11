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
#include "player.h"
#include "annotation.h"
#include "annotation_list.h"
#include "canvas_history.h"
#include "document_metadata.h"
#include "draw_context.h"
#include "dump_reader.h"
#include "frame.h"
#include "image.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "limits.h"
#include "load.h"
#include "local_state.h"
#include "tile.h"
#include "timeline.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpcommon/perf.h>
#include <dpcommon/vector.h>
#include <dpmsg/acl.h>
#include <dpmsg/binary_reader.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/text_reader.h>
#include <ctype.h>
#include <parson.h>
#include <uthash_inc.h>

#define DP_PERF_CONTEXT "player"


#define INDEX_EXTENSION       "dpidx"
#define INDEX_MAGIC           "DPIDX"
#define INDEX_MAGIC_LENGTH    6
#define INDEX_VERSION         11
#define INDEX_VERSION_LENGTH  2
#define INDEX_HEADER_LENGTH   (INDEX_MAGIC_LENGTH + INDEX_VERSION_LENGTH + 12)
#define INITAL_ENTRY_CAPACITY 64

static_assert(INDEX_MAGIC_LENGTH < sizeof(DP_OutputBinaryEntry),
              "index header fits into output binary entry");


typedef struct DP_PlayerIndex {
    DP_BufferedInput input;
    unsigned int message_count;
    DP_PlayerIndexEntry *entries;
    size_t entry_count;
} DP_PlayerIndex;

typedef union DP_PlayerReader {
    DP_BinaryReader *binary;
    DP_TextReader *text;
    DP_DumpReader *dump;
} DP_PlayerReader;

struct DP_Player {
    char *recording_path;
    char *index_path;
    DP_PlayerType type;
    DP_PlayerReader reader;
    DP_AclState *acls;
    JSON_Value *text_reader_header_value;
    long long position;
    bool compatible;
    bool input_error;
    bool end;
    DP_PlayerIndex index;
};

struct DP_PlayerIndexEntrySnapshot {
    DP_CanvasState *cs;
    int message_count;
    DP_Message *messages[];
};


static void player_index_dispose(DP_PlayerIndex *pi)
{
    DP_ASSERT(pi);
    DP_free(pi->entries);
    DP_buffered_input_dispose(&pi->input);
    *pi = (DP_PlayerIndex){DP_BUFFERD_INPUT_NULL, 0, NULL, 0};
}


static void assign_load_result(DP_LoadResult *out_result, DP_LoadResult result)
{
    if (out_result) {
        *out_result = result;
    }
}

static DP_LoadResult finish_guess(DP_Input *input, DP_PlayerType *out_type,
                                  DP_PlayerType type)
{
    if (DP_input_rewind(input)) {
        *out_type = type;
        return DP_LOAD_RESULT_SUCCESS;
    }
    else {
        return DP_LOAD_RESULT_READ_ERROR;
    }
}

static DP_LoadResult guess_type(DP_Input *input, DP_PlayerType *out_type)
{
    char buffer[64];
    bool error;
    size_t read = DP_input_read(input, buffer, sizeof(buffer), &error);
    if (error) {
        return DP_LOAD_RESULT_READ_ERROR;
    }

    bool is_binary = read >= DP_DPREC_MAGIC_LENGTH
                  && memcmp(buffer, DP_DPREC_MAGIC, DP_DPREC_MAGIC_LENGTH) == 0;
    if (is_binary) {
        return finish_guess(input, out_type, DP_PLAYER_TYPE_BINARY);
    }

    do {
        for (size_t i = 0; i < read; ++i) {
            char c = buffer[i];
            if (c == '!') {
                return finish_guess(input, out_type, DP_PLAYER_TYPE_TEXT);
            }
            else if (!isspace(c)) {
                DP_error_set("Couldn't guess recording format");
                return DP_LOAD_RESULT_UNKNOWN_FORMAT;
            }
        }

        read = DP_input_read(input, buffer, sizeof(buffer), &error);
        if (error) {
            return DP_LOAD_RESULT_READ_ERROR;
        }
    } while (read != 0);

    DP_error_set("Couldn't guess recording format");
    return DP_LOAD_RESULT_BAD_MIMETYPE;
}

static char *make_index_path(const char *path)
{
    const char *dot = strrchr(path, '.');
    size_t prefix_len = dot ? (size_t)(dot - path) : strlen(path);
    size_t extension_len = strlen(INDEX_EXTENSION);
    size_t total_len = prefix_len + extension_len + 1;
    char *index_path = DP_malloc(total_len + 1);
    memcpy(index_path, path, prefix_len);
    index_path[prefix_len] = '.';
    memcpy(index_path + prefix_len + 1, INDEX_EXTENSION, extension_len);
    index_path[total_len] = '\0';
    return index_path;
}

static bool check_namespace(char *version, size_t len, size_t *in_out_index)
{
    for (size_t i = *in_out_index; i < len; ++i) {
        if (version[i] == ':') {
            *in_out_index = i + 1;
            return i == strlen(DP_PROTOCOL_VERSION_NAMESPACE)
                && memcmp(version, DP_PROTOCOL_VERSION_NAMESPACE, i) == 0;
        }
    }
    return false;
}

static bool check_server(char *version, size_t len, size_t *in_out_index)
{
    size_t start = *in_out_index;
    for (size_t i = start; i < len; ++i) {
        if (version[i] == '.') {
            *in_out_index = i + 1;
            char *end;
            unsigned long value = strtoul(version + start, &end, 10);
            return end == version + i && value == DP_PROTOCOL_VERSION_SERVER;
        }
    }
    return false;
}

static bool check_major(char *version, size_t len, size_t *in_out_index)
{
    size_t start = *in_out_index;
    for (size_t i = start; i < len; ++i) {
        if (version[i] == '.') {
            *in_out_index = i + 1;
            char *end;
            unsigned long value = strtoul(version + start, &end, 10);
            return end == version + i && value > 21;
        }
    }
    return false;
}

static bool check_minor(char *version, size_t len, size_t index)
{
    char *end;
    strtoul(version + index, &end, 10);
    return end == version + len;
}

static bool is_version_compatible(char *version)
{
    if (version) {
        size_t len = strlen(version);
        size_t index = 0;
        return check_namespace(version, len, &index)
            && check_server(version, len, &index)
            && check_major(version, len, &index)
            && check_minor(version, len, index);
    }
    else {
        return false;
    }
}

static DP_Player *make_player(DP_PlayerType type, char *recording_path,
                              char *index_path, DP_PlayerReader reader,
                              char *version, JSON_Value *header_value)
{
    bool compatible = is_version_compatible(version);
    DP_free(version);
    DP_Player *player = DP_malloc(sizeof(*player));
    *player = (DP_Player){recording_path,
                          index_path,
                          type,
                          reader,
                          DP_acl_state_new(),
                          header_value,
                          0,
                          compatible,
                          false,
                          false,
                          {DP_BUFFERD_INPUT_NULL, 0, NULL, 0}};
    return player;
}

static DP_Player *new_binary_player(char *recording_path, char *index_path,
                                    DP_Input *input)
{
    DP_BinaryReader *binary_reader = DP_binary_reader_new(input);
    if (!binary_reader) {
        return NULL;
    }

    JSON_Object *header = DP_binary_reader_header(binary_reader);
    char *version = DP_strdup(json_object_get_string(header, "version"));
    return make_player(DP_PLAYER_TYPE_BINARY, recording_path, index_path,
                       (DP_PlayerReader){.binary = binary_reader}, version,
                       NULL);
}

static DP_Player *new_text_player(char *recording_path, char *index_path,
                                  DP_Input *input)
{
    DP_TextReader *text_reader = DP_text_reader_new(input);
    if (!text_reader) {
        return NULL;
    }

    JSON_Value *header_value = json_value_init_object();
    if (!header_value) {
        DP_error_set("Error creating JSON header");
        DP_text_reader_free(text_reader);
        return NULL;
    }

    JSON_Object *header = json_value_get_object(header_value);
    char *version = NULL;
    while (true) {
        const char *key, *value;
        DP_TextReaderResult result =
            DP_text_reader_read_header_field(text_reader, &key, &value);
        if (result == DP_TEXT_READER_SUCCESS) {
            if (json_object_set_string(header, key, value) != JSONSuccess) {
                DP_error_set("Error setting JSON header field %s=%s", key,
                             value);
                DP_free(version);
                json_value_free(header_value);
                DP_text_reader_free(text_reader);
                return NULL;
            }
            if (DP_str_equal(key, "version") && !version) {
                version = DP_strdup(value);
            }
        }
        else if (result == DP_TEXT_READER_HEADER_END) {
            break;
        }
        else {
            DP_free(version);
            DP_text_reader_free(text_reader);
            return NULL;
        }
    }

    return make_player(DP_PLAYER_TYPE_TEXT, recording_path, index_path,
                       (DP_PlayerReader){.text = text_reader}, version,
                       header_value);
}

static DP_Player *new_debug_dump_player(DP_Input *input)
{
    DP_Player *player = DP_malloc(sizeof(*player));
    *player = (DP_Player){NULL,
                          NULL,
                          DP_PLAYER_TYPE_DEBUG_DUMP,
                          {.dump = DP_dump_reader_new(input)},
                          NULL,
                          NULL,
                          0,
                          true,
                          false,
                          false,
                          {DP_BUFFERD_INPUT_NULL, 0, NULL, 0}};
    return player;
}

DP_Player *DP_player_new(DP_PlayerType type, const char *path_or_null,
                         DP_Input *input, DP_LoadResult *out_result)
{
    if (type == DP_PLAYER_TYPE_GUESS) {
        DP_LoadResult guess_result = guess_type(input, &type);
        if (guess_result != DP_LOAD_RESULT_SUCCESS) {
            assign_load_result(out_result, guess_result);
            DP_input_free(input);
            return NULL;
        }
    }

    char *recording_path, *index_path;
    if (path_or_null && type != DP_PLAYER_TYPE_DEBUG_DUMP) {
        recording_path = DP_strdup(path_or_null);
        index_path = make_index_path(path_or_null);
    }
    else {
        recording_path = NULL;
        index_path = NULL;
    }

    DP_Player *player;
    switch (type) {
    case DP_PLAYER_TYPE_BINARY:
        player = new_binary_player(recording_path, index_path, input);
        break;
    case DP_PLAYER_TYPE_TEXT:
        player = new_text_player(recording_path, index_path, input);
        break;
    case DP_PLAYER_TYPE_DEBUG_DUMP:
        player = new_debug_dump_player(input);
        break;
    default:
        DP_free(index_path);
        DP_free(recording_path);
        DP_input_free(input);
        DP_error_set("Unknown player format");
        assign_load_result(out_result, DP_LOAD_RESULT_UNKNOWN_FORMAT);
        return NULL;
    }

    if (!player) {
        DP_free(index_path);
        DP_free(recording_path);
        assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
        return NULL;
    }

    if (!DP_player_compatible(player)) {
        DP_player_free(player);
        DP_error_set("Incompatible recording");
        assign_load_result(out_result, DP_LOAD_RESULT_RECORDING_INCOMPATIBLE);
        return NULL;
    }

    assign_load_result(out_result, DP_LOAD_RESULT_SUCCESS);
    return player;
}

void DP_player_free(DP_Player *player)
{
    if (player) {
        player_index_dispose(&player->index);
        DP_acl_state_free(player->acls);
        switch (player->type) {
        case DP_PLAYER_TYPE_BINARY:
            DP_binary_reader_free(player->reader.binary);
            break;
        case DP_PLAYER_TYPE_TEXT:
            DP_text_reader_free(player->reader.text);
            json_value_free(player->text_reader_header_value);
            break;
        case DP_PLAYER_TYPE_DEBUG_DUMP:
            DP_dump_reader_free(player->reader.dump);
        default:
            break;
        }
        DP_free(player->index_path);
        DP_free(player->recording_path);
        DP_free(player);
    }
}

JSON_Object *DP_player_header(DP_Player *player)
{
    DP_ASSERT(player);
    switch (player->type) {
    case DP_PLAYER_TYPE_BINARY:
        return DP_binary_reader_header(player->reader.binary);
    case DP_PLAYER_TYPE_TEXT:
        return json_value_get_object(player->text_reader_header_value);
    case DP_PLAYER_TYPE_DEBUG_DUMP:
        return NULL;
    default:
        DP_UNREACHABLE();
    }
}

bool DP_player_compatible(DP_Player *player)
{
    DP_ASSERT(player);
    return player->compatible;
}

bool DP_player_index_loaded(DP_Player *player)
{
    DP_ASSERT(player);
    return player->index.entries;
}

size_t DP_player_tell(DP_Player *player)
{
    DP_ASSERT(player);
    switch (player->type) {
    case DP_PLAYER_TYPE_BINARY:
        return DP_binary_reader_tell(player->reader.binary);
    case DP_PLAYER_TYPE_TEXT:
        return DP_text_reader_tell(player->reader.text);
    case DP_PLAYER_TYPE_DEBUG_DUMP:
        return DP_dump_reader_tell(player->reader.dump);
    default:
        DP_UNREACHABLE();
    }
}

double DP_player_progress(DP_Player *player)
{
    DP_ASSERT(player);
    switch (player->type) {
    case DP_PLAYER_TYPE_BINARY:
        return DP_binary_reader_progress(player->reader.binary);
    case DP_PLAYER_TYPE_TEXT:
        return DP_text_reader_progress(player->reader.text);
    case DP_PLAYER_TYPE_DEBUG_DUMP:
        return 0.0;
    default:
        DP_UNREACHABLE();
    }
}

long long DP_player_position(DP_Player *player)
{
    DP_ASSERT(player);
    return player->position;
}


static DP_PlayerResult step_binary(DP_Player *player, DP_Message **out_msg)
{
    DP_BinaryReaderResult result =
        DP_binary_reader_read_message(player->reader.binary, out_msg);
    switch (result) {
    case DP_BINARY_READER_SUCCESS:
        return DP_PLAYER_SUCCESS;
    case DP_BINARY_READER_INPUT_END:
        player->end = true;
        return DP_PLAYER_RECORDING_END;
    case DP_BINARY_READER_ERROR_PARSE:
        return DP_PLAYER_ERROR_PARSE;
    default:
        player->input_error = true;
        return DP_PLAYER_ERROR_INPUT;
    }
}

static DP_PlayerResult step_text(DP_Player *player, DP_Message **out_msg)
{
    DP_TextReaderResult result =
        DP_text_reader_read_message(player->reader.text, out_msg);
    switch (result) {
    case DP_TEXT_READER_SUCCESS:
        return DP_PLAYER_SUCCESS;
    case DP_TEXT_READER_INPUT_END:
        player->end = true;
        return DP_PLAYER_RECORDING_END;
    case DP_TEXT_READER_ERROR_PARSE:
        return DP_PLAYER_ERROR_PARSE;
    default:
        player->input_error = true;
        return DP_PLAYER_ERROR_INPUT;
    }
}

static DP_PlayerResult step_message(DP_Player *player, DP_Message **out_msg)
{
    DP_PlayerResult result;
    switch (player->type) {
    case DP_PLAYER_TYPE_BINARY:
        result = step_binary(player, out_msg);
        break;
    case DP_PLAYER_TYPE_TEXT:
        result = step_text(player, out_msg);
        break;
    case DP_PLAYER_TYPE_DEBUG_DUMP:
        DP_error_set("Can't step debug dump like a recording");
        return DP_PLAYER_ERROR_OPERATION;
    default:
        DP_UNREACHABLE();
    }
    if (result == DP_PLAYER_SUCCESS || result == DP_PLAYER_ERROR_PARSE) {
        ++player->position;
    }
    return result;
}

static bool emit_message(DP_Message *msg, DP_Message **out_msg)
{
    // When playing back a recording, we are a single user in offline mode, so
    // don't allow anything to escape that would mess up the real ACL state,
    // relates to other users in some way or is a control message of some sort.
    switch (DP_message_type(msg)) {
    case DP_MSG_SERVER_COMMAND:
    case DP_MSG_DISCONNECT:
    case DP_MSG_PING:
    case DP_MSG_INTERNAL:
    case DP_MSG_JOIN:
    case DP_MSG_LEAVE:
    case DP_MSG_SESSION_OWNER:
    case DP_MSG_CHAT:
    case DP_MSG_TRUSTED_USERS:
    case DP_MSG_PRIVATE_CHAT:
    case DP_MSG_MARKER:
    case DP_MSG_USER_ACL:
    case DP_MSG_FEATURE_ACCESS_LEVELS:
    case DP_MSG_UNDO_DEPTH:
    case DP_MSG_DATA:
        return false;
    // Layer ACL messages have a tier, which we want to retain, and a user
    // component, which we do not. So extract the former and leave the latter.
    case DP_MSG_LAYER_ACL: {
        DP_MsgLayerAcl *mla = DP_message_internal(msg);
        *out_msg = DP_msg_layer_acl_new(
            DP_message_context_id(msg), DP_msg_layer_acl_id(mla),
            DP_msg_layer_acl_flags(mla), NULL, 0, NULL);
        DP_message_decref(msg);
        return true;
    }
    default:
        *out_msg = msg;
        return true;
    }
}

static DP_PlayerResult step_valid_message(DP_Player *player,
                                          DP_Message **out_msg)
{
    while (true) {
        DP_Message *msg;
        DP_PlayerResult result = step_message(player, &msg);
        if (result == DP_PLAYER_SUCCESS) {
            bool filtered = DP_acl_state_handle(player->acls, msg, false)
                          & DP_ACL_STATE_FILTERED_BIT;
            if (filtered) {
                DP_debug(
                    "ACL filtered recorded %s message from user %u",
                    DP_message_type_enum_name_unprefixed(DP_message_type(msg)),
                    DP_message_context_id(msg));
            }
            else if (emit_message(msg, out_msg)) {
                return result;
            }
            DP_message_decref(msg);
        }
        else if (result == DP_PLAYER_ERROR_PARSE) {
            DP_warn("Error parsing recording message: %s", DP_error());
        }
        else {
            return result; // Input error or reached the end.
        }
    }
}

DP_PlayerResult DP_player_step(DP_Player *player, DP_Message **out_msg)
{
    DP_ASSERT(player);
    DP_ASSERT(out_msg);
    if (player->input_error) {
        DP_error_set("Player input in error state");
        return DP_PLAYER_ERROR_INPUT;
    }
    else if (player->end) {
        return DP_PLAYER_RECORDING_END;
    }
    else {
        return step_valid_message(player, out_msg);
    }
}

DP_PlayerResult DP_player_step_dump(DP_Player *player, DP_DumpType *out_type,
                                    int *out_count, DP_Message ***out_msgs)
{
    DP_ASSERT(player);
    if (player->input_error) {
        DP_error_set("Player input in error state");
        return DP_PLAYER_ERROR_INPUT;
    }
    else if (player->end) {
        return DP_PLAYER_RECORDING_END;
    }
    else if (player->type != DP_PLAYER_TYPE_DEBUG_DUMP) {
        DP_error_set("Can't step recording like a debug dump");
        return DP_PLAYER_ERROR_OPERATION;
    }
    else {
        DP_DumpReader *dr = player->reader.dump;
        DP_DumpReaderResult result =
            DP_dump_reader_read(dr, out_type, out_count, out_msgs);
        player->position = DP_dump_reader_position(dr);
        switch (result) {
        case DP_DUMP_READER_SUCCESS:
            return DP_PLAYER_SUCCESS;
        case DP_DUMP_READER_INPUT_END:
            return DP_PLAYER_RECORDING_END;
        case DP_DUMP_READER_ERROR_INPUT:
            return DP_PLAYER_ERROR_INPUT;
        case DP_DUMP_READER_ERROR_PARSE:
            return DP_PLAYER_ERROR_PARSE;
        }
        DP_UNREACHABLE();
    }
}


bool DP_player_seek(DP_Player *player, long long position, size_t offset)
{
    DP_ASSERT(player);
    // No need to check for input errors, since seeking clears those.

    bool seek_ok;
    DP_debug("Seeking playback to %zu", offset);
    switch (player->type) {
    case DP_PLAYER_TYPE_BINARY:
        seek_ok = DP_binary_reader_seek(player->reader.binary, offset);
        break;
    case DP_PLAYER_TYPE_TEXT:
        seek_ok = DP_text_reader_seek(player->reader.text, offset);
        break;
    case DP_PLAYER_TYPE_DEBUG_DUMP:
        seek_ok = DP_dump_reader_seek(player->reader.dump,
                                      (DP_DumpReaderEntry){position, offset});
        break;
    default:
        DP_UNREACHABLE();
    }

    if (seek_ok) {
        DP_AclState *acls = player->acls;
        if (acls) {
            DP_acl_state_reset(acls, 0);
        }
        player->position = position;
        player->input_error = false;
        player->end = false;
        return true;
    }
    else {
        player->input_error = true;
        return false;
    }
}

static size_t player_body_offset(DP_Player *player)
{
    switch (player->type) {
    case DP_PLAYER_TYPE_BINARY:
        return DP_binary_reader_body_offset(player->reader.binary);
    case DP_PLAYER_TYPE_TEXT:
        return DP_text_reader_body_offset(player->reader.text);
    case DP_PLAYER_TYPE_DEBUG_DUMP:
        return 0;
    default:
        DP_UNREACHABLE();
    }
}

bool DP_player_rewind(DP_Player *player)
{
    DP_ASSERT(player);
    return DP_player_seek(player, 0, player_body_offset(player));
}

bool DP_player_seek_dump(DP_Player *player, long long position)
{
    DP_ASSERT(player);
    if (player->type == DP_PLAYER_TYPE_DEBUG_DUMP) {
        DP_DumpReader *dr = player->reader.dump;
        DP_DumpReaderEntry entry =
            DP_dump_reader_search_reset_for(dr, position);
        if (DP_dump_reader_seek(dr, entry)) {
            player->position = DP_dump_reader_position(dr);
            return true;
        }
        else {
            player->input_error = true;
            return false;
        }
    }
    else {
        DP_error_set("Can't seek recording like a debug dump");
        return false;
    }
}


typedef struct DP_BuildIndexTileMap {
    DP_Tile *t;
    size_t offset;
    UT_hash_handle hh;
} DP_BuildIndexTileMap;

typedef struct DP_BuildIndexLayerKey {
    union {
        DP_LayerContent *lc;
        DP_LayerGroup *lg;
    };
    DP_LayerProps *lp;
} DP_BuildIndexLayerKey;

typedef struct DP_BuildIndexLayerMap {
    DP_BuildIndexLayerKey key;
    size_t offset;
    UT_hash_handle hh;
} DP_BuildIndexLayerMap;

typedef struct DP_BuildIndexAnnotationMap {
    DP_Annotation *a;
    size_t offset;
    UT_hash_handle hh;
} DP_BuildIndexAnnotationMap;

typedef struct DP_BuildIndexMaps {
    DP_BuildIndexTileMap *tiles;
    DP_BuildIndexLayerMap *layers;
    DP_BuildIndexAnnotationMap *annotations;
    struct {
        DP_DocumentMetadata *dm;
        size_t offset;
    } metadata;
    struct {
        DP_Timeline *tl;
        size_t offset;
    } timeline;
} DP_BuildIndexMaps;

typedef struct DP_BuildIndexEntryContext {
    DP_Output *output;
    DP_AclState *acls;
    DP_LocalState *local_state;
    DP_CanvasHistory *ch;
    DP_CanvasState *cs;
    DP_DrawContext *dc;
    DP_BuildIndexMaps current;
    DP_BuildIndexMaps *last;
    int message_count;
    struct {
        size_t history;
        size_t layers;
        size_t background_tile;
        size_t snapshot;
        size_t thumbnail;
    } offset;
    struct {
        unsigned char *buffer;
        size_t size;
    } annotation;
} DP_BuildIndexEntryContext;

typedef struct DP_BuildIndexContext {
    DP_Player *player;
    DP_Output *output;
    DP_AclState *acls;
    DP_LocalState *local_state;
    DP_CanvasHistory *ch;
    DP_DrawContext *dc;
    long long message_count;
    DP_Vector entries;
    DP_BuildIndexMaps last;
    DP_PlayerIndexShouldSnapshotFn should_snapshot_fn;
    DP_PlayerIndexProgressFn progress_fn;
    void *user;
} DP_BuildIndexContext;

struct DP_BuildIndexLayerProps {
    uint16_t layer_id;
    uint16_t title_length;
    union {
        const char *title;
        char *buffer;
    };
    uint8_t opacity;
    uint8_t blend_mode;
    uint8_t hidden;
    uint8_t censored;
    uint8_t isolated;
    uint8_t group;
};

static bool write_index_header(DP_BuildIndexContext *c)
{
    return DP_OUTPUT_WRITE_LITTLEENDIAN(
        c->output, DP_OUTPUT_BYTES(INDEX_MAGIC, INDEX_MAGIC_LENGTH),
        DP_OUTPUT_UINT16(INDEX_VERSION), DP_OUTPUT_UINT32(0),
        DP_OUTPUT_UINT64(0));
}

unsigned char *get_message_buffer(void *user, size_t length)
{
    return DP_draw_context_pool_require(user, length);
}

static bool write_index_history_message_dec(DP_BuildIndexEntryContext *e,
                                            DP_Message *msg)
{
    size_t length = DP_message_serialize(msg, false, get_message_buffer, e->dc);
    DP_message_decref(msg);
    if (length == 0) {
        DP_error_set("Error serializing history message %d",
                     e->message_count + 1);
        return true; // Not a fatal error.
    }
    else {
        DP_Output *output = e->output;
        bool ok = DP_OUTPUT_WRITE_LITTLEENDIAN(output, DP_OUTPUT_UINT16(length))
               && DP_output_write(output, DP_draw_context_pool(e->dc), length);
        if (ok) {
            ++e->message_count;
            return true;
        }
        else {
            return false;
        }
    }
}

static bool init_reset_image(void *user, DP_CanvasState *cs)
{
    DP_BuildIndexEntryContext *e = user;
    e->cs = cs;
    DP_Message *msg = DP_acl_state_msg_feature_access_all_new(0);
    return write_index_history_message_dec(e, msg);
}

static bool write_reset_image_message(void *user, DP_Message *msg)
{
    return write_index_history_message_dec(user, msg);
}

static bool write_index_history(DP_BuildIndexEntryContext *e)
{
    bool error;
    size_t offset = DP_output_tell(e->output, &error);
    if (error) {
        return false;
    }

    if (!DP_canvas_history_reset_image_new(e->ch, init_reset_image,
                                           write_reset_image_message, e)) {
        return false;
    }

    // The state of the permissions at this point.
    if (!DP_acl_state_reset_image_build(
            e->acls, 0, DP_ACL_STATE_RESET_IMAGE_RECORDING_FLAGS,
            write_reset_image_message, e)) {
        return false;
    }
    // Local changes (hidden layers, local canvas background).
    if (!DP_local_state_reset_image_build(e->local_state, e->dc,
                                          write_reset_image_message, e)) {
        return false;
    }

    e->offset.history = offset;
    return true;
}

static DP_BuildIndexTileMap *search_tile(DP_BuildIndexTileMap *tiles,
                                         DP_Tile *t)
{
    DP_BuildIndexTileMap *entry;
    HASH_FIND_PTR(tiles, &t, entry);
    return entry;
}

static void move_tile_offset(DP_BuildIndexEntryContext *e,
                             DP_BuildIndexTileMap *entry)
{
    HASH_DEL(e->last->tiles, entry);
    HASH_ADD_PTR(e->current.tiles, t, entry);
}

static void move_tile_offsets(DP_BuildIndexEntryContext *e, DP_LayerContent *lc)
{
    DP_TileCounts tile_counts = DP_tile_counts_round(
        DP_layer_content_width(lc), DP_layer_content_height(lc));
    for (int y = 0; y < tile_counts.y; ++y) {
        for (int x = 0; x < tile_counts.x; ++x) {
            DP_Tile *t = DP_layer_content_tile_at_noinc(lc, x, y);
            DP_BuildIndexTileMap *entry;
            bool should_move = t && !search_tile(e->current.tiles, t)
                            && (entry = search_tile(e->last->tiles, t));
            if (should_move) {
                move_tile_offset(e, entry);
            }
        }
    }
}

static unsigned char *get_compression_buffer(size_t size, void *user)
{
    size_t required_capacity = sizeof(uint16_t) + size;
    return DP_draw_context_pool_require(user, required_capacity)
         + sizeof(uint16_t);
}

static size_t write_index_tile(DP_BuildIndexEntryContext *e, DP_Tile *t)
{
    size_t size = DP_tile_compress(t, DP_draw_context_tile8_buffer(e->dc),
                                   get_compression_buffer, e->dc);
    if (size == 0) {
        return 0;
    }

    bool error;
    size_t offset = DP_output_tell(e->output, &error);
    if (error) {
        return 0;
    }

    unsigned char *buffer = DP_draw_context_pool(e->dc);
    DP_write_littleendian_uint16(DP_size_to_uint16(size), buffer);
    if (!DP_output_write(e->output, buffer, size + sizeof(uint16_t))) {
        return 0;
    }

    return offset;
}

static bool maybe_write_index_tile(DP_BuildIndexEntryContext *e, DP_Tile *t,
                                   size_t *out_offset)
{
    if (t) {
        DP_BuildIndexTileMap *entry;
        if ((entry = search_tile(e->current.tiles, t))) {
            *out_offset = entry->offset;
        }
        else if ((entry = search_tile(e->last->tiles, t))) {
            move_tile_offset(e, entry);
            *out_offset = entry->offset;
        }
        else {
            size_t offset = write_index_tile(e, t);
            if (offset != 0) {
                *out_offset = offset;
            }
            else {
                return false;
            }
        }
    }
    else {
        *out_offset = 0;
    }
    return true;
}

static size_t search_existing_layer(DP_BuildIndexEntryContext *e,
                                    DP_BuildIndexLayerKey *key)
{
    DP_BuildIndexLayerMap *entry;
    // If we already wrote the layer this round, it might be here. Shouldn't
    // happen in practice, since layer ids are supposed to be unique.
    HASH_FIND(hh, e->current.layers, key, sizeof(*key), entry);
    if (entry) {
        return entry->offset;
    }
    // If we wrote the layer last snapshot, move it to the current set.
    HASH_FIND(hh, e->last->layers, key, sizeof(*key), entry);
    if (entry) {
        HASH_DEL(e->last->layers, entry);
        HASH_ADD(hh, e->current.layers, key, sizeof(entry->key), entry);
        if (!DP_layer_props_children_noinc(entry->key.lp)) {
            move_tile_offsets(e, entry->key.lc);
        }
        return entry->offset;
    }
    else {
        return 0;
    }
}

static size_t write_index_layer_list(DP_BuildIndexEntryContext *e,
                                     struct DP_BuildIndexLayerProps *bilp,
                                     DP_LayerList *ll, DP_LayerPropsList *lpl);

static struct DP_BuildIndexLayerProps to_index_layer_props(DP_LayerProps *lp)
{
    size_t title_length;
    const char *title = DP_layer_props_title(lp, &title_length);
    return (struct DP_BuildIndexLayerProps){
        DP_int_to_uint16(DP_layer_props_id(lp)),
        DP_size_to_uint16(title_length),
        {.title = title},
        DP_channel15_to_8(DP_layer_props_opacity(lp)),
        DP_int_to_uint8(DP_layer_props_blend_mode(lp)),
        DP_layer_props_hidden(lp) ? 1 : 0,
        DP_layer_props_censored(lp) ? 1 : 0,
        DP_layer_props_isolated(lp) ? 1 : 0,
        DP_layer_props_children_noinc(lp) ? 1 : 0};
}

static bool write_index_layer_props(DP_BuildIndexEntryContext *e,
                                    struct DP_BuildIndexLayerProps *bilp)
{
    DP_Output *output = e->output;
    return DP_OUTPUT_WRITE_LITTLEENDIAN(output,
                                        DP_OUTPUT_UINT16(bilp->layer_id),
                                        DP_OUTPUT_UINT16(bilp->title_length))
        && DP_output_write(output, bilp->title, bilp->title_length)
        && DP_OUTPUT_WRITE_LITTLEENDIAN(
               output, DP_OUTPUT_UINT8(bilp->opacity),
               DP_OUTPUT_UINT8(bilp->blend_mode), DP_OUTPUT_UINT8(bilp->hidden),
               DP_OUTPUT_UINT8(bilp->censored), DP_OUTPUT_UINT8(bilp->isolated),
               DP_OUTPUT_UINT8(bilp->group));
}

static bool is_relevant_sublayer(DP_LayerProps *sub_lp)
{
    int sub_id = DP_layer_props_id(sub_lp);
    return sub_id >= 0 && sub_id <= UINT8_MAX;
}

static size_t write_index_layer_content(DP_BuildIndexEntryContext *e,
                                        DP_LayerContent *lc, DP_LayerProps *lp,
                                        bool sublayer)
{
    if (!sublayer) { // Sublayers are ephemeral, don't try re-using them.
        DP_BuildIndexLayerKey key;    // There's not gonna be padding here, but
        memset(&key, 0, sizeof(key)); // we'll zero it anyway just to make sure.
        key.lc = lc;
        key.lp = lp;
        size_t existing_offset = search_existing_layer(e, &key);
        if (existing_offset != 0) {
            return existing_offset;
        }
    }

    DP_LayerPropsList *sub_lpl = DP_layer_content_sub_props_noinc(lc);
    int sub_count = DP_layer_props_list_count(sub_lpl);
    size_t relevant_sub_count = 0;
    for (int i = 0; i < sub_count; ++i) {
        DP_LayerProps *sub_lp = DP_layer_props_list_at_noinc(sub_lpl, i);
        if (is_relevant_sublayer(sub_lp)) {
            ++relevant_sub_count;
        }
    }

    size_t sub_buffer_size =
        sizeof(uint16_t) + sizeof(uint64_t) * relevant_sub_count;
    unsigned char *sub_buffer = DP_malloc(sub_buffer_size);
    DP_write_littleendian_uint16(DP_size_to_uint16(relevant_sub_count),
                                 sub_buffer);

    size_t sub_index = 0;
    DP_LayerList *sub_ll = DP_layer_content_sub_contents_noinc(lc);
    for (int i = sub_count - 1; i >= 0; --i) { // Top to bottom.
        DP_LayerProps *sub_lp = DP_layer_props_list_at_noinc(sub_lpl, i);
        if (is_relevant_sublayer(sub_lp)) {
            DP_LayerContent *sub_lc = DP_layer_list_content_at_noinc(sub_ll, i);
            size_t sub_offset =
                write_index_layer_content(e, sub_lc, sub_lp, true);

            if (sub_offset != 0) {
                size_t ii = relevant_sub_count - sub_index - 1;
                unsigned char *out =
                    sub_buffer + sizeof(uint16_t) + ii * sizeof(uint64_t);
                DP_write_littleendian_uint64(sub_offset, out);
                ++sub_index;
            }
            else {
                DP_free(sub_buffer);
                return 0;
            }
        }
    }
    DP_ASSERT(sub_index == relevant_sub_count);

    DP_TileCounts tile_counts = DP_tile_counts_round(
        DP_layer_content_width(lc), DP_layer_content_height(lc));
    int tile_total = tile_counts.x * tile_counts.y;
    size_t tile_buffer_size = DP_int_to_size(tile_total) * sizeof(uint64_t);
    unsigned char *tile_buffer = DP_malloc(tile_buffer_size);
    size_t written = 0;
    for (int y = 0; y < tile_counts.y; ++y) {
        for (int x = 0; x < tile_counts.x; ++x) {
            DP_Tile *t = DP_layer_content_tile_at_noinc(lc, x, y);
            size_t tile_offset;
            if (!maybe_write_index_tile(e, t, &tile_offset)) {
                DP_free(sub_buffer);
                DP_free(tile_buffer);
                return 0;
            }
            written += DP_write_littleendian_uint64(tile_offset,
                                                    tile_buffer + written);
        }
    }
    DP_ASSERT(written == tile_buffer_size);

    bool error;
    size_t offset = DP_output_tell(e->output, &error);
    struct DP_BuildIndexLayerProps bilp = to_index_layer_props(lp);
    bool ok = !error && write_index_layer_props(e, &bilp)
           && DP_output_write(e->output, sub_buffer, sub_buffer_size)
           && DP_output_write(e->output, tile_buffer, tile_buffer_size);
    DP_free(sub_buffer);
    DP_free(tile_buffer);
    return ok ? offset : 0;
}

static size_t write_index_layer_group(DP_BuildIndexEntryContext *e,
                                      DP_LayerGroup *lg, DP_LayerProps *lp)
{
    DP_BuildIndexLayerKey key;    // There's not gonna be padding here, but
    memset(&key, 0, sizeof(key)); // we'll zero it anyway just to make sure.
    key.lg = lg;
    key.lp = lp;
    size_t existing_offset = search_existing_layer(e, &key);
    if (existing_offset != 0) {
        return existing_offset;
    }

    struct DP_BuildIndexLayerProps bilp = to_index_layer_props(lp);
    size_t offset =
        write_index_layer_list(e, &bilp, DP_layer_group_children_noinc(lg),
                               DP_layer_props_children_noinc(lp));

    if (offset != 0) {
        DP_BuildIndexLayerMap *entry = DP_malloc_zeroed(sizeof(*entry));
        entry->key.lg = DP_layer_group_incref(lg);
        entry->key.lp = DP_layer_props_incref(lp);
        entry->offset = offset;
        HASH_ADD(hh, e->current.layers, key, sizeof(entry->key), entry);
    }

    return offset;
}

static size_t write_index_layer_list(DP_BuildIndexEntryContext *e,
                                     struct DP_BuildIndexLayerProps *bilp,
                                     DP_LayerList *ll, DP_LayerPropsList *lpl)
{
    int count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    // A buffer to hold the layer count plus each layer's offset.
    size_t size = sizeof(uint16_t) + DP_int_to_size(count) * sizeof(uint64_t);
    unsigned char *buffer = DP_malloc(size);
    DP_write_littleendian_uint16(DP_int_to_uint16(count), buffer);

    for (int i = count - 1; i >= 0; --i) { // Top to bottom.
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        size_t child_offset;
        if (DP_layer_props_children_noinc(lp)) {
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            child_offset = write_index_layer_group(e, lg, lp);
        }
        else {
            DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
            child_offset = write_index_layer_content(e, lc, lp, false);
        }

        if (child_offset != 0) {
            size_t ii = DP_int_to_size(count - i - 1);
            unsigned char *out =
                buffer + sizeof(uint16_t) + ii * sizeof(uint64_t);
            DP_write_littleendian_uint64(child_offset, out);
        }
        else {
            DP_free(buffer);
            return 0;
        }
    }

    bool error;
    size_t offset = DP_output_tell(e->output, &error);
    bool ok = !error && write_index_layer_props(e, bilp)
           && DP_output_write(e->output, buffer, size);
    DP_free(buffer);
    return ok ? offset : 0;
}

static bool write_index_layers(DP_BuildIndexEntryContext *e)
{
    struct DP_BuildIndexLayerProps bilp = {
        0, 0, {.title = ""}, UINT8_MAX, DP_BLEND_MODE_NORMAL, 0, 0, 0, 1};
    size_t offset =
        write_index_layer_list(e, &bilp, DP_canvas_state_layers_noinc(e->cs),
                               DP_canvas_state_layer_props_noinc(e->cs));
    if (offset != 0) {
        e->offset.layers = offset;
        return true;
    }
    else {
        return false;
    }
}

static size_t write_index_annotation(DP_BuildIndexEntryContext *e,
                                     DP_Annotation *a)
{
    DP_BuildIndexAnnotationMap *entry;

    HASH_FIND_PTR(e->current.annotations, &a, entry);
    if (entry) {
        return entry->offset;
    }

    HASH_FIND_PTR(e->last->annotations, &a, entry);
    if (entry) {
        HASH_DEL(e->last->annotations, entry);
        HASH_ADD_PTR(e->current.annotations, a, entry);
        return entry->offset;
    }

    DP_Output *output = e->output;
    bool error;
    size_t offset = DP_output_tell(output, &error);
    if (error) {
        return 0;
    }

    size_t text_length;
    const char *text = DP_annotation_text(a, &text_length);
    bool ok = DP_OUTPUT_WRITE_LITTLEENDIAN(
                  output, DP_OUTPUT_UINT16(DP_annotation_id(a)),
                  DP_OUTPUT_INT32(DP_annotation_x(a)),
                  DP_OUTPUT_INT32(DP_annotation_y(a)),
                  DP_OUTPUT_INT32(DP_annotation_width(a)),
                  DP_OUTPUT_INT32(DP_annotation_height(a)),
                  DP_OUTPUT_UINT32(DP_annotation_background_color(a)),
                  DP_OUTPUT_UINT8(DP_annotation_valign(a)),
                  DP_OUTPUT_UINT16(text_length))
           && DP_output_write(output, text, text_length);
    if (!ok) {
        return 0;
    }

    entry = DP_malloc(sizeof(*entry));
    entry->a = DP_annotation_incref(a);
    entry->offset = offset;
    HASH_ADD_PTR(e->current.annotations, a, entry);
    return offset;
}

static bool write_index_annotations(DP_BuildIndexEntryContext *e)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(e->cs);
    int count = DP_annotation_list_count(al);
    size_t size = sizeof(uint16_t) + DP_int_to_size(count) * sizeof(uint64_t);
    unsigned char *buffer = DP_malloc(size);
    DP_write_littleendian_uint16(DP_int_to_uint16(count), buffer);

    for (int i = 0; i < count; ++i) {
        DP_Annotation *a = DP_annotation_list_at_noinc(al, i);
        size_t offset = write_index_annotation(e, a);
        if (offset != 0) {
            unsigned char *out = buffer + sizeof(uint16_t)
                               + DP_int_to_size(i) * sizeof(uint64_t);
            DP_write_littleendian_uint64(offset, out);
        }
        else {
            DP_free(buffer);
            return false;
        }
    }

    e->annotation.buffer = buffer;
    e->annotation.size = size;
    return true;
}

static bool write_index_background_tile(DP_BuildIndexEntryContext *e)
{
    DP_Tile *t = DP_canvas_state_background_tile_noinc(e->cs);
    size_t offset;
    if (maybe_write_index_tile(e, t, &offset)) {
        e->offset.background_tile = offset;
        return true;
    }
    else {
        return false;
    }
}

static bool write_index_timeline(DP_BuildIndexEntryContext *e)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(e->cs);
    if (e->last->timeline.tl == tl) {
        e->current.timeline = e->last->timeline;
        e->last->timeline.tl = NULL;
        return true;
    }

    DP_Output *output = e->output;
    bool error;
    size_t offset = DP_output_tell(output, &error);
    if (error) {
        return false;
    }

    int frame_count = DP_timeline_frame_count(tl);
    if (!DP_OUTPUT_WRITE_LITTLEENDIAN(output, DP_OUTPUT_UINT16(frame_count))) {
        return false;
    }

    for (int i = 0; i < frame_count; ++i) {
        DP_Frame *f = DP_timeline_frame_at_noinc(tl, i);
        int layer_id_count = DP_frame_layer_id_count(f);
        if (!DP_OUTPUT_WRITE_LITTLEENDIAN(output,
                                          DP_OUTPUT_UINT16(layer_id_count))) {
            return false;
        }

        for (int j = 0; j < layer_id_count; ++j) {
            int layer_id = j < layer_id_count ? DP_frame_layer_id_at(f, j) : 0;
            if (!DP_OUTPUT_WRITE_LITTLEENDIAN(output,
                                              DP_OUTPUT_UINT16(layer_id))) {
                return false;
            }
        }
    }

    e->current.timeline.tl = DP_timeline_incref(tl);
    e->current.timeline.offset = offset;
    return true;
}

static bool write_index_metadata(DP_BuildIndexEntryContext *e)
{
    DP_DocumentMetadata *dm = DP_canvas_state_metadata_noinc(e->cs);
    if (e->last->metadata.dm == dm) {
        e->current.metadata = e->last->metadata;
        e->last->metadata.dm = NULL;
        return true;
    }

    DP_Output *output = e->output;
    bool error;
    size_t offset = DP_output_tell(output, &error);
    if (error) {
        return false;
    }

    if (!DP_OUTPUT_WRITE_LITTLEENDIAN(
            output, DP_OUTPUT_INT32(DP_document_metadata_dpix(dm)),
            DP_OUTPUT_INT32(DP_document_metadata_dpiy(dm)),
            DP_OUTPUT_INT32(DP_document_metadata_framerate(dm)),
            DP_OUTPUT_UINT8(DP_document_metadata_use_timeline(dm)))) {
        return false;
    }

    e->current.metadata.dm = DP_document_metadata_incref(dm);
    e->current.metadata.offset = offset;
    return true;
}

static bool write_index_canvas_state(DP_BuildIndexEntryContext *e)
{
    DP_Output *output = e->output;
    bool error;
    size_t offset = DP_output_tell(output, &error);
    if (error) {
        return false;
    }

    DP_debug("Write canvas state at offset %zu", offset);
    DP_CanvasState *cs = e->cs;
    if (!DP_OUTPUT_WRITE_LITTLEENDIAN(
            output, DP_OUTPUT_UINT32(DP_canvas_state_width(cs)),
            DP_OUTPUT_UINT32(DP_canvas_state_height(cs)),
            DP_OUTPUT_UINT16(e->message_count),
            DP_OUTPUT_UINT64(e->offset.history),
            DP_OUTPUT_UINT64(e->offset.background_tile),
            DP_OUTPUT_UINT64(e->current.timeline.offset),
            DP_OUTPUT_UINT64(e->current.metadata.offset),
            DP_OUTPUT_UINT64(e->offset.layers))) {
        return false;
    }

    if (!DP_output_write(output, e->annotation.buffer, e->annotation.size)) {
        return false;
    }

    e->offset.snapshot = offset;
    return true;
}

static bool write_index_snapshot(DP_BuildIndexEntryContext *e)
{
    bool ok = write_index_history(e) && write_index_layers(e)
           && write_index_annotations(e) && write_index_background_tile(e)
           && write_index_timeline(e) && write_index_metadata(e)
           && write_index_canvas_state(e);
    DP_free(e->annotation.buffer);
    return ok;
}

static bool write_index_thumbnail(DP_BuildIndexEntryContext *e)
{
    DP_Output *output = e->output;
    bool error;
    size_t thumbnail_offset = DP_output_tell(output, &error);
    if (error) {
        return false;
    }

    DP_Image *img = DP_canvas_state_to_flat_image(
        e->cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
    if (!img) {
        DP_warn("Error creating index thumbnail: %s", DP_error());
        return true; // Keep going without a thumbnail.
    }

    DP_Image *thumb;
    if (DP_image_thumbnail(img, e->dc, 256, 256, &thumb)) {
        if (thumb) {
            DP_image_free(img);
        }
        else {
            thumb = img;
        }
    }
    else {
        DP_image_free(img);
        DP_warn("Error scaling index thumbnail: %s", DP_error());
        return true; // Keep going without a thumbnail.
    }

    if (!DP_OUTPUT_WRITE_LITTLEENDIAN(output, DP_OUTPUT_UINT32(0))) {
        return false;
    }

    bool write_ok = DP_image_write_png(thumb, output);
    DP_image_free(thumb);
    if (!write_ok) {
        return false;
    }

    size_t end_offset = DP_output_tell(output, &error);
    if (error) {
        return false;
    }

    if (!DP_output_seek(output, thumbnail_offset)) {
        return false;
    }

    size_t size = end_offset - thumbnail_offset - 4;
    if (!DP_OUTPUT_WRITE_LITTLEENDIAN(output, DP_OUTPUT_UINT32(size))) {
        return false;
    }

    if (!DP_output_seek(output, end_offset)) {
        return false;
    }

    e->offset.thumbnail = thumbnail_offset;
    return true;
}

static void dispose_index_maps(DP_BuildIndexMaps *maps)
{
    DP_BuildIndexTileMap *tile_entry, *tile_tmp;
    HASH_ITER(hh, maps->tiles, tile_entry, tile_tmp) {
        HASH_DEL(maps->tiles, tile_entry);
        DP_tile_decref(tile_entry->t);
        DP_free(tile_entry);
    }

    DP_BuildIndexLayerMap *layer_entry, *layer_tmp;
    HASH_ITER(hh, maps->layers, layer_entry, layer_tmp) {
        HASH_DEL(maps->layers, layer_entry);
        if (DP_layer_props_children_noinc(layer_entry->key.lp)) {
            DP_layer_group_decref(layer_entry->key.lg);
        }
        else {
            DP_layer_content_decref(layer_entry->key.lc);
        }
        DP_layer_props_decref(layer_entry->key.lp);
        DP_free(layer_entry);
    }

    DP_BuildIndexAnnotationMap *annotation_entry, *annotation_tmp;
    HASH_ITER(hh, maps->annotations, annotation_entry, annotation_tmp) {
        HASH_DEL(maps->annotations, annotation_entry);
        DP_annotation_decref(annotation_entry->a);
        DP_free(annotation_entry);
    }

    DP_document_metadata_decref_nullable(maps->metadata.dm);
    DP_timeline_decref_nullable(maps->timeline.tl);
}

static bool make_index_entry(DP_BuildIndexContext *c, long long message_index,
                             size_t message_offset)
{
    DP_BuildIndexEntryContext e = {c->output,
                                   c->acls,
                                   c->local_state,
                                   c->ch,
                                   NULL,
                                   c->dc,
                                   {NULL, NULL, NULL, {NULL, 0}, {NULL, 0}},
                                   &c->last,
                                   0,
                                   {0, 0, 0, 0, 0},
                                   {NULL, 0}};
    bool ok = write_index_snapshot(&e) && write_index_thumbnail(&e);
    if (!ok) {
        dispose_index_maps(&e.current);
        return false;
    }

    DP_PlayerIndexEntry entry = {message_index, message_offset,
                                 e.offset.snapshot, e.offset.thumbnail};
    DP_VECTOR_PUSH_TYPE(&c->entries, DP_PlayerIndexEntry, entry);

    dispose_index_maps(&c->last);
    c->last = e.current;

    return true;
}

static bool write_index_messages(DP_BuildIndexContext *c)
{
    DP_Player *player = c->player;
    DP_AclState *acls = c->acls;
    DP_LocalState *ls = c->local_state;
    DP_CanvasHistory *ch = c->ch;
    DP_DrawContext *dc = c->dc;
    int last_percent = 0;
    long long last_written_message_index = 0;

    while (true) {
        size_t message_offset = DP_player_tell(player);
        DP_Message *msg;
        DP_PlayerResult result = DP_player_step(player, &msg);
        if (result == DP_PLAYER_SUCCESS) {
            bool filtered = DP_acl_state_handle(acls, msg, false)
                          & DP_ACL_STATE_FILTERED_BIT;
            if (filtered) {
                DP_debug(
                    "ACL filtered recorded %s message from user %u",
                    DP_message_type_enum_name_unprefixed(DP_message_type(msg)),
                    DP_message_context_id(msg));
            }
            else {
                DP_local_state_handle(ls, dc, msg);
                if (DP_message_type_command(DP_message_type(msg))) {
                    if (!DP_canvas_history_handle(ch, dc, msg)) {
                        DP_warn("Error handling message in index: %s",
                                DP_error());
                    }

                    long long message_index = c->message_count++;
                    if (c->should_snapshot_fn(c->user)) {
                        if (!make_index_entry(c, message_index,
                                              message_offset)) {
                            return false;
                        }
                        last_written_message_index = message_index;
                    }
                }
            }
            DP_message_decref(msg);

            if (c->progress_fn) {
                double progress = DP_player_progress(player);
                int percent = DP_double_to_int(progress * 100.0 + 0.5);
                if (percent > last_percent) {
                    last_percent = percent;
                    c->progress_fn(c->user, percent);
                }
            }
        }
        else if (result == DP_PLAYER_ERROR_PARSE) {
            DP_warn("Can't index message: %s", DP_error());
        }
        else if (result == DP_PLAYER_RECORDING_END) {
            break;
        }
        else {
            return false; // Input error, bail out.
        }
    }

    long long message_index = c->message_count - 1;
    if (message_index >= 0 && message_index != last_written_message_index) {
        return make_index_entry(c, message_index, DP_player_tell(player));
    }
    else {
        return true;
    }
}

static bool write_index_entry(DP_BuildIndexContext *c,
                              DP_PlayerIndexEntry *entry)
{
    return DP_OUTPUT_WRITE_LITTLEENDIAN(
        c->output, DP_OUTPUT_UINT32(entry->message_index),
        DP_OUTPUT_UINT64(entry->message_offset),
        DP_OUTPUT_UINT64(entry->snapshot_offset),
        DP_OUTPUT_UINT64(entry->thumbnail_offset));
}

static bool write_index_finish(DP_BuildIndexContext *c)
{
    DP_Output *output = c->output;
    bool error;
    size_t entries_offset = DP_output_tell(output, &error);
    if (error) {
        return false;
    }

    size_t count = c->entries.used;
    for (size_t i = 0; i < count; ++i) {
        if (!write_index_entry(
                c, &DP_VECTOR_AT_TYPE(&c->entries, DP_PlayerIndexEntry, i))) {
            return false;
        }
    }

    if (!DP_output_seek(output, INDEX_MAGIC_LENGTH + INDEX_VERSION_LENGTH)) {
        return false;
    }

    return DP_OUTPUT_WRITE_LITTLEENDIAN(output,
                                        DP_OUTPUT_UINT32(c->message_count),
                                        DP_OUTPUT_UINT64(entries_offset));
}

static bool write_index(DP_BuildIndexContext *c)
{
    return write_index_header(c) && write_index_messages(c)
        && write_index_finish(c) && DP_output_flush(c->output);
}

bool DP_player_index_build(DP_Player *player, DP_DrawContext *dc,
                           DP_PlayerIndexShouldSnapshotFn should_snapshot_fn,
                           DP_PlayerIndexProgressFn progress_fn, void *user)
{
    DP_ASSERT(player);
    DP_ASSERT(dc);
    DP_ASSERT(should_snapshot_fn);
    if (player->type == DP_PLAYER_TYPE_DEBUG_DUMP) {
        DP_error_set("Can't index a debug dump");
        return false;
    }

    const char *recording_path = player->recording_path;
    if (!recording_path || !player->index_path) {
        DP_error_set("Can't index player without a path");
        return false;
    }

    DP_Input *input = DP_file_input_new_from_path(recording_path);
    if (!input) {
        return false;
    }

    DP_Player *index_player =
        DP_player_new(player->type, recording_path, input, NULL);
    if (!index_player) {
        return false;
    }

    const char *path = index_player->index_path;
    DP_Output *output = DP_file_output_save_new_from_path(path);
    if (!output) {
        DP_player_free(index_player);
        return false;
    }

    DP_PERF_BEGIN_DETAIL(fn, "index_build", "path=%s", path);
    DP_AclState *acls = DP_acl_state_new();
    DP_LocalState *ls = DP_local_state_new(NULL, NULL, NULL, NULL);
    DP_CanvasHistory *ch = DP_canvas_history_new(NULL, NULL, false, NULL);
    DP_BuildIndexContext c = {index_player,
                              output,
                              acls,
                              ls,
                              ch,
                              dc,
                              0,
                              DP_VECTOR_NULL,
                              {NULL, NULL, NULL, {NULL, 0}, {NULL, 0}},
                              should_snapshot_fn,
                              progress_fn,
                              user};
    DP_VECTOR_INIT_TYPE(&c.entries, DP_PlayerIndexEntry, INITAL_ENTRY_CAPACITY);
    bool ok = write_index(&c);
    dispose_index_maps(&c.last);
    DP_vector_dispose(&c.entries);
    DP_canvas_history_free(ch);
    DP_local_state_free(ls);
    DP_acl_state_free(acls);
    DP_output_free(output);
    DP_player_free(index_player);
    DP_PERF_END(fn);
    return ok;
}


typedef struct DP_ReadIndexContext {
    DP_BufferedInput input;
    unsigned int message_count;
    size_t index_offset;
    DP_Vector entries;
} DP_ReadIndexContext;

static bool read_index_input(DP_BufferedInput *input, size_t size)
{
    bool error;
    size_t read = DP_buffered_input_read(input, size, &error);
    if (error) {
        return false;
    }
    else if (read != size) {
        DP_error_set("Tried to read %zu byte(s), but got %zu", size, read);
        return false;
    }
    else {
        return true;
    }
}

#define READ_INDEX(INPUT, TYPE, OUT)             \
    (read_index_input((INPUT), sizeof(TYPE##_t)) \
     && ((OUT) = DP_read_littleendian_##TYPE((INPUT)->buffer), true))

#define READ_INDEX_SIZE(INPUT, OUT)              \
    (read_index_input((INPUT), sizeof(uint64_t)) \
     && ((OUT) = read_littleendian_size((INPUT)->buffer), true))

static size_t read_littleendian_size(const unsigned char *d)
{
    return DP_uint64_to_size(DP_read_littleendian_uint64(d));
}

static bool check_index_magic(DP_ReadIndexContext *c)
{
    DP_BufferedInput *input = &c->input;
    if (read_index_input(input, INDEX_MAGIC_LENGTH)) {
        if (memcmp(input->buffer, INDEX_MAGIC, INDEX_MAGIC_LENGTH) == 0) {
            return true;
        }
        else {
            DP_error_set("File does not start with magic '" INDEX_MAGIC "\\0'");
        }
    }
    return false;
}

static bool check_index_version(DP_ReadIndexContext *c)
{
    int version;
    if (READ_INDEX(&c->input, uint16, version)) {
        if (version == INDEX_VERSION) {
            return true;
        }
        else {
            DP_error_set("Index version mismatch: expected %d, got %d",
                         INDEX_VERSION, version);
        }
    }
    return false;
}

static bool read_index_offset(DP_ReadIndexContext *c)
{
    if (READ_INDEX_SIZE(&c->input, c->index_offset)) {
        if (c->index_offset >= INDEX_HEADER_LENGTH) {
            return true;
        }
        else {
            DP_error_set("Offset %zu is inside header (incomplete index?)",
                         c->index_offset);
        }
    }
    return false;
}

static bool read_index_header(DP_ReadIndexContext *c)
{
    DP_BufferedInput *input = &c->input;
    return check_index_magic(c) && check_index_version(c)
        && READ_INDEX(input, uint32, c->message_count) && read_index_offset(c);
}

#define ENTRY_SIZE (sizeof(uint32_t) + sizeof(uint64_t) * (size_t)3)

static bool read_index_entries(DP_ReadIndexContext *c)
{
    if (!DP_buffered_input_seek(&c->input, c->index_offset)) {
        return false;
    }

    DP_VECTOR_INIT_TYPE(&c->entries, DP_PlayerIndexEntry,
                        INITAL_ENTRY_CAPACITY);

    while (true) {
        bool error;
        size_t read = DP_buffered_input_read(&c->input, ENTRY_SIZE, &error);
        if (error) {
            return false;
        }
        else if (read == ENTRY_SIZE) {
            DP_PlayerIndexEntry entry = {
                DP_read_littleendian_uint32(c->input.buffer),
                read_littleendian_size(c->input.buffer + 4),
                read_littleendian_size(c->input.buffer + 12),
                read_littleendian_size(c->input.buffer + 20),
            };
            DP_debug("Read index entry %zu with message index %lld, message "
                     "offset %zu, snapshot offset %zu, thumbnail offset %zu",
                     c->entries.used, entry.message_index, entry.message_offset,
                     entry.snapshot_offset, entry.thumbnail_offset);
            DP_VECTOR_PUSH_TYPE(&c->entries, DP_PlayerIndexEntry, entry);
        }
        else if (read == 0) {
            DP_debug("Reached end of index entries");
            return true;
        }
        else {
            DP_error_set("Expected index entry of %zu bytes, but got %zu",
                         ENTRY_SIZE, read);
            return false;
        }
    }
}

bool DP_player_index_load(DP_Player *player)
{
    DP_ASSERT(player);
    if (player->type == DP_PLAYER_TYPE_DEBUG_DUMP) {
        DP_error_set("Can't load index of a debug dump");
        return false;
    }

    const char *path = player->index_path;
    if (!path) {
        DP_error_set("Can't load index of a player without a path");
        return false;
    }

    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        return false;
    }

    DP_PERF_BEGIN_DETAIL(fn, "index_load", "path=%s", path);
    DP_ReadIndexContext c = {DP_buffered_input_init(input), 0, 0,
                             DP_VECTOR_NULL};

    bool ok = read_index_header(&c) && read_index_entries(&c);
    if (ok) {
        player_index_dispose(&player->index);
        player->index = (DP_PlayerIndex){c.input, c.message_count,
                                         c.entries.elements, c.entries.used};
    }
    else {
        DP_vector_dispose(&c.entries);
        DP_buffered_input_dispose(&c.input);
    }

    DP_PERF_END(fn);
    return ok;
}

static bool check_index(DP_Player *player)
{
    if (DP_player_index_loaded(player)) {
        return true;
    }
    else {
        DP_error_set("No index set");
        return false;
    }
}


typedef struct DP_ReadTileMap {
    size_t offset;
    DP_Tile *t;
    UT_hash_handle hh;
} DP_ReadTileMap;

typedef struct DP_ReadSnapshotContext {
    DP_BufferedInput *input;
    DP_DrawContext *dc;
    DP_TransientCanvasState *tcs;
    DP_ReadTileMap *tiles;
    DP_PlayerIndexEntrySnapshot *snapshot;
} DP_ReadSnapshotContext;

struct DP_ReadSnapshotLayer {
    union {
        DP_TransientLayerContent *tlc;
        DP_TransientLayerGroup *tlg;
    };
    DP_TransientLayerProps *tlp;
};

static struct DP_ReadSnapshotLayer read_snapshot_layer_null(void)
{
    return (struct DP_ReadSnapshotLayer){{NULL}, NULL};
}

static bool read_snapshot_layer_ok(struct DP_ReadSnapshotLayer rsl)
{
    return rsl.tlp != NULL;
}

static bool read_index_tile_inc(DP_ReadSnapshotContext *c, size_t offset,
                                DP_Tile **out_tile)
{
    if (offset == 0) {
        *out_tile = NULL;
        return true;
    }

    DP_ReadTileMap *entry;
    HASH_FIND(hh, c->tiles, &offset, sizeof(offset), entry);
    if (!entry) {
        DP_debug("Loading tile from offset %zu", offset);
        DP_BufferedInput *input = c->input;
        size_t size;
        bool ok = DP_buffered_input_seek(input, offset)
               && READ_INDEX(input, uint16, size)
               && read_index_input(input, size);
        if (!ok) {
            return false;
        }

        DP_Tile *t = DP_tile_new_from_compressed(c->dc, 0, input->buffer, size);
        if (!t) {
            return false;
        }

        entry = DP_malloc(sizeof(*entry));
        entry->offset = offset;
        entry->t = t;
        HASH_ADD(hh, c->tiles, offset, sizeof(offset), entry);
    }

    *out_tile = DP_tile_incref(entry->t);
    return true;
}

static bool read_index_offsets(DP_BufferedInput *input, int count,
                               size_t **out_offsets)
{
    if (count == 0) {
        DP_debug("Read nothing for 0 offsets");
        *out_offsets = NULL;
        return true;
    }

    size_t scount = DP_int_to_size(count);
    size_t size = sizeof(uint64_t) * scount;
    DP_debug("Read %zu bytes for %d offset(s)", size, count);
    if (!read_index_input(input, size)) {
        return false;
    }

    size_t *offsets = scount == 0 ? NULL : DP_malloc(sizeof(*offsets) * scount);
    for (size_t i = 0, j = 0; i < scount; i += 1, j += sizeof(uint64_t)) {
        offsets[i] = read_littleendian_size(input->buffer + j);
    }
    *out_offsets = offsets;
    return true;
}

static bool read_index_canvas_state(DP_ReadSnapshotContext *c,
                                    unsigned int width, unsigned int height)
{
    if (width > UINT16_MAX || height > UINT16_MAX) {
        DP_error_set("Canvas dimensions %ux%u out of bounds", width, height);
        return false;
    }
    c->tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_width_set(c->tcs, DP_uint_to_int(width));
    DP_transient_canvas_state_height_set(c->tcs, DP_uint_to_int(height));
    return true;
}

static DP_Annotation *read_index_annotation(DP_BufferedInput *input,
                                            size_t offset)
{
    DP_debug("Read annotation at offset %zu", offset);
    int id, x, y, width, height, valign;
    uint32_t background_color;
    size_t text_length;
    bool ok = DP_buffered_input_seek(input, offset)
           && READ_INDEX(input, uint16, id) && READ_INDEX(input, int32, x)
           && READ_INDEX(input, int32, y) && READ_INDEX(input, int32, width)
           && READ_INDEX(input, int32, height)
           && READ_INDEX(input, uint32, background_color)
           && READ_INDEX(input, uint8, valign)
           && READ_INDEX(input, uint16, text_length)
           && read_index_input(input, text_length);
    if (ok) {
        DP_TransientAnnotation *ta =
            DP_transient_annotation_new_init(id, x, y, width, height);
        DP_transient_annotation_background_color_set(ta, background_color);
        DP_transient_annotation_valign_set(ta, valign);
        DP_transient_annotation_text_set(ta, (const char *)input->buffer,
                                         text_length);
        return DP_transient_annotation_persist(ta);
    }
    else {
        return NULL;
    }
}

static bool read_index_annotations(DP_ReadSnapshotContext *c, int count)
{
    DP_debug("Read %d annotation(s)", count);
    DP_BufferedInput *input = c->input;
    size_t *offsets;
    if (!read_index_offsets(input, count, &offsets)) {
        return false;
    }

    bool ok = true;
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(c->tcs, count);
    for (int i = 0; i < count; ++i) {
        DP_Annotation *a = read_index_annotation(input, offsets[i]);
        if (a) {
            DP_transient_annotation_list_insert_noinc(tal, a, i);
        }
        else {
            ok = false;
            break;
        }
    }

    DP_free(offsets);
    return ok;
}

static bool read_index_background_tile(DP_ReadSnapshotContext *c, size_t offset)
{
    DP_debug("Read background tile at offset %zu", offset);
    DP_Tile *t;
    if (read_index_tile_inc(c, offset, &t)) {
        DP_transient_canvas_state_background_tile_set_noinc(c->tcs, t,
                                                            DP_tile_opaque(t));
        return true;
    }
    else {
        return false;
    }
}

static DP_TransientFrame *read_index_frame(DP_ReadSnapshotContext *c)
{
    DP_BufferedInput *input = c->input;
    int layer_id_count;
    bool ok = READ_INDEX(input, uint16, layer_id_count)
           && read_index_input(input, sizeof(uint16_t)
                                          * DP_int_to_size(layer_id_count));
    if (!ok) {
        return NULL;
    }

    DP_TransientFrame *tf = DP_transient_frame_new_init(layer_id_count);
    for (int j = 0; j < layer_id_count; ++j) {
        int layer_id = DP_read_littleendian_uint16(
            input->buffer + sizeof(uint16_t) * DP_int_to_size(j));
        DP_transient_frame_layer_id_set_at(tf, layer_id, j);
    }
    return tf;
}

static bool read_index_timeline(DP_ReadSnapshotContext *c, size_t offset)
{
    DP_debug("Read timeline at offset %zu", offset);
    DP_BufferedInput *input = c->input;
    int frame_count;
    bool ok = DP_buffered_input_seek(input, offset)
           && READ_INDEX(input, uint16, frame_count);
    if (!ok) {
        return false;
    }

    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(c->tcs, frame_count);
    DP_debug("Read %d timeline frame(s)", frame_count);
    for (int i = 0; i < frame_count; ++i) {
        DP_TransientFrame *tf = read_index_frame(c);
        if (!tf) {
            return false;
        }
        DP_transient_timeline_insert_transient_noinc(ttl, tf, i);
    }
    return true;
}

static bool read_index_metadata(DP_ReadSnapshotContext *c, size_t offset)
{
    DP_debug("Read document metadata at offset %zu", offset);
    DP_BufferedInput *input = c->input;
    int dpix, dpiy, framerate;
    uint8_t use_timeline;
    bool ok = DP_buffered_input_seek(input, offset)
           && READ_INDEX(input, int32, dpix) && READ_INDEX(input, int32, dpiy)
           && READ_INDEX(input, int32, framerate)
           && READ_INDEX(input, uint8, use_timeline);
    if (ok) {
        DP_TransientDocumentMetadata *tdm =
            DP_transient_canvas_state_transient_metadata(c->tcs);
        DP_transient_document_metadata_dpix_set(tdm, dpix);
        DP_transient_document_metadata_dpiy_set(tdm, dpiy);
        DP_transient_document_metadata_framerate_set(tdm, framerate);
        DP_transient_document_metadata_use_timeline_set(
            tdm, use_timeline == 0 ? false : true);
        return true;
    }
    else {
        return false;
    }
}

static bool read_index_layer_title(DP_BufferedInput *input, size_t title_length,
                                   char **out_title)
{
    if (read_index_input(input, title_length)) {
        char *title = DP_malloc(title_length);
        memcpy(title, input->buffer, title_length);
        *out_title = title;
        return true;
    }
    else {
        return false;
    }
}

static DP_TransientLayerProps *
to_transient_layer_props(struct DP_BuildIndexLayerProps *bilp,
                         DP_TransientLayerPropsList *child_tlpl)
{
    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_new_init_with_transient_children_noinc(
            bilp->layer_id, child_tlpl);
    DP_transient_layer_props_title_set(tlp, bilp->title, bilp->title_length);
    DP_transient_layer_props_opacity_set(tlp, DP_channel8_to_15(bilp->opacity));
    if (DP_blend_mode_valid_for_layer(bilp->blend_mode)) {
        DP_transient_layer_props_blend_mode_set(tlp, bilp->blend_mode);
    }
    DP_transient_layer_props_hidden_set(tlp, bilp->hidden != 0);
    DP_transient_layer_props_censored_set(tlp, bilp->censored != 0);
    DP_transient_layer_props_isolated_set(tlp, bilp->isolated != 0);
    return tlp;
}

#define READ_INDEX_LAYER_ALLOW_CONTENT (1 << 0)
#define READ_INDEX_LAYER_ALLOW_GROUP   (1 << 1)
#define READ_INDEX_LAYER_ALLOW_ANY \
    (READ_INDEX_LAYER_ALLOW_CONTENT | READ_INDEX_LAYER_ALLOW_GROUP)
#define READ_INDEX_LAYER_SUBLAYER (1 << 2)

static struct DP_ReadSnapshotLayer
read_index_layer(DP_ReadSnapshotContext *c, size_t offset, unsigned int flags);

static struct DP_ReadSnapshotLayer
read_index_layer_content(DP_ReadSnapshotContext *c,
                         struct DP_BuildIndexLayerProps *bilp,
                         int sublayer_count, size_t *sublayer_offsets)
{
    DP_BufferedInput *input = c->input;
    int width = DP_transient_canvas_state_width(c->tcs);
    int height = DP_transient_canvas_state_height(c->tcs);
    int tile_total = DP_tile_total_round(width, height);

    size_t *tile_offsets;
    if (!read_index_offsets(input, tile_total, &tile_offsets)) {
        return read_snapshot_layer_null();
    }

    DP_TransientLayerList *sub_tll =
        DP_transient_layer_list_new_init(sublayer_count);
    DP_TransientLayerPropsList *sub_tlpl =
        DP_transient_layer_props_list_new_init(sublayer_count);
    for (int i = 0; i < sublayer_count; ++i) {
        struct DP_ReadSnapshotLayer rsl = read_index_layer(
            c, sublayer_offsets[i],
            READ_INDEX_LAYER_ALLOW_CONTENT | READ_INDEX_LAYER_SUBLAYER);
        if (read_snapshot_layer_ok(rsl)) {
            // Layers are stored inverted, so prepend the new ones at index 0.
            DP_transient_layer_list_insert_transient_content_noinc(sub_tll,
                                                                   rsl.tlc, 0);
            DP_transient_layer_props_list_insert_transient_noinc(sub_tlpl,
                                                                 rsl.tlp, 0);
        }
        else {
            DP_transient_layer_props_list_decref(sub_tlpl);
            DP_transient_layer_list_decref(sub_tll);
            DP_free(tile_offsets);
            return read_snapshot_layer_null();
        }
    }

    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init_with_transient_sublayers_noinc(
            width, height, NULL, sub_tll, sub_tlpl);

    for (int i = 0; i < tile_total; ++i) {
        DP_Tile *t;
        if (read_index_tile_inc(c, tile_offsets[i], &t)) {
            DP_transient_layer_content_tile_set_noinc(tlc, t, i);
        }
        else {
            DP_transient_layer_content_decref(tlc);
            DP_free(tile_offsets);
            return read_snapshot_layer_null();
        }
    }
    DP_free(tile_offsets);

    DP_TransientLayerProps *tlp = to_transient_layer_props(bilp, NULL);
    return (struct DP_ReadSnapshotLayer){{.tlc = tlc}, tlp};
}

static struct DP_ReadSnapshotLayer
read_index_layer_group(DP_ReadSnapshotContext *c,
                       struct DP_BuildIndexLayerProps *bilp, int child_count,
                       size_t *child_offsets)
{
    DP_TransientLayerList *child_tll =
        DP_transient_layer_list_new_init(child_count);
    DP_TransientLayerPropsList *child_tlpl =
        DP_transient_layer_props_list_new_init(child_count);
    for (int i = 0; i < child_count; ++i) {
        struct DP_ReadSnapshotLayer rsl =
            read_index_layer(c, child_offsets[i], READ_INDEX_LAYER_ALLOW_ANY);
        if (read_snapshot_layer_ok(rsl)) {
            // Layers are stored inverted, so prepend the new ones at index 0.
            if (DP_transient_layer_props_children_noinc(rsl.tlp)) {
                DP_transient_layer_list_insert_transient_group_noinc(
                    child_tll, rsl.tlg, 0);
            }
            else {
                DP_transient_layer_list_insert_transient_content_noinc(
                    child_tll, rsl.tlc, 0);
            }
            DP_transient_layer_props_list_insert_transient_noinc(child_tlpl,
                                                                 rsl.tlp, 0);
        }
        else {
            DP_transient_layer_props_list_decref(child_tlpl);
            DP_transient_layer_list_decref(child_tll);
            return read_snapshot_layer_null();
        }
    }

    DP_TransientLayerGroup *tlg =
        DP_transient_layer_group_new_init_with_transient_children_noinc(
            DP_transient_canvas_state_width(c->tcs),
            DP_transient_canvas_state_height(c->tcs), child_tll);
    DP_TransientLayerProps *tlp = to_transient_layer_props(bilp, child_tlpl);
    return (struct DP_ReadSnapshotLayer){{.tlg = tlg}, tlp};
}

static struct DP_ReadSnapshotLayer
read_index_layer(DP_ReadSnapshotContext *c, size_t offset, unsigned int flags)
{
    DP_debug("Read layer at offset %zu", offset);
    DP_BufferedInput *input = c->input;
    struct DP_BuildIndexLayerProps bilp = {0};
    int child_count;
    size_t *child_offsets = NULL;
    bool ok = DP_buffered_input_seek(input, offset)
           && READ_INDEX(input, uint16, bilp.layer_id)
           && READ_INDEX(input, uint16, bilp.title_length)
           && read_index_layer_title(input, bilp.title_length, &bilp.buffer)
           && READ_INDEX(input, uint8, bilp.opacity)
           && READ_INDEX(input, uint8, bilp.blend_mode)
           && READ_INDEX(input, uint8, bilp.hidden)
           && READ_INDEX(input, uint8, bilp.censored)
           && READ_INDEX(input, uint8, bilp.isolated)
           && READ_INDEX(input, uint8, bilp.group)
           && READ_INDEX(input, uint16, child_count)
           && read_index_offsets(input, child_count, &child_offsets);

    struct DP_ReadSnapshotLayer rsl = read_snapshot_layer_null();
    if (ok) {
        if (bilp.group) {
            if (flags & READ_INDEX_LAYER_ALLOW_GROUP) {
                rsl = read_index_layer_group(c, &bilp, child_count,
                                             child_offsets);
            }
            else {
                DP_error_set("Expected layer content, but got layer group");
            }
        }
        else if (flags & READ_INDEX_LAYER_ALLOW_CONTENT) {
            bool sublayer = flags & READ_INDEX_LAYER_SUBLAYER;
            rsl = read_index_layer_content(c, &bilp, sublayer ? 0 : child_count,
                                           sublayer ? NULL : child_offsets);
        }
        else {
            DP_error_set("Expected layer group, but got layer content");
        }
    }

    DP_free(child_offsets);
    DP_free(bilp.buffer);
    return rsl;
}

static bool read_index_layers(DP_ReadSnapshotContext *c, size_t offset)
{
    DP_debug("Read layers at offset %zu", offset);
    struct DP_ReadSnapshotLayer rsl =
        read_index_layer(c, offset, READ_INDEX_LAYER_ALLOW_GROUP);
    if (read_snapshot_layer_ok(rsl)) {
        DP_transient_canvas_state_layers_set_inc(
            c->tcs, DP_transient_layer_group_children_noinc(rsl.tlg));
        DP_transient_canvas_state_layer_props_set_inc(
            c->tcs, DP_transient_layer_props_children_noinc(rsl.tlp));
        DP_transient_layer_group_decref(rsl.tlg);
        DP_transient_layer_props_decref(rsl.tlp);
        return true;
    }
    else {
        return false;
    }
}

static bool read_index_history(DP_ReadSnapshotContext *c, size_t offset,
                               int message_count)
{
    DP_BufferedInput *input = c->input;
    if (!DP_buffered_input_seek(input, offset)) {
        return false;
    }

    DP_PlayerIndexEntrySnapshot *snapshot = DP_malloc(DP_FLEX_SIZEOF(
        DP_PlayerIndexEntrySnapshot, messages, DP_int_to_size(message_count)));
    snapshot->cs = NULL;
    snapshot->message_count = message_count;
    for (int i = 0; i < message_count; ++i) {
        snapshot->messages[i] = NULL;
    }
    c->snapshot = snapshot;

    for (int i = 0; i < message_count; ++i) {
        size_t length;
        bool ok = READ_INDEX(input, uint16, length)
               && read_index_input(input, length);
        if (!ok) {
            return false;
        }

        DP_Message *msg = DP_message_deserialize_length(
            input->buffer, length, length < 2 ? 0 : length - 2);
        if (msg) {
            snapshot->messages[i] = msg;
        }
        else {
            DP_warn("Error deserializing history message %d", i);
        }
    }

    return true;
}

static bool read_index_snapshot(DP_ReadSnapshotContext *c)
{
    DP_BufferedInput *input = c->input;
    uint32_t width, height;
    size_t history_offset, background_tile_offset, timeline_offset,
        metadata_offset, layers_offset;
    int message_count, annotation_count;
    return READ_INDEX(input, uint32, width) && READ_INDEX(input, uint32, height)
        && READ_INDEX(input, uint16, message_count)
        && READ_INDEX_SIZE(input, history_offset)
        && READ_INDEX_SIZE(input, background_tile_offset)
        && READ_INDEX_SIZE(input, timeline_offset)
        && READ_INDEX_SIZE(input, metadata_offset)
        && READ_INDEX_SIZE(input, layers_offset)
        && READ_INDEX(input, uint16, annotation_count)
        && read_index_canvas_state(c, width, height)
        && read_index_annotations(c, annotation_count)
        && read_index_background_tile(c, background_tile_offset)
        && read_index_timeline(c, timeline_offset)
        && read_index_metadata(c, metadata_offset)
        && read_index_layers(c, layers_offset)
        && read_index_history(c, history_offset, message_count);
}

DP_PlayerIndexEntrySnapshot *
DP_player_index_entry_load(DP_Player *player, DP_DrawContext *dc,
                           DP_PlayerIndexEntry entry)
{
    DP_ASSERT(player);
    size_t snapshot_offset = entry.snapshot_offset;
    DP_debug("Load snapshot from offset %zu", snapshot_offset);
    if (snapshot_offset == 0) {
        DP_PlayerIndexEntrySnapshot *snapshot = DP_malloc(sizeof(*snapshot));
        snapshot->cs = DP_canvas_state_new();
        snapshot->message_count = 0;
        return snapshot;
    }

    DP_BufferedInput *input = &player->index.input;
    if (!DP_buffered_input_seek(input, snapshot_offset)) {
        return NULL;
    }

    DP_PERF_BEGIN_DETAIL(fn, "index_entry_load", "offset=%zu", snapshot_offset);
    DP_ReadSnapshotContext c = {input, dc, NULL, NULL, NULL};
    bool ok = read_index_snapshot(&c);

    DP_ReadTileMap *tile_entry, *tile_tmp;
    HASH_ITER(hh, c.tiles, tile_entry, tile_tmp) {
        HASH_DEL(c.tiles, tile_entry);
        DP_tile_decref(tile_entry->t);
        DP_free(tile_entry);
    }

    if (ok) {
        DP_transient_canvas_state_layer_routes_reindex(c.tcs, dc);
        DP_transient_canvas_state_timeline_cleanup(c.tcs);
        c.snapshot->cs = DP_transient_canvas_state_persist(c.tcs);
    }
    else {
        DP_transient_canvas_state_decref_nullable(c.tcs);
        DP_player_index_entry_snapshot_free(c.snapshot);
        c.snapshot = NULL;
    }

    DP_PERF_END(fn);
    return c.snapshot;
}

DP_CanvasState *DP_player_index_entry_snapshot_canvas_state_inc(
    DP_PlayerIndexEntrySnapshot *snapshot)
{
    DP_ASSERT(snapshot);
    return DP_canvas_state_incref(snapshot->cs);
}

int DP_player_index_entry_snapshot_message_count(
    DP_PlayerIndexEntrySnapshot *snapshot)
{
    DP_ASSERT(snapshot);
    return snapshot->message_count;
}

DP_Message *DP_player_index_entry_snapshot_message_at_inc(
    DP_PlayerIndexEntrySnapshot *snapshot, int i)
{
    DP_ASSERT(snapshot);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < snapshot->message_count);
    return DP_message_incref_nullable(snapshot->messages[i]);
}

void DP_player_index_entry_snapshot_free(DP_PlayerIndexEntrySnapshot *snapshot)
{
    if (snapshot) {
        int message_count = snapshot->message_count;
        for (int i = 0; i < message_count; ++i) {
            DP_message_decref_nullable(snapshot->messages[i]);
        }
        DP_canvas_state_decref_nullable(snapshot->cs);
        DP_free(snapshot);
    }
}


unsigned int DP_player_index_message_count(DP_Player *player)
{
    DP_ASSERT(player);
    return check_index(player) ? player->index.message_count : 0;
}

size_t DP_player_index_entry_count(DP_Player *player)
{
    DP_ASSERT(player);
    return check_index(player) ? player->index.entry_count : 0;
}

DP_PlayerIndexEntry DP_player_index_entry_search(DP_Player *player,
                                                 long long position, bool after)
{
    DP_ASSERT(player);
    if (!check_index(player)) {
        return (DP_PlayerIndexEntry){0, 0, 0, 0};
    }

    size_t entry_count = player->index.entry_count;
    DP_PlayerIndexEntry best_entry = (DP_PlayerIndexEntry){0, 0, 0, 0};
    long long best_message_index = -1;
    for (size_t i = 0; i < entry_count; ++i) {
        DP_PlayerIndexEntry entry = player->index.entries[i];
        long long message_index = entry.message_index;
        bool is_best_candidate = after
                                   ? (message_index >= position
                                      && (message_index < best_message_index
                                          || best_message_index == -1))
                                   : (message_index <= position
                                      && message_index > best_message_index);
        if (is_best_candidate) {
            best_entry = entry;
            best_message_index = message_index;
        }
    }
    // If we didn't find any index, rewind back to the beginning of the
    // recording. Which isn't at offset 0, since there's a header to skip.
    if (best_message_index == -1) {
        best_entry.message_offset = player_body_offset(player);
    }
    return best_entry;
}

static void assign_out_error(bool error, bool *out_error)
{
    if (out_error) {
        *out_error = error;
    }
}

DP_Image *DP_player_index_thumbnail_at(DP_Player *player, size_t index,
                                       bool *out_error)
{
    DP_ASSERT(player);
    if (!check_index(player)) {
        assign_out_error(true, out_error);
        return NULL;
    }

    size_t entry_count = player->index.entry_count;
    if (index >= entry_count) {
        DP_error_set("Entry index %zu beyond count %zu", index, entry_count);
        assign_out_error(true, out_error);
        return NULL;
    }

    size_t thumbnail_offset = player->index.entries[index].thumbnail_offset;
    if (thumbnail_offset == 0) {
        assign_out_error(false, out_error);
        return NULL;
    }

    DP_BufferedInput *input = &player->index.input;
    if (!DP_buffered_input_seek(input, thumbnail_offset)) {
        assign_out_error(true, out_error);
        return NULL;
    }

    bool error;
    size_t read = DP_buffered_input_read(input, sizeof(uint32_t), &error);
    if (error) {
        assign_out_error(true, out_error);
        return NULL;
    }
    else if (read != sizeof(uint32_t)) {
        DP_error_set("Premature end of thumbnail length");
        assign_out_error(true, out_error);
        return NULL;
    }

    size_t length = DP_read_littleendian_uint32(input->buffer);
    if (length == 0) {
        DP_error_set("Thumbnail has zero length");
        assign_out_error(true, out_error);
        return NULL;
    }

    read = DP_buffered_input_read(input, length, &error);
    if (error) {
        assign_out_error(true, out_error);
        return NULL;
    }
    else if (read != length) {
        DP_error_set("Premature end of thumbnail data");
        assign_out_error(true, out_error);
        return NULL;
    }

    DP_Input *mem_input = DP_mem_input_new(input->buffer, length, NULL, NULL);
    DP_Image *img = DP_image_read_png(mem_input);
    DP_input_free(mem_input);

    assign_out_error(img == NULL, out_error);
    return img;
}
