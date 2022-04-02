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
#include "canvas_state.h"
#include "blend_mode.h"
#include "canvas_diff.h"
#include "compress.h"
#include "image.h"
#include "layer_content.h"
#include "layer_content_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "ops.h"
#include "paint.h"
#include "tile.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpmsg/message.h>
#include <dpmsg/messages/canvas_background.h>
#include <dpmsg/messages/canvas_resize.h>
#include <dpmsg/messages/draw_dabs.h>
#include <dpmsg/messages/fill_rect.h>
#include <dpmsg/messages/layer_attr.h>
#include <dpmsg/messages/layer_create.h>
#include <dpmsg/messages/layer_delete.h>
#include <dpmsg/messages/layer_order.h>
#include <dpmsg/messages/layer_retitle.h>
#include <dpmsg/messages/layer_visibility.h>
#include <dpmsg/messages/put_image.h>
#include <dpmsg/messages/put_tile.h>
#include <dpmsg/messages/region_move.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_CanvasState {
    DP_Atomic refcount;
    const bool transient;
    const int width, height;
    DP_Tile *const background_tile;
    DP_LayerContentList *const layer_contents;
    DP_LayerPropsList *const layer_props;
};

struct DP_TransientCanvasState {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    DP_Tile *background_tile;
    union {
        DP_LayerContentList *layer_contents;
        DP_TransientLayerContentList *transient_layer_contents;
    };
    union {
        DP_LayerPropsList *layer_props;
        DP_TransientLayerPropsList *transient_layer_props;
    };
};

#else

struct DP_CanvasState {
    DP_Atomic refcount;
    bool transient;
    const int width, height;
    DP_Tile *background_tile;
    union {
        DP_LayerContentList *layer_contents;
        DP_TransientLayerContentList *transient_layer_contents;
    };
    union {
        DP_LayerPropsList *layer_props;
        DP_TransientLayerPropsList *transient_layer_props;
    };
};

#endif


static DP_TransientCanvasState *allocate_canvas_state(bool transient, int width,
                                                      int height)
{
    DP_TransientCanvasState *cs = DP_malloc(sizeof(*cs));
    *cs = (DP_TransientCanvasState){
        DP_ATOMIC_INIT(1), transient, width, height, NULL, {NULL}, {NULL}};
    return cs;
}

DP_CanvasState *DP_canvas_state_new(void)
{
    DP_TransientCanvasState *tcs = allocate_canvas_state(false, 0, 0);
    tcs->layer_contents = DP_layer_content_list_new();
    tcs->layer_props = DP_layer_props_list_new();
    return (DP_CanvasState *)tcs;
}

DP_CanvasState *DP_canvas_state_incref(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_atomic_inc(&cs->refcount);
    return cs;
}

DP_CanvasState *DP_canvas_state_incref_nullable(DP_CanvasState *cs_or_null)
{
    return cs_or_null ? DP_canvas_state_incref(cs_or_null) : NULL;
}

void DP_canvas_state_decref(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    if (DP_atomic_dec(&cs->refcount)) {
        DP_tile_decref_nullable(cs->background_tile);
        DP_layer_content_list_decref(cs->layer_contents);
        DP_layer_props_list_decref(cs->layer_props);
        DP_free(cs);
    }
}

void DP_canvas_state_decref_nullable(DP_CanvasState *cs_or_null)
{
    if (cs_or_null) {
        DP_canvas_state_decref(cs_or_null);
    }
}

int DP_canvas_state_refcount(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return DP_atomic_get(&cs->refcount);
}

bool DP_canvas_state_transient(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->transient;
}

int DP_canvas_state_width(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->width;
}

int DP_canvas_state_height(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->height;
}

DP_LayerContentList *DP_canvas_state_layer_contents_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->layer_contents;
}

DP_LayerPropsList *DP_canvas_state_layer_props_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->layer_props;
}


