// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPMSG_RESET_STREAM_H
#define DPMSG_RESET_STREAM_H
#include <dpcommon/common.h>

typedef struct DP_Message DP_Message;


typedef struct DP_ResetStreamProducer DP_ResetStreamProducer;

DP_ResetStreamProducer *DP_reset_stream_producer_new(bool compatibility_mode);

void DP_reset_stream_producer_free_discard(DP_ResetStreamProducer *rsp);

DP_Message **DP_reset_stream_producer_free_finish(DP_ResetStreamProducer *rsp,
                                                  int *out_count);

// Size of the reset image in bytes thus far.
size_t DP_reset_stream_producer_image_size(DP_ResetStreamProducer *rsp);

bool DP_reset_stream_producer_push(DP_ResetStreamProducer *rsp,
                                   DP_Message *msg);


typedef struct DP_ResetStreamConsumer DP_ResetStreamConsumer;

typedef bool (*DP_ResetStreamConsumerMessageFn)(void *user, DP_Message *msg);

DP_ResetStreamConsumer *
DP_reset_stream_consumer_new(DP_ResetStreamConsumerMessageFn fn, void *user,
                             bool decode_opaque);

void DP_reset_stream_consumer_free_discard(DP_ResetStreamConsumer *rsc);

bool DP_reset_stream_consumer_free_finish(DP_ResetStreamConsumer *rsc);

bool DP_reset_stream_consumer_push(DP_ResetStreamConsumer *rsc,
                                   const void *data, size_t size);


#endif
