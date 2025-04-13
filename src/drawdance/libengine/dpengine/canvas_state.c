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
#include "brush.h"
#include "canvas_diff.h"
#include "compress.h"
#include "document_metadata.h"
#include "draw_context.h"
#include "image.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "layer_routes.h"
#include "ops.h"
#include "paint.h"
#include "selection_set.h"
#include "tile.h"
#include "tile_iterator.h"
#include "timeline.h"
#include "track.h"
#include "view_mode.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/perf.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>

#define DP_PERF_CONTEXT "canvas_state"


#ifdef DP_NO_STRICT_ALIASING

struct DP_CanvasState {
    DP_Atomic refcount;
    const bool transient;
    const int width, height;
    const int offset_x, offset_y;
    DP_Tile *const background_tile;
    const bool background_opaque;
    DP_LayerList *const layers;
    DP_LayerPropsList *const layer_props;
    DP_LayerRoutes *const layer_routes;
    DP_AnnotationList *const annotations;
    DP_Timeline *const timeline;
    DP_DocumentMetadata *const metadata;
    DP_SelectionSet *const selections;
};

struct DP_TransientCanvasState {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    int offset_x, offset_y;
    DP_Tile *background_tile;
    bool background_opaque;
    union {
        DP_LayerList *layers;
        DP_TransientLayerList *transient_layers;
    };
    union {
        DP_LayerPropsList *layer_props;
        DP_TransientLayerPropsList *transient_layer_props;
    };
    DP_LayerRoutes *layer_routes;
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
    union {
        DP_SelectionSet *selections;
        DP_TransientSelectionSet *transient_selections;
    };
};

#else

struct DP_CanvasState {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    int offset_x, offset_y;
    DP_Tile *background_tile;
    bool background_opaque;
    union {
        DP_LayerList *layers;
        DP_TransientLayerList *transient_layers;
    };
    union {
        DP_LayerPropsList *layer_props;
        DP_TransientLayerPropsList *transient_layer_props;
    };
    DP_LayerRoutes *layer_routes;
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
    union {
        DP_SelectionSet *selections;
        DP_TransientSelectionSet *transient_selections;
    };
};

#endif


static DP_TransientCanvasState *allocate_canvas_state(bool transient, int width,
                                                      int height, int offset_x,
                                                      int offset_y)
{
    DP_TransientCanvasState *cs = DP_malloc(sizeof(*cs));
    *cs = (DP_TransientCanvasState){DP_ATOMIC_INIT(1),
                                    transient,
                                    width,
                                    height,
                                    offset_x,
                                    offset_y,
                                    NULL,
                                    false,
                                    {NULL},
                                    {NULL},
                                    NULL,
                                    {NULL},
                                    {NULL},
                                    {NULL},
                                    {NULL}};
    return cs;
}

DP_CanvasState *DP_canvas_state_new(void)
{
    DP_TransientCanvasState *tcs = allocate_canvas_state(false, 0, 0, 0, 0);
    tcs->layers = DP_layer_list_new();
    tcs->layer_props = DP_layer_props_list_new();
    tcs->layer_routes = DP_layer_routes_new();
    tcs->annotations = DP_annotation_list_new();
    tcs->timeline = DP_timeline_new();
    tcs->metadata = DP_document_metadata_new();
    return (DP_CanvasState *)tcs;
}

DP_CanvasState *DP_canvas_state_new_with_selections_noinc(DP_CanvasState *cs,
                                                          DP_SelectionSet *ss)
{
    DP_TransientCanvasState *tcs = allocate_canvas_state(
        false, cs->width, cs->height, cs->offset_x, cs->offset_y);
    tcs->background_tile = DP_tile_incref_nullable(cs->background_tile);
    tcs->background_opaque = cs->background_opaque;
    tcs->layers = DP_layer_list_incref(cs->layers);
    tcs->layer_props = DP_layer_props_list_incref(cs->layer_props);
    tcs->layer_routes = DP_layer_routes_incref(cs->layer_routes);
    tcs->annotations = DP_annotation_list_incref(cs->annotations);
    tcs->timeline = DP_timeline_incref(cs->timeline);
    tcs->metadata = DP_document_metadata_incref(cs->metadata);
    tcs->selections = ss;
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
        DP_selection_set_decref_nullable(cs->selections);
        DP_document_metadata_decref(cs->metadata);
        DP_timeline_decref(cs->timeline);
        DP_annotation_list_decref(cs->annotations);
        DP_layer_routes_decref(cs->layer_routes);
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

int DP_canvas_state_offset_x(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->offset_x;
}

int DP_canvas_state_offset_y(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->offset_y;
}

DP_Tile *DP_canvas_state_background_tile_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->background_tile;
}

bool DP_canvas_state_background_opaque(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->background_opaque;
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

DP_LayerRoutes *DP_canvas_state_layer_routes_noinc(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->layer_routes;
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

DP_SelectionSet *DP_canvas_state_selections_noinc_nullable(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return cs->selections;
}

DP_Selection *DP_canvas_state_selection_search_noinc(DP_CanvasState *cs,
                                                     unsigned int context_id,
                                                     int selection_id)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_SelectionSet *ss = cs->selections;
    return ss ? DP_selection_set_search_noinc(ss, context_id, selection_id)
              : NULL;
}

int DP_canvas_state_frame_count(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return DP_document_metadata_frame_count(cs->metadata);
}

int DP_canvas_state_framerate(DP_CanvasState *cs)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return DP_document_metadata_framerate(cs->metadata);
}

bool DP_canvas_state_same_frame(DP_CanvasState *cs, int frame_index_a,
                                int frame_index_b)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return DP_timeline_same_frame(DP_canvas_state_timeline_noinc(cs),
                                  frame_index_a, frame_index_b);
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

static DP_CanvasState *handle_layer_attr(DP_CanvasState *cs,
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
    bool clip = flags & DP_MSG_LAYER_ATTRIBUTES_FLAGS_CLIP;

    return DP_ops_layer_attributes(
        cs, layer_id, DP_msg_layer_attributes_sublayer(mla),
        DP_channel8_to_15(DP_msg_layer_attributes_opacity(mla)),
        DP_msg_layer_attributes_blend(mla), censored, isolated, clip);
}


static DP_CanvasState *handle_layer_retitle(DP_CanvasState *cs,
                                            DP_MsgLayerRetitle *mlr)
{
    int layer_id = DP_msg_layer_retitle_id(mlr);
    if (layer_id == 0) {
        DP_error_set("Layer retitle: layer id 0 is invalid");
        return NULL;
    }

    size_t title_length;
    const char *title = DP_msg_layer_retitle_title(mlr, &title_length);

    return DP_ops_layer_retitle(cs, layer_id, title, title_length);
}

