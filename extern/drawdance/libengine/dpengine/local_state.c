// SPDX-License-Identifier: MIT
#include "local_state.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "pixels.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/vector.h>
#include <dpmsg/message.h>
#include <dpmsg/msg_internal.h>

#define ID_BOOL_SIZE (sizeof(int32_t) + sizeof(uint8_t))
#define INT_SIZE     sizeof(int32_t)

struct DP_LocalState {
    DP_Vector hidden_layer_ids;
    DP_Tile *background_tile;
    bool background_opaque;
    DP_ViewMode view_mode;
    int active_layer_id;
    int active_frame_index;
    DP_OnionSkins *onion_skins;
    DP_Vector track_states;
    DP_LocalStateViewInvalidatedFn view_invalidated;
    void *user;
};


static void apply_hidden_layers_recursive(DP_Vector *hidden_layers,
                                          DP_LayerPropsList *lpl)
{
    int count = DP_layer_props_list_count(lpl);
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (DP_layer_props_hidden(lp)) {
            int layer_id = DP_layer_props_id(lp);
            DP_VECTOR_PUSH_TYPE(hidden_layers, int, layer_id);
        }
        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        if (child_lpl) {
            apply_hidden_layers_recursive(hidden_layers, child_lpl);
        }
    }
}

static void apply_track_states(DP_Vector *track_states, DP_Timeline *tl)
{
    int track_count = DP_timeline_count(tl);
    for (int i = 0; i < track_count; ++i) {
        DP_Track *t = DP_timeline_at_noinc(tl, i);
        bool hidden = DP_track_hidden(t);
        bool onion_skin = DP_track_onion_skin(t);
        if (hidden || onion_skin) {
            DP_LocalTrackState lts = {DP_track_id(t), hidden, onion_skin};
            DP_VECTOR_PUSH_TYPE(track_states, DP_LocalTrackState, lts);
        }
    }
}

DP_LocalState *
DP_local_state_new(DP_CanvasState *cs_or_null,
                   DP_LocalStateViewInvalidatedFn view_invalidated, void *user)
{
    DP_LocalState *ls = DP_malloc(sizeof(*ls));
    *ls = (DP_LocalState){DP_VECTOR_NULL,
                          NULL,
                          false,
                          DP_VIEW_MODE_NORMAL,
                          0,
                          0,
                          NULL,
                          DP_VECTOR_NULL,
                          view_invalidated,
                          user};
    DP_VECTOR_INIT_TYPE(&ls->hidden_layer_ids, int, 8);
    DP_VECTOR_INIT_TYPE(&ls->track_states, DP_LocalTrackState, 8);
    if (cs_or_null) {
        DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs_or_null);
        apply_hidden_layers_recursive(&ls->hidden_layer_ids, lpl);
        apply_track_states(&ls->track_states,
                           DP_canvas_state_timeline_noinc(cs_or_null));
    }
    return ls;
}

void DP_local_state_free(DP_LocalState *ls)
{
    if (ls) {
        DP_vector_dispose(&ls->track_states);
        DP_onion_skins_free(ls->onion_skins);
        DP_vector_dispose(&ls->hidden_layer_ids);
        DP_free(ls);
    }
}


const int *DP_local_state_hidden_layer_ids(DP_LocalState *ls, int *out_count)
{
    DP_ASSERT(ls);
    if (out_count) {
        *out_count = DP_size_to_int(ls->hidden_layer_ids.used);
    }
    return ls->hidden_layer_ids.elements;
}

int DP_local_state_hidden_layer_id_count(DP_LocalState *ls)
{
    DP_ASSERT(ls);
    return DP_size_to_int(ls->hidden_layer_ids.used);
}

DP_Tile *DP_local_state_background_tile_noinc(DP_LocalState *ls)
{
    DP_ASSERT(ls);
    return ls->background_tile;
}

bool DP_local_state_background_opaque(DP_LocalState *ls)
{
    DP_ASSERT(ls);
    return ls->background_opaque;
}

DP_ViewMode DP_local_state_view_mode(DP_LocalState *ls)
{
    DP_ASSERT(ls);
    return ls->view_mode;
}

int DP_local_state_active_layer_id(DP_LocalState *ls)
{
    DP_ASSERT(ls);
    return ls->active_layer_id;
}