static DP_CanvasState *handle_canvas_resize(DP_CanvasState *cs,
                                            unsigned int context_id,
                                            DP_MsgCanvasResize *mcr)
{
    int top, right, bottom, left;
    DP_msg_canvas_resize_dimensions(mcr, &top, &right, &bottom, &left);
    return DP_ops_canvas_resize(cs, context_id, top, right, bottom, left);
}

static DP_CanvasState *handle_layer_create(DP_CanvasState *cs,
                                           unsigned int context_id,
                                           DP_MsgLayerCreate *mlc)
{
    unsigned int flags = DP_msg_layer_create_flags(mlc);
    bool insert = flags & DP_MSG_LAYER_CREATE_FLAG_INSERT;
    bool copy = flags & DP_MSG_LAYER_CREATE_FLAG_COPY;

    uint32_t fill = DP_msg_layer_create_fill(mlc);
    DP_Tile *tile = fill == 0 ? NULL : DP_tile_new_from_bgra(context_id, fill);

    size_t title_length;
    const char *title = DP_msg_layer_create_title(mlc, &title_length);

    DP_CanvasState *next =
        DP_ops_layer_create(cs, DP_msg_layer_create_layer_id(mlc),
                            DP_msg_layer_create_source_id(mlc), tile, insert,
                            copy, title, title_length);

    DP_tile_decref_nullable(tile);
    return next;
}

static DP_CanvasState *handle_layer_attr(DP_CanvasState *cs,
                                         DP_MsgLayerAttr *mla)
{
    unsigned int flags = DP_msg_layer_attr_flags(mla);
    bool censored = flags & DP_MSG_LAYER_ATTR_FLAG_CENSORED;
    bool fixed = flags & DP_MSG_LAYER_ATTR_FLAG_FIXED;
    return DP_ops_layer_attr(
        cs, DP_msg_layer_attr_layer_id(mla), DP_msg_layer_attr_sublayer_id(mla),
        DP_msg_layer_attr_opacity(mla), DP_msg_layer_attr_blend_mode(mla),
        censored, fixed);
}


static int get_layer_order_id(void *mlo, int index)
{
    return DP_msg_layer_order_layer_id_at(mlo, index);
}

static DP_CanvasState *handle_layer_order(DP_CanvasState *cs,
                                          DP_MsgLayerOrder *mlo)
{
    return DP_ops_layer_reorder(cs, DP_msg_layer_order_layer_id_count(mlo),
                                get_layer_order_id, mlo);
}

static DP_CanvasState *handle_layer_retitle(DP_CanvasState *cs,
                                            DP_MsgLayerRetitle *mlr)
{
    size_t title_length;
    const char *title = DP_msg_layer_retitle_title(mlr, &title_length);
    return DP_ops_layer_retitle(cs, DP_msg_layer_retitle_layer_id(mlr), title,
                                title_length);
}

static DP_CanvasState *handle_layer_delete(DP_CanvasState *cs,
                                           unsigned int context_id,
                                           DP_MsgLayerDelete *mld)
{
    return DP_ops_layer_delete(cs, context_id,
                               DP_msg_layer_delete_layer_id(mld),
                               DP_msg_layer_delete_merge(mld));
}

static DP_CanvasState *handle_layer_visibility(DP_CanvasState *cs,
                                               DP_MsgLayerVisibility *mlv)
{
    return DP_ops_layer_visibility(cs, DP_msg_layer_visibility_layer_id(mlv),
                                   DP_msg_layer_visibility_visible(mlv));
}

static DP_CanvasState *handle_put_image(DP_CanvasState *cs,
                                        unsigned int context_id,
                                        DP_MsgPutImage *mpi)
{
    int blend_mode = DP_msg_put_image_blend_mode(mpi);
    if (!DP_blend_mode_exists(blend_mode)) {
        DP_error_set("Put image: unknown blend mode %d", blend_mode);
        return NULL;
    }

    size_t image_size;
    const unsigned char *image = DP_msg_put_image_image(mpi, &image_size);
    return DP_ops_put_image(cs, context_id, DP_msg_put_image_layer_id(mpi),
                            blend_mode, DP_msg_put_image_x(mpi),
                            DP_msg_put_image_y(mpi),
                            DP_msg_put_image_width(mpi),
                            DP_msg_put_image_height(mpi), image, image_size);
}

