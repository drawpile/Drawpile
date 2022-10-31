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
#include "annotation.h"
#include "annotation_list.h"
#include "canvas_diff.h"
#include "compress.h"
#include "document_metadata.h"
#include "image.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "ops.h"
#include "paint.h"
#include "tile.h"
#include "timeline.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>
#include <limits.h>


#ifdef DP_NO_STRICT_ALIASING

struct DP_CanvasState {
    DP_Atomic refcount;
    const bool transient;
    const int width, height;
    DP_Tile *const background_tile;
    DP_LayerList *const layers;
    DP_LayerPropsList *const layer_props;
    DP_AnnotationList *const annotations;
    DP_Timeline *const timeline;
    DP_DocumentMetadata *const metadata;
};

struct DP_TransientCanvasState {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    DP_Tile *background_tile;
    union {
        DP_LayerList *layers;
        DP_TransientLayerList *transient_layers;
    };
    union {
        DP_LayerPropsList *layer_props;
        DP_TransientLayerPropsList *transient_layer_props;
    };
    union {
        DP_AnnotationList *annotations;
        DP_TransientAnnotationList *transient_annotations;
    };
    union {
        DP_Timeline *timeline;
        DP_TransientTimeline *transient_timeline;
    };
    union {
        DP_DocumentMetadata *metadata;
        DP_TransientDocumentMetadata *transient_metadata;
    };
};

#else

struct DP_CanvasState {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    DP_Tile *background_tile;
    union {
        DP_LayerList *layers;
        DP_TransientLayerList *transient_layers;
    };
    union {
        DP_LayerPropsList *layer_props;
        DP_TransientLayerPropsList *transient_layer_props;
    };
    union {
        DP_AnnotationList *annotations;
        DP_TransientAnnotationList *transient_annotations;
    };
    union {
        DP_Timeline *timeline;
        DP_TransientTimeline *transient_timeline;
    };
    union {
        DP_DocumentMetadata *metadata;
        DP_TransientDocumentMetadata *transient_metadata;
    };
};

#endif


static DP_TransientCanvasState *allocate_canvas_state(bool transient, int width,
                                                      int height)
{
    DP_TransientCanvasState *cs = DP_malloc(sizeof(*cs));
    *cs = (DP_TransientCanvasState){DP_ATOMIC_INIT(1),
                                    transient,
                                    width,
                                    height,
                                    NULL,
                                    {NULL},
                                    {NULL},
                                    {NULL},
                                    {NULL},
                                    {NULL}};
    return cs;
}

DP_CanvasState *DP_canvas_state_new(void)
{
    DP_TransientCanvasState *tcs = allocate_canvas_state(false, 0, 0);
    tcs->layers = DP_layer_list_new();
    tcs->layer_props = DP_layer_props_list_new();
    tcs->annotations = DP_annotation_list_new();
    tcs->timeline = DP_timeline_new();
    tcs->metadata = DP_document_metadata_new();
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
        DP_document_metadata_decref(cs->metadata);
        DP_timeline_decref(cs->timeline);
        DP_annotation_list_decref(cs->annotations);
        DP_layer_props_list_decref(cs->layer_props);
        DP_layer_list_decref(cs->layers);
        DP_tile_decref_nullable(cs->background_tile);
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

DP_Tile *DP_canvas_state_background_tile_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->background_tile;
}

DP_LayerList *DP_canvas_state_layers_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->layers;
}

DP_LayerPropsList *DP_canvas_state_layer_props_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->layer_props;
}

DP_AnnotationList *DP_canvas_state_annotations_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->annotations;
}

DP_Timeline *DP_canvas_state_timeline_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->timeline;
}

DP_DocumentMetadata *DP_canvas_state_metadata_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->metadata;
}


static DP_CanvasState *handle_canvas_resize(DP_CanvasState *cs,
                                            unsigned int context_id,
                                            DP_MsgCanvasResize *mcr)
{
    return DP_ops_canvas_resize(cs, context_id, DP_msg_canvas_resize_top(mcr),
                                DP_msg_canvas_resize_right(mcr),
                                DP_msg_canvas_resize_bottom(mcr),
                                DP_msg_canvas_resize_left(mcr));
}

static DP_CanvasState *handle_layer_create(DP_CanvasState *cs,
                                           DP_DrawContext *dc,
                                           unsigned int context_id,
                                           DP_MsgLayerCreate *mlc)
{
    int layer_id = DP_msg_layer_create_id(mlc);
    if (layer_id == 0) {
        DP_error_set("Create layer: layer id 0 is invalid");
        return NULL;
    }

    unsigned int flags = DP_msg_layer_create_flags(mlc);
    bool into = flags & DP_MSG_LAYER_CREATE_FLAGS_INTO;
    bool group = flags & DP_MSG_LAYER_CREATE_FLAGS_GROUP;

    uint32_t fill = DP_msg_layer_create_fill(mlc);
    DP_Tile *tile =
        group || fill == 0 ? NULL : DP_tile_new_from_bgra(context_id, fill);

    size_t title_length;
    const char *title = DP_msg_layer_create_name(mlc, &title_length);

    DP_CanvasState *next =
        DP_ops_layer_create(cs, dc, layer_id, DP_msg_layer_create_source(mlc),
                            DP_msg_layer_create_target(mlc), tile, into, group,
                            title, title_length);

    DP_tile_decref_nullable(tile);
    return next;
}

