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
#include "canvas_history.h"
#include "dump_reader.h"
#include "image.h"
#include "local_state.h"
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
#include <dpmsg/protover.h>
#include <dpmsg/text_reader.h>
#include <ctype.h>
#include <parson.h>

#define INDEX_EXTENSION "dpidx"

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
    DP_ProtocolVersion *protover;
#ifdef DP_PROTOCOL_COMPAT_VERSION
    bool compatibility_mode;
#endif
    bool acl_override;
    unsigned char pass;
    bool input_error;
    bool end;
    DP_PlayerIndex index;
};


static void player_index_dispose(DP_PlayerIndex *pi)
{
    DP_ASSERT(pi);
    DP_free(pi->entries);
    DP_buffered_input_dispose(&pi->input);
    *pi = (DP_PlayerIndex){DP_BUFFERED_INPUT_NULL, 0, NULL, 0};
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

static DP_Player *make_player(DP_PlayerType type, char *recording_path,
                              char *index_path, DP_PlayerReader reader,
                              char *version, JSON_Value *header_value)
{
    DP_ProtocolVersion *protover = DP_protocol_version_parse(version);
    DP_free(version);
    DP_Player *player = DP_malloc(sizeof(*player));
    *player =
        (DP_Player){recording_path,
                    index_path,
                    type,
                    reader,
                    DP_acl_state_new_playback(),
                    header_value,
                    0,
                    protover,
#ifdef DP_PROTOCOL_COMPAT_VERSION
                    type == DP_PLAYER_TYPE_BINARY
                        && DP_protocol_version_is_past_compatible(protover),
#endif
                    false,
                    DP_PLAYER_PASS_CLIENT_PLAYBACK,
                    false,
                    false,
                    {DP_BUFFERED_INPUT_NULL, 0, NULL, 0}};
    return player;
}

static DP_Player *new_binary_player(char *recording_path, char *index_path,
                                    DP_Input *input)
{
    DP_BinaryReader *binary_reader = DP_binary_reader_new(input, 0);
    if (!binary_reader) {
        return NULL;
    }

    JSON_Object *header =
        json_value_get_object(DP_binary_reader_header(binary_reader));
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
                          NULL,
#ifdef DP_PROTOCOL_COMPAT_VERSION
                          false,
#endif
                          false,
                          DP_PLAYER_PASS_CLIENT_PLAYBACK,
                          false,
                          false,
                          {DP_BUFFERED_INPUT_NULL, 0, NULL, 0}};
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
            break;
        default:
            break;
        }
        DP_protocol_version_free(player->protover);
        DP_free(player->index_path);
        DP_free(player->recording_path);
        DP_free(player);
    }
}

DP_PlayerType DP_player_type(DP_Player *player)
{
    DP_ASSERT(player);
    return player->type;
}

JSON_Value *DP_player_header(DP_Player *player)
{
    DP_ASSERT(player);
    switch (player->type) {
    case DP_PLAYER_TYPE_BINARY:
        return DP_binary_reader_header(player->reader.binary);
    case DP_PLAYER_TYPE_TEXT:
        return player->text_reader_header_value;
    case DP_PLAYER_TYPE_DEBUG_DUMP:
        return NULL;
    default:
        DP_UNREACHABLE();
    }
}

DP_PlayerCompatibility DP_player_compatibility(DP_Player *player)
{
    DP_ASSERT(player);
    DP_ASSERT(player->type != DP_PLAYER_TYPE_DEBUG_DUMP);
    DP_ProtocolVersion *protover = player->protover;
    DP_ProtocolCompatibility compat =
        protover ? DP_protocol_version_client_compatibility(player->protover)
                 : DP_PROTOCOL_COMPATIBILITY_INCOMPATIBLE;
    switch (compat) {
    case DP_PROTOCOL_COMPATIBILITY_COMPATIBLE:
        return DP_PLAYER_COMPATIBLE;
    case DP_PROTOCOL_COMPATIBILITY_MINOR_INCOMPATIBILITY:
        return DP_PLAYER_MINOR_INCOMPATIBILITY;
#ifdef DP_PROTOCOL_COMPAT_VERSION
    case DP_PROTOCOL_COMPATIBILITY_BACKWARD_COMPATIBLE:
        return player->type == DP_PLAYER_TYPE_BINARY
                 ? DP_PLAYER_BACKWARD_COMPATIBLE
                 : DP_PLAYER_INCOMPATIBLE;
#endif
    default:
        return DP_PLAYER_INCOMPATIBLE;
    }
}