static DP_CanvasState *handle_fill_rect(DP_CanvasState *cs,
                                        unsigned int context_id,
                                        DP_MsgFillRect *mfr)
{
    int blend_mode = DP_msg_fill_rect_blend_mode(mfr);
    if (!DP_blend_mode_exists(blend_mode)) {
        DP_error_set("Fill rect: unknown blend mode %d", blend_mode);
        return NULL;
    }
    else if (!DP_blend_mode_valid_for_brush(blend_mode)) {
        DP_error_set("Fill rect: blend mode %s not applicable to brushes",
                     DP_blend_mode_enum_name_unprefixed(blend_mode));
        return NULL;
    }

    int x = DP_msg_fill_rect_x(mfr);
    int y = DP_msg_fill_rect_y(mfr);
    int width = DP_msg_fill_rect_width(mfr);
    int height = DP_msg_fill_rect_height(mfr);
    int left = DP_max_int(x, 0);
    int top = DP_max_int(y, 0);
    int right = DP_min_int(x + width, cs->width);
    int bottom = DP_min_int(y + height, cs->height);
    if (left >= right || top >= bottom) {
        DP_error_set("Fill rect: effective area to fill is zero");
        return NULL;
    }

    return DP_ops_fill_rect(cs, context_id, DP_msg_fill_rect_layer_id(mfr),
                            blend_mode, left, top, right, bottom,
                            DP_msg_fill_rect_color(mfr));
}

static DP_CanvasState *handle_region_move(DP_CanvasState *cs,
                                          DP_DrawContext *dc,
                                          unsigned int context_id,
                                          DP_MsgRegionMove *mrm)
{
    int src_x, src_y, src_width, src_height;
    DP_msg_region_move_src_rect(mrm, &src_x, &src_y, &src_width, &src_height);
    if (src_width <= 0 || src_height <= 0) {
        DP_error_set("Region move: selection is empty");
        return NULL;
    }

    int x1, y1, x2, y2, x3, y3, x4, y4;
    DP_msg_region_move_dst_quad(mrm, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4);
    DP_Quad dst_quad = DP_quad_make(x1, y1, x2, y2, x3, y3, x4, y4);

    long long canvas_width = cs->width;
    long long canvas_height = cs->height;
    long long max_size = (canvas_width + 1LL) * (canvas_height + 1LL);
    if (DP_rect_size(DP_quad_bounds(dst_quad)) > max_size) {
        DP_error_set("Region move: attempt to scale beyond image size");
        return NULL;
    }

    size_t in_mask_size;
    const unsigned char *in_mask = DP_msg_region_move_mask(mrm, &in_mask_size);
    DP_Image *mask;
    if (in_mask) {
        mask = DP_image_new_from_compressed_monochrome(src_width, src_height,
                                                       in_mask, in_mask_size);
        if (!mask) {
            return NULL;
        }
    }
    else {
        mask = NULL;
    }

    DP_Rect src_rect = DP_rect_make(src_x, src_y, src_width, src_height);
    DP_CanvasState *next =
        DP_ops_region_move(cs, dc, context_id, DP_msg_region_move_layer_id(mrm),
                           &src_rect, &dst_quad, mask);
    DP_free(mask);
    return next;
}