int DP_local_state_active_frame_index(DP_LocalState *ls)
{
    DP_ASSERT(ls);
    return ls->active_frame_index;
}

DP_ViewModeFilter DP_local_state_view_mode_filter_make(DP_LocalState *ls,
                                                       DP_ViewModeBuffer *vmb,
                                                       DP_CanvasState *cs)
{
    DP_ASSERT(ls);
    return DP_view_mode_filter_make(vmb, ls->view_mode, cs, ls->active_layer_id,
                                    ls->active_frame_index, ls->onion_skins);
}

const DP_LocalTrackState *DP_local_state_track_states(DP_LocalState *ls,
                                                      int *out_count)
{
    DP_ASSERT(ls);
    if (out_count) {
        *out_count = DP_size_to_int(ls->track_states.used);
    }
    return ls->track_states.elements;
}

int DP_local_state_track_state_count(DP_LocalState *ls)
{
    DP_ASSERT(ls);
    return DP_size_to_int(ls->track_states.used);
}


static bool is_hidden_layer_id(void *element, void *user)
{
    return *(int *)element == *(int *)user;
}

static void notify_view_invalidated(DP_LocalState *ls, bool check_all,
                                    int layer_id)
{
    DP_LocalStateViewInvalidatedFn fn = ls->view_invalidated;
    if (fn) {
        fn(ls->user, check_all, layer_id);
    }
}

static void set_layer_visibility(DP_LocalState *ls, int layer_id, bool hidden)
{
    DP_Vector *hidden_layer_ids = &ls->hidden_layer_ids;
    int index = DP_vector_search_index(hidden_layer_ids, sizeof(int),
                                       is_hidden_layer_id, &layer_id);
    if (hidden && index == -1) {
        DP_VECTOR_PUSH_TYPE(hidden_layer_ids, int, layer_id);
        notify_view_invalidated(ls, false, layer_id);
    }
    else if (!hidden && index != -1) {
        DP_VECTOR_REMOVE_TYPE(hidden_layer_ids, int, index);
        notify_view_invalidated(ls, false, layer_id);
    }
}

static bool read_id_bool_message(DP_MsgLocalChange *mlc, const char *title,
                                 int *out_id, bool *out_value)
{
    size_t size;
    const unsigned char *body = DP_msg_local_change_body(mlc, &size);
    if (size == ID_BOOL_SIZE) {
        *out_id = DP_read_bigendian_int32(body);
        *out_value = DP_read_bigendian_uint8(body + sizeof(int32_t));
        return true;
    }
    else {
        DP_warn("Wrong size for local %s change: wanted %zu, got %zu", title,
                ID_BOOL_SIZE, size);
        return false;
    }
}

static void handle_layer_visibility(DP_LocalState *ls, DP_MsgLocalChange *mlc)
{
    int layer_id;
    bool hidden;
    if (read_id_bool_message(mlc, "layer visibility", &layer_id, &hidden)) {
        set_layer_visibility(ls, layer_id, hidden);
    }
}

static void set_background_tile(DP_LocalState *ls, DP_Tile *tile_or_null)
{
    DP_Tile *prev = ls->background_tile;
    if (prev != tile_or_null) {
        ls->background_tile = DP_tile_incref_nullable(tile_or_null);
        ls->background_opaque = DP_tile_opaque(tile_or_null);
        DP_tile_decref_nullable(prev);
        notify_view_invalidated(ls, false, 0);
    }
}

static void handle_background_tile(DP_LocalState *ls, DP_DrawContext *dc,
                                   DP_MsgLocalChange *mlc)
{
    size_t size;
    const unsigned char *body = DP_msg_local_change_body(mlc, &size);
    if (size == 0) {
        set_background_tile(ls, NULL);
    }
    else {
        DP_Tile *t = DP_tile_new_from_compressed(dc, 0, body, size);
        if (t) {
            set_background_tile(ls, t);
        }
        else {
            DP_warn("Error decompressing background tile: %s", DP_error());
        }
    }
}

static bool read_int_message(DP_MsgLocalChange *mlc, const char *title,
                             int *out_value)
{
    size_t size;
    const unsigned char *body = DP_msg_local_change_body(mlc, &size);
    if (size == INT_SIZE) {
        *out_value = DP_read_bigendian_int32(body);
        return true;
    }
    else {
        DP_warn("Wrong size for local %s change: wanted %zu, got %zu", title,
                INT_SIZE, size);
        return false;
    }
}

