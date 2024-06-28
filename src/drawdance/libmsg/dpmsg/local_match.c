// SPDX-License-Identifier: GPL-3.0-or-later
#include "local_match.h"
#include "blend_mode.h"
#include "message.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


static const unsigned char *local_match_data(DP_Message *msg, size_t *out_size)
{
    switch (DP_message_type(msg)) {
    case DP_MSG_LOCAL_MATCH:
        return DP_msg_local_match_data(DP_message_internal(msg), out_size);
    case DP_MSG_PUT_IMAGE:
        return DP_msg_put_image_image(DP_message_internal(msg), out_size);
    default:
        DP_UNREACHABLE();
    }
}

static void extract_selection_ids(int layer_id, uint8_t *out_source_id,
                                  uint8_t *out_target_id)
{
    *out_source_id = DP_uint16_to_uint8(layer_id & 0xff);
    *out_target_id = DP_int_to_uint8((layer_id >> 8) & 0xff);
}


static_assert(DP_MSG_MOVE_REGION_STATIC_LENGTH == 50,
              "Need to update move region local match");

#define MATCH_MOVE_REGION_SIZE DP_MSG_MOVE_REGION_STATIC_LENGTH

static void set_move_region_data(DP_UNUSED size_t size, unsigned char *out,
                                 void *user)
{
    DP_ASSERT(size == MATCH_MOVE_REGION_SIZE);
    DP_MsgMoveRegion *mmr = user;
    DP_ASSERT(DP_msg_move_region_layer(mmr) == 0);
    size_t written = 0;
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_bx(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_by(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_bw(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_bh(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_x1(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_y1(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_x2(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_y2(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_x3(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_y3(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_x4(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_region_y4(mmr), out + written);
    written += DP_write_bigendian_uint16(
        DP_size_to_uint16(DP_msg_move_region_mask_size(mmr)), out + written);
    DP_ASSERT(written == size);
}

static bool match_move_region(DP_MsgMoveRegion *mmr,
                              DP_Message *local_match_msg)
{
    DP_ASSERT(DP_msg_move_region_layer(mmr) == 0);
    size_t size;
    const unsigned char *data = local_match_data(local_match_msg, &size);
    size_t mask_size = DP_msg_move_region_mask_size(mmr);
    return size == MATCH_MOVE_REGION_SIZE
        && DP_read_bigendian_int32(data) == DP_msg_move_region_bx(mmr)
        && DP_read_bigendian_int32(data + 4) == DP_msg_move_region_by(mmr)
        && DP_read_bigendian_int32(data + 8) == DP_msg_move_region_bw(mmr)
        && DP_read_bigendian_int32(data + 12) == DP_msg_move_region_bh(mmr)
        && DP_read_bigendian_int32(data + 16) == DP_msg_move_region_x1(mmr)
        && DP_read_bigendian_int32(data + 20) == DP_msg_move_region_y1(mmr)
        && DP_read_bigendian_int32(data + 24) == DP_msg_move_region_x2(mmr)
        && DP_read_bigendian_int32(data + 28) == DP_msg_move_region_y2(mmr)
        && DP_read_bigendian_int32(data + 32) == DP_msg_move_region_x3(mmr)
        && DP_read_bigendian_int32(data + 36) == DP_msg_move_region_y3(mmr)
        && DP_read_bigendian_int32(data + 40) == DP_msg_move_region_x4(mmr)
        && DP_read_bigendian_int32(data + 44) == DP_msg_move_region_y4(mmr)
        && DP_uint16_to_size(DP_read_bigendian_uint16(data + 48)) == mask_size;
}


static_assert(DP_MSG_MOVE_RECT_STATIC_LENGTH == 28,
              "Need to update move rect local match");

#define MATCH_MOVE_RECT_SIZE DP_MSG_MOVE_RECT_STATIC_LENGTH

static void set_move_rect_data(DP_UNUSED size_t size, unsigned char *out,
                               void *user)
{
    DP_ASSERT(size == MATCH_MOVE_RECT_SIZE);
    DP_MsgMoveRect *mmr = user;
    DP_ASSERT(DP_msg_move_rect_source(mmr) == 0);
    uint8_t source_id, target_id;
    extract_selection_ids(DP_msg_move_rect_layer(mmr), &source_id, &target_id);
    size_t written = 0;
    written += DP_write_bigendian_uint8(source_id, out + written);
    written += DP_write_bigendian_uint8(target_id, out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_rect_sx(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_rect_sy(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_rect_tx(mmr), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_move_rect_ty(mmr), out + written);
    written += DP_write_bigendian_int32(DP_msg_move_rect_w(mmr), out + written);
    written += DP_write_bigendian_int32(DP_msg_move_rect_h(mmr), out + written);
    written += DP_write_bigendian_uint16(
        DP_size_to_uint16(DP_msg_move_rect_mask_size(mmr)), out + written);
    DP_ASSERT(written == size);
}

static bool match_move_rect(DP_MsgMoveRect *mmr, DP_Message *local_match_msg)
{
    DP_ASSERT(DP_msg_move_rect_source(mmr) == 0);
    size_t size;
    const unsigned char *data = local_match_data(local_match_msg, &size);
    uint8_t source_id, target_id;
    extract_selection_ids(DP_msg_move_rect_layer(mmr), &source_id, &target_id);
    size_t mask_size = DP_msg_move_rect_mask_size(mmr);
    return size == MATCH_MOVE_RECT_SIZE
        && DP_read_bigendian_uint8(data) == source_id
        && DP_read_bigendian_uint8(data + 1) == target_id
        && DP_read_bigendian_int32(data + 2) == DP_msg_move_rect_sx(mmr)
        && DP_read_bigendian_int32(data + 6) == DP_msg_move_rect_sy(mmr)
        && DP_read_bigendian_int32(data + 10) == DP_msg_move_rect_tx(mmr)
        && DP_read_bigendian_int32(data + 14) == DP_msg_move_rect_ty(mmr)
        && DP_read_bigendian_int32(data + 18) == DP_msg_move_rect_w(mmr)
        && DP_read_bigendian_int32(data + 22) == DP_msg_move_rect_h(mmr)
        && DP_uint16_to_size(DP_read_bigendian_uint16(data + 26)) == mask_size;
}


static_assert(DP_MSG_TRANSFORM_REGION_STATIC_LENGTH == 53,
              "Need to update transform region local match");

#define MATCH_TRANSFORM_REGION_SIZE DP_MSG_TRANSFORM_REGION_STATIC_LENGTH

static void set_transform_region_data(DP_UNUSED size_t size, unsigned char *out,
                                      void *user)
{
    DP_ASSERT(size == MATCH_TRANSFORM_REGION_SIZE);
    DP_MsgTransformRegion *mtr = user;
    DP_ASSERT(DP_msg_transform_region_source(mtr) == 0);
    uint8_t source_id, target_id;
    extract_selection_ids(DP_msg_transform_region_layer(mtr), &source_id,
                          &target_id);
    size_t written = 0;
    written += DP_write_bigendian_uint8(source_id, out + written);
    written += DP_write_bigendian_uint8(target_id, out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_bx(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_by(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_bw(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_bh(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_x1(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_y1(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_x2(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_y2(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_x3(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_y3(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_x4(mtr),
                                        out + written);
    written += DP_write_bigendian_int32(DP_msg_transform_region_y4(mtr),
                                        out + written);
    written += DP_write_bigendian_uint8(DP_msg_transform_region_mode(mtr),
                                        out + written);
    written += DP_write_bigendian_uint16(
        DP_size_to_uint16(DP_msg_transform_region_mask_size(mtr)),
        out + written);
    DP_ASSERT(written == size);
}

static bool match_transform_region(DP_MsgTransformRegion *mtr,
                                   DP_Message *local_match_msg)
{
    DP_ASSERT(DP_msg_transform_region_source(mtr) == 0);
    size_t size;
    const unsigned char *data = local_match_data(local_match_msg, &size);
    uint8_t source_id, target_id;
    extract_selection_ids(DP_msg_transform_region_layer(mtr), &source_id,
                          &target_id);
    size_t mask_size = DP_msg_transform_region_mask_size(mtr);
    return size == MATCH_TRANSFORM_REGION_SIZE
        && DP_read_bigendian_uint8(data) == source_id
        && DP_read_bigendian_uint8(data + 1) == target_id
        && DP_read_bigendian_int32(data + 2) == DP_msg_transform_region_bx(mtr)
        && DP_read_bigendian_int32(data + 6) == DP_msg_transform_region_by(mtr)
        && DP_read_bigendian_int32(data + 10) == DP_msg_transform_region_bw(mtr)
        && DP_read_bigendian_int32(data + 14) == DP_msg_transform_region_bh(mtr)
        && DP_read_bigendian_int32(data + 18) == DP_msg_transform_region_x1(mtr)
        && DP_read_bigendian_int32(data + 22) == DP_msg_transform_region_y1(mtr)
        && DP_read_bigendian_int32(data + 26) == DP_msg_transform_region_x2(mtr)
        && DP_read_bigendian_int32(data + 30) == DP_msg_transform_region_y2(mtr)
        && DP_read_bigendian_int32(data + 34) == DP_msg_transform_region_x3(mtr)
        && DP_read_bigendian_int32(data + 38) == DP_msg_transform_region_y3(mtr)
        && DP_read_bigendian_int32(data + 42) == DP_msg_transform_region_x4(mtr)
        && DP_read_bigendian_int32(data + 46) == DP_msg_transform_region_y4(mtr)
        && DP_read_bigendian_uint8(data + 50)
               == DP_msg_transform_region_mode(mtr)
        && DP_uint16_to_size(DP_read_bigendian_uint16(data + 51)) == mask_size;
}


static_assert(DP_MSG_SELECTION_PUT_STATIC_LENGTH == 14,
              "Need to update selection put local match");

#define MATCH_SELECTION_PUT_SIZE DP_MSG_SELECTION_PUT_STATIC_LENGTH + 2

static void set_selection_put_data(DP_UNUSED size_t size, unsigned char *out,
                                   void *user)
{
    DP_ASSERT(size == MATCH_SELECTION_PUT_SIZE);
    DP_MsgSelectionPut *msp = user;
    size_t written = 0;
    written += DP_write_bigendian_uint8(DP_msg_selection_put_selection_id(msp),
                                        out + written);
    written +=
        DP_write_bigendian_uint8(DP_msg_selection_put_op(msp), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_selection_put_x(msp), out + written);
    written +=
        DP_write_bigendian_int32(DP_msg_selection_put_y(msp), out + written);
    written +=
        DP_write_bigendian_uint16(DP_msg_selection_put_w(msp), out + written);
    written +=
        DP_write_bigendian_uint16(DP_msg_selection_put_h(msp), out + written);
    written += DP_write_bigendian_uint16(
        DP_size_to_uint16(DP_msg_selection_put_mask_size(msp)), out + written);
    DP_ASSERT(written == size);
}

static bool match_selection_put(DP_MsgSelectionPut *msp,
                                DP_Message *local_match_msg)
{
    size_t size;
    const unsigned char *data = local_match_data(local_match_msg, &size);
    size_t mask_size = DP_msg_selection_put_mask_size(msp);
    return size == MATCH_SELECTION_PUT_SIZE
        && DP_read_bigendian_uint8(data)
               == DP_msg_selection_put_selection_id(msp)
        && DP_read_bigendian_uint8(data + 1) == DP_msg_selection_put_op(msp)
        && DP_read_bigendian_int32(data + 2) == DP_msg_selection_put_x(msp)
        && DP_read_bigendian_int32(data + 6) == DP_msg_selection_put_y(msp)
        && DP_read_bigendian_uint16(data + 10) == DP_msg_selection_put_w(msp)
        && DP_read_bigendian_uint16(data + 12) == DP_msg_selection_put_h(msp)
        && DP_uint16_to_size(DP_read_bigendian_uint16(data + 14)) == mask_size;
}


static_assert(DP_MSG_SELECTION_CLEAR_STATIC_LENGTH == 1,
              "Need to update selection clear local match");

#define MATCH_SELECTION_CLEAR_SIZE 1

static void set_selection_clear_data(DP_UNUSED size_t size, unsigned char *out,
                                     void *user)
{
    DP_ASSERT(size == MATCH_SELECTION_CLEAR_SIZE);
    DP_MsgSelectionClear *msc = user;
    out[0] = DP_msg_selection_clear_selection_id(msc);
}

static bool match_selection_clear(DP_MsgSelectionClear *msc,
                                  DP_Message *local_match_msg)
{
    size_t size;
    const unsigned char *data = local_match_data(local_match_msg, &size);
    return size == MATCH_SELECTION_CLEAR_SIZE
        && DP_read_bigendian_uint8(data)
               == DP_msg_selection_clear_selection_id(msc);
}


DP_Message *DP_msg_local_match_make(DP_Message *msg, bool disguise_as_put_image)
{
    void (*set_data)(size_t, unsigned char *, void *);
    size_t data_size;
    DP_MessageType type = DP_message_type(msg);
    switch (type) {
    case DP_MSG_MOVE_REGION:
        if (DP_msg_move_region_layer(DP_message_internal(msg)) == 0) {
            set_data = set_move_region_data;
            data_size = MATCH_MOVE_REGION_SIZE;
            break;
        }
        else {
            return NULL;
        }
    case DP_MSG_MOVE_RECT:
        if (DP_msg_move_rect_source(DP_message_internal(msg)) == 0) {
            set_data = set_move_rect_data;
            data_size = MATCH_MOVE_RECT_SIZE;
            break;
        }
        else {
            return NULL;
        }
    case DP_MSG_TRANSFORM_REGION:
        if (DP_msg_transform_region_source(DP_message_internal(msg)) == 0) {
            set_data = set_transform_region_data;
            data_size = MATCH_TRANSFORM_REGION_SIZE;
            break;
        }
        else {
            return NULL;
        }
    case DP_MSG_SELECTION_PUT:
        set_data = set_selection_put_data;
        data_size = MATCH_SELECTION_PUT_SIZE;
        break;
    case DP_MSG_SELECTION_CLEAR:
        set_data = set_selection_clear_data;
        data_size = MATCH_SELECTION_CLEAR_SIZE;
        break;
    default:
        return NULL;
    }
    unsigned int context_id = DP_message_context_id(msg);
    if (disguise_as_put_image) {
        return DP_msg_put_image_new(
            context_id, (uint8_t)type, DP_BLEND_MODE_COMPAT_LOCAL_MATCH, 0, 0,
            0, 0, set_data, data_size, DP_message_internal(msg));
    }
    else {
        return DP_msg_local_match_new(context_id, (uint8_t)type, set_data,
                                      data_size, DP_message_internal(msg));
    }
}

bool DP_msg_local_match_is_local_match(DP_Message *msg)
{
    switch (DP_message_type(msg)) {
    case DP_MSG_LOCAL_MATCH:
        return true;
    case DP_MSG_PUT_IMAGE:
        return DP_msg_put_image_mode(DP_message_internal(msg))
            == DP_BLEND_MODE_COMPAT_LOCAL_MATCH;
    default:
        return false;
    }
}

bool DP_msg_local_match_matches(DP_Message *msg, DP_Message *local_match_msg)
{
    DP_ASSERT(DP_msg_local_match_is_local_match(local_match_msg));
    switch (DP_message_type(msg)) {
    case DP_MSG_MOVE_REGION: {
        DP_MsgMoveRegion *mmr = DP_message_internal(msg);
        return DP_msg_move_region_layer(mmr) == 0
            && match_move_region(mmr, local_match_msg);
    }
    case DP_MSG_MOVE_RECT: {
        DP_MsgMoveRect *mmr = DP_message_internal(msg);
        return DP_msg_move_rect_source(mmr) == 0
            && match_move_rect(mmr, local_match_msg);
    }
    case DP_MSG_TRANSFORM_REGION: {
        DP_MsgTransformRegion *mtr = DP_message_internal(msg);
        return DP_msg_transform_region_source(mtr) == 0
            && match_transform_region(mtr, local_match_msg);
    }
    case DP_MSG_SELECTION_PUT:
        return match_selection_put(DP_message_internal(msg), local_match_msg);
    case DP_MSG_SELECTION_CLEAR:
        return match_selection_clear(DP_message_internal(msg), local_match_msg);
    default:
        return false;
    }
}