bool DP_player_compatible(DP_Player *player)
{
    DP_ASSERT(player);
    switch (DP_player_compatibility(player)) {
    case DP_PLAYER_COMPATIBLE:
    case DP_PLAYER_MINOR_INCOMPATIBILITY:
#ifdef DP_PROTOCOL_COMPAT_VERSION
    case DP_PLAYER_BACKWARD_COMPATIBLE:
#endif
        return true;
    default:
        return false;
    }
}

void DP_player_acl_override_set(DP_Player *player, bool override)
{
    DP_ASSERT(player);
    player->acl_override = override;
}

void DP_player_pass_set(DP_Player *player, DP_PlayerPass pass)
{
    DP_ASSERT(player);
    player->pass = (unsigned char)pass;
}

const char *DP_player_recording_path(DP_Player *player)
{
    DP_ASSERT(player);
    return player->recording_path;
}

const char *DP_player_index_path(DP_Player *player)
{
    DP_ASSERT(player);
    return player->index_path;
}

bool DP_player_index_loaded(DP_Player *player)
{
    DP_ASSERT(player);
    return player->index.entries;
}

DP_PlayerIndex *DP_player_index(DP_Player *player)
{
    DP_ASSERT(player);
    return &player->index;
}

void DP_player_index_set(DP_Player *player, DP_PlayerIndex index)
{
    DP_ASSERT(player);
    player_index_dispose(&player->index);
    player->index = index;
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
    DP_BinaryReaderResult (*read_fn)(DP_BinaryReader *reader,
                                     bool decode_opaque, DP_Message **out_msg);
#ifdef DP_PROTOCOL_COMPAT_VERSION
    read_fn = player->compatibility_mode ? DP_binary_reader_read_message_compat
                                         : DP_binary_reader_read_message;
#else
    read_fn = DP_binary_reader_read_message;
#endif
    switch (read_fn(player->reader.binary, true, out_msg)) {
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

static bool emit_message(DP_Message *msg, DP_Message **out_msg,
                         unsigned char pass)
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
    case DP_MSG_USER_ACL:
    case DP_MSG_DATA:
        if (pass < (unsigned char)DP_PLAYER_PASS_ALL) {
            return false;
        }
        else {
            break;
        }
    // Feature access levels are relevant for session templates, so they have an
    // extra pass-through level that allows them through.
    case DP_MSG_FEATURE_ACCESS_LEVELS:
        if (pass < (unsigned char)DP_PLAYER_PASS_FEATURE_ACCESS) {
            return false;
        }
        else {
            break;
        }
    // Layer ACL messages have a tier, which we want to retain, and a user
    // component, which we do not. So extract the former and leave the latter.
    case DP_MSG_LAYER_ACL:
        if (pass < (unsigned char)DP_PLAYER_PASS_ALL) {
            DP_MsgLayerAcl *mla = DP_message_internal(msg);
            *out_msg = DP_msg_layer_acl_new(
                DP_message_context_id(msg), DP_msg_layer_acl_id(mla),
                DP_msg_layer_acl_flags(mla), NULL, 0, NULL);
            DP_message_decref(msg);
            return true;
        }
        else {
            break;
        }
    default:
        break;
    }
    *out_msg = msg;
    return true;
}

static DP_PlayerResult step_valid_message(DP_Player *player,
                                          DP_Message **out_msg)
{
    while (true) {
        DP_Message *msg;
        DP_PlayerResult result = step_message(player, &msg);
        if (result == DP_PLAYER_SUCCESS) {
            bool filtered =
                DP_acl_state_handle(player->acls, msg, player->acl_override)
                & DP_ACL_STATE_FILTERED_BIT;
            if (filtered) {
                DP_debug(
                    "ACL filtered recorded %s message from user %u",
                    DP_message_type_enum_name_unprefixed(DP_message_type(msg)),
                    DP_message_context_id(msg));
            }
            else if (emit_message(msg, out_msg, player->pass)) {
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

size_t DP_player_body_offset(DP_Player *player)
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
    return DP_player_seek(player, 0, DP_player_body_offset(player));
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