static DP_CanvasState *selection_put(DP_CanvasState *cs,
                                     unsigned int context_id, int selection_id,
                                     int op, int x, int y, int width,
                                     int height, size_t in_mask_size,
                                     const unsigned char *in_mask)
{
    if (selection_id == 0) {
        DP_error_set("Selection put: invalid selection id %d", selection_id);
        return NULL;
    }

    if (op >= DP_MSG_SELECTION_PUT_NUM_OP) {
        DP_error_set("Selection put: unknown op %d", op);
        return NULL;
    }

    int left = DP_max_int(x, 0);
    int top = DP_max_int(y, 0);
    int right = DP_min_int(x + width, cs->width);
    int bottom = DP_min_int(y + height, cs->height);
    if (left >= right || top >= bottom) {
        if (op == DP_MSG_SELECTION_PUT_OP_REPLACE) {
            return DP_ops_selection_clear(cs, context_id, selection_id);
        }
        else {
            return DP_canvas_state_incref(cs);
        }
    }

    if (in_mask_size == 0) {
        static_assert(DP_MSG_SELECTION_PUT_NUM_OP == 5, "Update selection ops");
        switch (op) {
        case DP_MSG_SELECTION_PUT_OP_REPLACE:
            return DP_ops_selection_put_replace(cs, context_id, selection_id,
                                                left, top, right, bottom, NULL);
        case DP_MSG_SELECTION_PUT_OP_UNITE:
            return DP_ops_selection_put_unite(cs, context_id, selection_id,
                                              left, top, right, bottom, NULL);
        case DP_MSG_SELECTION_PUT_OP_INTERSECT:
            return DP_ops_selection_put_intersect(
                cs, context_id, selection_id, left, top, right, bottom, NULL);
        case DP_MSG_SELECTION_PUT_OP_EXCLUDE:
            return DP_ops_selection_put_exclude(cs, context_id, selection_id,
                                                left, top, right, bottom, NULL);
        case DP_MSG_SELECTION_PUT_OP_COMPLEMENT:
            return DP_ops_selection_put_complement(
                cs, context_id, selection_id, left, top, right, bottom, NULL);
        default:
            DP_UNREACHABLE();
        }
    }
    else {
        DP_Image *mask = DP_image_new_from_compressed_alpha_mask(
            width, height, in_mask, in_mask_size);
        if (!mask) {
            return NULL;
        }

        DP_CanvasState *next_cs;
        switch (op) {
        case DP_MSG_SELECTION_PUT_OP_REPLACE:
            next_cs =
                DP_ops_selection_put_replace(cs, context_id, selection_id, x, y,
                                             x + width, y + height, mask);
            break;
        case DP_MSG_SELECTION_PUT_OP_UNITE:
            next_cs =
                DP_ops_selection_put_unite(cs, context_id, selection_id, x, y,
                                           x + width, y + height, mask);
            break;
        case DP_MSG_SELECTION_PUT_OP_INTERSECT:
            next_cs =
                DP_ops_selection_put_intersect(cs, context_id, selection_id, x,
                                               y, x + width, y + height, mask);
            break;
        case DP_MSG_SELECTION_PUT_OP_EXCLUDE:
            next_cs =
                DP_ops_selection_put_exclude(cs, context_id, selection_id, x, y,
                                             x + width, y + height, mask);
            break;
        default:
            DP_UNREACHABLE();
        }

        DP_image_free(mask);
        return next_cs;
    }
}

static DP_CanvasState *
selection_clear(DP_CanvasState *cs, unsigned int context_id, int selection_id)
{
    return DP_ops_selection_clear(cs, context_id, selection_id);
}

static DP_CanvasState *handle_put_image(DP_CanvasState *cs,
                                        DP_UserCursors *ucs_or_null,
                                        unsigned int context_id,
                                        DP_MsgPutImage *mpi)
{
    int blend_mode = DP_msg_put_image_mode(mpi);
    if (blend_mode == DP_BLEND_MODE_COMPAT_LOCAL_MATCH) {
        // This is a local match message disguised as a put image one for
        // compatibility. We don't need to act on local matches.
        return DP_canvas_state_incref(cs);
    }
    else if (!DP_blend_mode_exists(blend_mode)) {
        DP_error_set("Put image: unknown blend mode %d", blend_mode);
        return NULL;
    }

    size_t image_size;
    const unsigned char *image = DP_msg_put_image_image(mpi, &image_size);
    return DP_ops_put_image(
        cs, ucs_or_null, context_id, DP_msg_put_image_layer(mpi), blend_mode,
        DP_uint32_to_int(DP_msg_put_image_x(mpi)),
        DP_uint32_to_int(DP_msg_put_image_y(mpi)),
        DP_uint32_to_int(DP_msg_put_image_w(mpi)),
        DP_uint32_to_int(DP_msg_put_image_h(mpi)), image, image_size);
}

static DP_CanvasState *handle_fill_rect(DP_CanvasState *cs,
                                        DP_UserCursors *ucs_or_null,
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
    return DP_ops_fill_rect(cs, ucs_or_null, context_id,
                            DP_msg_fill_rect_layer(mfr), blend_mode, left, top,
                            right, bottom, pixel);
}

static DP_CanvasState *
handle_region(DP_CanvasState *cs, DP_DrawContext *dc,
              DP_UserCursors *ucs_or_null, unsigned int context_id,
              const char *title, int src_layer_id, int dst_layer_id,
              int interpolation, int bx, int by, int bw, int bh, int x1, int y1,
              int x2, int y2, int x3, int y3, int x4, int y4,
              const unsigned char *in_mask, size_t in_mask_size,
              DP_Image *(*decompress)(int, int, const unsigned char *, size_t))
{
    if (bw <= 0 || bh <= 0) {
        DP_error_set("%s: selection is empty", title);
        return NULL;
    }

    int canvas_width = cs->width;
    int canvas_height = cs->height;
    DP_Rect canvas_bounds = DP_rect_make(0, 0, canvas_width, canvas_height);

    DP_Rect src_rect = DP_rect_make(bx, by, bw, bh);
    if (!DP_rect_intersects(canvas_bounds, src_rect)) {
        DP_error_set("%s: source out of bounds", title);
        return NULL;
    }

    DP_Quad dst_quad = DP_quad_make(x1, y1, x2, y2, x3, y3, x4, y4);

    DP_Rect dst_bounds = DP_quad_bounds(dst_quad);
    if (!DP_rect_intersects(canvas_bounds, dst_bounds)) {
        DP_error_set("%s: destination out of bounds", title);
        return NULL;
    }

    if (DP_rect_width(dst_bounds) > (canvas_width * 2LL)
        || DP_rect_height(dst_bounds) > (canvas_height * 2LL)) {
        DP_error_set("%s: attempt to scale too far beyond image size", title);
        return NULL;
    }

    DP_Image *mask;
    if (in_mask_size > 0) {
        mask = decompress(bw, bh, in_mask, in_mask_size);
        if (!mask) {
            return NULL;
        }
    }
    else {
        mask = NULL;
    }

    DP_CanvasState *next_cs = DP_ops_move_region(
        cs, dc, ucs_or_null, context_id, src_layer_id, dst_layer_id, &src_rect,
        &dst_quad, interpolation, mask);
    DP_free(mask);
    return next_cs;
}

static DP_CanvasState *handle_put_tile(DP_CanvasState *cs, DP_DrawContext *dc,
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
    DP_Tile *tile = DP_tile_new_from_compressed(dc, DP_msg_put_tile_user(mpt),
                                                image, image_size);
    if (!tile) {
        return NULL;
    }

    DP_CanvasState *next = DP_ops_put_tile(cs, tile, DP_msg_put_tile_layer(mpt),
                                           DP_msg_put_tile_sublayer(mpt), x, y,
                                           DP_msg_put_tile_repeat(mpt));

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
        DP_transient_canvas_state_background_tile_set_noinc(
            tcs, tile, DP_tile_opaque(tile));
        return DP_transient_canvas_state_persist(tcs);
    }
    else {
        return NULL;
    }
}

static DP_CanvasState *handle_pen_up(DP_CanvasState *cs, DP_DrawContext *dc,
                                     DP_UserCursors *ucs_or_null,
                                     unsigned int context_id, DP_MsgPenUp *mpu)
{
    int layer_id = DP_msg_pen_up_layer(mpu);
    return DP_ops_pen_up(cs, dc, ucs_or_null, context_id, layer_id);
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
    bool alias = flags & DP_MSG_ANNOTATION_EDIT_FLAGS_ALIAS;
    bool rasterize = flags & DP_MSG_ANNOTATION_EDIT_FLAGS_RASTERIZE;
    int valign = annotation_valign_from_flags(flags);
    size_t text_length;
    const char *text = DP_msg_annotation_edit_text(mae, &text_length);
    return DP_ops_annotation_edit(cs, DP_msg_annotation_edit_id(mae),
                                  DP_msg_annotation_edit_bg(mae), protect,
                                  alias, rasterize, valign, text, text_length);
}

static DP_CanvasState *handle_annotation_delete(DP_CanvasState *cs,
                                                DP_MsgAnnotationDelete *mad)
{
    return DP_ops_annotation_delete(cs, DP_msg_annotation_delete_id(mad));
}


struct DP_NextDabContext {
    int i, count;
    DP_Message **msgs;
};

static DP_PaintDrawDabsParams
get_draw_dabs_classic_params(unsigned int context_id,
                             DP_MsgDrawDabsClassic *mddc)
{
    int dab_count;
    const DP_ClassicDab *dabs = DP_msg_draw_dabs_classic_dabs(mddc, &dab_count);
    return (DP_PaintDrawDabsParams){DP_MSG_DRAW_DABS_CLASSIC,
                                    context_id,
                                    DP_msg_draw_dabs_classic_layer(mddc),
                                    DP_msg_draw_dabs_classic_x(mddc),
                                    DP_msg_draw_dabs_classic_y(mddc),
                                    DP_msg_draw_dabs_classic_color(mddc),
                                    DP_msg_draw_dabs_classic_mode(mddc),
                                    DP_msg_draw_dabs_classic_paint_mode(mddc),
                                    dab_count,
                                    {.classic = {dabs}}};
}