static DP_CanvasState *
handle_put_tile(DP_CanvasState *cs, unsigned int context_id, DP_MsgPutTile *mpt)
{
    DP_TileCounts tile_counts = DP_tile_counts_round(cs->width, cs->height);
    int tile_total = tile_counts.x * tile_counts.y;
    int x = DP_msg_put_tile_x(mpt);
    int y = DP_msg_put_tile_y(mpt);
    int start = y * tile_counts.x + x;
    if (start >= tile_total) {
        DP_error_set("Put tile: starting index %d beyond total %d", start,
                     tile_total);
        return NULL;
    }

    DP_Tile *tile;
    uint32_t color;
    if (DP_msg_put_tile_color(mpt, &color)) {
        tile = DP_tile_new_from_bgra(context_id, color);
    }
    else {
        size_t image_size;
        const unsigned char *image = DP_msg_put_tile_image(mpt, &image_size);
        tile = DP_tile_new_from_compressed(context_id, image, image_size);
        if (!tile) {
            return NULL;
        }
    }

    DP_CanvasState *next = DP_ops_put_tile(
        cs, tile, DP_msg_put_tile_layer_id(mpt),
        DP_msg_put_tile_sublayer_id(mpt), x, y, DP_msg_put_tile_repeat(mpt));

    DP_tile_decref(tile);
    return next;
}

static DP_CanvasState *handle_canvas_background(DP_CanvasState *cs,
                                                unsigned int context_id,
                                                DP_MsgCanvasBackground *mcb)
{
    DP_Tile *tile;
    uint32_t color;
    if (DP_msg_canvas_background_color(mcb, &color)) {
        tile = DP_tile_new_from_bgra(context_id, color);
    }
    else {
        size_t image_size;
        const unsigned char *image =
            DP_msg_canvas_background_image(mcb, &image_size);
        tile = DP_tile_new_from_compressed(context_id, image, image_size);
    }

    if (tile) {
        DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
        DP_transient_canvas_state_background_tile_set_noinc(tcs, tile);
        return DP_transient_canvas_state_persist(tcs);
    }
    else {
        return NULL;
    }
}

static DP_CanvasState *handle_pen_up(DP_CanvasState *cs,
                                     unsigned int context_id)
{
    return DP_ops_pen_up(cs, context_id);
}

static void *get_classic_dabs(DP_MsgDrawDabs *mdd, int *out_dab_count)
{
    DP_MsgDrawDabsClassic *mddc = DP_msg_draw_dabs_cast_classic(mdd);
    return DP_msg_draw_dabs_classic_dabs(mddc, out_dab_count);
}

static void *get_pixel_dabs(DP_MsgDrawDabs *mdd, int *out_dab_count)
{
    DP_MsgDrawDabsPixel *mddp = DP_msg_draw_dabs_cast_pixel(mdd);
    return DP_msg_draw_dabs_pixel_dabs(mddp, out_dab_count);
}

static DP_CanvasState *
handle_draw_dabs(DP_CanvasState *cs, DP_DrawContext *dc, DP_MessageType type,
                 unsigned int context_id, DP_MsgDrawDabs *mdd,
                 void *(*get_dabs)(DP_MsgDrawDabs *, int *))
{
    int blend_mode = DP_msg_draw_dabs_blend_mode(mdd);
    if (!DP_blend_mode_exists(blend_mode)) {
        DP_error_set("Draw dabs: unknown blend mode %d", blend_mode);
        return NULL;
    }
    else if (!DP_blend_mode_valid_for_brush(blend_mode)) {
        DP_error_set("Draw dabs: blend mode %s not applicable to brushes",
                     DP_blend_mode_enum_name_unprefixed(blend_mode));
        return NULL;
    }

    int dab_count;
    void *dabs = get_dabs(mdd, &dab_count);
    if (dab_count < 1) {
        return DP_canvas_state_incref(cs); // Nothing to do here.
    }

    uint32_t color = DP_msg_draw_dabs_color(mdd);
    int sublayer_id, sublayer_opacity, sublayer_blend_mode, dabs_blend_mode;
    if (DP_msg_draw_dabs_indirect(mdd)) {
        sublayer_id = DP_uint_to_int(context_id);
        sublayer_opacity = DP_uint32_to_int((color & 0xff000000) >> 24);
        sublayer_blend_mode = blend_mode;
        dabs_blend_mode = DP_BLEND_MODE_NORMAL;
    }
    else {
        sublayer_id = 0;
        sublayer_opacity = -1;
        sublayer_blend_mode = -1;
        dabs_blend_mode = blend_mode;
    }

    DP_PaintDrawDabsParams params = {
        (int)type,
        dc,
        context_id,
        DP_msg_draw_dabs_origin_x(mdd),
        DP_msg_draw_dabs_origin_y(mdd),
        color,
        dabs_blend_mode,
        dab_count,
        dabs,
    };

    return DP_ops_draw_dabs(cs, DP_msg_draw_dabs_layer_id(mdd), sublayer_id,
                            sublayer_blend_mode, sublayer_opacity, &params);
}

