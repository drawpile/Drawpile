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
#include "affected_area.h"
#include "canvas_state.h"
#include "pixels.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/ids.h>
#include <dpmsg/message.h>


#define ALL_IDS INT_MIN

#define INVALID_BOUNDS                     \
    (DP_Rect)                              \
    {                                      \
        INT_MAX, INT_MAX, INT_MIN, INT_MIN \
    }

static DP_AffectedArea make_user_attrs(void)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_USER_ATTRS, 0, INVALID_BOUNDS};
}

static DP_AffectedArea make_layer_attrs(int layer_id)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_LAYER_ATTRS, layer_id,
                             INVALID_BOUNDS};
}

static DP_AffectedArea make_annotations(int annotation_id)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_ANNOTATIONS, annotation_id,
                             INVALID_BOUNDS};
}

static DP_AffectedArea make_pixels(int layer_id, DP_Rect bounds)
{
    return DP_rect_valid(bounds)
             ? (DP_AffectedArea){DP_AFFECTED_DOMAIN_PIXELS, layer_id, bounds}
             : make_user_attrs();
}

static DP_AffectedArea make_canvas_background(void)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_CANVAS_BACKGROUND, 0,
                             INVALID_BOUNDS};
}

static DP_AffectedArea make_document_metadata(int field)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_DOCUMENT_METADATA, field,
                             INVALID_BOUNDS};
}

static DP_AffectedArea make_timeline(int track_id)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_TIMELINE, track_id,
                             INVALID_BOUNDS};
}

static DP_AffectedArea make_selections(uint8_t context_id, uint8_t selection_id)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_SELECTIONS,
                             (context_id << 8) | selection_id, INVALID_BOUNDS};
}

static DP_AffectedArea make_everything(void)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_EVERYTHING, 0, INVALID_BOUNDS};
}

struct SubpixelDabs {
    double last_x, last_y;
    DP_Rect bounds;
};

static struct SubpixelDabs subpixel_dabs_init(int32_t x, int32_t y)
{
    struct SubpixelDabs s;
    s.last_x = DP_int32_to_double(x) / 4.0;
    s.last_y = DP_int32_to_double(y) / 4.0;
    s.bounds = DP_rect_make(DP_double_to_int(s.last_x),
                            DP_double_to_int(s.last_y), 1, 1);
    return s;
}

static void subpixel_dabs_update(struct SubpixelDabs *s, int32_t dab_x,
                                 int32_t dab_y, uint32_t dab_size)
{
    double x = s->last_x + DP_int32_to_double(dab_x) / 4.0;
    double y = s->last_y + DP_int32_to_double(dab_y) / 4.0;
    double d = DP_uint32_to_double(dab_size) / 256.0;
    double r = d / 2.0;
    int dim = DP_double_to_int(d) + 1;
    s->bounds = DP_rect_union(s->bounds,
                              DP_rect_make(DP_double_to_int(x - r),
                                           DP_double_to_int(y - r), dim, dim));
    s->last_x = x;
    s->last_y = y;
}

static DP_Rect classic_dabs_bounds(DP_MsgDrawDabsClassic *mddc)
{
    struct SubpixelDabs s = subpixel_dabs_init(
        DP_msg_draw_dabs_classic_x(mddc), DP_msg_draw_dabs_classic_y(mddc));
    int count;
    const DP_ClassicDab *dabs = DP_msg_draw_dabs_classic_dabs(mddc, &count);
    for (int i = 0; i < count; ++i) {
        const DP_ClassicDab *dab = DP_classic_dab_at(dabs, i);
        subpixel_dabs_update(&s, DP_classic_dab_x(dab), DP_classic_dab_y(dab),
                             DP_classic_dab_size(dab));
    }
    return s.bounds;
}

static DP_Rect mypaint_dabs_bounds(DP_MsgDrawDabsMyPaint *mddmp)
{
    struct SubpixelDabs s = subpixel_dabs_init(
        DP_msg_draw_dabs_mypaint_x(mddmp), DP_msg_draw_dabs_mypaint_y(mddmp));
    int count;
    const DP_MyPaintDab *dabs = DP_msg_draw_dabs_mypaint_dabs(mddmp, &count);
    for (int i = 0; i < count; ++i) {
        const DP_MyPaintDab *dab = DP_mypaint_dab_at(dabs, i);
        subpixel_dabs_update(&s, DP_mypaint_dab_x(dab), DP_mypaint_dab_y(dab),
                             DP_mypaint_dab_size(dab));
    }
    return s.bounds;
}