static DP_CanvasState *handle_layer_attr(DP_CanvasState *cs, DP_DrawContext *dc,
                                         DP_MsgLayerAttributes *mla)
{
    int layer_id = DP_msg_layer_attributes_id(mla);
    if (layer_id == 0) {
        DP_error_set("Layer attributes: layer id 0 is invalid");
        return NULL;
    }

    unsigned int flags = DP_msg_layer_attributes_flags(mla);
    bool censored = flags & DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR;
    bool isolated = flags & DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED;

    return DP_ops_layer_attributes(
        cs, dc, layer_id, DP_msg_layer_attributes_sublayer(mla),
        DP_channel8_to_15(DP_msg_layer_attributes_opacity(mla)),
        DP_msg_layer_attributes_blend(mla), censored, isolated);
}


static struct DP_LayerOrderPair get_layer_order_pair(void *user, int index)
{
    const uint16_t *layers = user;
    const uint16_t *pair = &layers[index * 2];
    return (struct DP_LayerOrderPair){pair[0], pair[1]};
}

static DP_CanvasState *handle_layer_order(DP_CanvasState *cs,
                                          DP_DrawContext *dc,
                                          DP_MsgLayerOrder *mlo)
{
    int count;
    const uint16_t *layers = DP_msg_layer_order_layers(mlo, &count);
    if (count != 0 && count % 2 == 0) {
        return DP_ops_layer_order(cs, dc, DP_msg_layer_order_root(mlo),
                                  count / 2, get_layer_order_pair,
                                  (void *)layers);
    }
    else {
        DP_error_set("Layer order: ordering %s",
                     count == 0 ? "empty" : "not even");
        return NULL;
    }
}

static DP_CanvasState *handle_layer_retitle(DP_CanvasState *cs,
                                            DP_DrawContext *dc,
                                            DP_MsgLayerRetitle *mlr)
{
    int layer_id = DP_msg_layer_retitle_id(mlr);
    if (layer_id == 0) {
        DP_error_set("Layer retitle: layer id 0 is invalid");
        return NULL;
    }

    size_t title_length;
    const char *title = DP_msg_layer_retitle_title(mlr, &title_length);

    return DP_ops_layer_retitle(cs, dc, layer_id, title, title_length);
}

static DP_CanvasState *handle_layer_delete(DP_CanvasState *cs,
                                           DP_DrawContext *dc,
                                           unsigned int context_id,
                                           DP_MsgLayerDelete *mld)
{
    int layer_id = DP_msg_layer_delete_id(mld);
    if (layer_id == 0) {
        DP_error_set("Layer delete: layer id 0 is invalid");
        return NULL;
    }

    int merge_layer_id = DP_msg_layer_delete_merge_to(mld);
    if (layer_id == merge_layer_id) {
        DP_error_set("Layer delete: invalid merge of layer %d into itself",
                     layer_id);
        return NULL;
    }

    return DP_ops_layer_delete(cs, dc, context_id, layer_id, merge_layer_id);
}

static DP_CanvasState *handle_layer_visibility(DP_CanvasState *cs,
                                               DP_DrawContext *dc,
                                               DP_MsgLayerVisibility *mlv)
{
    int layer_id = DP_msg_layer_visibility_id(mlv);
    if (layer_id == 0) {
        DP_error_set("Layer visibility: layer id 0 is invalid");
        return NULL;
    }

    return DP_ops_layer_visibility(cs, dc, layer_id,
                                   DP_msg_layer_visibility_visible(mlv));
}

static DP_CanvasState *handle_put_image(DP_CanvasState *cs, DP_DrawContext *dc,
                                        unsigned int context_id,
                                        DP_MsgPutImage *mpi)
{
    int blend_mode = DP_msg_put_image_mode(mpi);
    if (!DP_blend_mode_exists(blend_mode)) {
        DP_error_set("Put image: unknown blend mode %d", blend_mode);
        return NULL;
    }

    size_t image_size;
    const unsigned char *image = DP_msg_put_image_image(mpi, &image_size);
    return DP_ops_put_image(
        cs, dc, context_id, DP_msg_put_image_layer(mpi), blend_mode,
        DP_uint32_to_int(DP_msg_put_image_x(mpi)),
        DP_uint32_to_int(DP_msg_put_image_y(mpi)),
        DP_uint32_to_int(DP_msg_put_image_w(mpi)),
        DP_uint32_to_int(DP_msg_put_image_h(mpi)), image, image_size);
}

