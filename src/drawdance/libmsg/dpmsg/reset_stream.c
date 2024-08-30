// SPDX-License-Identifier: GPL-3.0-or-later
#include "reset_stream.h"
#include "message.h"
#include "message_queue.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/vector.h>
#include <zlib.h>


struct DP_ResetStreamProducer {
    z_stream stream;
    size_t image_size;
    DP_Vector msgs;
    struct {
        size_t capacity;
        size_t used;
        unsigned char *buffer;
    } in;
    struct {
        unsigned char buffer[DP_MSG_RESET_STREAM_DATA_MAX_SIZE];
    } out;
};


static voidpf malloc_z(DP_UNUSED voidpf opaque, uInt items, uInt size)
{
    return DP_malloc((size_t)items * (size_t)size);
}

static void free_z(DP_UNUSED voidpf opaque, voidpf address)
{
    DP_free(address);
}

static const char *get_z_error(z_stream *stream)
{
    const char *msg = stream->msg;
    return msg ? msg : "no error message";
}


static const char *reset_stream_producer_z_error(DP_ResetStreamProducer *rsp)
{
    return get_z_error(&rsp->stream);
}

DP_ResetStreamProducer *DP_reset_stream_producer_new(void)
{
    DP_ResetStreamProducer *rsp = DP_malloc(sizeof(*rsp));
    *rsp = (DP_ResetStreamProducer){{Z_NULL, 0, 0, Z_NULL, 0, 0, Z_NULL, Z_NULL,
                                     malloc_z, free_z, rsp, Z_BINARY, 0, 0},
                                    0,
                                    DP_VECTOR_NULL,
                                    {0, 0, NULL},
                                    {{0}}};
    int ret = deflateInit(&rsp->stream, 9);
    if (ret != Z_OK) {
        DP_error_set("Deflate init error %d: %s", ret,
                     reset_stream_producer_z_error(rsp));
        DP_free(rsp);
        return NULL;
    }
    rsp->stream.avail_out = (uInt)sizeof(rsp->out.buffer);
    rsp->stream.next_out = rsp->out.buffer;
    DP_message_vector_init(&rsp->msgs, 1024);
    return rsp;
}

void DP_reset_stream_producer_free_discard(DP_ResetStreamProducer *rsp)
{
    if (rsp) {
        DP_free(rsp->in.buffer);
        DP_message_vector_dispose(&rsp->msgs);
        int ret = deflateEnd(&rsp->stream);
        if (ret != Z_OK) {
            DP_warn("Deflate end error %d: %s", ret,
                    reset_stream_producer_z_error(rsp));
        }
        DP_free(rsp);
    }
}

static void reset_stream_producer_set_reset_data(size_t size,
                                                 unsigned char *out, void *user)
{
    DP_ResetStreamProducer *rsp = user;
    memcpy(out, rsp->out.buffer, size);
}

static void reset_stream_producer_flush_to_message(DP_ResetStreamProducer *rsp)
{
    size_t size = sizeof(rsp->out.buffer) - rsp->stream.avail_out;
    if (size > 0) {
        DP_Message *msg = DP_msg_reset_stream_new(
            0, reset_stream_producer_set_reset_data, size, rsp);
        DP_message_vector_push_noinc(&rsp->msgs, msg);
        rsp->stream.avail_out = (uInt)sizeof(rsp->out.buffer);
        rsp->stream.next_out = rsp->out.buffer;
    }
}

DP_Message **DP_reset_stream_producer_free_finish(DP_ResetStreamProducer *rsp,
                                                  int *out_count)
{
    DP_ASSERT(rsp);

    while (true) {
        rsp->stream.avail_in = (uInt)rsp->in.used;
        rsp->stream.next_in = rsp->in.buffer;
        int ret = deflate(&rsp->stream, Z_FINISH);
        if (ret == Z_STREAM_END) {
            reset_stream_producer_flush_to_message(rsp);
            break;
        }
        else if (ret == Z_OK) {
            reset_stream_producer_flush_to_message(rsp);
        }
        else {
            DP_error_set("Deflate finish error %d: %s", ret,
                         reset_stream_producer_z_error(rsp));
            DP_reset_stream_producer_free_discard(rsp);
            return NULL;
        }
    }

    DP_Message **msgs = rsp->msgs.elements;
    if (out_count) {
        *out_count = DP_size_to_int(rsp->msgs.used);
    }
    rsp->msgs = DP_VECTOR_NULL;

    DP_reset_stream_producer_free_discard(rsp);
    return msgs;
}

static unsigned char *stream_producer_get_serialize_buffer(void *user,
                                                           size_t size)
{
    DP_ResetStreamProducer *rsp = user;
    size_t used = rsp->in.used;
    size_t required_size = used + size;
    if (rsp->in.capacity < required_size) {
        DP_free(rsp->in.buffer);
        rsp->in.buffer = DP_malloc(required_size);
        rsp->in.capacity = required_size;
    }
    return rsp->in.buffer + used;
}

size_t DP_reset_stream_producer_image_size(DP_ResetStreamProducer *rsp)
{
    DP_ASSERT(rsp);
    return rsp->image_size;
}