static DP_PaintDrawDabsParams
get_draw_dabs_pixel_params(DP_MessageType type, unsigned int context_id,
                           DP_MsgDrawDabsPixel *mddp)
{
    int dab_count;
    const DP_PixelDab *dabs = DP_msg_draw_dabs_pixel_dabs(mddp, &dab_count);
    return (DP_PaintDrawDabsParams){(int)type,
                                    context_id,
                                    DP_msg_draw_dabs_pixel_layer(mddp),
                                    DP_msg_draw_dabs_pixel_x(mddp),
                                    DP_msg_draw_dabs_pixel_y(mddp),
                                    DP_msg_draw_dabs_pixel_color(mddp),
                                    DP_msg_draw_dabs_pixel_mode(mddp),
                                    DP_msg_draw_dabs_pixel_paint_mode(mddp),
                                    dab_count,
                                    {.pixel = {dabs}}};
}

static DP_PaintDrawDabsParams
get_draw_dabs_mypaint_params(unsigned int context_id,
                             DP_MsgDrawDabsMyPaint *mddmp)
{
    int dab_count;
    const DP_MyPaintDab *dabs =
        DP_msg_draw_dabs_mypaint_dabs(mddmp, &dab_count);
    uint8_t posterize_num = DP_msg_draw_dabs_mypaint_posterize_num(mddmp);
    return (DP_PaintDrawDabsParams){
        DP_MSG_DRAW_DABS_MYPAINT,
        context_id,
        DP_msg_draw_dabs_mypaint_layer(mddmp),
        DP_msg_draw_dabs_mypaint_x(mddmp),
        DP_msg_draw_dabs_mypaint_y(mddmp),
        DP_msg_draw_dabs_mypaint_color(mddmp),
        (posterize_num & 0x80) ? DP_BLEND_MODE_PIGMENT_AND_ERASER
                               : DP_BLEND_MODE_NORMAL_AND_ERASER,
        DP_PAINT_MODE_DIRECT,
        dab_count,
        {.mypaint = {dabs, DP_msg_draw_dabs_mypaint_lock_alpha(mddmp),
                     DP_msg_draw_dabs_mypaint_colorize(mddmp),
                     DP_msg_draw_dabs_mypaint_posterize(mddmp),
                     posterize_num & (uint8_t)0x7f}}};
}

static DP_PaintDrawDabsParams
get_draw_dabs_mypaint_blend_params(unsigned int context_id,
                                   DP_MsgDrawDabsMyPaintBlend *mddmpb)
{
    int dab_count;
    const DP_MyPaintBlendDab *dabs =
        DP_msg_draw_dabs_mypaint_blend_dabs(mddmpb, &dab_count);
    return (DP_PaintDrawDabsParams){
        DP_MSG_DRAW_DABS_MYPAINT_BLEND,
        context_id,
        DP_msg_draw_dabs_mypaint_blend_layer(mddmpb),
        DP_msg_draw_dabs_mypaint_blend_x(mddmpb),
        DP_msg_draw_dabs_mypaint_blend_y(mddmpb),
        DP_msg_draw_dabs_mypaint_blend_color(mddmpb),
        DP_msg_draw_dabs_mypaint_blend_mode(mddmpb),
        DP_msg_draw_dabs_mypaint_blend_paint_mode(mddmpb),
        dab_count,
        {.mypaint_blend = {dabs}}};
}

static bool next_dab(void *user, DP_PaintDrawDabsParams *out_params)
{
    struct DP_NextDabContext *c = user;
    int i = c->i;
    int count = c->count;
    while (i < count) {
        DP_Message *msg = c->msgs[i++];
        // Messages may be null if they were already executed in the local fork.
        if (msg) {
            unsigned int context_id = DP_message_context_id(msg);
            switch (DP_message_type(msg)) {
            case DP_MSG_DRAW_DABS_CLASSIC:
                *out_params = get_draw_dabs_classic_params(
                    context_id, DP_message_internal(msg));
                break;
            case DP_MSG_DRAW_DABS_PIXEL:
                *out_params = get_draw_dabs_pixel_params(
                    DP_MSG_DRAW_DABS_PIXEL, context_id,
                    DP_message_internal(msg));
                break;
            case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
                *out_params = get_draw_dabs_pixel_params(
                    DP_MSG_DRAW_DABS_PIXEL_SQUARE, context_id,
                    DP_message_internal(msg));
                break;
            case DP_MSG_DRAW_DABS_MYPAINT:
                *out_params = get_draw_dabs_mypaint_params(
                    context_id, DP_message_internal(msg));
                break;
            case DP_MSG_DRAW_DABS_MYPAINT_BLEND:
                *out_params = get_draw_dabs_mypaint_blend_params(
                    context_id, DP_message_internal(msg));
                break;
            default:
                DP_UNREACHABLE();
            }
            c->i = i;
            return true;
        }
    }
    c->i = i;
    return false;
}

static DP_CanvasState *handle_draw_dabs(DP_CanvasState *cs, DP_DrawContext *dc,
                                        DP_UserCursors *ucs_or_null, int count,
                                        DP_Message **msgs)
{
    struct DP_NextDabContext c = {0, count, msgs};
    return DP_ops_draw_dabs(cs, dc, ucs_or_null, next_dab, &c);
}


static DP_CanvasState *handle_move_rect(DP_CanvasState *cs,
                                        DP_UserCursors *ucs_or_null,
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
        mask = DP_image_new_from_compressed_alpha_mask(width, height, in_mask,
                                                       in_mask_size);
        if (!mask) {
            return NULL;
        }
    }
    else {
        mask = NULL;
    }

    DP_CanvasState *next_cs = DP_ops_move_rect(
        cs, ucs_or_null, context_id, DP_msg_move_rect_source(mmr),
        DP_msg_move_rect_layer(mmr), &src_rect, dst_x, dst_y, mask);
    DP_free(mask);
    return next_cs;
}

static void truncate_tracks(DP_TransientCanvasState *tcs, int frame_count)
{
    DP_Timeline *tl = tcs->timeline;
    DP_TransientTimeline *ttl =
        DP_timeline_transient(tl) ? (DP_TransientTimeline *)tl : NULL;
    int track_count = DP_timeline_count(tl);
    for (int t_index = 0; t_index < track_count; ++t_index) {
        DP_Track *t = DP_timeline_at_noinc(tl, t_index);
        int kf_index =
            DP_track_key_frame_search_at_or_after(t, frame_count, NULL);
        if (kf_index != -1) {
            if (!ttl) {
                ttl = DP_transient_canvas_state_transient_timeline(tcs, 0);
                tl = (DP_Timeline *)ttl;
            }
            DP_TransientTrack *tt =
                DP_transient_timeline_transient_at_noinc(ttl, t_index, 0);
            DP_transient_track_truncate(tt, kf_index);
        }
    }
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
    case DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT:
        set_field = DP_transient_document_metadata_frame_count_set;
        break;
    default:
        DP_error_set("Set metadata int: unknown field %d", field);
        return NULL;
    }
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientDocumentMetadata *tdm =
        DP_transient_canvas_state_transient_metadata(tcs);
    int value = DP_msg_set_metadata_int_value(msmi);
    set_field(tdm, value);
    if (field == DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT) {
        truncate_tracks(tcs, value);
    }
    return DP_transient_canvas_state_persist(tcs);
}