static DP_Rect mypaint_blend_dabs_bounds(DP_MsgDrawDabsMyPaintBlend *mddmpb)
{
    struct SubpixelDabs s =
        subpixel_dabs_init(DP_msg_draw_dabs_mypaint_blend_x(mddmpb),
                           DP_msg_draw_dabs_mypaint_blend_y(mddmpb));
    int count;
    const DP_MyPaintBlendDab *dabs =
        DP_msg_draw_dabs_mypaint_blend_dabs(mddmpb, &count);
    for (int i = 0; i < count; ++i) {
        const DP_MyPaintBlendDab *dab = DP_mypaint_blend_dab_at(dabs, i);
        subpixel_dabs_update(&s, DP_mypaint_blend_dab_x(dab),
                             DP_mypaint_blend_dab_y(dab),
                             DP_mypaint_blend_dab_size(dab));
    }
    return s.bounds;
}

static DP_Rect pixel_dabs_bounds(DP_MsgDrawDabsPixel *mddp)
{
    int last_x = DP_msg_draw_dabs_pixel_x(mddp);
    int last_y = DP_msg_draw_dabs_pixel_y(mddp);
    DP_Rect bounds = DP_rect_make(last_x, last_y, 1, 1);

    int count;
    const DP_PixelDab *dabs = DP_msg_draw_dabs_pixel_dabs(mddp, &count);
    for (int i = 0; i < count; ++i) {
        const DP_PixelDab *dab = DP_pixel_dab_at(dabs, i);
        int x = last_x + DP_pixel_dab_x(dab);
        int y = last_y + DP_pixel_dab_y(dab);
        int d = DP_pixel_dab_size(dab) + 1;
        int r = d / 2;
        bounds =
            DP_rect_union(bounds, DP_rect_make(DP_double_to_int(x - r),
                                               DP_double_to_int(y - r), d, d));
        last_x = x;
        last_y = y;
    }

    return bounds;
}

static DP_Rect region_bounds(int bx, int by, int bw, int bh, int x1, int y1,
                             int x2, int y2, int x3, int y3, int x4, int y4)
{
    DP_Rect src = DP_rect_make(bx, by, bw, bh);
    if (!DP_rect_valid(src)) {
        return INVALID_BOUNDS;
    }

    DP_Quad dst_quad = DP_quad_make(x1, y1, x2, y2, x3, y3, x4, y4);
    DP_Rect dst = DP_quad_bounds(dst_quad);
    if (!DP_rect_valid(dst)) {
        return INVALID_BOUNDS;
    }

    return DP_rect_union(src, dst);
}

static DP_Rect move_rect_bounds(DP_MsgMoveRect *mrr)
{
    int sx = DP_msg_move_rect_sx(mrr);
    int sy = DP_msg_move_rect_sy(mrr);
    int w = DP_msg_move_rect_w(mrr);
    int h = DP_msg_move_rect_h(mrr);
    DP_Rect src = DP_rect_make(sx, sy, w, h);
    if (!DP_rect_valid(src)) {
        return INVALID_BOUNDS;
    }

    int tx = DP_msg_move_rect_tx(mrr);
    int ty = DP_msg_move_rect_ty(mrr);
    DP_Rect dst = DP_rect_make(tx, ty, w, h);
    if (!DP_rect_valid(dst)) {
        return INVALID_BOUNDS;
    }

    return DP_rect_union(src, dst);
}