static DP_CanvasState *handle_fill_rect(DP_CanvasState *cs, DP_DrawContext *dc,
                                        unsigned int context_id,
                                        DP_MsgFillRect *mfr)
{
    int blend_mode = DP_msg_fill_rect_mode(mfr);
    if (!DP_blend_mode_exists(blend_mode)) {
        DP_error_set("Fill rect: unknown blend mode %d", blend_mode);
        return NULL;
    }
    else if (!DP_blend_mode_valid_for_brush(blend_mode)) {
        DP_error_set("Fill rect: blend mode %s not applicable to brushes",
                     DP_blend_mode_enum_name_unprefixed(blend_mode));
        return NULL;
    }

    int x = DP_uint32_to_int(DP_msg_fill_rect_x(mfr));
    int y = DP_uint32_to_int(DP_msg_fill_rect_y(mfr));
    int width = DP_uint32_to_int(DP_msg_fill_rect_w(mfr));
    int height = DP_uint32_to_int(DP_msg_fill_rect_h(mfr));
    int left = DP_max_int(x, 0);
    int top = DP_max_int(y, 0);
    int right = DP_min_int(x + width, cs->width);
    int bottom = DP_min_int(y + height, cs->height);
    if (left >= right || top >= bottom) {
        DP_error_set("Fill rect: effective area to fill is zero");
        return NULL;
    }

    DP_UPixel15 pixel = DP_upixel15_from_color(DP_msg_fill_rect_color(mfr));
    return DP_ops_fill_rect(cs, dc, context_id, DP_msg_fill_rect_layer(mfr),
                            blend_mode, left, top, right, bottom, pixel);
}

static DP_CanvasState *handle_move_region(DP_CanvasState *cs,
                                          DP_DrawContext *dc,
                                          unsigned int context_id,
                                          DP_MsgMoveRegion *mmr)
{
    int src_x = DP_msg_move_region_bx(mmr);
    int src_y = DP_msg_move_region_by(mmr);
    int src_width = DP_msg_move_region_bw(mmr);
    int src_height = DP_msg_move_region_bh(mmr);
    if (src_width <= 0 || src_height <= 0) {
        DP_error_set("Move region: selection is empty");
        return NULL;
    }

    int canvas_width = cs->width;
    int canvas_height = cs->height;
    DP_Rect canvas_bounds = DP_rect_make(0, 0, canvas_width, canvas_height);

    DP_Rect src_rect = DP_rect_make(src_x, src_y, src_width, src_height);
    if (!DP_rect_intersects(canvas_bounds, src_rect)) {
        DP_error_set("Move region: source out of bounds");
        return NULL;
    }

    DP_Quad dst_quad =
        DP_quad_make(DP_msg_move_region_x1(mmr), DP_msg_move_region_y1(mmr),
                     DP_msg_move_region_x2(mmr), DP_msg_move_region_y2(mmr),
                     DP_msg_move_region_x3(mmr), DP_msg_move_region_y3(mmr),
                     DP_msg_move_region_x4(mmr), DP_msg_move_region_y4(mmr));

    DP_Rect dst_bounds = DP_quad_bounds(dst_quad);
    if (!DP_rect_intersects(canvas_bounds, dst_bounds)) {
        DP_error_set("Move region: destination out of bounds");
        return NULL;
    }

    long long max_size = (canvas_width + 1LL) * (canvas_height + 1LL);
    if (DP_rect_size(dst_bounds) > max_size) {
        DP_error_set("Move region: attempt to scale beyond image size");
        return NULL;
    }

    size_t in_mask_size;
    const unsigned char *in_mask = DP_msg_move_region_mask(mmr, &in_mask_size);
    DP_Image *mask;
    if (in_mask_size > 0) {
        mask = DP_image_new_from_compressed_monochrome(src_width, src_height,
                                                       in_mask, in_mask_size);
        if (!mask) {
            return NULL;
        }
    }
    else {
        mask = NULL;
    }

    DP_CanvasState *next =
        DP_ops_move_region(cs, dc, context_id, DP_msg_move_region_layer(mmr),
                           &src_rect, &dst_quad, mask);
    DP_free(mask);
    return next;
}

static DP_CanvasState *handle_put_tile(DP_CanvasState *cs, DP_DrawContext *dc,
                                       unsigned int context_id,
                                       DP_MsgPutTile *mpt)
{
    DP_TileCounts tile_counts = DP_tile_counts_round(cs->width, cs->height);
    int tile_total = tile_counts.x * tile_counts.y;
    int x = DP_msg_put_tile_col(mpt);
    int y = DP_msg_put_tile_row(mpt);
    int start = y * tile_counts.x + x;
    if (start >= tile_total) {
        DP_error_set("Put tile: starting index %d beyond total %d", start,
                     tile_total);
        return NULL;
    }

    size_t image_size;
    const unsigned char *image = DP_msg_put_tile_image(mpt, &image_size);
    DP_Tile *tile =
        DP_tile_new_from_compressed(dc, context_id, image, image_size);
    if (!tile) {
        return NULL;
    }

    DP_CanvasState *next = DP_ops_put_tile(
        cs, dc, tile, DP_msg_put_tile_layer(mpt), DP_msg_put_tile_sublayer(mpt),
        x, y, DP_msg_put_tile_repeat(mpt));

    DP_tile_decref(tile);
    return next;
}