static DP_CanvasState *handle_layer_tree_create(DP_CanvasState *cs,
                                                DP_DrawContext *dc,
                                                unsigned int context_id,
                                                DP_MsgLayerTreeCreate *mtlc)
{
    int layer_id = DP_msg_layer_tree_create_id(mtlc);
    if (layer_id == 0) {
        DP_error_set("Create layer tree: layer id 0 is invalid");
        return NULL;
    }

    unsigned int flags = DP_msg_layer_tree_create_flags(mtlc);
    bool into = flags & DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO;
    bool group = flags & DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP;

    uint32_t fill = DP_msg_layer_tree_create_fill(mtlc);
    DP_Tile *tile =
        group || fill == 0 ? NULL : DP_tile_new_from_bgra(context_id, fill);

    size_t title_length;
    const char *title = DP_msg_layer_tree_create_title(mtlc, &title_length);

    DP_CanvasState *next = DP_ops_layer_tree_create(
        cs, dc, layer_id, DP_msg_layer_tree_create_source(mtlc),
        DP_msg_layer_tree_create_target(mtlc), tile, into, group, title,
        title_length);

    DP_tile_decref_nullable(tile);
    return next;
}

static DP_CanvasState *handle_layer_tree_move(DP_CanvasState *cs,
                                              DP_DrawContext *dc,
                                              DP_MsgLayerTreeMove *mltm)
{
    int layer_id = DP_msg_layer_tree_move_layer(mltm);
    if (layer_id == 0) {
        DP_error_set("Move layer tree: layer id 0 is invalid");
        return NULL;
    }

    int parent_id = DP_msg_layer_tree_move_parent(mltm);
    int sibling_id = DP_msg_layer_tree_move_sibling(mltm);
    bool overlapping_ids = layer_id == parent_id || layer_id == sibling_id
                        || (parent_id == sibling_id && parent_id != 0);
    if (overlapping_ids) {
        DP_error_set(
            "Move layer tree: layer %d, parent %d and sibling %d overlap",
            layer_id, parent_id, sibling_id);
        return NULL;
    }

    return DP_ops_layer_tree_move(cs, dc, layer_id, parent_id, sibling_id);
}

static DP_CanvasState *handle_layer_tree_delete(DP_CanvasState *cs,
                                                DP_DrawContext *dc,
                                                unsigned int context_id,
                                                DP_MsgLayerTreeDelete *mltd)
{
    int layer_id = DP_msg_layer_tree_delete_id(mltd);
    if (layer_id == 0) {
        DP_error_set("Layer tree delete: layer id 0 is invalid");
        return NULL;
    }

    return DP_ops_layer_tree_delete(cs, dc, context_id, layer_id,
                                    DP_msg_layer_tree_delete_merge_to(mltd));
}

static DP_CanvasState *handle_transform_region(DP_CanvasState *cs,
                                               DP_DrawContext *dc,
                                               DP_UserCursors *ucs_or_null,
                                               unsigned int context_id,
                                               DP_MsgTransformRegion *mtr)
{
    size_t in_mask_size;
    const unsigned char *in_mask =
        DP_msg_transform_region_mask(mtr, &in_mask_size);
    return handle_region(
        cs, dc, ucs_or_null, context_id, "Transform region",
        DP_msg_transform_region_source(mtr), DP_msg_transform_region_layer(mtr),
        DP_msg_transform_region_mode(mtr), DP_msg_transform_region_bx(mtr),
        DP_msg_transform_region_by(mtr), DP_msg_transform_region_bw(mtr),
        DP_msg_transform_region_bh(mtr), DP_msg_transform_region_x1(mtr),
        DP_msg_transform_region_y1(mtr), DP_msg_transform_region_x2(mtr),
        DP_msg_transform_region_y2(mtr), DP_msg_transform_region_x3(mtr),
        DP_msg_transform_region_y3(mtr), DP_msg_transform_region_x4(mtr),
        DP_msg_transform_region_y4(mtr), in_mask, in_mask_size,
        DP_image_new_from_compressed_alpha_mask);
}

static DP_CanvasState *handle_track_create(DP_CanvasState *cs,
                                           DP_MsgTrackCreate *mtc)
{
    int track_id = DP_msg_track_create_id(mtc);
    if (track_id == 0) {
        DP_error_set("Track create: track id 0 is invalid");
        return NULL;
    }

    size_t title_length;
    const char *title = DP_msg_track_create_title(mtc, &title_length);

    return DP_ops_track_create(cs, track_id, DP_msg_track_create_insert_id(mtc),
                               DP_msg_track_create_source_id(mtc), title,
                               title_length);
}

static DP_CanvasState *handle_track_retitle(DP_CanvasState *cs,
                                            DP_MsgTrackRetitle *mtr)
{
    int track_id = DP_msg_track_retitle_id(mtr);
    if (track_id == 0) {
        DP_error_set("Track retitle: track id 0 is invalid");
        return NULL;
    }

    size_t title_length;
    const char *title = DP_msg_track_retitle_title(mtr, &title_length);

    return DP_ops_track_retitle(cs, track_id, title, title_length);
}

static DP_CanvasState *handle_track_delete(DP_CanvasState *cs,
                                           DP_MsgTrackDelete *mtd)
{
    int track_id = DP_msg_track_delete_id(mtd);
    if (track_id == 0) {
        DP_error_set("Track delete: track id 0 is invalid");
        return NULL;
    }

    return DP_ops_track_delete(cs, track_id);
}

static int get_order_id(void *user, int index)
{
    const uint16_t *layer_ids = user;
    return layer_ids[index];
}

static DP_CanvasState *handle_track_order(DP_CanvasState *cs,
                                          DP_MsgTrackOrder *mto)
{
    int count;
    const uint16_t *track_ids = DP_msg_track_order_tracks(mto, &count);
    return DP_ops_track_order(cs, count, get_order_id, (void *)track_ids);
}

static DP_CanvasState *handle_key_frame_set(DP_CanvasState *cs,
                                            DP_MsgKeyFrameSet *mkfs)
{
    int track_id = DP_msg_key_frame_set_track_id(mkfs);
    if (track_id == 0) {
        DP_error_set("Key frame set: track id 0 is invalid");
        return NULL;
    }

    int frame_index = DP_msg_key_frame_set_frame_index(mkfs);
    int source_id = DP_msg_key_frame_set_source_id(mkfs);
    int source = DP_msg_key_frame_set_source(mkfs);
    switch (source) {
    case DP_MSG_KEY_FRAME_SET_SOURCE_LAYER:
        return DP_ops_key_frame_set(cs, track_id, frame_index, source_id);
    case DP_MSG_KEY_FRAME_SET_SOURCE_KEY_FRAME: {
        int source_index = DP_msg_key_frame_set_source_index(mkfs);
        if (track_id == source_id && frame_index == source_index) {
            DP_error_set("Key frame set: can't copy a key frame to itself");
            return NULL;
        }
        return DP_ops_key_frame_copy(cs, track_id, frame_index, source_id,
                                     source_index);
    }
    default:
        DP_error_set("Key frame set: unknown source type %d", source);
        return NULL;
    }
}

static DP_CanvasState *handle_key_frame_retitle(DP_CanvasState *cs,
                                                DP_MsgKeyFrameRetitle *mkfr)
{
    int track_id = DP_msg_key_frame_retitle_track_id(mkfr);
    if (track_id == 0) {
        DP_error_set("Key frame retitle: track id 0 is invalid");
        return NULL;
    }

    size_t title_length;
    const char *title = DP_msg_key_frame_retitle_title(mkfr, &title_length);

    return DP_ops_key_frame_retitle(cs, track_id,
                                    DP_msg_key_frame_retitle_frame_index(mkfr),
                                    title, title_length);
}

static DP_KeyFrameLayer get_key_frame_layer_attribute(void *user, int index)
{
    const uint16_t *layers = user;
    const uint16_t *pair = &layers[index * 2];
    return (DP_KeyFrameLayer){pair[0], pair[1]};
}

static DP_CanvasState *
handle_key_frame_layer_attributes(DP_CanvasState *cs, DP_DrawContext *dc,
                                  DP_MsgKeyFrameLayerAttributes *mkfla)
{
    int track_id = DP_msg_key_frame_layer_attributes_track_id(mkfla);
    if (track_id == 0) {
        DP_error_set("Key frame layer attributes: track id 0 is invalid");
        return NULL;
    }

    int count;
    const uint16_t *layers =
        DP_msg_key_frame_layer_attributes_layers(mkfla, &count);
    if (count % 2 != 0) {
        DP_error_set("Key frame layer attributes: count %d is not even", count);
        return NULL;
    }

    return DP_ops_key_frame_layer_attributes(
        cs, dc, track_id, DP_msg_key_frame_layer_attributes_frame_index(mkfla),
        count / 2, get_key_frame_layer_attribute, (void *)layers);
}

