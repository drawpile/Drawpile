// SPDX-License-Identifier: MIT
#include "local_state.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "tile.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/vector.h>
#include <dpmsg/message.h>
#include <dpmsg/msg_internal.h>

#define LAYER_VISIBILITY_SIZE (sizeof(int32_t) + sizeof(uint8_t))

struct DP_LocalState {
    DP_Vector hidden_layer_ids;
    DP_Tile *background_tile;
    bool background_opaque;
    DP_LocalStateLayerVisibilityChangedFn layer_visibility_changed;
    DP_LocalStateBackgroundTileChangedFn background_tile_changed;
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

DP_LocalState *DP_local_state_new(
    DP_CanvasState *cs_or_null,
    DP_LocalStateLayerVisibilityChangedFn layer_visibility_changed,
    DP_LocalStateBackgroundTileChangedFn background_tile_changed, void *user)
{
    DP_LocalState *ls = DP_malloc(sizeof(*ls));
    *ls = (DP_LocalState){
        DP_VECTOR_NULL,          NULL, false, layer_visibility_changed,
        background_tile_changed, user};
    DP_VECTOR_INIT_TYPE(&ls->hidden_layer_ids, int, 8);
    if (cs_or_null) {
        DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs_or_null);
        apply_hidden_layers_recursive(&ls->hidden_layer_ids, lpl);
    }
    return ls;
}

void DP_local_state_free(DP_LocalState *ls)
{
    if (ls) {
        DP_vector_dispose(&ls->hidden_layer_ids);
        DP_free(ls);
    }
}


int *DP_local_state_hidden_layer_ids(DP_LocalState *ls, int *out_count)
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

static void handle_layer_visibility(DP_LocalState *ls, DP_MsgLocalChange *mlc)
{
    size_t size;
    const unsigned char *body = DP_msg_local_change_body(mlc, &size);
    if (size == LAYER_VISIBILITY_SIZE) {
        int layer_id = DP_read_bigendian_int32(body);
        bool hidden = DP_read_bigendian_uint8(body + sizeof(int32_t));
        set_layer_visibility(ls, layer_id, hidden);
    }
    else {
        DP_warn(
            "Wrong size for local layer visibility change: wanted %zu, got %zu",
            LAYER_VISIBILITY_SIZE, size);
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

    int count;
    int *layer_ids = DP_local_state_hidden_layer_ids(ls, &count);
    for (int i = 0; i < count; ++i) {
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

    return true;
}


static void set_body(size_t size, unsigned char *out, void *user)
{
    if (size > 0) {
        memcpy(out, user, size);
    }
}

DP_Message *DP_local_state_msg_layer_visibility_new(int layer_id, bool hidden)
{
    unsigned char body[LAYER_VISIBILITY_SIZE];
    size_t written = 0;
    written +=
        DP_write_bigendian_int32(DP_int_to_int32(layer_id), body + written);
    written += DP_write_bigendian_uint8(hidden ? 1 : 0, body + written);
    DP_ASSERT(written == sizeof(body));
    return DP_msg_local_change_new(0, DP_MSG_LOCAL_CHANGE_TYPE_LAYER_VISIBILITY,
                                   set_body, written, body);
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
