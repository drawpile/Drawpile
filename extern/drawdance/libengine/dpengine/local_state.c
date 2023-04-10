// SPDX-License-Identifier: MIT
#include "local_state.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "layer_props.h"
#include "layer_props_list.h"
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

struct DP_LocalState {
    DP_Vector hidden_layer_ids;
    DP_Tile *background_tile;
    bool background_opaque;
    DP_Vector track_states;
    DP_LocalStateLayerVisibilityChangedFn layer_visibility_changed;
    DP_LocalStateBackgroundTileChangedFn background_tile_changed;
    DP_LocalStateTrackStateChangedFn track_state_changed;
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

DP_LocalState *DP_local_state_new(
    DP_CanvasState *cs_or_null,
    DP_LocalStateLayerVisibilityChangedFn layer_visibility_changed,
    DP_LocalStateBackgroundTileChangedFn background_tile_changed,
    DP_LocalStateTrackStateChangedFn track_state_changed, void *user)
{
    DP_LocalState *ls = DP_malloc(sizeof(*ls));
    *ls = (DP_LocalState){DP_VECTOR_NULL,
                          NULL,
                          false,
                          DP_VECTOR_NULL,
                          layer_visibility_changed,
                          background_tile_changed,
                          track_state_changed,
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

static void notify_layer_visibility_changed(DP_LocalState *ls, int layer_id)
{
    DP_LocalStateLayerVisibilityChangedFn fn = ls->layer_visibility_changed;
    if (fn) {
        fn(ls->user, layer_id);
    }
}

static void set_layer_visibility(DP_LocalState *ls, int layer_id, bool hidden)
{
    DP_Vector *hidden_layer_ids = &ls->hidden_layer_ids;
    int index = DP_vector_search_index(hidden_layer_ids, sizeof(int),
                                       is_hidden_layer_id, &layer_id);
    if (hidden && index == -1) {
        DP_VECTOR_PUSH_TYPE(hidden_layer_ids, int, layer_id);
        notify_layer_visibility_changed(ls, layer_id);
    }
    else if (!hidden && index != -1) {
        DP_VECTOR_REMOVE_TYPE(hidden_layer_ids, int, index);
        notify_layer_visibility_changed(ls, layer_id);
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
        DP_LocalStateBackgroundTileChangedFn fn = ls->background_tile_changed;
        if (fn) {
            fn(ls->user);
        }
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

        DP_LocalStateTrackStateChangedFn fn = ls->track_state_changed;
        if (fn) {
            fn(ls->user, track_id);
        }
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