static DP_CanvasState *handle_key_frame_delete(DP_CanvasState *cs,
                                               DP_MsgKeyFrameDelete *mkfd)
{
    int track_id = DP_msg_key_frame_delete_track_id(mkfd);
    if (track_id == 0) {
        DP_error_set("Key frame delete: track id 0 is invalid");
        return NULL;
    }

    int frame_index = DP_msg_key_frame_delete_frame_index(mkfd);
    int move_track_id = DP_msg_key_frame_delete_move_track_id(mkfd);
    int move_frame_index = DP_msg_key_frame_delete_move_frame_index(mkfd);
    if (track_id == move_track_id && frame_index == move_frame_index) {
        DP_error_set("Key frame delete: can't move a key frame to itself");
        return NULL;
    }

    return DP_ops_key_frame_delete(cs, track_id, frame_index, move_track_id,
                                   move_frame_index);
}

static DP_CanvasState *handle_selection_put(DP_CanvasState *cs,
                                            unsigned int context_id,
                                            DP_MsgSelectionPut *msp)
{
    size_t in_mask_size;
    const unsigned char *in_mask =
        DP_msg_selection_put_mask(msp, &in_mask_size);
    return selection_put(
        cs, context_id, DP_msg_selection_put_selection_id(msp),
        DP_msg_selection_put_op(msp), DP_msg_selection_put_x(msp),
        DP_msg_selection_put_y(msp), DP_msg_selection_put_w(msp),
        DP_msg_selection_put_h(msp), in_mask_size, in_mask);
}

static DP_CanvasState *handle_selection_clear(DP_CanvasState *cs,
                                              unsigned int context_id,
                                              DP_MsgSelectionClear *msc)
{
    return selection_clear(cs, context_id,
                           DP_msg_selection_clear_selection_id(msc));
}

static DP_CanvasState *handle(DP_CanvasState *cs, DP_DrawContext *dc,
                              DP_UserCursors *ucs_or_null, DP_Message *msg,
                              DP_MessageType type)
{
    switch (type) {
    case DP_MSG_CANVAS_RESIZE:
        return handle_canvas_resize(cs, DP_message_context_id(msg),
                                    DP_msg_canvas_resize_cast(msg));
    case DP_MSG_LAYER_ATTRIBUTES:
        return handle_layer_attr(cs, DP_msg_layer_attributes_cast(msg));
    case DP_MSG_LAYER_RETITLE:
        return handle_layer_retitle(cs, DP_msg_layer_retitle_cast(msg));
    case DP_MSG_PUT_IMAGE:
        return handle_put_image(cs, ucs_or_null, DP_message_context_id(msg),
                                DP_msg_put_image_cast(msg));
    case DP_MSG_FILL_RECT:
        return handle_fill_rect(cs, ucs_or_null, DP_message_context_id(msg),
                                DP_msg_fill_rect_cast(msg));
    case DP_MSG_PUT_TILE:
        return handle_put_tile(cs, dc, DP_msg_put_tile_cast(msg));
    case DP_MSG_CANVAS_BACKGROUND:
        return handle_canvas_background(cs, dc, DP_message_context_id(msg),
                                        DP_msg_canvas_background_cast(msg));
    case DP_MSG_PEN_UP:
        return handle_pen_up(cs, dc, ucs_or_null, DP_message_context_id(msg),
                             DP_message_internal(msg));
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
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
    case DP_MSG_DRAW_DABS_MYPAINT:
    case DP_MSG_DRAW_DABS_MYPAINT_BLEND:
        return handle_draw_dabs(cs, dc, ucs_or_null, 1, &msg);
    case DP_MSG_MOVE_RECT:
        return handle_move_rect(cs, ucs_or_null, DP_message_context_id(msg),
                                DP_msg_move_rect_cast(msg));
    case DP_MSG_SET_METADATA_INT:
        return handle_set_metadata_int(cs, DP_msg_set_metadata_int_cast(msg));
    case DP_MSG_LAYER_TREE_CREATE:
        return handle_layer_tree_create(cs, dc, DP_message_context_id(msg),
                                        DP_msg_layer_tree_create_cast(msg));
    case DP_MSG_LAYER_TREE_MOVE:
        return handle_layer_tree_move(cs, dc, DP_msg_layer_tree_move_cast(msg));
    case DP_MSG_LAYER_TREE_DELETE:
        return handle_layer_tree_delete(cs, dc, DP_message_context_id(msg),
                                        DP_msg_layer_tree_delete_cast(msg));
    case DP_MSG_TRANSFORM_REGION:
        return handle_transform_region(cs, dc, ucs_or_null,
                                       DP_message_context_id(msg),
                                       DP_msg_transform_region_cast(msg));
    case DP_MSG_TRACK_CREATE:
        return handle_track_create(cs, DP_msg_track_create_cast(msg));
    case DP_MSG_TRACK_RETITLE:
        return handle_track_retitle(cs, DP_msg_track_retitle_cast(msg));
    case DP_MSG_TRACK_DELETE:
        return handle_track_delete(cs, DP_msg_track_delete_cast(msg));
    case DP_MSG_TRACK_ORDER:
        return handle_track_order(cs, DP_msg_track_order_cast(msg));
    case DP_MSG_KEY_FRAME_SET:
        return handle_key_frame_set(cs, DP_msg_key_frame_set_cast(msg));
    case DP_MSG_KEY_FRAME_RETITLE:
        return handle_key_frame_retitle(cs, DP_msg_key_frame_retitle_cast(msg));
    case DP_MSG_KEY_FRAME_LAYER_ATTRIBUTES:
        return handle_key_frame_layer_attributes(
            cs, dc, DP_msg_key_frame_layer_attributes_cast(msg));
    case DP_MSG_KEY_FRAME_DELETE:
        return handle_key_frame_delete(cs, DP_msg_key_frame_delete_cast(msg));
    case DP_MSG_SELECTION_PUT:
        return handle_selection_put(cs, DP_message_context_id(msg),
                                    DP_message_internal(msg));
    case DP_MSG_SELECTION_CLEAR:
        return handle_selection_clear(cs, DP_message_context_id(msg),
                                      DP_message_internal(msg));
    case DP_MSG_LOCAL_MATCH:
        // Local synchronization message, presumably from another user. It
        // doesn't contain any interesting information for us.
        return DP_canvas_state_incref(cs);
    default:
        DP_error_set("Unhandled draw message type %d", (int)type);
        return NULL;
    }
}

DP_CanvasState *DP_canvas_state_handle(DP_CanvasState *cs, DP_DrawContext *dc,
                                       DP_UserCursors *ucs_or_null,
                                       DP_Message *msg)
{
    DP_ASSERT(cs);
    DP_ASSERT(msg);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_ASSERT(!cs->transient);
    DP_MessageType type = DP_message_type(msg);
    DP_PERF_BEGIN_DETAIL(fn, "handle", "type=%d", (int)type);
    DP_CanvasState *next_cs = handle(cs, dc, ucs_or_null, msg, type);
    DP_PERF_END(fn);
    return next_cs;
}

