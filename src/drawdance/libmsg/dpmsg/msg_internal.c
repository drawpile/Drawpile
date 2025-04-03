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
#include "msg_internal.h"
#include "message.h"
#include "text_writer.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


struct DP_MsgInternal {
    // The alignment attribute is required to avoid warnings about casts
    // changing the alignment requirements. I think they're false positives, but
    // this stuff is maximally aligned anyway, so the attribute does no harm.
    alignas(DP_max_align_t) DP_MsgInternalType type;
};

typedef struct DP_MsgInternalResetToState {
    DP_MsgInternal parent;
    void *data;
} DP_MsgInternalResetToState;

typedef struct DP_MsgInternalCatchup {
    DP_MsgInternal parent;
    int progress;
} DP_MsgInternalCatchup;

typedef struct DP_MsgInternalPreview {
    DP_MsgInternal parent;
    int type;
    void *data;
} DP_MsgInternalPreview;

typedef struct DP_MsgInternalPlayback {
    DP_MsgInternal parent;
    long long position;
} DP_MsgInternalPlayback;

typedef struct DP_MsgInternalDumpPlayback {
    DP_MsgInternal parent;
    long long position;
} DP_MsgInternalDumpPlayback;

typedef struct DP_MsgInternalDumpCommand {
    DP_MsgInternal parent;
    int type;
    int count;
    DP_Message *messages[];
} DP_MsgInternalDumpCommand;

typedef struct DP_MsgInternalStreamResetStart {
    DP_MsgInternal parent;
    size_t correlator_length;
    char correlator[];
} DP_MsgInternalStreamResetStart;

static size_t payload_length(DP_UNUSED DP_Message *msg)
{
    DP_warn("DP_MsgInternal: payload_length called on internal message");
    return 0;
}

static size_t serialize_payload(DP_UNUSED DP_Message *msg,
                                DP_UNUSED unsigned char *data)
{
    DP_warn("DP_MsgInternal: serialize_payload called on internal message");
    return 0;
}

static bool write_payload_text(DP_Message *msg, DP_TextWriter *writer)
{
    DP_MsgInternal *mi = DP_msg_internal_cast(msg);
    DP_ASSERT(mi);
    return DP_text_writer_write_int(writer, "type", (int)mi->type);
}

static bool equals(DP_Message *DP_RESTRICT msg, DP_Message *DP_RESTRICT other)
{
    DP_MsgInternal *a = DP_msg_internal_cast(msg);
    DP_ASSERT(a);
    DP_MsgInternal *b = DP_msg_internal_cast(other);
    DP_ASSERT(b);
    return a->type == b->type;
}

static const DP_MessageMethods methods = {
    payload_length,
    serialize_payload,
    write_payload_text,
    equals,
};

static DP_Message *msg_internal_new(unsigned int context_id,
                                    DP_MsgInternalType internal_type,
                                    size_t size)
{
    DP_MsgInternal *mi;
    DP_Message *msg =
        DP_message_new(DP_MSG_INTERNAL, context_id, &methods, size);
    mi = DP_message_internal(msg);
    mi->type = internal_type;
    return msg;
}

DP_Message *DP_msg_internal_reset_new(unsigned int context_id)
{
    return msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_RESET,
                            sizeof(DP_MsgInternal));
}

DP_Message *DP_msg_internal_reset_to_state_new(unsigned int context_id,
                                               void *data)
{
    DP_Message *msg =
        msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_RESET_TO_STATE,
                         sizeof(DP_MsgInternalResetToState));
    DP_MsgInternalResetToState *mirst = DP_message_internal(msg);
    mirst->data = data;
    return msg;
}

DP_Message *DP_msg_internal_snapshot_new(unsigned int context_id)
{
    return msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_SNAPSHOT,
                            sizeof(DP_MsgInternal));
}

DP_Message *DP_msg_internal_catchup_new(unsigned int context_id, int progress)
{
    DP_Message *msg = msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_CATCHUP,
                                       sizeof(DP_MsgInternalCatchup));
    DP_MsgInternalCatchup *mic = DP_message_internal(msg);
    mic->progress = progress;
    return msg;
}

DP_Message *DP_msg_internal_cleanup_new(unsigned int context_id)
{
    return msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_CLEANUP,
                            sizeof(DP_MsgInternal));
}

DP_Message *DP_msg_internal_preview_new(unsigned int context_id, int type,
                                        void *data)
{
    DP_Message *msg = msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_PREVIEW,
                                       sizeof(DP_MsgInternalPreview));
    DP_MsgInternalPreview *mip = DP_message_internal(msg);
    mip->type = type;
    mip->data = data;
    return msg;
}

