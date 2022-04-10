#include "affected_area.h"
#include "canvas_state.h"
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/geom.h>
#include <dpmsg/message.h>
#include <dpmsg/messages/annotation_create.h>
#include <dpmsg/messages/annotation_delete.h>
#include <dpmsg/messages/annotation_edit.h>
#include <dpmsg/messages/annotation_reshape.h>
#include <dpmsg/messages/draw_dabs.h>
#include <dpmsg/messages/fill_rect.h>
#include <dpmsg/messages/layer_attr.h>
#include <dpmsg/messages/layer_create.h>
#include <dpmsg/messages/layer_retitle.h>
#include <dpmsg/messages/put_image.h>
#include <dpmsg/messages/put_tile.h>
#include <dpmsg/messages/region_move.h>
#include <limits.h>


#define INVALID_BOUNDS                     \
    (DP_Rect)                              \
    {                                      \
        INT_MAX, INT_MIN, INT_MAX, INT_MIN \
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

static DP_AffectedArea make_everything(void)
{
    return (DP_AffectedArea){DP_AFFECTED_DOMAIN_EVERYTHING, 0, INVALID_BOUNDS};
}

static DP_Rect draw_dabs_bounds(DP_MsgDrawDabs *mdd)
{
    int x, y, w, h;
    DP_msg_draw_dabs_bounds(mdd, &x, &y, &w, &h);
    return DP_rect_make(x, y, w, h);
}

static DP_Rect region_move_bounds(DP_MsgRegionMove *mrm)
{
    int src_x, src_y, src_w, src_h;
    DP_msg_region_move_src_rect(mrm, &src_x, &src_y, &src_w, &src_h);
    DP_Rect src = DP_rect_make(src_x, src_y, src_w, src_h);
    if (!DP_rect_valid(src)) {
        return INVALID_BOUNDS;
    }

    int x1, y1, x2, y2, x3, y3, x4, y4;
    DP_msg_region_move_dst_quad(mrm, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4);
    DP_Rect dst = DP_quad_bounds(DP_quad_make(x1, y1, x2, y2, x3, y3, x4, y4));
    if (!DP_rect_valid(dst)) {
        return INVALID_BOUNDS;
    }

    return DP_rect_union(src, dst);
}

DP_AffectedArea DP_affected_area_make(DP_Message *msg, DP_CanvasState *cs)
{
    DP_MessageType type = DP_message_type(msg);
    switch (type) {
    case DP_MSG_LAYER_CREATE:
        return make_layer_attrs(
            DP_msg_layer_create_layer_id(DP_msg_layer_create_cast(msg)));
    case DP_MSG_LAYER_ATTR:
        return make_layer_attrs(
            DP_msg_layer_attr_layer_id(DP_msg_layer_attr_cast(msg)));
    case DP_MSG_LAYER_RETITLE:
        return make_layer_attrs(
            DP_msg_layer_retitle_layer_id(DP_msg_layer_retitle_cast(msg)));
    case DP_MSG_LAYER_VISIBILITY:
        return make_user_attrs();
    case DP_MSG_PUT_IMAGE: {
        DP_MsgPutImage *mpi = DP_msg_put_image_cast(msg);
        return make_pixels(DP_msg_put_image_layer_id(mpi),
                           DP_rect_make(DP_msg_put_image_x(mpi),
                                        DP_msg_put_image_y(mpi),
                                        DP_msg_put_image_width(mpi),
                                        DP_msg_put_image_height(mpi)));
    }
    case DP_MSG_PUT_TILE: {
        DP_MsgPutTile *mpt = DP_msg_put_tile_cast(msg);
        return make_pixels(DP_msg_put_tile_layer_id(mpt),
                           DP_rect_make(DP_msg_put_tile_x(mpt) * DP_TILE_SIZE,
                                        DP_msg_put_tile_y(mpt) * DP_TILE_SIZE,
                                        DP_TILE_SIZE, DP_TILE_SIZE));
    }
    case DP_MSG_DRAW_DABS_CLASSIC:
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE: {
        DP_MsgDrawDabs *mdd = DP_msg_draw_dabs_cast(msg);
        if (DP_msg_draw_dabs_indirect(mdd)) {
            return make_user_attrs(); // Will be dealt with on pen up.
        }
        else {
            return make_pixels(DP_msg_draw_dabs_layer_id(mdd),
                               draw_dabs_bounds(mdd));
        }
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
        return make_pixels(DP_msg_fill_rect_layer_id(mfr),
                           DP_rect_make(DP_msg_fill_rect_x(mfr),
                                        DP_msg_fill_rect_y(mfr),
                                        DP_msg_fill_rect_width(mfr),
                                        DP_msg_fill_rect_height(mfr)));
    }
    case DP_MSG_ANNOTATION_CREATE:
        return make_annotations(DP_msg_annotation_create_annotation_id(
            DP_msg_annotation_create_cast(msg)));
    case DP_MSG_ANNOTATION_RESHAPE:
        return make_annotations(DP_msg_annotation_reshape_annotation_id(
            DP_msg_annotation_reshape_cast(msg)));
    case DP_MSG_ANNOTATION_EDIT:
        return make_annotations(DP_msg_annotation_edit_annotation_id(
            DP_msg_annotation_edit_cast(msg)));
    case DP_MSG_ANNOTATION_DELETE:
        return make_annotations(DP_msg_annotation_delete_annotation_id(
            DP_msg_annotation_delete_cast(msg)));
    case DP_MSG_REGION_MOVE: {
        DP_MsgRegionMove *mrm = DP_msg_region_move_cast(msg);
        return make_pixels(DP_msg_region_move_layer_id(mrm),
                           region_move_bounds(mrm));
    }
    case DP_MSG_UNDO_POINT:
        return make_user_attrs();
    case DP_MSG_CANVAS_BACKGROUND:
        return make_canvas_background();
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