static void handle_view_mode(DP_LocalState *ls, DP_MsgLocalChange *mlc)
{
    int value;
    if (read_int_message(mlc, "view mode", &value)) {
        switch (value) {
        case DP_VIEW_MODE_NORMAL:
        case DP_VIEW_MODE_LAYER:
        case DP_VIEW_MODE_FRAME: {
            DP_ViewMode view_mode = (DP_ViewMode)value;
            if (view_mode != ls->view_mode) {
                ls->view_mode = view_mode;
                notify_view_invalidated(ls, true, 0);
            }
            break;
        }
        default:
            DP_warn("Unknown view mode %d", value);
            break;
        }
    }
}

static void handle_active_layer(DP_LocalState *ls, DP_MsgLocalChange *mlc)
{
    int layer_id;
    if (read_int_message(mlc, "active layer", &layer_id)) {
        if (ls->active_layer_id != layer_id) {
            ls->active_layer_id = layer_id;
            if (ls->view_mode == DP_VIEW_MODE_LAYER) {
                notify_view_invalidated(ls, true, 0);
            }
        }
    }
}

static void handle_active_frame(DP_LocalState *ls, DP_MsgLocalChange *mlc)
{
    int frame_index;
    if (read_int_message(mlc, "active frame", &frame_index)) {
        if (ls->active_frame_index != frame_index) {
            ls->active_frame_index = frame_index;
            if (ls->view_mode == DP_VIEW_MODE_FRAME) {
                notify_view_invalidated(ls, true, 0);
            }
        }
    }
}

static int read_int(const unsigned char *body, size_t *in_out_read)
{
    int value = DP_int32_to_int(DP_read_bigendian_int32(body + *in_out_read));
    *in_out_read += sizeof(int32_t);
    return value;
}

static uint16_t read_uint16(const unsigned char *body, size_t *in_out_read)
{
    uint16_t value = DP_read_bigendian_uint16(body + *in_out_read);
    *in_out_read += sizeof(uint16_t);
    return value;
}

static DP_UPixel15 read_upixel(const unsigned char *body, size_t *in_out_read)
{
    uint32_t value = DP_read_bigendian_uint32(body + *in_out_read);
    *in_out_read += sizeof(uint32_t);
    return DP_upixel8_to_15((DP_UPixel8){value});
}

static void handle_onion_skins(DP_LocalState *ls, DP_MsgLocalChange *mlc)
{
    size_t size;
    const unsigned char *body = DP_msg_local_change_body(mlc, &size);
    size_t minimum_size = sizeof(int32_t) + sizeof(int32_t);
    if (size < minimum_size) {
        DP_warn("Wrong size for local onion skins change: wanted at least %zu, "
                "got %zu",
                minimum_size, size);
        return;
    }

    size_t read = 0;
    int count_below = read_int(body, &read);
    int count_above = read_int(body, &read);

    size_t expected_size = minimum_size
                         + DP_int_to_size(count_below + count_above)
                               * (sizeof(uint16_t) + sizeof(uint32_t));
    if (size != expected_size) {
        DP_warn("Wrong size for local onion skins change: wanted %zu, got %zu",
                expected_size, size);
        return;
    }

    DP_OnionSkins *oss;
    if (count_below == 0 && count_above == 0) {
        oss = NULL;
    }
    else {
        oss = DP_onion_skins_new(count_below, count_above);
        for (int i = 0; i < count_below; ++i) {
            uint16_t opacity = read_uint16(body, &read);
            DP_UPixel15 tint = read_upixel(body, &read);
            DP_onion_skins_skin_below_at_set(oss, i, opacity, tint);
        }
        for (int i = 0; i < count_above; ++i) {
            uint16_t opacity = read_uint16(body, &read);
            DP_UPixel15 tint = read_upixel(body, &read);
            DP_onion_skins_skin_above_at_set(oss, i, opacity, tint);
        }
    }
    DP_ASSERT(read == expected_size);
    DP_onion_skins_free(ls->onion_skins);
    ls->onion_skins = oss;

    if (ls->view_mode == DP_VIEW_MODE_FRAME) {
        notify_view_invalidated(ls, true, 0);
    }
}