DP_CanvasState *DP_canvas_state_handle(DP_CanvasState *cs, DP_DrawContext *dc,
                                       DP_Message *msg)
{
    DP_ASSERT(cs);
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_ASSERT(!cs->transient);
    DP_MessageType type = DP_message_type(msg);
    DP_debug("Draw command %d %s", (int)type, DP_message_type_enum_name(type));
    switch (type) {
    case DP_MSG_CANVAS_RESIZE:
        return handle_canvas_resize(cs, DP_message_context_id(msg),
                                    DP_msg_canvas_resize_cast(msg));
    case DP_MSG_LAYER_CREATE:
        return handle_layer_create(cs, DP_message_context_id(msg),
                                   DP_msg_layer_create_cast(msg));
    case DP_MSG_LAYER_ATTR:
        return handle_layer_attr(cs, DP_msg_layer_attr_cast(msg));
    case DP_MSG_LAYER_ORDER:
        return handle_layer_order(cs, DP_msg_layer_order_cast(msg));
    case DP_MSG_LAYER_RETITLE:
        return handle_layer_retitle(cs, DP_msg_layer_retitle_cast(msg));
    case DP_MSG_LAYER_DELETE:
        return handle_layer_delete(cs, DP_message_context_id(msg),
                                   DP_msg_layer_delete_cast(msg));
    case DP_MSG_LAYER_VISIBILITY:
        return handle_layer_visibility(cs, DP_msg_layer_visibility_cast(msg));
    case DP_MSG_PUT_IMAGE:
        return handle_put_image(cs, DP_message_context_id(msg),
                                DP_msg_put_image_cast(msg));
    case DP_MSG_FILL_RECT:
        return handle_fill_rect(cs, DP_message_context_id(msg),
                                DP_msg_fill_rect_cast(msg));
    case DP_MSG_REGION_MOVE:
        return handle_region_move(cs, dc, DP_message_context_id(msg),
                                  DP_msg_region_move_cast(msg));
    case DP_MSG_PUT_TILE:
        return handle_put_tile(cs, DP_message_context_id(msg),
                               DP_msg_put_tile_cast(msg));
    case DP_MSG_CANVAS_BACKGROUND:
        return handle_canvas_background(cs, DP_message_context_id(msg),
                                        DP_msg_canvas_background_cast(msg));
    case DP_MSG_PEN_UP:
        return handle_pen_up(cs, DP_message_context_id(msg));
    case DP_MSG_DRAW_DABS_CLASSIC:
        return handle_draw_dabs(cs, dc, type, DP_message_context_id(msg),
                                DP_msg_draw_dabs_cast(msg), get_classic_dabs);
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
        return handle_draw_dabs(cs, dc, type, DP_message_context_id(msg),
                                DP_msg_draw_dabs_cast(msg), get_pixel_dabs);
    default:
        DP_error_set("Unhandled draw message type %d", (int)type);
        return NULL;
    }
}

DP_Image *DP_canvas_state_to_flat_image(DP_CanvasState *cs, unsigned int flags)
{
    DP_ASSERT(cs);
    int width = cs->width;
    int height = cs->height;
    if (width <= 0 || height <= 0) {
        DP_error_set("Can't create a flat image with zero pixels");
        return NULL;
    }

    // Create a layer to flatten the image into. Start by filling it with the
    // background tile if requested, otherwise leave it transparent.
    bool include_background = flags & DP_FLAT_IMAGE_INCLUDE_BACKGROUND;
    DP_Tile *background_tile = include_background ? cs->background_tile : NULL;
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init(width, height, background_tile);

    // Merge the other layers into the flattening layer.
    DP_layer_content_list_merge_to_flat_image(cs->layer_contents,
                                              cs->layer_props, tlc, flags);

    // Write it all to an image and toss the flattening layer.
    DP_LayerContent *lc = DP_transient_layer_content_persist(tlc);
    DP_Image *img = DP_layer_content_to_image(lc);
    DP_layer_content_decref(lc);
    return img;
}