static DP_CanvasState *handle_canvas_background(DP_CanvasState *cs,
                                                DP_DrawContext *dc,
                                                unsigned int context_id,
                                                DP_MsgCanvasBackground *mcb)
{
    size_t image_size;
    const unsigned char *image =
        DP_msg_canvas_background_image(mcb, &image_size);
    DP_Tile *tile =
        DP_tile_new_from_compressed(dc, context_id, image, image_size);

    if (tile) {
        DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
        DP_transient_canvas_state_background_tile_set_noinc(tcs, tile);
        return DP_transient_canvas_state_persist(tcs);
    }
    else {
        return NULL;
    }
}

static DP_CanvasState *handle_pen_up(DP_CanvasState *cs, DP_DrawContext *dc,
                                     unsigned int context_id)
{
    return DP_ops_pen_up(cs, dc, context_id);
}

static DP_CanvasState *handle_annotation_create(DP_CanvasState *cs,
                                                DP_MsgAnnotationCreate *mac)
{
    return DP_ops_annotation_create(
        cs, DP_msg_annotation_create_id(mac), DP_msg_annotation_create_x(mac),
        DP_msg_annotation_create_y(mac), DP_msg_annotation_create_w(mac),
        DP_msg_annotation_create_h(mac));
}

static DP_CanvasState *handle_annotation_reshape(DP_CanvasState *cs,
                                                 DP_MsgAnnotationReshape *mar)
{
    return DP_ops_annotation_reshape(
        cs, DP_msg_annotation_reshape_id(mar), DP_msg_annotation_reshape_x(mar),
        DP_msg_annotation_reshape_y(mar), DP_msg_annotation_reshape_w(mar),
        DP_msg_annotation_reshape_h(mar));
}

static int annotation_valign_from_flags(int flags)
{
    int mask = DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER
             | DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_BOTTOM;
    switch (flags & mask) {
    case DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER:
        return DP_ANNOTATION_VALIGN_CENTER;
    case DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_BOTTOM:
        return DP_ANNOTATION_VALIGN_BOTTOM;
    default:
        return DP_ANNOTATION_VALIGN_TOP;
    }
}

static DP_CanvasState *handle_annotation_edit(DP_CanvasState *cs,
                                              DP_MsgAnnotationEdit *mae)
{
    int flags = DP_msg_annotation_edit_flags(mae);
    bool protect = flags & DP_MSG_ANNOTATION_EDIT_FLAGS_PROTECT;
    int valign = annotation_valign_from_flags(flags);
    size_t text_length;
    const char *text = DP_msg_annotation_edit_text(mae, &text_length);
    return DP_ops_annotation_edit(cs, DP_msg_annotation_edit_id(mae),
                                  DP_msg_annotation_edit_bg(mae), protect,
                                  valign, text, text_length);
}

static DP_CanvasState *handle_annotation_delete(DP_CanvasState *cs,
                                                DP_MsgAnnotationDelete *mad)
{
    return DP_ops_annotation_delete(cs, DP_msg_annotation_delete_id(mad));
}

static DP_CanvasState *handle_draw_dabs(DP_CanvasState *cs, int layer_id,
                                        bool indirect,
                                        DP_PaintDrawDabsParams params)
{
    int blend_mode = params.blend_mode;
    if (!DP_blend_mode_exists(blend_mode)) {
        DP_error_set("Draw dabs: unknown blend mode %d", blend_mode);
        return NULL;
    }
    else if (!DP_blend_mode_valid_for_brush(blend_mode)) {
        DP_error_set("Draw dabs: blend mode %s not applicable to brushes",
                     DP_blend_mode_enum_name_unprefixed(blend_mode));
        return NULL;
    }

    if (params.dab_count < 1) {
        return DP_canvas_state_incref(cs); // Nothing to do here.
    }

    int sublayer_id, sublayer_opacity, sublayer_blend_mode;
    if (indirect) {
        sublayer_id = DP_uint_to_int(params.context_id);
        sublayer_opacity = DP_channel8_to_15(
            DP_uint32_to_uint8((params.color & 0xff000000) >> 24));
        sublayer_blend_mode = blend_mode;
        params.blend_mode = DP_BLEND_MODE_NORMAL;
    }
    else {
        sublayer_id = 0;
        sublayer_opacity = -1;
        sublayer_blend_mode = -1;
    }

    return DP_ops_draw_dabs(cs, layer_id, sublayer_id, sublayer_blend_mode,
                            sublayer_opacity, &params);
}

