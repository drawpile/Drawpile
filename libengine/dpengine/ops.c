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
#include "ops.h"
#include "annotation.h"
#include "annotation_list.h"
#include "blend_mode.h"
#include "canvas_state.h"
#include "image.h"
#include "layer_content.h"
#include "layer_content_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "paint.h"
#include "tile.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>


DP_CanvasState *DP_ops_canvas_resize(DP_CanvasState *cs,
                                     unsigned int context_id, int top,
                                     int right, int bottom, int left)
{
    int north = -top;
    int west = -left;
    int east = DP_canvas_state_width(cs) + right;
    int south = DP_canvas_state_height(cs) + bottom;
    if (north >= south || west >= east) {
        DP_error_set("Invalid resize: borders are reversed");
        return NULL;
    }

    int width = east + left;
    int height = south + top;
    if (width < 1 || height < 1 || width > INT16_MAX || height > INT16_MAX) {
        DP_error_set("Invalid resize: %dx%d", width, height);
        return NULL;
    }

    DP_debug("Resize: width %d, height %d", width, height);
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_transient_canvas_state_width_set(tcs, width);
    DP_transient_canvas_state_height_set(tcs, height);

    DP_LayerContentList *lcl =
        DP_transient_canvas_state_layer_contents_noinc(tcs);
    if (DP_layer_content_list_count(lcl) > 0) {
        DP_TransientLayerContentList *tlcl = DP_layer_content_list_resize(
            lcl, context_id, top, right, bottom, left);
        DP_transient_canvas_state_transient_layer_contents_set_noinc(tcs, tlcl);
    }

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_layer_create(DP_CanvasState *cs, int layer_id,
                                    int source_id, DP_Tile *tile, bool insert,
                                    bool copy, const char *title,
                                    size_t title_length)
{
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    if (DP_layer_props_list_index_by_id(lpl, layer_id) >= 0) {
        DP_error_set("Create layer: id %d already exists", layer_id);
        return NULL;
    }

    int source_index;
    if (source_id > 0) {
        source_index = DP_layer_props_list_index_by_id(lpl, source_id);
        if (source_index < 0) {
            DP_error_set("Create layer: source id %d not found", source_id);
            return NULL;
        }
    }
    else {
        source_index = -1;
    }

    DP_TransientLayerContent *tlc;
    DP_TransientLayerProps *tlp;
    if (copy) {
        if (source_index < 0) {
            DP_error_set("Create layer: no copy source specified");
            return NULL;
        }
        DP_LayerContentList *lcl = DP_canvas_state_layer_contents_noinc(cs);
        DP_LayerContent *lc = DP_layer_content_list_at_noinc(lcl, source_index);
        tlc = DP_transient_layer_content_new(lc);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, source_index);
        tlp = DP_transient_layer_props_new(lp);
        DP_transient_layer_props_id_set(tlp, layer_id);
    }
    else {
        int width = DP_canvas_state_width(cs);
        int height = DP_canvas_state_height(cs);
        tlc = DP_transient_layer_content_new_init(width, height, tile);
        tlp = DP_transient_layer_props_new_init(layer_id);
    }
    DP_transient_layer_props_title_set(tlp, title, title_length);

    int target_index =
        insert ? source_index + 1 : DP_layer_props_list_count(lpl);
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);

    DP_transient_layer_content_list_insert_transient_noinc(
        DP_transient_canvas_state_transient_layer_contents(tcs, 1), tlc,
        target_index);

    DP_transient_layer_props_list_insert_transient_noinc(
        DP_transient_canvas_state_transient_layer_props(tcs, 1), tlp,
        target_index);

    return DP_transient_canvas_state_persist(tcs);
}


static DP_TransientLayerProps *get_layer_props(DP_TransientCanvasState *tcs,
                                               int index, int sublayer_id)
{
    if (sublayer_id > 0) {
        DP_TransientLayerContentList *tlcl =
            DP_transient_canvas_state_transient_layer_contents(tcs, 0);
        DP_TransientLayerContent *tlc =
            DP_transient_layer_content_list_transient_at_noinc(tlcl, index);
        DP_TransientLayerProps *sub_tlp;
        DP_transient_layer_content_list_transient_sublayer(tlc, sublayer_id,
                                                           NULL, &sub_tlp);
        return sub_tlp;
    }
    else {
        DP_TransientLayerPropsList *tlpl =
            DP_transient_canvas_state_transient_layer_props(tcs, 0);
        return DP_transient_layer_props_list_transient_at_noinc(tlpl, index);
    }
}