DP_Tile *DP_canvas_state_flatten_tile(DP_CanvasState *cs, int tile_index)
{
    DP_ASSERT(cs);
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(cs->width, cs->height));
    DP_Tile *background_tile = cs->background_tile;
    DP_TransientTile *tt = background_tile
                             ? DP_transient_tile_new(background_tile, 0)
                             : DP_transient_tile_new_blank(0);
    DP_layer_content_list_flatten_tile_to(cs->layer_contents, cs->layer_props,
                                          tile_index, tt);
    return DP_transient_tile_persist(tt);
}


static void diff_states(DP_CanvasState *cs, DP_CanvasState *prev,
                        DP_CanvasDiff *diff)
{
    bool check_all = cs->background_tile != prev->background_tile
                  || cs->width != prev->width || cs->height != prev->height;
    if (check_all) {
        DP_canvas_diff_check_all(diff);
    }
    else {
        DP_layer_content_list_diff(cs->layer_contents, cs->layer_props,
                                   prev->layer_contents, prev->layer_props,
                                   diff);
    }
}

void DP_canvas_state_diff(DP_CanvasState *cs, DP_CanvasState *prev_or_null,
                          DP_CanvasDiff *diff)
{
    DP_ASSERT(cs);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    if (prev_or_null) {
        DP_ASSERT(DP_atomic_get(&prev_or_null->refcount) > 0);
        bool change = cs != prev_or_null;
        DP_canvas_diff_begin(
            diff, prev_or_null->width, prev_or_null->height, cs->width,
            cs->height, change && cs->layer_props != prev_or_null->layer_props);
        if (change) {
            diff_states(cs, prev_or_null, diff);
        }
    }
    else {
        DP_canvas_diff_begin(diff, 0, 0, cs->width, cs->height, true);
    }
}

static void render_tile(void *data, int tile_index)
{
    DP_CanvasState *cs = ((void **)data)[0];
    DP_TransientLayerContent *target = ((void **)data)[1];
    DP_transient_layer_content_render_tile(target, cs, tile_index);
}

DP_TransientLayerContent *DP_canvas_state_render(DP_CanvasState *cs,
                                                 DP_TransientLayerContent *lc,
                                                 DP_CanvasDiff *diff)
{
    DP_ASSERT(cs);
    DP_ASSERT(lc);
    DP_ASSERT(diff);
    DP_TransientLayerContent *target =
        DP_transient_layer_content_resize_to(lc, 0, cs->width, cs->height);
    DP_canvas_diff_each_index(diff, render_tile, (void *[]){cs, target});
    return target;
}


DP_TransientCanvasState *DP_transient_canvas_state_new_init(void)
{
    DP_TransientCanvasState *tcs = allocate_canvas_state(true, 0, 0);
    tcs->layer_contents = DP_layer_content_list_new();
    tcs->layer_props = DP_layer_props_list_new();
    return tcs;
}

static DP_TransientCanvasState *new_transient_canvas_state(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_ASSERT(!cs->transient);
    DP_debug("New transient canvas state");
    DP_TransientCanvasState *tcs =
        allocate_canvas_state(true, cs->width, cs->height);
    tcs->background_tile = DP_tile_incref_nullable(cs->background_tile);
    return tcs;
}

DP_TransientCanvasState *DP_transient_canvas_state_new(DP_CanvasState *cs)
{
    DP_TransientCanvasState *tcs = new_transient_canvas_state(cs);
    tcs->layer_contents = DP_layer_content_list_incref(cs->layer_contents);
    tcs->layer_props = DP_layer_props_list_incref(cs->layer_props);
    return tcs;
}