static DP_CanvasState *handle_draw_dabs_classic(DP_CanvasState *cs,
                                                DP_DrawContext *dc,
                                                unsigned int context_id,
                                                DP_MsgDrawDabsClassic *mddc)
{
    int dab_count;
    const DP_ClassicDab *dabs = DP_msg_draw_dabs_classic_dabs(mddc, &dab_count);
    return handle_draw_dabs(
        cs, DP_msg_draw_dabs_classic_layer(mddc),
        DP_msg_draw_dabs_classic_indirect(mddc),
        (DP_PaintDrawDabsParams){DP_MSG_DRAW_DABS_CLASSIC,
                                 dc,
                                 context_id,
                                 DP_msg_draw_dabs_classic_x(mddc),
                                 DP_msg_draw_dabs_classic_y(mddc),
                                 DP_msg_draw_dabs_classic_color(mddc),
                                 DP_msg_draw_dabs_classic_mode(mddc),
                                 dab_count,
                                 {.classic = {dabs}}});
}

static DP_CanvasState *handle_draw_dabs_pixel(DP_CanvasState *cs,
                                              DP_DrawContext *dc,
                                              DP_MessageType type,
                                              unsigned int context_id,
                                              DP_MsgDrawDabsPixel *mddp)
{
    int dab_count;
    const DP_PixelDab *dabs = DP_msg_draw_dabs_pixel_dabs(mddp, &dab_count);
    return handle_draw_dabs(
        cs, DP_msg_draw_dabs_pixel_layer(mddp),
        DP_msg_draw_dabs_pixel_indirect(mddp),
        (DP_PaintDrawDabsParams){(int)type,
                                 dc,
                                 context_id,
                                 DP_msg_draw_dabs_pixel_x(mddp),
                                 DP_msg_draw_dabs_pixel_y(mddp),
                                 DP_msg_draw_dabs_pixel_color(mddp),
                                 DP_msg_draw_dabs_pixel_mode(mddp),
                                 dab_count,
                                 {.pixel = {dabs}}});
}

static DP_CanvasState *handle_draw_dabs_mypaint(DP_CanvasState *cs,
                                                DP_DrawContext *dc,
                                                unsigned int context_id,
                                                DP_MsgDrawDabsMyPaint *mddmp)
{
    int dab_count;
    const DP_MyPaintDab *dabs =
        DP_msg_draw_dabs_mypaint_dabs(mddmp, &dab_count);
    return handle_draw_dabs(
        cs, DP_msg_draw_dabs_mypaint_layer(mddmp), false,
        (DP_PaintDrawDabsParams){
            DP_MSG_DRAW_DABS_MYPAINT,
            dc,
            context_id,
            DP_msg_draw_dabs_mypaint_x(mddmp),
            DP_msg_draw_dabs_mypaint_y(mddmp),
            DP_msg_draw_dabs_mypaint_color(mddmp),
            DP_BLEND_MODE_NORMAL_AND_ERASER,
            dab_count,
            {.mypaint = {dabs, DP_msg_draw_dabs_mypaint_lock_alpha(mddmp),
                         // TODO: Fill in colorize, posterize and posterize_num
                         // when they're implemented into the protocol.
                         0, 0, 0}}});
}

static DP_CanvasState *handle_move_rect(DP_CanvasState *cs, DP_DrawContext *dc,
                                        unsigned int context_id,
                                        DP_MsgMoveRect *mmr)
{
    int width = DP_msg_move_rect_w(mmr);
    int height = DP_msg_move_rect_h(mmr);
    if (width <= 0 || height <= 0) {
        DP_error_set("Move rect: selection is empty");
        return NULL;
    }

    DP_Rect canvas_bounds = DP_rect_make(0, 0, DP_canvas_state_width(cs),
                                         DP_canvas_state_height(cs));
    DP_Rect src_rect = DP_rect_make(DP_msg_move_rect_sx(mmr),
                                    DP_msg_move_rect_sy(mmr), width, height);
    if (!DP_rect_intersects(canvas_bounds, src_rect)) {
        DP_error_set("Move rect: source out of bounds");
        return NULL;
    }

    int dst_x = DP_msg_move_rect_tx(mmr);
    int dst_y = DP_msg_move_rect_ty(mmr);
    if (!DP_rect_intersects(canvas_bounds,
                            DP_rect_make(dst_x, dst_y, width, height))) {
        DP_error_set("Move rect: destination out of bounds");
        return NULL;
    }

    size_t in_mask_size;
    const unsigned char *in_mask = DP_msg_move_rect_mask(mmr, &in_mask_size);
    DP_Image *mask;
    if (in_mask_size > 0) {
        // TODO: this could be monochrome, but the move rect message sends
        // 32 bit masks instead. Could probably make that more efficient.
        mask =
            DP_image_new_from_compressed(width, height, in_mask, in_mask_size);
        if (!mask) {
            return NULL;
        }
    }
    else {
        mask = NULL;
    }

    DP_CanvasState *next =
        DP_ops_move_rect(cs, dc, context_id, DP_msg_move_rect_layer(mmr),
                         &src_rect, dst_x, dst_y, mask);
    DP_free(mask);
    return next;
}