static DP_AffectedArea update_indirect_area(DP_AffectedIndirectAreas *aia,
                                            unsigned int context_id,
                                            int layer_id, DP_Rect bounds)
{
    DP_ASSERT(context_id < DP_AFFECTED_INDIRECT_AREAS_COUNT);
    DP_IndirectArea *ia = &aia->areas[context_id];
    if (DP_rect_valid(ia->bounds)) {
        // This shouldn't happen if a client behaves properly. If it
        // does, we'll just punt to the pen up affecting everything.
        if (ia->layer_id != layer_id && ia->layer_id != -1) {
            DP_warn("User %u drawing on layers %d and %d without a pen up",
                    context_id, ia->layer_id, layer_id);
            ia->layer_id = -1;
        }
        ia->bounds = DP_rect_union(ia->bounds, bounds);
    }
    else {
        ia->layer_id = layer_id;
        ia->bounds = bounds;
    }
    return make_user_attrs();
}

static DP_AffectedArea take_indirect_area(DP_AffectedIndirectAreas *aia,
                                          unsigned int context_id)
{
    DP_ASSERT(context_id < DP_AFFECTED_INDIRECT_AREAS_COUNT);
    DP_IndirectArea *ia = &aia->areas[context_id];
    DP_Rect bounds = ia->bounds;
    if (DP_rect_valid(bounds)) {
        int layer_id = ia->layer_id;
        ia->layer_id = -1;
        ia->bounds = INVALID_BOUNDS;
        return layer_id > 0 ? make_pixels(layer_id, bounds) : make_everything();
    }
    else {
        return make_user_attrs();
    }
}