DP_CanvasState *DP_canvas_state_handle_multidab(DP_CanvasState *cs,
                                                DP_DrawContext *dc,
                                                DP_UserCursors *ucs_or_null,
                                                int count, DP_Message **msgs)
{
    DP_ASSERT(cs);
    DP_ASSERT(dc);
    DP_ASSERT(count <= 0 || msgs);
    DP_PERF_BEGIN_DETAIL(fn, "handle_multidab", "count=%d", (int)count);
    DP_CanvasState *next_cs =
        handle_draw_dabs(cs, dc, ucs_or_null, count, msgs);
    DP_PERF_END(fn);
    return next_cs;
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


static DP_Tile *get_flat_background_tile_or_null(DP_CanvasState *cs,
                                                 unsigned int flags)
{
    bool include_background = flags & DP_FLAT_IMAGE_INCLUDE_BACKGROUND;
    return include_background ? cs->background_tile : NULL;
}

static void init_flattening_tile(DP_TransientTile *tt, DP_Tile *background_tile)
{
    if (background_tile) {
        memcpy(DP_transient_tile_pixels(tt), DP_tile_pixels(background_tile),
               DP_TILE_BYTES);
    }
    else {
        memset(DP_transient_tile_pixels(tt), 0, DP_TILE_BYTES);
    }
}

DP_TransientLayerContent *
DP_canvas_state_to_flat_layer(DP_CanvasState *cs, unsigned int flags,
                              const DP_ViewModeFilter *vmf_or_null)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    int width = cs->width;
    int height = cs->height;
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init(width, height, NULL);

    DP_Tile *background_tile = get_flat_background_tile_or_null(cs, flags);
    int wt = DP_tile_count_round(width);
    bool include_sublayers = flags & DP_FLAT_IMAGE_INCLUDE_SUBLAYERS;
    DP_ViewModeFilter vmf =
        vmf_or_null ? *vmf_or_null : DP_view_mode_filter_make_default();

    DP_TileIterator ti = DP_tile_iterator_make(
        cs->width, cs->height, DP_rect_make(0, 0, width, height));
    while (DP_tile_iterator_next(&ti)) {
        DP_TransientTile *tt = DP_transient_tile_new_blank(0);
        init_flattening_tile(tt, background_tile);
        int i = ti.row * wt + ti.col;
        DP_canvas_state_flatten_tile_to(cs, i, tt, include_sublayers, &vmf);
        DP_transient_layer_content_transient_tile_set_noinc(tlc, tt, i);
    }

    return tlc;
}

static DP_TransientTile *
flatten_onion_skin(int tile_index, DP_TransientTile *tt, DP_LayerListEntry *lle,
                   DP_LayerProps *lp, uint16_t parent_opacity,
                   bool include_sublayers, DP_ViewModeContext *vmc,
                   const DP_OnionSkin *os)
{
    DP_TransientTile *skin_tt = DP_layer_list_entry_flatten_tile_to(
        lle, lp, tile_index, DP_transient_tile_new_blank(0),
        DP_fix15_mul(parent_opacity, os->opacity), (DP_UPixel8){.color = 0},
        include_sublayers, false, false, vmc);

    DP_UPixel8 tint = os->tint;
    if (tint.a != 0) {
        DP_transient_tile_tint(skin_tt, tint);
    }

    if (tt) {
        DP_transient_tile_merge(tt, (DP_Tile *)skin_tt, DP_BIT15,
                                DP_BLEND_MODE_NORMAL);
        DP_transient_tile_decref(skin_tt);
        return tt;
    }
    else {
        return skin_tt;
    }
}

static DP_ViewModeContext get_clip_layer(void *user, int i,
                                         DP_LayerListEntry **out_lle,
                                         DP_LayerProps **out_lp)
{
    DP_ViewModeContextRoot *vmcr = ((void **)user)[0];
    DP_CanvasState *cs = ((void **)user)[1];
    return DP_view_mode_context_root_at_clip(vmcr, cs, i, out_lle, out_lp);
}

DP_TransientTile *DP_canvas_state_flatten_tile_to(DP_CanvasState *cs,
                                                  int tile_index,
                                                  DP_TransientTile *tt_or_null,
                                                  bool include_sublayers,
                                                  const DP_ViewModeFilter *vmf)
{
    DP_ViewModeContextRoot vmcr = DP_view_mode_context_root_init(vmf, cs);
    DP_TransientTile *tt = tt_or_null;
    for (int i = 0; i < vmcr.count; ++i) {
        DP_LayerListEntry *lle;
        DP_LayerProps *lp;
        const DP_OnionSkin *os;
        uint16_t parent_opacity;
        DP_UPixel8 parent_tint;
        int clip_count;
        DP_ViewModeContext vmc = DP_view_mode_context_root_at(
            &vmcr, cs, i, &lle, &lp, &os, &parent_opacity, &parent_tint,
            &clip_count);
        if (!DP_view_mode_context_excludes_everything(&vmc)) {
            if (os) {
                tt = flatten_onion_skin(tile_index, tt, lle, lp, parent_opacity,
                                        include_sublayers, &vmc, os);
            }
            else if (clip_count == 0) {
                tt = DP_layer_list_entry_flatten_tile_to(
                    lle, lp, tile_index, tt, parent_opacity, parent_tint,
                    include_sublayers, false, false, &vmc);
            }
            else {
                tt = DP_layer_list_flatten_clipping_tile_to(
                    (void *[]){&vmcr, cs}, get_clip_layer, i, clip_count,
                    tile_index, tt, parent_opacity, include_sublayers, &vmc);
                i += clip_count;
            }
        }
    }
    return tt;
}

static void *flatten_canvas(
    DP_CanvasState *cs, unsigned int flags, const DP_Rect *area_or_null,
    const DP_ViewModeFilter *vmf_or_null, void *(*get_buffer)(void *, int, int),
    void (*to_buffer)(void *, DP_TransientTile *, DP_TileIterator *),
    void *user)
{
    DP_Rect area = area_or_null ? *area_or_null
                                : DP_rect_make(0, 0, cs->width, cs->height);
    if (!DP_rect_valid(area)) {
        DP_error_set("Can't create a flat image with zero pixels");
        return NULL;
    }

    DP_Tile *background_tile = get_flat_background_tile_or_null(cs, flags);
    int wt = DP_tile_count_round(cs->width);
    bool include_sublayers = flags & DP_FLAT_IMAGE_INCLUDE_SUBLAYERS;
    DP_ViewModeFilter vmf =
        vmf_or_null ? *vmf_or_null : DP_view_mode_filter_make_default();

    void *buffer = get_buffer(user, DP_rect_width(area), DP_rect_height(area));
    DP_TransientTile *tt = DP_transient_tile_new_blank(0);
    DP_TileIterator ti = DP_tile_iterator_make(cs->width, cs->height, area);
    while (DP_tile_iterator_next(&ti)) {
        init_flattening_tile(tt, background_tile);
        int i = ti.row * wt + ti.col;
        DP_canvas_state_flatten_tile_to(cs, i, tt, include_sublayers, &vmf);
        to_buffer(buffer, tt, &ti);
    }
    DP_transient_tile_decref(tt);
    return buffer;
}

DP_Image *DP_canvas_state_to_flat_image(DP_CanvasState *cs, unsigned int flags,
                                        const DP_Rect *area_or_null,
                                        const DP_ViewModeFilter *vmf_or_null)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return DP_canvas_state_into_flat_image(cs, flags, area_or_null, vmf_or_null,
                                           NULL);
}

static void *into_flat_image_get_buffer(void *user, int width, int height)
{
    DP_Image **pp = user;
    if (pp) {
        DP_Image *img = *pp;
        if (img) {
            if (DP_image_width(img) == width
                && DP_image_height(img) == height) {
                return img;
            }
            else {
                DP_image_free(img);
            }
        }
    }
    DP_Image *img = DP_image_new(width, height);
    if (pp) {
        *pp = img;
    }
    return img;
}

static void into_flat_image_to_buffer(void *buffer, DP_TransientTile *tt,
                                      DP_TileIterator *ti)
{
    DP_Image *img = buffer;
    DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(ti);
    while (DP_tile_into_dst_iterator_next(&tidi)) {
        DP_image_pixel_at_set(img, tidi.dst_x, tidi.dst_y,
                              DP_pixel15_to_8(DP_transient_tile_pixel_at(
                                  tt, tidi.tile_x, tidi.tile_y)));
    }
}

static void into_flat_image_to_buffer_one_bit_alpha(void *buffer,
                                                    DP_TransientTile *tt,
                                                    DP_TileIterator *ti)
{
    DP_Image *img = buffer;
    DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(ti);
    while (DP_tile_into_dst_iterator_next(&tidi)) {
        DP_Pixel15 pixel15 =
            DP_transient_tile_pixel_at(tt, tidi.tile_x, tidi.tile_y);
        DP_Pixel8 pixel8;
        if (pixel15.a == DP_BIT15) {
            pixel8 = DP_pixel15_to_8(pixel15);
        }
        else if (pixel15.a >= DP_BIT15 / 2) {
            DP_UPixel15 upixel15 = DP_pixel15_unpremultiply(pixel15);
            pixel8 = (DP_Pixel8){
                .b = DP_channel15_to_8(upixel15.b),
                .g = DP_channel15_to_8(upixel15.g),
                .r = DP_channel15_to_8(upixel15.r),
                .a = 255,
            };
        }
        else {
            pixel8.color = 0;
        }
        DP_image_pixel_at_set(img, tidi.dst_x, tidi.dst_y, pixel8);
    }
}