static DP_CanvasState *handle_set_metadata_int(DP_CanvasState *cs,
                                               DP_MsgSetMetadataInt *msmi)
{
    int field = DP_msg_set_metadata_int_field(msmi);
    void (*set_field)(DP_TransientDocumentMetadata *, int);
    switch (field) {
    case DP_MSG_SET_METADATA_INT_FIELD_DPIX:
        set_field = DP_transient_document_metadata_dpix_set;
        break;
    case DP_MSG_SET_METADATA_INT_FIELD_DPIY:
        set_field = DP_transient_document_metadata_dpiy_set;
        break;
    case DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE:
        set_field = DP_transient_document_metadata_framerate_set;
        break;
    case DP_MSG_SET_METADATA_INT_FIELD_USE_TIMELINE:
        set_field = DP_transient_document_metadata_use_timeline_set;
        break;
    default:
        DP_error_set("Set metadata int: unknown field %d", field);
        return NULL;
    }
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientDocumentMetadata *tdm =
        DP_transient_canvas_state_transient_metadata(tcs);
    set_field(tdm, DP_msg_set_metadata_int_value(msmi));
    return DP_transient_canvas_state_persist(tcs);
}

static DP_CanvasState *handle_set_metadata_str(DP_UNUSED DP_CanvasState *cs,
                                               DP_MsgSetMetadataStr *msms)
{
    // No string metadata is actually implemented, so just error out.
    int field = DP_msg_set_metadata_str_field(msms);
    DP_error_set("Set metadata str: unknown field %d", field);
    return NULL;
}

static int get_layer_id(void *user, int index)
{
    const uint16_t *layer_ids = user;
    return layer_ids[index];
}

static DP_CanvasState *handle_set_timeline_frame(DP_CanvasState *cs,
                                                 DP_MsgSetTimelineFrame *mstf)
{
    int layer_id_count;
    const uint16_t *layer_ids =
        DP_msg_set_timeline_frame_layers(mstf, &layer_id_count);
    return DP_ops_timeline_frame_set(cs, DP_msg_set_timeline_frame_frame(mstf),
                                     DP_msg_set_timeline_frame_insert(mstf),
                                     layer_id_count, get_layer_id,
                                     (void *)layer_ids);
}

static DP_CanvasState *
handle_remove_timeline_frame(DP_CanvasState *cs,
                             DP_MsgRemoveTimelineFrame *mrtf)
{
    return DP_ops_timeline_frame_delete(
        cs, DP_msg_remove_timeline_frame_frame(mrtf));
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
        return handle_layer_create(cs, dc, DP_message_context_id(msg),
                                   DP_msg_layer_create_cast(msg));
    case DP_MSG_LAYER_ATTRIBUTES:
        return handle_layer_attr(cs, dc, DP_msg_layer_attributes_cast(msg));
    case DP_MSG_LAYER_ORDER:
        return handle_layer_order(cs, dc, DP_msg_layer_order_cast(msg));
    case DP_MSG_LAYER_RETITLE:
        return handle_layer_retitle(cs, dc, DP_msg_layer_retitle_cast(msg));
    case DP_MSG_LAYER_DELETE:
        return handle_layer_delete(cs, dc, DP_message_context_id(msg),
                                   DP_msg_layer_delete_cast(msg));
    case DP_MSG_LAYER_VISIBILITY:
        return handle_layer_visibility(cs, dc,
                                       DP_msg_layer_visibility_cast(msg));
    case DP_MSG_PUT_IMAGE:
        return handle_put_image(cs, dc, DP_message_context_id(msg),
                                DP_msg_put_image_cast(msg));
    case DP_MSG_FILL_RECT:
        return handle_fill_rect(cs, dc, DP_message_context_id(msg),
                                DP_msg_fill_rect_cast(msg));
    case DP_MSG_MOVE_REGION:
        return handle_move_region(cs, dc, DP_message_context_id(msg),
                                  DP_msg_move_region_cast(msg));
    case DP_MSG_PUT_TILE:
        return handle_put_tile(cs, dc, DP_message_context_id(msg),
                               DP_msg_put_tile_cast(msg));
    case DP_MSG_CANVAS_BACKGROUND:
        return handle_canvas_background(cs, dc, DP_message_context_id(msg),
                                        DP_msg_canvas_background_cast(msg));
    case DP_MSG_PEN_UP:
        return handle_pen_up(cs, dc, DP_message_context_id(msg));
    case DP_MSG_ANNOTATION_CREATE:
        return handle_annotation_create(cs, DP_msg_annotation_create_cast(msg));
    case DP_MSG_ANNOTATION_RESHAPE:
        return handle_annotation_reshape(cs,
                                         DP_msg_annotation_reshape_cast(msg));
    case DP_MSG_ANNOTATION_EDIT:
        return handle_annotation_edit(cs, DP_msg_annotation_edit_cast(msg));
    case DP_MSG_ANNOTATION_DELETE:
        return handle_annotation_delete(cs, DP_msg_annotation_delete_cast(msg));
    case DP_MSG_DRAW_DABS_CLASSIC:
        return handle_draw_dabs_classic(cs, dc, DP_message_context_id(msg),
                                        DP_msg_draw_dabs_classic_cast(msg));
    case DP_MSG_DRAW_DABS_PIXEL:
        return handle_draw_dabs_pixel(cs, dc, DP_MSG_DRAW_DABS_PIXEL,
                                      DP_message_context_id(msg),
                                      DP_msg_draw_dabs_pixel_cast(msg));
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
        return handle_draw_dabs_pixel(cs, dc, DP_MSG_DRAW_DABS_PIXEL_SQUARE,
                                      DP_message_context_id(msg),
                                      DP_msg_draw_dabs_pixel_square_cast(msg));
    case DP_MSG_DRAW_DABS_MYPAINT:
        return handle_draw_dabs_mypaint(cs, dc, DP_message_context_id(msg),
                                        DP_msg_draw_dabs_mypaint_cast(msg));
    case DP_MSG_MOVE_RECT:
        return handle_move_rect(cs, dc, DP_message_context_id(msg),
                                DP_msg_move_rect_cast(msg));
    case DP_MSG_SET_METADATA_INT:
        return handle_set_metadata_int(cs, DP_msg_set_metadata_int_cast(msg));
    case DP_MSG_SET_METADATA_STR:
        return handle_set_metadata_str(cs, DP_msg_set_metadata_str_cast(msg));
    case DP_MSG_SET_TIMELINE_FRAME:
        return handle_set_timeline_frame(cs,
                                         DP_msg_set_timeline_frame_cast(msg));
    case DP_MSG_REMOVE_TIMELINE_FRAME:
        return handle_remove_timeline_frame(
            cs, DP_msg_remove_timeline_frame_cast(msg));
    default:
        DP_error_set("Unhandled draw message type %d", (int)type);
        return NULL;
    }
}

