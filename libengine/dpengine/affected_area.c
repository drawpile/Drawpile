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
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpmsg/message.h>
#include <limits.h>


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

static DP_AffectedArea make_timeline(int frame)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_TIMELINE, frame,
                             INVALID_BOUNDS};
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
                                 int32_t dab_y, uint16_t dab_size)
{
    double x = s->last_x + DP_int32_to_double(dab_x) / 4.0;
    double y = s->last_y + DP_int32_to_double(dab_y) / 4.0;
    double d = DP_uint16_to_double(dab_size) / 256.0;
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

static DP_AffectedArea make_pixel_dabs_affected_area(DP_MsgDrawDabsPixel *mddp)
{
    if (DP_msg_draw_dabs_pixel_indirect(mddp)) {
        return make_user_attrs();
    }
    else {
        return make_pixels(DP_msg_draw_dabs_pixel_layer(mddp),
                           pixel_dabs_bounds(mddp));
    }
}

DP_AffectedArea DP_affected_area_make(DP_Message *msg, DP_CanvasState *cs)
{
    DP_MessageType type = DP_message_type(msg);
    switch (type) {
    case DP_MSG_LAYER_CREATE:
        return make_layer_attrs(
            DP_msg_layer_create_id(DP_msg_layer_create_cast(msg)));
    case DP_MSG_LAYER_ATTRIBUTES:
        return make_layer_attrs(
            DP_msg_layer_attributes_id(DP_msg_layer_attributes_cast(msg)));
    case DP_MSG_LAYER_RETITLE:
        return make_layer_attrs(
            DP_msg_layer_retitle_id(DP_msg_layer_retitle_cast(msg)));
    case DP_MSG_LAYER_VISIBILITY:
        return make_user_attrs();
    case DP_MSG_PUT_IMAGE: {
        DP_MsgPutImage *mpi = DP_msg_put_image_cast(msg);
        return make_pixels(
            DP_msg_put_image_layer(mpi),
            DP_rect_make(DP_uint32_to_int(DP_msg_put_image_x(mpi)),
                         DP_uint32_to_int(DP_msg_put_image_y(mpi)),
                         DP_uint32_to_int(DP_msg_put_image_w(mpi)),
                         DP_uint32_to_int(DP_msg_put_image_h(mpi))));
    }
    case DP_MSG_PUT_TILE: {
        DP_MsgPutTile *mpt = DP_msg_put_tile_cast(msg);
        if (DP_msg_put_tile_sublayer(mpt) > 0) {
            return make_user_attrs();
        }
        else if (DP_msg_put_tile_repeat(mpt) > 0) {
            // We can't intuit the affected area because we don't know the
            // canvas size here. This only happens during resets though, so
            // local fork performance is irrelevant and we punt to everything.
            return make_everything();
        }
        else {
            return make_pixels(
                DP_msg_put_tile_layer(mpt),
                DP_rect_make(DP_msg_put_tile_col(mpt) * DP_TILE_SIZE,
                             DP_msg_put_tile_row(mpt) * DP_TILE_SIZE,
                             DP_TILE_SIZE, DP_TILE_SIZE));
        }
    }
    case DP_MSG_DRAW_DABS_CLASSIC: {
        DP_MsgDrawDabsClassic *mddc = DP_msg_draw_dabs_classic_cast(msg);
        if (DP_msg_draw_dabs_classic_indirect(mddc)) {
            return make_user_attrs();
        }
        else {
            return make_pixels(DP_msg_draw_dabs_classic_layer(mddc),
                               classic_dabs_bounds(mddc));
        }
    }
    case DP_MSG_DRAW_DABS_PIXEL:
        return make_pixel_dabs_affected_area(DP_msg_draw_dabs_pixel_cast(msg));
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
        return make_pixel_dabs_affected_area(DP_msg_draw_dabs_pixel_square_cast(msg));
    case DP_MSG_DRAW_DABS_MYPAINT: {
        DP_MsgDrawDabsMyPaint *mddmp = DP_msg_draw_dabs_mypaint_cast(msg);
        return make_pixels(DP_msg_draw_dabs_mypaint_layer(mddmp),
                           mypaint_dabs_bounds(mddmp));
    }
    case DP_MSG_PEN_UP: {
        int x, y, w, h;
        int layer_id = DP_canvas_state_search_change_bounds(
            cs, DP_message_context_id(msg), &x, &y, &w, &h);
        return layer_id == 0 ? make_user_attrs()
                             : make_pixels(layer_id, DP_rect_make(x, y, w, h));
    }
    case DP_MSG_FILL_RECT: {
        DP_MsgFillRect *mfr = DP_msg_fill_rect_cast(msg);
        return make_pixels(
            DP_msg_fill_rect_layer(mfr),
            DP_rect_make(DP_uint32_to_int(DP_msg_fill_rect_x(mfr)),
                         DP_uint32_to_int(DP_msg_fill_rect_y(mfr)),
                         DP_uint32_to_int(DP_msg_fill_rect_w(mfr)),
                         DP_uint32_to_int(DP_msg_fill_rect_h(mfr))));
    }
    case DP_MSG_ANNOTATION_CREATE:
        return make_annotations(
            DP_msg_annotation_create_id(DP_msg_annotation_create_cast(msg)));
    case DP_MSG_ANNOTATION_RESHAPE:
        return make_annotations(
            DP_msg_annotation_reshape_id(DP_msg_annotation_reshape_cast(msg)));
    case DP_MSG_ANNOTATION_EDIT:
        return make_annotations(
            DP_msg_annotation_edit_id(DP_msg_annotation_edit_cast(msg)));
    case DP_MSG_ANNOTATION_DELETE:
        return make_annotations(
            DP_msg_annotation_delete_id(DP_msg_annotation_delete_cast(msg)));
    case DP_MSG_MOVE_RECT: {
        DP_MsgMoveRect *mrr = DP_msg_move_rect_cast(msg);
        return make_pixels(DP_msg_move_rect_layer(mrr), move_rect_bounds(mrr));
    }
    case DP_MSG_UNDO_POINT:
        return make_user_attrs();
    case DP_MSG_CANVAS_BACKGROUND:
        return make_canvas_background();
    case DP_MSG_SET_METADATA_INT:
        return make_document_metadata(
            DP_msg_set_metadata_int_field(DP_msg_set_metadata_int_cast(msg)));
    case DP_MSG_SET_METADATA_STR:
        return make_document_metadata(
            DP_msg_set_metadata_str_field(DP_msg_set_metadata_str_cast(msg)));
    case DP_MSG_SET_TIMELINE_FRAME:
        return make_timeline(DP_msg_set_timeline_frame_frame(
            DP_msg_set_timeline_frame_cast(msg)));
    case DP_MSG_REMOVE_TIMELINE_FRAME:
        return make_timeline(DP_msg_remove_timeline_frame_frame(
            DP_msg_remove_timeline_frame_cast(msg)));
    default:
        DP_debug("Unhandled message of type %d (%s) affects everything",
                 (int)type, DP_message_type_enum_name(type));
        return make_everything();
    }
}

bool DP_affected_area_concurrent_with(const DP_AffectedArea *a,
                                      const DP_AffectedArea *b)
{
    DP_ASSERT(a);
    DP_ASSERT(b);
    DP_AffectedDomain a_domain = a->domain, b_domain = b->domain;
    // Affecting everything means being concurrent with nothing.
    return a_domain != DP_AFFECTED_DOMAIN_EVERYTHING
        && b_domain != DP_AFFECTED_DOMAIN_EVERYTHING
        && ( // The local user's changes are always concurrent.
               a_domain == DP_AFFECTED_DOMAIN_USER_ATTRS
               // Affecting different domains is concurrent.
               || a_domain != b_domain
               // Affecting different layers or annotations is concurrent.
               || a->affected_id != b->affected_id
               // Affecting different pixels on the same layer is concurrent.
               || (a_domain == DP_AFFECTED_DOMAIN_PIXELS
                   && DP_rect_intersects(a->bounds, b->bounds)));
}
