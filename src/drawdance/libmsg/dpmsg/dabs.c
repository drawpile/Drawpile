// SPDX-License-Identifier: GPL-3.0-or-later
#include "dabs.h"
#include "blend_mode.h"
#include "message.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/vector.h>

static_assert(DP_PAINT_MODE_COUNT == 4, "Paint modes fit into two bits");

#define DP_INT24_MIN  (-8388608)
#define DP_INT24_MAX  (8388607)
#define DP_UINT24_MAX (16777215)

typedef union DP_DabBufferDab {
    DP_ClassicDabX classic;
} DP_DabBufferDab;

struct DP_DabBuffer {
    int type;
    unsigned int context_id;
    int paint_mode;
    int layer_id;
    int blend_mode;
    int selection_id;
    DP_Vector dabs;
    size_t payload_size;
};

typedef enum DP_DabDeltaSize {
    DP_DAB_DELTA_0,
    DP_DAB_DELTA_8,
    DP_DAB_DELTA_16,
    DP_DAB_DELTA_24,
    DP_DAB_DELTA_EXCEEDED,
} DP_DabDeltaSize;


static void init_buffer(DP_DabBuffer *db, int type, unsigned int context_id,
                        int paint_mode, int layer_id, int blend_mode,
                        int selection_id)
{
    db->type = type;
    db->context_id = context_id;
    db->paint_mode = paint_mode;
    db->layer_id = layer_id;
    db->blend_mode = blend_mode;
    db->selection_id = selection_id;
}

static void flush_buffer(DP_DabBuffer *db, DP_DabBufferFlushFn flush,
                         void *user)
{
    if (db->payload_size != 0) {
        DP_Message *msg = NULL;
        flush(user, msg);
    }
}

static DP_DabDeltaSize get_delta_size(long long a, long long b)
{
    long long delta = a < b ? b - a : a - b;
    if (delta == 0LL) {
        return DP_DAB_DELTA_0;
    }
    else if (delta >= INT8_MIN && delta <= INT8_MAX) {
        return DP_DAB_DELTA_8;
    }
    else if (delta >= INT16_MIN && delta <= INT16_MAX) {
        return DP_DAB_DELTA_16;
    }
    else if (delta >= DP_INT24_MIN && delta <= DP_INT24_MAX) {
        return DP_DAB_DELTA_24;
    }
    else {
        return DP_DAB_DELTA_EXCEEDED;
    }
}

static DP_DabDeltaSize delta_size_max(DP_DabDeltaSize a, DP_DabDeltaSize b)
{
    return a < b ? b : a;
}

static size_t count_dab_classic(DP_ClassicDabX prev, DP_ClassicDabX next)
{
    size_t payload_size = 1;

    if (prev.color != next.color) {
        payload_size += 3;
    }

    switch (delta_size_max(get_delta_size(prev.x, next.x),
                           get_delta_size(prev.y, next.y))) {
    case DP_DAB_DELTA_0:
        break;
    case DP_DAB_DELTA_8:
        payload_size += 2;
        break;
    case DP_DAB_DELTA_16:
        payload_size += 4;
        break;
    default:
        payload_size += 8;
        break;
    }

    switch (get_delta_size(prev.size, next.size)) {
    case DP_DAB_DELTA_0:
        break;
    case DP_DAB_DELTA_8:
        payload_size += 1;
        break;
    case DP_DAB_DELTA_16:
        payload_size += 2;
        break;
    case DP_DAB_DELTA_24:
        payload_size += 3;
        break;
    default:
        if (next.size <= UINT8_MAX) {
            payload_size += 1;
        }
        else if (next.size <= UINT16_MAX) {
            payload_size += 2;
        }
        else if (next.size <= DP_UINT24_MAX) {
            payload_size += 3;
        }
        else {
            payload_size += 4;
        }
        break;
    }

    if (prev.opacity != next.opacity) {
        payload_size += 1;
    }

    if (prev.hardness != next.hardness) {
        payload_size += 1;
    }

    return payload_size;
}

void DP_dab_buffer_push_classic(DP_DabBuffer *db, unsigned int context_id,
                                int paint_mode, int layer_id, int blend_mode,
                                int selection_id, uint32_t color, float x,
                                float y, float size, float opacity,
                                float hardness, DP_DabBufferFlushFn flush,
                                void *user)
{
    DP_ASSERT(db);
    DP_ASSERT(flush);
    DP_ClassicDabX dab = {};
    if (db->dabs.used != 0 && db->type == DP_MSG_DRAW_DABS_CLASSIC
        && db->context_id == context_id && db->paint_mode == paint_mode
        && db->layer_id == layer_id && db->blend_mode == blend_mode
        && db->selection_id == selection_id) {
        DP_ClassicDabX prev_dab =
            DP_VECTOR_LAST_TYPE(&db->dabs, DP_DabBufferDab).classic;
    }

    flush_buffer(db, flush, user);
    init_buffer(db, DP_MSG_DRAW_DABS_CLASSIC, context_id, paint_mode, layer_id,
                blend_mode, selection_id);
    db->payload_size =
        1 + 2 + 1 // flags, layer id, blend mode
        + (paint_mode == DP_PAINT_MODE_DIRECT ? 0 : 1) // indirect alpha
        + ((selection_id & 0xff) == 0 ? 1 : 0);        // selection id

    if (db->payload_size == 0 || db->type != DP_MSG_DRAW_DABS_CLASSIC
        || db->context_id != context_id || db->paint_mode != paint_mode
        || db->layer_id != layer_id || db->blend_mode != blend_mode
        || db->selection_id != selection_id) {
        flush_buffer(db, flush, user);
        init_buffer(db, DP_MSG_DRAW_DABS_CLASSIC, context_id, paint_mode,
                    layer_id, blend_mode, selection_id);
        db->payload_size =
            1 + 2 + 1 // flags, layer id, blend mode
            + (paint_mode == DP_PAINT_MODE_DIRECT ? 0 : 1) // indirect alpha
            + ((selection_id & 0xff) == 0 ? 1 : 0);        // selection id
    }
}