DP_AffectedArea DP_affected_area_make(DP_Message *msg,
                                      DP_AffectedIndirectAreas *aia_or_null)
{
    DP_MessageType type = DP_message_type(msg);
    switch (type) {
    case DP_MSG_CANVAS_RESIZE:
        return make_everything();
    case DP_MSG_LAYER_TREE_CREATE:
        // Creating a layer is complicated business, it might copy an entire
        // layer group for example. So we'll err on the side of caution and
        // say that it conflicts with every other layer domain message. Not
        // like those are super common anyway, so it shouldn't be disruptive.
        return make_layer_attrs(ALL_IDS);
    case DP_MSG_LAYER_ATTRIBUTES:
        return make_layer_attrs(DP_protocol_to_layer_id(
            DP_msg_layer_attributes_id(DP_message_internal(msg))));
    case DP_MSG_LAYER_RETITLE:
        return make_layer_attrs(DP_protocol_to_layer_id(
            DP_msg_layer_retitle_id(DP_message_internal(msg))));
    case DP_MSG_LAYER_TREE_MOVE:
        // Moving a layer is dependent on the state of the source, parent and
        // sibling layer, which is beyond what we can represent.
        return make_layer_attrs(ALL_IDS);
    case DP_MSG_LAYER_TREE_DELETE: {
        DP_MsgLayerTreeDelete *mtld = DP_message_internal(msg);
        // If this layer gets merged into another, we affect two layers, which
        // we can't represent in our affected area structure. But since changing
        // layer properties is pretty rare, we'll just say that it conflicts
        // with every other layer domain message and not bother special-casing.
        return make_layer_attrs(
            DP_msg_layer_tree_delete_merge_to(mtld) == 0
                ? DP_protocol_to_layer_id(DP_msg_layer_tree_delete_id(mtld))
                : ALL_IDS);
    }
    case DP_MSG_PUT_IMAGE: {
        DP_MsgPutImage *mpi = DP_message_internal(msg);
        switch (DP_msg_put_image_mode(mpi)) {
        case DP_BLEND_MODE_COMPAT_LOCAL_MATCH:
            // Compatibility hack: LocalMatch in disguise.
            return make_user_attrs();
        default:
            return make_pixels(
                DP_protocol_to_layer_id(DP_msg_put_image_layer(mpi)),
                DP_rect_make(DP_uint32_to_int(DP_msg_put_image_x(mpi)),
                             DP_uint32_to_int(DP_msg_put_image_y(mpi)),
                             DP_uint32_to_int(DP_msg_put_image_w(mpi)),
                             DP_uint32_to_int(DP_msg_put_image_h(mpi))));
        }
    }
    case DP_MSG_PUT_TILE: {
        DP_MsgPutTile *mpt = DP_message_internal(msg);
        int layer_id = DP_protocol_to_layer_id(DP_msg_put_tile_layer(mpt));
        unsigned int sublayer_id = DP_msg_put_tile_sublayer(mpt);
        // If there's a repetition involved, we can't intuit where it will end,
        // since we don't know the canvas size at this time. So we just act as
        // if it covers the entire rest of the canvas from this row onwards.
        int y = DP_msg_put_tile_row(mpt) * DP_TILE_SIZE;
        DP_Rect bounds =
            DP_msg_put_tile_repeat(mpt) > 0
                ? DP_rect_make(0, y, INT_MAX, INT_MAX - y)
                : DP_rect_make(DP_msg_put_tile_col(mpt) * DP_TILE_SIZE, y,
                               DP_TILE_SIZE, DP_TILE_SIZE);
        if (aia_or_null && sublayer_id != 0) {
            return update_indirect_area(aia_or_null, sublayer_id, layer_id,
                                        bounds);
        }
        else {
            return make_pixels(layer_id, bounds);
        }
    }
    case DP_MSG_DRAW_DABS_CLASSIC: {
        DP_MsgDrawDabsClassic *mddc = DP_message_internal(msg);
        int layer_id =
            DP_protocol_to_layer_id(DP_msg_draw_dabs_classic_layer(mddc));
        DP_Rect bounds = classic_dabs_bounds(mddc);
        if (aia_or_null
            && DP_msg_draw_dabs_classic_paint_mode(mddc)
                   != DP_PAINT_MODE_DIRECT) {
            return update_indirect_area(aia_or_null, DP_message_context_id(msg),
                                        layer_id, bounds);
        }
        else {
            return make_pixels(layer_id, bounds);
        }
    }
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE: {
        DP_MsgDrawDabsPixel *mddp = DP_message_internal(msg);
        int layer_id =
            DP_protocol_to_layer_id(DP_msg_draw_dabs_pixel_layer(mddp));
        DP_Rect bounds = pixel_dabs_bounds(mddp);
        if (aia_or_null
            && DP_msg_draw_dabs_pixel_paint_mode(mddp)
                   != DP_PAINT_MODE_DIRECT) {
            return update_indirect_area(aia_or_null, DP_message_context_id(msg),
                                        layer_id, bounds);
        }
        else {
            return make_pixels(layer_id, bounds);
        }
    }
    case DP_MSG_DRAW_DABS_MYPAINT: {
        DP_MsgDrawDabsMyPaint *mddmp = DP_message_internal(msg);
        int layer_id =
            DP_protocol_to_layer_id(DP_msg_draw_dabs_mypaint_layer(mddmp));
        DP_Rect bounds = mypaint_dabs_bounds(mddmp);
        return make_pixels(layer_id, bounds);
    }
    case DP_MSG_DRAW_DABS_MYPAINT_BLEND: {
        DP_MsgDrawDabsMyPaintBlend *mddmpb = DP_message_internal(msg);
        int layer_id = DP_protocol_to_layer_id(
            DP_msg_draw_dabs_mypaint_blend_layer(mddmpb));
        DP_Rect bounds = mypaint_blend_dabs_bounds(mddmpb);
        if (aia_or_null
            && DP_msg_draw_dabs_mypaint_blend_paint_mode(mddmpb)
                   != DP_PAINT_MODE_DIRECT) {
            return update_indirect_area(aia_or_null, DP_message_context_id(msg),
                                        layer_id, bounds);
        }
        else {
            return make_pixels(layer_id, bounds);
        }
    }
    case DP_MSG_PEN_UP: {
        unsigned int context_id = DP_message_context_id(msg);
        DP_ASSERT(context_id < DP_AFFECTED_INDIRECT_AREAS_COUNT);
        // Context id 0 is only used when drawing locally, where there won't be
        // any conflicts, and when merging all sublayers after a disconnect.
        return context_id == 0 || !aia_or_null
                 ? make_everything()
                 : take_indirect_area(aia_or_null, context_id);
    }
    case DP_MSG_FILL_RECT: {
        DP_MsgFillRect *mfr = DP_message_internal(msg);
        return make_pixels(
            DP_protocol_to_layer_id(DP_msg_fill_rect_layer(mfr)),
            DP_rect_make(DP_uint32_to_int(DP_msg_fill_rect_x(mfr)),
                         DP_uint32_to_int(DP_msg_fill_rect_y(mfr)),
                         DP_uint32_to_int(DP_msg_fill_rect_w(mfr)),
                         DP_uint32_to_int(DP_msg_fill_rect_h(mfr))));
    }
    case DP_MSG_ANNOTATION_CREATE:
        return make_annotations(
            DP_msg_annotation_create_id(DP_message_internal(msg)));
    case DP_MSG_ANNOTATION_RESHAPE:
        return make_annotations(
            DP_msg_annotation_reshape_id(DP_message_internal(msg)));
    case DP_MSG_ANNOTATION_EDIT:
        return make_annotations(
            DP_msg_annotation_edit_id(DP_message_internal(msg)));
    case DP_MSG_ANNOTATION_DELETE:
        return make_annotations(
            DP_msg_annotation_delete_id(DP_message_internal(msg)));
    // Move rect and transform regionmessages can take stuff from one layer and
    // move it to another, affecting two different layers. Since we can't
    // represent that, we punt to affecting all layers in that case. Switching
    // layers during a transform is pretty uncommon, so that's okay.
    case DP_MSG_MOVE_RECT: {
        DP_MsgMoveRect *mmr = DP_message_internal(msg);
        int source_id = DP_protocol_to_layer_id(DP_msg_move_rect_source(mmr));
        int target_id = DP_protocol_to_layer_id(DP_msg_move_rect_layer(mmr));
        return make_pixels(source_id == target_id ? source_id : ALL_IDS,
                           move_rect_bounds(mmr));
    }
    case DP_MSG_TRANSFORM_REGION: {
        DP_MsgTransformRegion *mtr = DP_message_internal(msg);
        int source_id =
            DP_protocol_to_layer_id(DP_msg_transform_region_source(mtr));
        int target_id =
            DP_protocol_to_layer_id(DP_msg_transform_region_layer(mtr));
        return make_pixels(source_id == target_id ? source_id : ALL_IDS,
                           region_bounds(DP_msg_transform_region_bx(mtr),
                                         DP_msg_transform_region_by(mtr),
                                         DP_msg_transform_region_bw(mtr),
                                         DP_msg_transform_region_bh(mtr),
                                         DP_msg_transform_region_x1(mtr),
                                         DP_msg_transform_region_y1(mtr),
                                         DP_msg_transform_region_x2(mtr),
                                         DP_msg_transform_region_y2(mtr),
                                         DP_msg_transform_region_x3(mtr),
                                         DP_msg_transform_region_y3(mtr),
                                         DP_msg_transform_region_x4(mtr),
                                         DP_msg_transform_region_y4(mtr)));
    }
    case DP_MSG_UNDO_POINT:
        return make_user_attrs();
    case DP_MSG_CANVAS_BACKGROUND:
        return make_canvas_background();
    case DP_MSG_SET_METADATA_INT: {
        int field = DP_msg_set_metadata_int_field(DP_message_internal(msg));
        return field == DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT
                 ? make_timeline(ALL_IDS)
                 : make_document_metadata(field);
    }
    case DP_MSG_TRACK_CREATE: {
        DP_MsgTrackCreate *mtc = DP_message_internal(msg);
        // Duplicating a track affects two track ids, so we punt to all.
        return make_timeline(DP_msg_track_create_source_id(mtc) == 0
                                 ? DP_msg_track_create_id(mtc)
                                 : ALL_IDS);
    }
    case DP_MSG_TRACK_RETITLE:
        return make_timeline(DP_msg_track_retitle_id(DP_message_internal(msg)));
    case DP_MSG_TRACK_DELETE:
        return make_timeline(DP_msg_track_delete_id(DP_message_internal(msg)));
    case DP_MSG_TRACK_ORDER:
        return make_timeline(ALL_IDS);
    case DP_MSG_KEY_FRAME_SET: {
        DP_MsgKeyFrameSet *mkfs = DP_message_internal(msg);
        int track_id = DP_msg_key_frame_set_track_id(mkfs);
        bool is_layer_source = DP_msg_key_frame_set_source(mkfs)
                            == DP_MSG_KEY_FRAME_SET_SOURCE_LAYER;
        bool single_track_id =
            is_layer_source
            || track_id
                   == DP_uint32_to_int(DP_msg_key_frame_set_source_id(mkfs));
        return make_timeline(single_track_id ? track_id : ALL_IDS);
    }
    case DP_MSG_KEY_FRAME_RETITLE:
        return make_timeline(
            DP_msg_key_frame_retitle_track_id(DP_message_internal(msg)));
    case DP_MSG_KEY_FRAME_LAYER_ATTRIBUTES:
        return make_timeline(DP_msg_key_frame_layer_attributes_track_id(
            DP_message_internal(msg)));
    case DP_MSG_KEY_FRAME_DELETE: {
        DP_MsgKeyFrameDelete *mkfd = DP_message_internal(msg);
        int track_id = DP_msg_key_frame_delete_track_id(mkfd);
        int move_track_id = DP_msg_key_frame_delete_move_track_id(mkfd);
        bool single_track_id = move_track_id == 0 || track_id == move_track_id;
        return make_timeline(single_track_id ? track_id : ALL_IDS);
    }
    case DP_MSG_SELECTION_PUT:
        return make_selections(
            DP_uint_to_uint8(DP_message_context_id(msg)),
            DP_msg_selection_put_selection_id(DP_message_internal(msg)));
    case DP_MSG_SELECTION_CLEAR:
        return make_selections(
            DP_uint_to_uint8(DP_message_context_id(msg)),
            DP_msg_selection_clear_selection_id(DP_message_internal(msg)));
    case DP_MSG_LOCAL_CHANGE:
    case DP_MSG_UNDO:
        return make_user_attrs();
    default:
        DP_debug("Unhandled message of type %d (%s) affects everything",
                 (int)type, DP_message_type_enum_name(type));
        return make_everything();
    }
}