DP_TransientCanvasState *DP_transient_canvas_state_new_with_layers_noinc(
    DP_CanvasState *cs, DP_TransientLayerContentList *tlcl,
    DP_TransientLayerPropsList *tlpl)
{
    DP_TransientCanvasState *tcs = new_transient_canvas_state(cs);
    tcs->transient_layer_contents = tlcl;
    tcs->transient_layer_props = tlpl;
    return tcs;
}

DP_TransientCanvasState *
DP_transient_canvas_state_incref(DP_TransientCanvasState *tcs)
{
    return (DP_TransientCanvasState *)DP_canvas_state_incref(
        (DP_CanvasState *)tcs);
}

void DP_transient_canvas_state_decref(DP_TransientCanvasState *tcs)
{
    DP_canvas_state_decref((DP_CanvasState *)tcs);
}

int DP_transient_canvas_state_refcount(DP_TransientCanvasState *tcs)
{
    return DP_canvas_state_refcount((DP_CanvasState *)tcs);
}

DP_CanvasState *DP_transient_canvas_state_persist(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    if (DP_layer_content_list_transient(tcs->layer_contents)) {
        DP_transient_layer_content_list_persist(tcs->transient_layer_contents);
    }
    if (DP_layer_props_list_transient(tcs->layer_props)) {
        DP_transient_layer_props_list_persist(tcs->transient_layer_props);
    }
    tcs->transient = false;
    return (DP_CanvasState *)tcs;
}

void DP_transient_canvas_state_width_set(DP_TransientCanvasState *tcs,
                                         int width)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    tcs->width = width;
}

void DP_transient_canvas_state_height_set(DP_TransientCanvasState *tcs,
                                          int height)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    tcs->height = height;
}

void DP_transient_canvas_state_background_tile_set_noinc(
    DP_TransientCanvasState *tcs, DP_Tile *tile)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_tile_decref_nullable(tcs->background_tile);
    tcs->background_tile = tile;
}

DP_LayerContentList *
DP_transient_canvas_state_layer_contents_noinc(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return DP_canvas_state_layer_contents_noinc((DP_CanvasState *)tcs);
}

DP_LayerPropsList *
DP_transient_canvas_state_layer_props_noinc(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return DP_canvas_state_layer_props_noinc((DP_CanvasState *)tcs);
}

void DP_transient_canvas_state_transient_layer_contents_set_noinc(
    DP_TransientCanvasState *tcs, DP_TransientLayerContentList *tlcl)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_layer_content_list_decref(tcs->layer_contents);
    tcs->transient_layer_contents = tlcl;
}

DP_TransientLayerContentList *
DP_transient_canvas_state_transient_layer_contents(DP_TransientCanvasState *tcs,
                                                   int reserve)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_ASSERT(reserve >= 0);
    DP_LayerContentList *lcl = tcs->layer_contents;
    if (!DP_layer_content_list_transient(lcl)) {
        tcs->transient_layer_contents =
            DP_transient_layer_content_list_new(lcl, reserve);
        DP_layer_content_list_decref(lcl);
    }
    else if (reserve > 0) {
        tcs->transient_layer_contents = DP_transient_layer_content_list_reserve(
            tcs->transient_layer_contents, reserve);
    }
    return tcs->transient_layer_contents;
}

DP_TransientLayerPropsList *
DP_transient_canvas_state_transient_layer_props(DP_TransientCanvasState *tcs,
                                                int reserve)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_ASSERT(reserve >= 0);
    DP_LayerPropsList *lpl = tcs->layer_props;
    if (!DP_layer_props_list_transient(lpl)) {
        tcs->transient_layer_props =
            DP_transient_layer_props_list_new(lpl, reserve);
        DP_layer_props_list_decref(lpl);
    }
    else if (reserve > 0) {
        tcs->transient_layer_props = DP_transient_layer_props_list_reserve(
            tcs->transient_layer_props, reserve);
    }
    return tcs->transient_layer_props;
}