static bool is_track_state(void *element, void *user)
{
    return ((DP_LocalTrackState *)element)->track_id == *(int *)user;
}

static void set_track_state_hidden(DP_LocalTrackState *lts, bool hidden)
{
    lts->hidden = hidden;
}

static void set_track_state_onion_skin(DP_LocalTrackState *lts, bool onion_skin)
{
    lts->onion_skin = onion_skin;
}

static void set_track_state_all(DP_LocalTrackState *lts, bool value)
{
    set_track_state_hidden(lts, value);
    set_track_state_onion_skin(lts, value);
}

static void update_track_state(DP_LocalState *ls, int track_id,
                               void (*update)(DP_LocalTrackState *, bool),
                               bool value)
{
    DP_Vector *track_states = &ls->track_states;
    int index = DP_vector_search_index(track_states, sizeof(DP_LocalTrackState),
                                       is_track_state, &track_id);
    DP_LocalTrackState prev_lts =
        index == -1
            ? (DP_LocalTrackState){track_id, false, false}
            : DP_VECTOR_AT_TYPE(track_states, DP_LocalTrackState, index);

    DP_LocalTrackState lts = prev_lts;
    update(&lts, value);

    bool track_state_changed =
        prev_lts.hidden != lts.hidden || prev_lts.onion_skin != lts.onion_skin;
    if (track_state_changed) {
        if (lts.hidden || lts.onion_skin) {
            if (index == -1) {
                DP_VECTOR_PUSH_TYPE(track_states, DP_LocalTrackState, lts);
            }
            else {
                DP_VECTOR_AT_TYPE(track_states, DP_LocalTrackState, index) =
                    lts;
            }
        }
        else if (index != -1) {
            DP_VECTOR_REMOVE_TYPE(track_states, DP_LocalTrackState, index);
        }

        notify_view_invalidated(ls, ls->view_mode == DP_VIEW_MODE_FRAME, 0);
    }
}

static void handle_track_visibility(DP_LocalState *ls, DP_MsgLocalChange *mlc)
{
    int track_id;
    bool hidden;
    if (read_id_bool_message(mlc, "track visibility", &track_id, &hidden)) {
        update_track_state(ls, track_id, set_track_state_hidden, hidden);
    }
}

static void handle_track_onion_skin(DP_LocalState *ls, DP_MsgLocalChange *mlc)
{
    int track_id;
    bool onion_skin;
    if (read_id_bool_message(mlc, "track onion skin", &track_id, &onion_skin)) {
        update_track_state(ls, track_id, set_track_state_onion_skin,
                           onion_skin);
    }
}

static void handle_internal(DP_LocalState *ls, DP_MsgInternal *mi)
{
    if (DP_msg_internal_type(mi) == DP_MSG_INTERNAL_TYPE_RESET_TO_STATE) {
        set_background_tile(ls, NULL);
        while (ls->hidden_layer_ids.used != 0) {
            set_layer_visibility(
                ls, DP_VECTOR_FIRST_TYPE(&ls->hidden_layer_ids, int), false);
        }
    }
}

static void handle_local_change(DP_LocalState *ls, DP_DrawContext *dc,
                                DP_MsgLocalChange *mlc)
{
    int type = DP_msg_local_change_type(mlc);
    switch (type) {
    case DP_MSG_LOCAL_CHANGE_TYPE_LAYER_VISIBILITY:
        handle_layer_visibility(ls, mlc);
        break;
    case DP_MSG_LOCAL_CHANGE_TYPE_BACKGROUND_TILE:
        handle_background_tile(ls, dc, mlc);
        break;
    case DP_MSG_LOCAL_CHANGE_TYPE_VIEW_MODE:
        handle_view_mode(ls, mlc);
        break;
    case DP_MSG_LOCAL_CHANGE_TYPE_ACTIVE_LAYER:
        handle_active_layer(ls, mlc);
        break;
    case DP_MSG_LOCAL_CHANGE_TYPE_ACTIVE_FRAME:
        handle_active_frame(ls, mlc);
        break;
    case DP_MSG_LOCAL_CHANGE_TYPE_ONION_SKINS:
        handle_onion_skins(ls, mlc);
        break;
    case DP_MSG_LOCAL_CHANGE_TYPE_TRACK_VISIBILITY:
        handle_track_visibility(ls, mlc);
        break;
    case DP_MSG_LOCAL_CHANGE_TYPE_TRACK_ONION_SKIN:
        handle_track_onion_skin(ls, mlc);
        break;
    default:
        DP_warn("Unknown local change type %d", type);
        break;
    }
}