int DP_canvas_state_search_change_bounds(DP_CanvasState *cs,
                                         unsigned int context_id, int *out_x,
                                         int *out_y, int *out_width,
                                         int *out_height)
{
    DP_ASSERT(cs);
    DP_ASSERT(context_id != 0);
    DP_ASSERT(context_id <= INT_MAX);
    DP_LayerList *ll = DP_canvas_state_layers_noinc(cs);
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    int count = DP_layer_list_count(cs->layers);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            int layer_id = DP_layer_group_search_change_bounds(
                DP_layer_list_entry_group_noinc(lle),
                DP_layer_props_list_at_noinc(lpl, i), context_id, out_x, out_y,
                out_width, out_height);
            if (layer_id != 0) {
                return layer_id;
            }
        }
        else if (DP_layer_content_search_change_bounds(
                     DP_layer_list_entry_content_noinc(lle), context_id, out_x,
                     out_y, out_width, out_height)) {
            return DP_layer_props_id(DP_layer_props_list_at_noinc(lpl, i));
        }
    }
    return 0;
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
    bool include_sublayers = flags & DP_FLAT_IMAGE_INCLUDE_SUBLAYERS;
    DP_layer_list_merge_to_flat_image(cs->layers, cs->layer_props, tlc,
                                      DP_BIT15, include_sublayers);

    // Write it all to an image and toss the flattening layer.
    DP_LayerContent *lc = DP_transient_layer_content_persist(tlc);
    DP_Image *img = DP_layer_content_to_image(lc);
    DP_layer_content_decref(lc);
    return img;
}

DP_TransientTile *DP_canvas_state_flatten_tile(DP_CanvasState *cs,
                                               int tile_index)
{
    DP_ASSERT(cs);
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(cs->width, cs->height));
    DP_Tile *background_tile = cs->background_tile;
    DP_TransientTile *tt = background_tile
                             ? DP_transient_tile_new(background_tile, 0)
                             : DP_transient_tile_new_blank(0);
    return DP_layer_list_flatten_tile_to(cs->layers, cs->layer_props,
                                         tile_index, tt, DP_BIT15);
}

DP_TransientTile *DP_canvas_state_flatten_tile_at(DP_CanvasState *cs, int x,
                                                  int y)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_ASSERT(x >= 0);
    DP_ASSERT(x < cs->width);
    DP_ASSERT(y >= 0);
    DP_ASSERT(y < cs->height);
    int i = y * DP_tile_count_round(cs->width) + x;
    return DP_canvas_state_flatten_tile(cs, i);
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
        DP_layer_list_diff(cs->layers, cs->layer_props, prev->layers,
                           prev->layer_props, diff);
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
    tcs->layers = DP_layer_list_new();
    tcs->layer_props = DP_layer_props_list_new();
    tcs->annotations = DP_annotation_list_new();
    tcs->timeline = DP_timeline_new();
    tcs->metadata = DP_document_metadata_new();
    return tcs;
}