DP_Message *DP_msg_internal_recorder_sync_new(unsigned int context_id)
{
    return msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_RECORDER_SYNC,
                            sizeof(DP_MsgInternal));
}

DP_Message *DP_msg_internal_playback_new(unsigned int context_id,
                                         long long position)
{
    DP_Message *msg =
        msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_PLAYBACK,
                         sizeof(DP_MsgInternalPlayback));
    DP_MsgInternalPlayback *mip = DP_message_internal(msg);
    mip->position = position;
    return msg;
}

DP_Message *DP_msg_internal_dump_playback_new(unsigned int context_id,
                                              long long position)
{
    DP_Message *msg =
        msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_DUMP_PLAYBACK,
                         sizeof(DP_MsgInternalDumpPlayback));
    DP_MsgInternalDumpPlayback *mip = DP_message_internal(msg);
    mip->position = position;
    return msg;
}

DP_Message *DP_msg_internal_dump_command_new_inc(unsigned int context_id,
                                                 int type, int count,
                                                 DP_Message **messages)
{
    DP_Message *msg =
        msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_DUMP_COMMAND,
                         DP_FLEX_SIZEOF(DP_MsgInternalDumpCommand, messages,
                                        DP_int_to_size(count)));
    DP_MsgInternalDumpCommand *midc = DP_message_internal(msg);
    midc->type = type;
    midc->count = count;
    for (int i = 0; i < count; ++i) {
        midc->messages[i] = DP_message_incref(messages[i]);
    }
    return msg;
}

DP_Message *DP_msg_internal_local_fork_clear_new(unsigned int context_id)
{
    return msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_LOCAL_FORK_CLEAR,
                            sizeof(DP_MsgInternal));
}

DP_Message *DP_msg_internal_flush_new(unsigned int context_id)
{
    return msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_FLUSH,
                            sizeof(DP_MsgInternal));
}

DP_Message *DP_msg_internal_stream_reset_start_new(unsigned int context_id,
                                                   size_t correlator_length,
                                                   const char *correlator)
{
    DP_ASSERT(correlator || correlator_length == 0);
    DP_Message *msg =
        msg_internal_new(context_id, DP_MSG_INTERNAL_TYPE_STREAM_RESET_START,
                         DP_FLEX_SIZEOF(DP_MsgInternalStreamResetStart,
                                        correlator, correlator_length + 1));
    DP_MsgInternalStreamResetStart *misrs = DP_message_internal(msg);
    misrs->correlator_length = correlator_length;
    if (correlator_length != 0) {
        memcpy(misrs->correlator, correlator, correlator_length);
    }
    misrs->correlator[correlator_length] = '\0';
    return msg;
}


DP_MsgInternal *DP_msg_internal_cast(DP_Message *msg)
{
    return DP_message_cast(msg, DP_MSG_INTERNAL);
}


DP_MsgInternalType DP_msg_internal_type(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    return mi->type;
}

void *DP_msg_internal_reset_to_state_data(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_RESET_TO_STATE);
    return ((DP_MsgInternalResetToState *)mi)->data;
}

int DP_msg_internal_catchup_progress(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_CATCHUP);
    return ((DP_MsgInternalCatchup *)mi)->progress;
}

int DP_msg_internal_preview_type(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_PREVIEW);
    return ((DP_MsgInternalPreview *)mi)->type;
}

void *DP_msg_internal_preview_data(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_PREVIEW);
    return ((DP_MsgInternalPreview *)mi)->data;
}

long long DP_msg_internal_playback_position(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_PLAYBACK);
    return ((DP_MsgInternalPlayback *)mi)->position;
}

long long DP_msg_internal_dump_playback_position(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_DUMP_PLAYBACK);
    return ((DP_MsgInternalDumpPlayback *)mi)->position;
}

int DP_msg_internal_dump_command_type(DP_MsgInternal *mi)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_DUMP_COMMAND);
    return ((DP_MsgInternalDumpCommand *)mi)->type;
}

DP_Message **DP_msg_internal_dump_command_messages(DP_MsgInternal *mi,
                                                   int *out_count)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_DUMP_COMMAND);
    DP_MsgInternalDumpCommand *midc = (DP_MsgInternalDumpCommand *)mi;
    if (out_count) {
        *out_count = midc->count;
    }
    return midc->messages;
}

const char *DP_msg_internal_stream_reset_start_correlator(DP_MsgInternal *mi,
                                                          size_t *out_length)
{
    DP_ASSERT(mi);
    DP_ASSERT(mi->type == DP_MSG_INTERNAL_TYPE_STREAM_RESET_START);
    DP_MsgInternalStreamResetStart *misrs =
        (DP_MsgInternalStreamResetStart *)mi;
    if (out_length) {
        *out_length = misrs->correlator_length;
    }
    return misrs->correlator;
}