void DP_local_state_handle(DP_LocalState *ls, DP_DrawContext *dc,
                           DP_Message *msg)
{
    DP_ASSERT(ls);
    DP_ASSERT(dc);
    DP_ASSERT(msg);
    switch (DP_message_type(msg)) {
    case DP_MSG_INTERNAL:
        handle_internal(ls, DP_message_internal(msg));
        break;
    case DP_MSG_LOCAL_CHANGE:
        handle_local_change(ls, dc, DP_message_internal(msg));
        break;
    case DP_MSG_LAYER_CREATE:
        set_layer_visibility(
            ls, DP_msg_layer_create_id(DP_message_internal(msg)), false);
        break;
    case DP_MSG_LAYER_TREE_CREATE:
        set_layer_visibility(
            ls, DP_msg_layer_tree_create_id(DP_message_internal(msg)), false);
        break;
    case DP_MSG_TRACK_CREATE:
        update_track_state(ls, DP_msg_track_create_id(DP_message_internal(msg)),
                           set_track_state_all, false);
    default:
        break;
    }
}

bool DP_local_state_reset_image_build(DP_LocalState *ls, DP_DrawContext *dc,
                                      DP_LocalStateAcceptResetMessageFn fn,
                                      void *user)
{
    DP_ASSERT(ls);
    DP_ASSERT(fn);

    int layer_id_count;
    const int *layer_ids = DP_local_state_hidden_layer_ids(ls, &layer_id_count);
    for (int i = 0; i < layer_id_count; ++i) {
        DP_Message *msg =
            DP_local_state_msg_layer_visibility_new(layer_ids[i], true);
        if (!fn(user, msg)) {
            return false;
        }
    }

    DP_Tile *t = ls->background_tile;
    if (t) {
        DP_Message *msg = DP_local_state_msg_background_tile_new(dc, t);
        if (msg) {
            if (!fn(user, msg)) {
                return false;
            }
        }
        else {
            DP_warn("Error serializing local background tile: %s", DP_error());
        }
    }

    bool ok = fn(user, DP_local_state_msg_view_mode_new(ls->view_mode))
           && fn(user, DP_local_state_msg_active_layer_new(ls->active_layer_id))
           && fn(user,
                 DP_local_state_msg_active_frame_new(ls->active_frame_index));
    if (!ok) {
        return false;
    }

    DP_OnionSkins *oss = ls->onion_skins;
    if (oss && !fn(user, DP_local_state_msg_onion_skins_new(oss))) {
        return false;
    }

    int track_state_count;
    const DP_LocalTrackState *track_states =
        DP_local_state_track_states(ls, &track_state_count);
    for (int i = 0; i < track_state_count; ++i) {
        DP_LocalTrackState lts = track_states[i];
        if (lts.hidden) {
            DP_Message *msg =
                DP_local_state_msg_track_visibility_new(lts.track_id, true);
            if (!fn(user, msg)) {
                return false;
            }
        }
        if (lts.onion_skin) {
            DP_Message *msg =
                DP_local_state_msg_track_onion_skin_new(lts.track_id, true);
            if (!fn(user, msg)) {
                return false;
            }
        }
    }

    return true;
}


static void set_body(size_t size, unsigned char *out, void *user)
{
    if (size > 0) {
        memcpy(out, user, size);
    }
}

static DP_Message *make_id_bool_message(uint8_t type, int id, bool value)
{
    unsigned char body[ID_BOOL_SIZE];
    size_t written = 0;
    written += DP_write_bigendian_int32(DP_int_to_int32(id), body + written);
    written += DP_write_bigendian_uint8(value ? 1 : 0, body + written);
    DP_ASSERT(written == sizeof(body));
    return DP_msg_local_change_new(0, type, set_body, written, body);
}

DP_Message *DP_local_state_msg_layer_visibility_new(int layer_id, bool hidden)
{
    return make_id_bool_message(DP_MSG_LOCAL_CHANGE_TYPE_LAYER_VISIBILITY,
                                layer_id, hidden);
}