bool DP_reset_stream_producer_push(DP_ResetStreamProducer *rsp, DP_Message *msg)
{
    DP_ASSERT(rsp);
    DP_ASSERT(msg);

    size_t size = DP_message_serialize(
        msg, true, stream_producer_get_serialize_buffer, rsp);
    if (size == 0) {
        return false;
    }

    rsp->image_size += size;
    rsp->in.used += size;
    rsp->stream.avail_in = (uInt)rsp->in.used;
    rsp->stream.next_in = rsp->in.buffer;

    while (true) {
        DP_ASSERT(rsp->stream.avail_out != 0);
        int ret = deflate(&rsp->stream, Z_NO_FLUSH);
        if (ret != Z_OK) {
            DP_error_set("Deflate error %d: %s", ret,
                         reset_stream_producer_z_error(rsp));
            return false;
        }

        if (rsp->stream.avail_out == 0) {
            reset_stream_producer_flush_to_message(rsp);
            unsigned int pending;
            ret = deflatePending(&rsp->stream, &pending, Z_NULL);
            if (ret != Z_OK) {
                DP_error_set("Deflate pending error %d: %s", ret,
                             reset_stream_producer_z_error(rsp));
                return false;
            }
            else if (pending == 0) {
                break;
            }
        }
        else {
            break;
        }
    }

    size_t used = rsp->in.used;
    size_t remaining = rsp->stream.avail_in;
    rsp->in.used = remaining;
    if (remaining != 0) {
        memmove(rsp->in.buffer, rsp->in.buffer + used - remaining, remaining);
    }
    return true;
}


struct DP_ResetStreamConsumer {
    z_stream stream;
    DP_ResetStreamConsumerMessageFn fn;
    void *user;
    bool decode_opaque;
    unsigned char buffer[((size_t)DP_MESSAGE_HEADER_LENGTH
                          + (size_t)DP_MESSAGE_MAX_PAYLOAD_LENGTH)
                         * (size_t)2];
};


static const char *reset_stream_consumer_z_error(DP_ResetStreamConsumer *rsc)
{
    return get_z_error(&rsc->stream);
}

DP_ResetStreamConsumer *
DP_reset_stream_consumer_new(DP_ResetStreamConsumerMessageFn fn, void *user,
                             bool decode_opaque)
{
    DP_ASSERT(fn);
    DP_ResetStreamConsumer *rsc = DP_malloc(sizeof(*rsc));
    *rsc = (DP_ResetStreamConsumer){{Z_NULL, 0, 0, rsc->buffer,
                                     sizeof(rsc->buffer), 0, Z_NULL, Z_NULL,
                                     malloc_z, free_z, rsc, 0, 0, 0},
                                    fn,
                                    user,
                                    decode_opaque,
                                    {0}};
    int ret = inflateInit(&rsc->stream);
    if (ret != Z_OK) {
        DP_error_set("Inflate init error %d: %s", ret,
                     reset_stream_consumer_z_error(rsc));
        DP_free(rsc);
        return NULL;
    }
    return rsc;
}

void DP_reset_stream_consumer_free_discard(DP_ResetStreamConsumer *rsc)
{
    if (rsc) {
        int ret = inflateEnd(&rsc->stream);
        if (ret != Z_OK) {
            DP_warn("Inflate end error %d: %s", ret,
                    reset_stream_consumer_z_error(rsc));
        }
        DP_free(rsc);
    }
}

static bool reset_stream_consumer_flush_messages(DP_ResetStreamConsumer *rsc)
{
    size_t size = sizeof(rsc->buffer) - (size_t)rsc->stream.avail_out;
    size_t remaining = size;
    size_t offset = 0;
    bool ok = true;
    while (remaining >= DP_MESSAGE_HEADER_LENGTH) {
        size_t payload_length = DP_read_bigendian_uint16(rsc->buffer + offset);
        size_t message_length = DP_MESSAGE_HEADER_LENGTH + payload_length;
        if (remaining >= message_length) {
            DP_Message *msg = DP_message_deserialize(
                rsc->buffer + offset, remaining, rsc->decode_opaque);
            remaining -= message_length;
            offset += message_length;
            ok = msg && rsc->fn(rsc->user, msg);
        }
        else {
            break;
        }
    }

    if (offset != 0) {
        memmove(rsc->buffer, rsc->buffer + offset, remaining);
        rsc->stream.avail_out = (uInt)(sizeof(rsc->buffer) - remaining);
        rsc->stream.next_out = rsc->buffer + remaining;
    }

    return ok;
}

bool DP_reset_stream_consumer_free_finish(DP_ResetStreamConsumer *rsc)
{
    rsc->stream.next_in = Z_NULL;
    rsc->stream.avail_in = 0;

    while (true) {
        int ret = inflate(&rsc->stream, Z_FINISH);
        if (ret == Z_STREAM_END) {
            if (!reset_stream_consumer_flush_messages(rsc)) {
                DP_reset_stream_consumer_free_discard(rsc);
                return false;
            }
            break;
        }
        else if (ret == Z_OK) {
            if (!reset_stream_consumer_flush_messages(rsc)) {
                DP_reset_stream_consumer_free_discard(rsc);
                return false;
            }
        }
        else {
            DP_error_set("Inflate finish error %d: %s", ret,
                         reset_stream_consumer_z_error(rsc));
            DP_reset_stream_consumer_free_discard(rsc);
            return false;
        }
    }

    if (rsc->stream.avail_out != sizeof(rsc->buffer)) {
        DP_error_set("Leftover data from inflate");
        DP_reset_stream_consumer_free_discard(rsc);
        return false;
    }

    DP_reset_stream_consumer_free_discard(rsc);
    return true;
}

bool DP_reset_stream_consumer_push(DP_ResetStreamConsumer *rsc,
                                   const void *data, size_t size)
{
    if (data && size != 0) {
        rsc->stream.next_in = (Bytef *)data;
        rsc->stream.avail_in = (uInt)size;
        do {
            int ret = inflate(&rsc->stream, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                DP_warn("Inflate error %d: %s", ret,
                        reset_stream_consumer_z_error(rsc));
                return false;
            }

            if (!reset_stream_consumer_flush_messages(rsc)) {
                return false;
            }
        } while (rsc->stream.avail_in > 0);
    }
    return true;
}