DP_Image *DP_canvas_state_into_flat_image(DP_CanvasState *cs,
                                          unsigned int flags,
                                          const DP_Rect *area_or_null,
                                          const DP_ViewModeFilter *vmf_or_null,
                                          DP_Image **inout_img_or_null)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return flatten_canvas(cs, flags, area_or_null, vmf_or_null,
                          into_flat_image_get_buffer,
                          flags & DP_FLAT_IMAGE_ONE_BIT_ALPHA
                              ? into_flat_image_to_buffer_one_bit_alpha
                              : into_flat_image_to_buffer,
                          inout_img_or_null);
}

static void *to_flat_separated_urgba8_get_buffer(void *user,
                                                 DP_UNUSED int width,
                                                 DP_UNUSED int height)
{
    return user;
}

static void to_flat_separated_urgba8_to_buffer(void *buffer,
                                               DP_TransientTile *tt,
                                               DP_TileIterator *ti)
{
    unsigned char *channels = buffer;
    int width = DP_rect_width(ti->dst);
    int size = width * DP_rect_height(ti->dst);
    DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(ti);
    while (DP_tile_into_dst_iterator_next(&tidi)) {
        DP_UPixel8 pixel = DP_upixel15_to_8(DP_pixel15_unpremultiply(
            DP_transient_tile_pixel_at(tt, tidi.tile_x, tidi.tile_y)));
        int i = tidi.dst_y * width + tidi.dst_x;
        channels[i] = pixel.r;
        channels[i + size] = pixel.g;
        channels[i + size + size] = pixel.b;
        channels[i + size + size + size] = pixel.a;
    }
}

bool DP_canvas_state_to_flat_separated_urgba8(
    DP_CanvasState *cs, unsigned int flags, const DP_Rect *area_or_null,
    const DP_ViewModeFilter *vmf_or_null, unsigned char *buffer)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    return flatten_canvas(cs, flags, area_or_null, vmf_or_null,
                          to_flat_separated_urgba8_get_buffer,
                          to_flat_separated_urgba8_to_buffer, buffer);
}

DP_TransientTile *
DP_canvas_state_flatten_tile(DP_CanvasState *cs, int tile_index,
                             unsigned int flags,
                             const DP_ViewModeFilter *vmf_or_null)
{
    DP_ASSERT(cs);
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(cs->width, cs->height));
    DP_Tile *background_tile = get_flat_background_tile_or_null(cs, flags);
    DP_TransientTile *tt = background_tile
                             ? DP_transient_tile_new(background_tile, 0)
                             : DP_transient_tile_new_blank(0);
    bool include_sublayers = flags & DP_FLAT_IMAGE_INCLUDE_SUBLAYERS;
    DP_ViewModeFilter vmf =
        vmf_or_null ? *vmf_or_null : DP_view_mode_filter_make_default();
    return DP_canvas_state_flatten_tile_to(cs, tile_index, tt,
                                           include_sublayers, &vmf);
}

DP_TransientTile *
DP_canvas_state_flatten_tile_at(DP_CanvasState *cs, int x, int y,
                                unsigned int flags,
                                const DP_ViewModeFilter *vmf_or_null)
{
    DP_ASSERT(cs);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_ASSERT(x >= 0);
    DP_ASSERT(x < cs->width);
    DP_ASSERT(y >= 0);
    DP_ASSERT(y < cs->height);
    int i = y * DP_tile_count_round(cs->width) + x;
    return DP_canvas_state_flatten_tile(cs, i, flags, vmf_or_null);
}


static void diff_states(DP_CanvasState *cs, DP_CanvasState *prev,
                        DP_CanvasDiff *diff, int only_layer_id)
{
    bool check_all = cs->background_tile != prev->background_tile
                  || cs->width != prev->width || cs->height != prev->height;
    if (check_all) {
        DP_canvas_diff_check_all(diff);
    }
    else {
        DP_layer_list_diff(cs->layers, cs->layer_props, prev->layers,
                           prev->layer_props, diff, only_layer_id);
    }
}

void DP_canvas_state_diff(DP_CanvasState *cs, DP_CanvasState *prev_or_null,
                          DP_CanvasDiff *diff, int only_layer_id)
{
    DP_ASSERT(cs);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&cs->refcount) > 0);
    DP_PERF_BEGIN_DETAIL(fn, "diff", "prev=%d", prev_or_null ? 1 : 0);
    if (prev_or_null) {
        DP_ASSERT(DP_atomic_get(&prev_or_null->refcount) > 0);
        bool change = cs != prev_or_null;
        DP_canvas_diff_begin(
            diff, prev_or_null->width, prev_or_null->height, cs->width,
            cs->height, change && cs->layer_props != prev_or_null->layer_props);
        if (change) {
            diff_states(cs, prev_or_null, diff, only_layer_id);
        }
    }
    else {
        DP_canvas_diff_begin(diff, 0, 0, cs->width, cs->height, true);
    }
    DP_PERF_END(fn);
}

static void render_tile(void *data, int tile_index)
{
    DP_CanvasState *cs = ((void **)data)[0];
    DP_TransientLayerContent *target = ((void **)data)[1];
    DP_transient_layer_content_render_tile(target, cs, tile_index, NULL);
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
    DP_TransientCanvasState *tcs = allocate_canvas_state(true, 0, 0, 0, 0);
    tcs->layers = DP_layer_list_new();
    tcs->layer_props = DP_layer_props_list_new();
    tcs->layer_routes = DP_layer_routes_new();
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
    DP_TransientCanvasState *tcs = allocate_canvas_state(
        true, cs->width, cs->height, cs->offset_x, cs->offset_y);
    tcs->background_tile = DP_tile_incref_nullable(cs->background_tile);
    tcs->background_opaque = cs->background_opaque;
    return tcs;
}

DP_TransientCanvasState *DP_transient_canvas_state_new(DP_CanvasState *cs)
{
    DP_TransientCanvasState *tcs = new_transient_canvas_state(cs);
    tcs->layers = DP_layer_list_incref(cs->layers);
    tcs->layer_props = DP_layer_props_list_incref(cs->layer_props);
    tcs->layer_routes = DP_layer_routes_incref(cs->layer_routes);
    tcs->annotations = DP_annotation_list_incref(cs->annotations);
    tcs->timeline = DP_timeline_incref(cs->timeline);
    tcs->metadata = DP_document_metadata_incref(cs->metadata);
    tcs->selections = DP_selection_set_incref_nullable(cs->selections);
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
    tcs->selections = DP_selection_set_incref_nullable(cs->selections);
    return tcs;
}

DP_TransientCanvasState *
DP_transient_canvas_state_new_with_timeline_noinc(DP_CanvasState *cs,
                                                  DP_TransientTimeline *ttl)
{
    DP_TransientCanvasState *tcs = new_transient_canvas_state(cs);
    tcs->layers = DP_layer_list_incref(cs->layers);
    tcs->layer_props = DP_layer_props_list_incref(cs->layer_props);
    tcs->layer_routes = DP_layer_routes_incref(cs->layer_routes);
    tcs->annotations = DP_annotation_list_incref(cs->annotations);
    tcs->transient_timeline = ttl;
    tcs->metadata = DP_document_metadata_incref(cs->metadata);
    tcs->selections = DP_selection_set_incref_nullable(cs->selections);
    return tcs;
}

DP_TransientCanvasState *
DP_transient_canvas_state_incref(DP_TransientCanvasState *tcs)
{
    return (DP_TransientCanvasState *)DP_canvas_state_incref(
        (DP_CanvasState *)tcs);
}

DP_TransientCanvasState *
DP_transient_canvas_state_incref_nullable(DP_TransientCanvasState *tcs_or_null)
{
    return tcs_or_null ? DP_transient_canvas_state_incref(tcs_or_null) : NULL;
}

void DP_transient_canvas_state_decref(DP_TransientCanvasState *tcs)
{
    DP_canvas_state_decref((DP_CanvasState *)tcs);
}

void DP_transient_canvas_state_decref_nullable(
    DP_TransientCanvasState *tcs_or_null)
{
    if (tcs_or_null) {
        DP_transient_canvas_state_decref(tcs_or_null);
    }
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
    if (tcs->selections && DP_selection_set_transient(tcs->selections)) {
        DP_transient_selection_set_persist(tcs->transient_selections);
    }
    tcs->transient = false;
    return (DP_CanvasState *)tcs;
}