static unsigned char *get_compression_buffer(size_t size, void *user)
{
    return DP_draw_context_pool_require(user, size);
}

DP_Message *DP_local_state_msg_background_tile_new(DP_DrawContext *dc,
                                                   DP_Tile *tile_or_null)
{
    if (tile_or_null) {
        DP_ASSERT(dc);
        size_t size =
            DP_tile_compress(tile_or_null, DP_draw_context_tile8_buffer(dc),
                             get_compression_buffer, dc);
        if (size == 0) {
            return NULL;
        }
        else {
            return DP_msg_local_change_new(
                0, DP_MSG_LOCAL_CHANGE_TYPE_BACKGROUND_TILE, set_body, size,
                DP_draw_context_pool(dc));
        }
    }
    else {
        return DP_msg_local_change_new(
            0, DP_MSG_LOCAL_CHANGE_TYPE_BACKGROUND_TILE, set_body, 0, NULL);
    }
}

static DP_Message *make_int_message(uint8_t type, int value)
{
    unsigned char body[INT_SIZE];
    size_t written = DP_write_bigendian_int32(DP_int_to_int32(value), body);
    DP_ASSERT(written == sizeof(body));
    return DP_msg_local_change_new(0, type, set_body, written, body);
}

DP_Message *DP_local_state_msg_view_mode_new(DP_ViewMode view_mode)
{
    return make_int_message(DP_MSG_LOCAL_CHANGE_TYPE_VIEW_MODE, (int)view_mode);
}

DP_Message *DP_local_state_msg_active_layer_new(int layer_id)
{
    return make_int_message(DP_MSG_LOCAL_CHANGE_TYPE_ACTIVE_LAYER, layer_id);
}

DP_Message *DP_local_state_msg_active_frame_new(int frame_index)
{
    return make_int_message(DP_MSG_LOCAL_CHANGE_TYPE_ACTIVE_FRAME, frame_index);
}

static void set_onion_skins(DP_UNUSED size_t size, unsigned char *out,
                            void *user)
{
    const DP_OnionSkins *oss = user;
    size_t written = 0;

    int count_below = DP_onion_skins_count_below(oss);
    written +=
        DP_write_bigendian_int32(DP_int_to_int32(count_below), out + written);

    int count_above = DP_onion_skins_count_above(oss);
    written +=
        DP_write_bigendian_int32(DP_int_to_int32(count_above), out + written);

    for (int i = 0; i < count_below; ++i) {
        const DP_OnionSkin *os = DP_onion_skins_skin_below_at(oss, i);
        written += DP_write_bigendian_uint16(os->opacity, out + written);
        written += DP_write_bigendian_uint32(DP_upixel15_to_8(os->tint).color,
                                             out + written);
    }

    for (int i = 0; i < count_above; ++i) {
        const DP_OnionSkin *os = DP_onion_skins_skin_above_at(oss, i);
        written += DP_write_bigendian_uint16(os->opacity, out + written);
        written += DP_write_bigendian_uint32(DP_upixel15_to_8(os->tint).color,
                                             out + written);
    }

    DP_ASSERT(written == size);
}

DP_Message *DP_local_state_msg_onion_skins_new(const DP_OnionSkins *oss)
{
    size_t size = sizeof(int32_t) + sizeof(int32_t)
                + DP_int_to_size(DP_onion_skins_count_below(oss)
                                 + DP_onion_skins_count_above(oss))
                      * (sizeof(uint16_t) + sizeof(uint32_t));
    return DP_msg_local_change_new(0, DP_MSG_LOCAL_CHANGE_TYPE_ONION_SKINS,
                                   set_onion_skins, size, (void *)oss);
}

DP_Message *DP_local_state_msg_track_visibility_new(int track_id, bool hidden)
{
    return make_id_bool_message(DP_MSG_LOCAL_CHANGE_TYPE_TRACK_VISIBILITY,
                                track_id, hidden);
}

DP_Message *DP_local_state_msg_track_onion_skin_new(int track_id,
                                                    bool onion_skin)
{
    return make_id_bool_message(DP_MSG_LOCAL_CHANGE_TYPE_TRACK_ONION_SKIN,
                                track_id, onion_skin);
}