DP_CanvasState *DP_ops_layer_attr(DP_CanvasState *cs, int layer_id,
                                  int sublayer_id, uint8_t opacity,
                                  int blend_mode, bool censored, bool fixed)
{
    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Layer attributes: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerProps *tlp = get_layer_props(tcs, index, sublayer_id);
    DP_transient_layer_props_opacity_set(tlp, opacity);
    DP_transient_layer_props_blend_mode_set(tlp, blend_mode);
    DP_transient_layer_props_censored_set(tlp, censored);
    DP_transient_layer_props_fixed_set(tlp, fixed);

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_layer_reorder(DP_CanvasState *cs, int layer_id_count,
                                     int (*get_layer_id)(void *, int),
                                     void *user)
{
    DP_LayerContentList *lcl = DP_canvas_state_layer_contents_noinc(cs);
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    int layer_count = DP_layer_content_list_count(lcl);
    DP_ASSERT(layer_count == DP_layer_props_list_count(lpl));

    DP_TransientLayerContentList *tlcl =
        DP_transient_layer_content_list_new_init(layer_count);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_layer_props_list_new_init(layer_count);
    DP_TransientCanvasState *tcs =
        DP_transient_canvas_state_new_with_layers_noinc(cs, tlcl, tlpl);

    int fill = 0;
    for (int i = 0; i < layer_id_count; ++i) {
        int layer_id = get_layer_id(user, i);
        if (DP_transient_layer_props_list_index_by_id(tlpl, layer_id) == -1) {
            int from_index = DP_layer_props_list_index_by_id(lpl, layer_id);
            if (from_index != -1) {
                DP_LayerContent *lc =
                    DP_layer_content_list_at_noinc(lcl, from_index);
                DP_LayerProps *lp =
                    DP_layer_props_list_at_noinc(lpl, from_index);
                DP_transient_layer_content_list_insert_inc(tlcl, lc, fill);
                DP_transient_layer_props_list_insert_inc(tlpl, lp, fill);
                ++fill;
            }
            else {
                DP_warn("Layer reorder: unknown layer id %d", layer_id);
            }
        }
        else {
            DP_warn("Layer reorder: duplicate layer id %d", layer_id);
        }
    }

    // If further layers remain, just move them over in the order they appear.
    for (int i = 0; fill < layer_count && i < layer_count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int layer_id = DP_layer_props_id(lp);
        if (DP_transient_layer_props_list_index_by_id(tlpl, layer_id) == -1) {
            DP_LayerContent *lc = DP_layer_content_list_at_noinc(lcl, i);
            DP_transient_layer_content_list_insert_inc(tlcl, lc, fill);
            DP_transient_layer_props_list_insert_inc(tlpl, lp, fill);
            ++fill;
        }
    }

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_layer_retitle(DP_CanvasState *cs, int layer_id,
                                     const char *title, size_t title_length)
{
    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Layer retitle: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 0);
    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_list_transient_at_noinc(tlpl, index);

    DP_transient_layer_props_title_set(tlp, title, title_length);

    return DP_transient_canvas_state_persist(tcs);
}


static void merge_down_at(DP_TransientLayerContentList *tlcl,
                          DP_TransientLayerPropsList *tlpl,
                          unsigned int context_id, int index)
{
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_list_transient_at_noinc(tlcl, index - 1);
    DP_LayerContent *lc = DP_transient_layer_content_list_at_noinc(tlcl, index);
    DP_LayerProps *lp = DP_transient_layer_props_list_at_noinc(tlpl, index);
    DP_transient_layer_content_merge(tlc, context_id, lc,
                                     DP_layer_props_opacity(lp),
                                     DP_layer_props_blend_mode(lp));
}

DP_CanvasState *DP_ops_layer_delete(DP_CanvasState *cs, unsigned int context_id,
                                    int layer_id, bool merge)
{
    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Layer delete: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContentList *tlcl =
        DP_transient_canvas_state_transient_layer_contents(tcs, 0);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 0);

    if (merge) {
        if (index > 0) {
            merge_down_at(tlcl, tlpl, context_id, index);
        }
        else {
            DP_warn("Attempt to merge down bottom layer ignored");
        }
    }

    DP_transient_layer_content_list_delete_at(tlcl, index);
    DP_transient_layer_props_list_delete_at(tlpl, index);

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_layer_visibility(DP_CanvasState *cs, int layer_id,
                                        bool visible)
{
    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Layer visibility: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerPropsList *tlpl =
        DP_transient_canvas_state_transient_layer_props(tcs, 0);
    DP_TransientLayerProps *tlp =
        DP_transient_layer_props_list_transient_at_noinc(tlpl, index);

    DP_transient_layer_props_hidden_set(tlp, !visible);

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_put_image(DP_CanvasState *cs, unsigned int context_id,
                                 int layer_id, int blend_mode, int x, int y,
                                 int width, int height,
                                 const unsigned char *image, size_t image_size)
{
    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Put image: id %d not found", layer_id);
        return NULL;
    }

    DP_Image *img =
        DP_image_new_from_compressed(width, height, image, image_size);
    if (!img) {
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContentList *tlcl =
        DP_transient_canvas_state_transient_layer_contents(tcs, 0);
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_list_transient_at_noinc(tlcl, index);

    DP_transient_layer_content_put_image(tlc, context_id, blend_mode, x, y,
                                         img);
    DP_image_free(img);

    return DP_transient_canvas_state_persist(tcs);
}


static DP_Image *select_pixels(DP_CanvasState *cs, int index,
                               const DP_Rect *src_rect, DP_Image *mask)
{
    DP_LayerContentList *lcl = DP_canvas_state_layer_contents_noinc(cs);
    DP_LayerContent *lc = DP_layer_content_list_at_noinc(lcl, index);
    return DP_layer_content_select(lc, src_rect, mask);
}

static bool looks_like_translation_only(DP_Rect src_rect, DP_Quad dst_quad)
{
    DP_Rect dst_bounds = DP_quad_bounds(dst_quad);
    return DP_rect_width(src_rect) == DP_rect_width(dst_bounds)
        && DP_rect_height(src_rect) == DP_rect_width(dst_bounds)
        && dst_quad.x1 < dst_quad.x2;
}

DP_CanvasState *DP_ops_region_move(DP_CanvasState *cs, DP_DrawContext *dc,
                                   unsigned int context_id, int layer_id,
                                   const DP_Rect *src_rect,
                                   const DP_Quad *dst_quad, DP_Image *mask)
{
    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Region move: id %d not found", layer_id);
        return NULL;
    }

    DP_Image *src_img = select_pixels(cs, index, src_rect, mask);

    int offset_x, offset_y;
    DP_Image *dst_img;
    if (looks_like_translation_only(*src_rect, *dst_quad)) {
        offset_x = dst_quad->x1;
        offset_y = dst_quad->y2;
        dst_img = src_img;
    }
    else {
        dst_img =
            DP_image_transform(src_img, dc, dst_quad, &offset_x, &offset_y);
        if (!dst_img) {
            DP_free(src_img);
            return NULL;
        }
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContentList *tlcl =
        DP_transient_canvas_state_transient_layer_contents(tcs, 0);
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_list_transient_at_noinc(tlcl, index);

    if (mask) {
        DP_transient_layer_content_put_image(
            tlc, context_id, DP_BLEND_MODE_ERASE, DP_rect_x(*src_rect),
            DP_rect_y(*src_rect), mask);
    }
    else {
        DP_transient_layer_content_fill_rect(
            tlc, context_id, DP_BLEND_MODE_REPLACE, src_rect->x1, src_rect->y1,
            src_rect->x2 + 1, src_rect->y2 + 1, 0u);
    }

    DP_transient_layer_content_put_image(tlc, context_id, DP_BLEND_MODE_NORMAL,
                                         offset_x, offset_y, dst_img);

    if (dst_img != src_img) {
        DP_image_free(dst_img);
    }
    DP_image_free(src_img);

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_fill_rect(DP_CanvasState *cs, unsigned int context_id,
                                 int layer_id, int blend_mode, int left,
                                 int top, int right, int bottom, uint32_t color)
{
    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Fill rect: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContentList *tlcl =
        DP_transient_canvas_state_transient_layer_contents(tcs, 0);
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_list_transient_at_noinc(tlcl, index);

    DP_transient_layer_content_fill_rect(tlc, context_id, blend_mode, left, top,
                                         right, bottom, color);

    return DP_transient_canvas_state_persist(tcs);
}


static DP_TransientLayerContent *get_layer_content(DP_TransientCanvasState *tcs,
                                                   int index, int sublayer_id)
{
    if (sublayer_id > 0) {
        DP_TransientLayerContentList *tlcl =
            DP_transient_canvas_state_transient_layer_contents(tcs, 0);
        DP_TransientLayerContent *tlc =
            DP_transient_layer_content_list_transient_at_noinc(tlcl, index);
        DP_TransientLayerContent *sub_tlc;
        DP_transient_layer_content_list_transient_sublayer(tlc, sublayer_id,
                                                           &sub_tlc, NULL);
        return sub_tlc;
    }
    else {
        DP_TransientLayerContentList *tlcl =
            DP_transient_canvas_state_transient_layer_contents(tcs, 0);
        return DP_transient_layer_content_list_transient_at_noinc(tlcl, index);
    }
}

DP_CanvasState *DP_ops_put_tile(DP_CanvasState *cs, DP_Tile *tile, int layer_id,
                                int sublayer_id, int x, int y, int repeat)
{
    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Put tile: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContent *tlc = get_layer_content(tcs, index, sublayer_id);

    DP_transient_layer_content_put_tile(tlc, tile, x, y, repeat);

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_pen_up(DP_CanvasState *cs, unsigned int context_id)
{
    // We only need to do any work here if the user was drawing in indirect mode
    // and now there's sublayers with their id that need to be merged. So we
    // don't do any transient business right away, but instead put it off until
    // we actually find something to do. This makes the algorithm complicated,
    // but it avoids doing a bunch of pointless allocations in direct draw mode.
    DP_TransientCanvasState *tcs = NULL;
    int sublayer_id = DP_uint_to_int(context_id);
    DP_LayerContentList *lcl = DP_canvas_state_layer_contents_noinc(cs);

    int layer_count = DP_layer_content_list_count(lcl);
    for (int i = 0; i < layer_count; ++i) {
        DP_LayerContent *lc = DP_layer_content_list_at_noinc(lcl, i);
        DP_LayerPropsList *sub_lpl = DP_layer_content_sub_props_noinc(lc);
        DP_TransientLayerContent *tlc = NULL;

        int sublayer_count = DP_layer_props_list_count(sub_lpl);
        for (int j = 0; j < sublayer_count; ++j) {
            DP_LayerProps *sub_lp = DP_layer_props_list_at_noinc(sub_lpl, j);
            if (DP_layer_props_id(sub_lp) == sublayer_id) {
                // We found something to merge.
                if (tlc) {
                    // Everything is already transient, just merge the sublayer.
                    DP_transient_layer_content_merge_sublayer_at(tlc,
                                                                 context_id, j);
                }
                else {
                    // Something isn't transient yet, so we take care of that.
                    DP_TransientLayerContentList *tlcl;
                    if (tcs) {
                        // Canvas state and layer list are already transient.
                        tlcl = (DP_TransientLayerContentList *)lcl;
                    }
                    else {
                        // Nothing is transient yet.
                        tcs = DP_transient_canvas_state_new(cs);
                        tlcl =
                            DP_transient_canvas_state_transient_layer_contents(
                                tcs, 0);
                        lcl = (DP_LayerContentList *)tlcl;
                    }
                    // Make the layer transient so we can merge.
                    tlc = DP_transient_layer_content_list_transient_at_noinc(
                        tlcl, i);
                    DP_transient_layer_content_merge_sublayer_at(tlc,
                                                                 context_id, j);
                    // Use the transient layer's sublayers from now on.
                    sub_lpl = DP_transient_layer_content_sub_props_noinc(tlc);
                }
                // The merged sublayer is gone, so take that into account.
                --sublayer_count;
                --j;
            }
        }
    }

    if (tcs) {
        return DP_transient_canvas_state_persist(tcs);
    }
    else {
        return DP_canvas_state_incref(cs);
    }
}


DP_CanvasState *DP_ops_annotation_create(DP_CanvasState *cs, int annotation_id,
                                         int x, int y, int width, int height)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    if (DP_annotation_list_index_by_id(al, annotation_id) != -1) {
        DP_error_set("Annotation create: id %d already exists", annotation_id);
        return NULL;
    }

    int index = DP_annotation_list_count(al);
    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, 1);
    DP_Annotation *a = DP_annotation_new(annotation_id, x, y, width, height);
    DP_transient_annotation_list_insert_noinc(tal, a, index);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_annotation_reshape(DP_CanvasState *cs, int annotation_id,
                                          int x, int y, int width, int height)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    int index = DP_annotation_list_index_by_id(al, annotation_id);
    if (index < 0) {
        DP_error_set("Annotation reshape: id %d not found", annotation_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, 0);
    DP_TransientAnnotation *ta =
        DP_transient_annotation_list_transient_at_noinc(tal, index);

    DP_transient_annotation_x_set(ta, x);
    DP_transient_annotation_y_set(ta, y);
    DP_transient_annotation_width_set(ta, width);
    DP_transient_annotation_height_set(ta, height);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_annotation_edit(DP_CanvasState *cs, int annotation_id,
                                       uint32_t background_color, bool protect,
                                       int valign, const char *text,
                                       size_t text_length)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    int index = DP_annotation_list_index_by_id(al, annotation_id);
    if (index < 0) {
        DP_error_set("Annotation edit: id %d not found", annotation_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, 0);
    DP_TransientAnnotation *ta =
        DP_transient_annotation_list_transient_at_noinc(tal, index);

    DP_transient_annotation_background_color_set(ta, background_color);
    DP_transient_annotation_protect_set(ta, protect);
    DP_transient_annotation_valign_set(ta, valign);
    DP_transient_annotation_text_set(ta, text, text_length);

    return DP_transient_canvas_state_persist(tcs);
}

DP_CanvasState *DP_ops_annotation_delete(DP_CanvasState *cs, int annotation_id)
{
    DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
    int index = DP_annotation_list_index_by_id(al, annotation_id);
    if (index < 0) {
        DP_error_set("Annotation delete: id %d not found", annotation_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, 0);
    DP_transient_annotation_list_delete_at(tal, index);

    return DP_transient_canvas_state_persist(tcs);
}


DP_CanvasState *DP_ops_draw_dabs(DP_CanvasState *cs, int layer_id,
                                 int sublayer_id, int sublayer_blend_mode,
                                 int sublayer_opacity,
                                 DP_PaintDrawDabsParams *params)
{
    DP_ASSERT(params);
    DP_ASSERT(sublayer_id >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_blend_mode >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_blend_mode < DP_BLEND_MODE_COUNT);
    DP_ASSERT(sublayer_id == 0 || sublayer_opacity >= 0);
    DP_ASSERT(sublayer_id == 0 || sublayer_opacity <= UINT8_MAX);

    int index = DP_layer_props_list_index_by_id(
        DP_canvas_state_layer_props_noinc(cs), layer_id);
    if (index < 0) {
        DP_error_set("Draw dabs: id %d not found", layer_id);
        return NULL;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_TransientLayerContentList *tlcl =
        DP_transient_canvas_state_transient_layer_contents(tcs, 0);
    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_list_transient_at_noinc(tlcl, index);

    DP_TransientLayerContent *target;
    if (sublayer_id == 0) {
        target = tlc;
    }
    else {
        DP_LayerPropsList *lpl =
            DP_transient_layer_content_sub_props_noinc(tlc);
        int sublayer_index = DP_layer_props_list_index_by_id(lpl, sublayer_id);
        if (sublayer_index < 0) {
            DP_TransientLayerProps *tlp;
            DP_transient_layer_content_list_transient_sublayer(tlc, sublayer_id,
                                                               &target, &tlp);
            // Only set these once, when the sublayer is created. They should
            // always be the same values for a single sublayer anyway.
            DP_transient_layer_props_blend_mode_set(tlp, sublayer_blend_mode);
            DP_transient_layer_props_opacity_set(
                tlp, DP_int_to_uint8(sublayer_opacity));
        }
        else {
            DP_transient_layer_content_list_transient_sublayer_at(
                tlc, sublayer_index, &target, NULL);
        }
    }

    DP_paint_draw_dabs(params, target);

    return DP_transient_canvas_state_persist(tcs);
}