int DP_transient_canvas_state_width(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return tcs->width;
}

int DP_transient_canvas_state_height(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return tcs->height;
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

void DP_transient_canvas_state_offsets_add(DP_TransientCanvasState *tcs,
                                           int offset_x, int offset_y)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    tcs->offset_x += offset_x;
    tcs->offset_y += offset_y;
}

void DP_transient_canvas_state_background_tile_set_noinc(
    DP_TransientCanvasState *tcs, DP_Tile *tile, bool opaque)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_tile_decref_nullable(tcs->background_tile);
    tcs->background_tile = tile;
    tcs->background_opaque = opaque;
}

void DP_transient_canvas_state_layer_routes_reindex(
    DP_TransientCanvasState *tcs, DP_DrawContext *dc)
{
    DP_layer_routes_decref_nullable(tcs->layer_routes);
    DP_LayerRoutes *lr = DP_layer_routes_new_index(tcs->layer_props, dc);
    tcs->layer_routes = lr;
}

static void timeline_cleanup_make_transient(
    DP_TransientCanvasState *tcs, DP_Timeline **ptl,
    DP_TransientTimeline **pttl, DP_Track **pt, DP_TransientTrack **ptt,
    DP_KeyFrame **pkf, DP_TransientKeyFrame **ptkf, int t_index, int kf_index)
{
    if (!*pttl) {
        *pttl = DP_transient_canvas_state_transient_timeline(tcs, 0);
        *ptl = (DP_Timeline *)*pttl;
    }

    if (!*ptt) {
        *ptt = DP_transient_timeline_transient_at_noinc(*pttl, t_index, 0);
        *pt = (DP_Track *)*ptt;
    }

    if (!*ptkf) {
        *ptkf = DP_transient_track_transient_at_noinc(*ptt, kf_index);
        *pkf = (DP_KeyFrame *)*ptkf;
    }
}

void DP_transient_canvas_state_timeline_cleanup(DP_TransientCanvasState *tcs)
{
    DP_LayerRoutes *lr = tcs->layer_routes;
    DP_Timeline *tl = tcs->timeline;
    DP_TransientTimeline *ttl =
        DP_timeline_transient(tl) ? (DP_TransientTimeline *)tl : NULL;

    int t_count = DP_timeline_count(tl);
    for (int t_index = 0; t_index < t_count; ++t_index) {
        DP_Track *t = DP_timeline_at_noinc(tl, t_index);
        DP_TransientTrack *tt =
            DP_track_transient(t) ? (DP_TransientTrack *)t : NULL;

        int kf_count = DP_track_key_frame_count(t);
        for (int kf_index = 0; kf_index < kf_count; ++kf_index) {
            DP_KeyFrame *kf = DP_track_key_frame_at_noinc(t, kf_index);
            DP_TransientKeyFrame *tkf =
                DP_key_frame_transient(kf) ? (DP_TransientKeyFrame *)kf : NULL;
            // Key frames pointing at invalid layers get set to layer 0 (none).
            int layer_id = DP_key_frame_layer_id(kf);
            if (layer_id != 0 && !DP_layer_routes_search(lr, layer_id)) {
                timeline_cleanup_make_transient(tcs, &tl, &ttl, &t, &tt, &kf,
                                                &tkf, t_index, kf_index);
                DP_transient_key_frame_layer_id_set(tkf, 0);
            }
            // Layer attributes referring to invalid layers get removed.
            int kfl_count;
            const DP_KeyFrameLayer *kfls = DP_key_frame_layers(kf, &kfl_count);
            int kfls_deleted = 0;
            for (int kfl_index = 0; kfl_index < kfl_count; ++kfl_index) {
                DP_KeyFrameLayer kfl = kfls[kfl_index];
                if (!DP_layer_routes_search(lr, kfl.layer_id)) {
                    timeline_cleanup_make_transient(
                        tcs, &tl, &ttl, &t, &tt, &kf, &tkf, t_index, kf_index);
                    DP_transient_key_frame_layer_delete_at(
                        tkf, kfl_index - kfls_deleted);
                    ++kfls_deleted;
                }
            }
        }
    }
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


DP_LayerRoutes *
DP_transient_canvas_state_layer_routes_noinc(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return DP_canvas_state_layer_routes_noinc((DP_CanvasState *)tcs);
}

DP_AnnotationList *
DP_transient_canvas_state_annotations_noinc(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return DP_canvas_state_annotations_noinc((DP_CanvasState *)tcs);
}

DP_DocumentMetadata *
DP_transient_canvas_state_metadata_noinc(DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return DP_canvas_state_metadata_noinc((DP_CanvasState *)tcs);
}

DP_SelectionSet *DP_transient_canvas_state_selections_noinc_nullable(
    DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    return DP_canvas_state_selections_noinc_nullable((DP_CanvasState *)tcs);
}

void DP_transient_canvas_state_layers_set_inc(DP_TransientCanvasState *tcs,
                                              DP_LayerList *ll)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_ASSERT(ll);
    DP_layer_list_decref(tcs->layers);
    tcs->layers = DP_layer_list_incref(ll);
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

void DP_transient_canvas_state_layer_props_set_inc(DP_TransientCanvasState *tcs,
                                                   DP_LayerPropsList *lpl)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_layer_props_list_decref(tcs->layer_props);
    tcs->layer_props = DP_layer_props_list_incref(lpl);
}

void DP_transient_canvas_state_transient_layer_props_set_noinc(
    DP_TransientCanvasState *tcs, DP_TransientLayerPropsList *tlpl)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_layer_props_list_decref(tcs->layer_props);
    tcs->transient_layer_props = tlpl;
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

void DP_transient_canvas_state_timeline_set_inc(DP_TransientCanvasState *tcs,
                                                DP_Timeline *tl)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_timeline_decref(tcs->timeline);
    tcs->timeline = DP_timeline_incref(tl);
}

void DP_transient_canvas_state_transient_timeline_set_noinc(
    DP_TransientCanvasState *tcs, DP_TransientTimeline *ttl)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_timeline_decref(tcs->timeline);
    tcs->transient_timeline = ttl;
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

void DP_transient_canvas_state_transient_selections_set_noinc(
    DP_TransientCanvasState *tcs, DP_TransientSelectionSet *tss)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_ASSERT(tss);
    DP_ASSERT(tss != tcs->transient_selections);
    DP_selection_set_decref_nullable(tcs->selections);
    tcs->transient_selections = tss;
}

void DP_transient_canvas_state_transient_selections_clear(
    DP_TransientCanvasState *tcs)
{
    DP_ASSERT(tcs);
    DP_ASSERT(DP_atomic_get(&tcs->refcount) > 0);
    DP_ASSERT(tcs->transient);
    DP_selection_set_decref_nullable(tcs->selections);
    tcs->transient_selections = NULL;
}

void DP_transient_canvas_state_intuit_background(DP_TransientCanvasState *tcs)
{
    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 0);
    int count = DP_transient_layer_props_list_count(tlpl);

    DP_LayerProps *lp =
        count == 0 ? NULL : DP_transient_layer_props_list_at_noinc(tlpl, 0);
    bool looks_like_background =
        lp && DP_layer_props_opacity(lp) == DP_BIT15
        && DP_layer_props_blend_mode(lp) == DP_BLEND_MODE_NORMAL
        && !DP_layer_props_hidden(lp) && !DP_layer_props_children_noinc(lp);

    if (looks_like_background) {
        DP_TransientLayerList *tll =
            DP_transient_canvas_state_transient_layers(tcs, 0);
        DP_LayerContent *lc = DP_layer_list_entry_content_noinc(
            DP_transient_layer_list_at_noinc(tll, 0));

        DP_Pixel15 pixel;
        if (DP_layer_content_same_pixel(lc, &pixel)) {
            DP_transient_canvas_state_background_tile_set_noinc(
                tcs, DP_tile_new_from_pixel15(0, pixel), pixel.a == DP_BIT15);
            DP_transient_layer_props_list_delete_at(tlpl, 0);
            DP_transient_layer_list_delete_at(tll, 0);
        }
    }
}