static DP_TransientCanvasState *new_transient_canvas_state(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_ASSERT(!cs->transient);
    DP_TransientCanvasState *tcs =
        allocate_canvas_state(true, cs->width, cs->height);
    tcs->background_tile = DP_tile_incref_nullable(cs->background_tile);
    return tcs;
}

DP_TransientCanvasState *DP_transient_canvas_state_new(DP_CanvasState *cs)
{
    DP_TransientCanvasState *tcs = new_transient_canvas_state(cs);
    tcs->layers = DP_layer_list_incref(cs->layers);
    tcs->layer_props = DP_layer_props_list_incref(cs->layer_props);
    tcs->annotations = DP_annotation_list_incref(cs->annotations);
    tcs->timeline = DP_timeline_incref(cs->timeline);
    tcs->metadata = DP_document_metadata_incref(cs->metadata);
    return tcs;
}

DP_TransientCanvasState *DP_transient_canvas_state_new_with_layers_noinc(
    DP_CanvasState *cs, DP_TransientLayerList *tll,
    DP_TransientLayerPropsList *tlpl)
{
    DP_TransientCanvasState *tcs = new_transient_canvas_state(cs);
    tcs->transient_layers = tll;
    tcs->transient_layer_props = tlpl;
    tcs->annotations = DP_annotation_list_incref(cs->annotations);
    tcs->timeline = DP_timeline_incref(cs->timeline);
    tcs->metadata = DP_document_metadata_incref(cs->metadata);
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
    if (DP_layer_list_transient(tcs->layers)) {
        DP_transient_layer_list_persist(tcs->transient_layers);
    }
    if (DP_layer_props_list_transient(tcs->layer_props)) {
        DP_transient_layer_props_list_persist(tcs->transient_layer_props);
    }
    if (DP_annotation_list_transient(tcs->annotations)) {
        DP_transient_annotation_list_persist(tcs->transient_annotations);
    }
    if (DP_timeline_transient(tcs->timeline)) {
        DP_transient_timeline_persist(tcs->transient_timeline);
    }
    if (DP_document_metadata_transient(tcs->metadata)) {
        DP_transient_document_metadata_persist(tcs->transient_metadata);
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

DP_LayerList *
DP_transient_canvas_state_layers_noinc(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return DP_canvas_state_layers_noinc((DP_CanvasState *)tcs);
}

DP_LayerPropsList *
DP_transient_canvas_state_layer_props_noinc(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return DP_canvas_state_layer_props_noinc((DP_CanvasState *)tcs);
}

void DP_transient_canvas_state_transient_layers_set_noinc(
    DP_TransientCanvasState *tcs, DP_TransientLayerList *tll)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_layer_list_decref(tcs->layers);
    tcs->transient_layers = tll;
}

DP_TransientLayerList *
DP_transient_canvas_state_transient_layers(DP_TransientCanvasState *tcs,
                                           int reserve)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_ASSERT(reserve >= 0);
    DP_LayerList *ll = tcs->layers;
    if (!DP_layer_list_transient(ll)) {
        tcs->transient_layers = DP_transient_layer_list_new(ll, reserve);
        DP_layer_list_decref(ll);
    }
    else if (reserve > 0) {
        tcs->transient_layers =
            DP_transient_layer_list_reserve(tcs->transient_layers, reserve);
    }
    return tcs->transient_layers;
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

DP_TransientAnnotationList *
DP_transient_canvas_state_transient_annotations(DP_TransientCanvasState *tcs,
                                                int reserve)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_ASSERT(reserve >= 0);
    DP_AnnotationList *al = tcs->annotations;
    if (!DP_annotation_list_transient(al)) {
        tcs->transient_annotations =
            DP_transient_annotation_list_new(al, reserve);
        DP_annotation_list_decref(al);
    }
    else if (reserve > 0) {
        tcs->transient_annotations = DP_transient_annotation_list_reserve(
            tcs->transient_annotations, reserve);
    }
    return tcs->transient_annotations;
}

DP_TransientTimeline *
DP_transient_canvas_state_transient_timeline(DP_TransientCanvasState *tcs,
                                             int reserve)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_Timeline *tl = tcs->timeline;
    if (!DP_timeline_transient(tl)) {
        tcs->transient_timeline = DP_transient_timeline_new(tl, reserve);
        DP_timeline_decref(tl);
    }
    else if (reserve > 0) {
        tcs->transient_timeline =
            DP_transient_timeline_reserve(tcs->transient_timeline, reserve);
    }
    return tcs->transient_timeline;
}

DP_TransientDocumentMetadata *
DP_transient_canvas_state_transient_metadata(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_DocumentMetadata *dm = tcs->metadata;
    if (!DP_document_metadata_transient(dm)) {
        tcs->transient_metadata = DP_transient_document_metadata_new(dm);
        DP_document_metadata_decref(dm);
    }
    return tcs->transient_metadata;
}