static bool domains_conflict(DP_AffectedDomain a, DP_AffectedDomain b)
{
    // Affecting everything means being concurrent with nothing. The timeline
    // refers to layers, so to be safe, we make those domains always conflict
    // with each other too. It may work without this, but layer and timeline
    // operations are so rare that it ain't worth the desync risk.
    return a == DP_AFFECTED_DOMAIN_EVERYTHING
        || b == DP_AFFECTED_DOMAIN_EVERYTHING
        || (a == DP_AFFECTED_DOMAIN_LAYER_ATTRS
            && b == DP_AFFECTED_DOMAIN_TIMELINE)
        || (a == DP_AFFECTED_DOMAIN_TIMELINE
            && b == DP_AFFECTED_DOMAIN_LAYER_ATTRS);
}

static bool affected_ids_differ(int a, int b)
{
    return a != b && a != ALL_IDS && b != ALL_IDS;
}

bool DP_affected_area_concurrent_with(const DP_AffectedArea *a,
                                      const DP_AffectedArea *b)
{
    DP_ASSERT(a);
    DP_ASSERT(b);
    DP_AffectedDomain a_domain = a->domain, b_domain = b->domain;
    return !domains_conflict(a_domain, b_domain)
        && ( // The local user's changes are always concurrent.
            a_domain == DP_AFFECTED_DOMAIN_USER_ATTRS
            // Affecting different domains is generally concurrent.
            || a_domain != b_domain
            // Affecting different layers, annotations etc. is concurrent.
            || affected_ids_differ(a->affected_id, b->affected_id)
            // Affecting different pixels on the same layer is concurrent.
            || (a_domain == DP_AFFECTED_DOMAIN_PIXELS
                && !DP_rect_intersects(a->bounds, b->bounds)));
}

bool DP_affected_area_in_bounds(const DP_AffectedArea *aa, int x, int y,
                                int width, int height)
{
    DP_ASSERT(aa);
    DP_ASSERT(width >= 0);
    DP_ASSERT(height >= 0);
    return aa->domain != DP_AFFECTED_DOMAIN_PIXELS
        || DP_rect_intersects(DP_rect_make(x, y, width, height), aa->bounds);
}

void DP_affected_indirect_areas_clear(DP_AffectedIndirectAreas *aia)
{
    DP_ASSERT(aia);
    for (int i = 0; i < DP_AFFECTED_INDIRECT_AREAS_COUNT; ++i) {
        aia->areas[i] = (DP_IndirectArea){-1, INVALID_BOUNDS};
    }
}
