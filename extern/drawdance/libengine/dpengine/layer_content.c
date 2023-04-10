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
#include "layer_content.h"
#include "canvas_diff.h"
#include "draw_context.h"
#include "image.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "paint.h"
#include "tile.h"
#include "tile_iterator.h"
#include "view_mode.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpmsg/blend_mode.h>
#include <helpers.h> // CLAMP


#ifdef DP_NO_STRICT_ALIASING

struct DP_LayerContent {
    DP_Atomic refcount;
    const bool transient;
    const int width, height;
    struct {
        DP_LayerList *contents;
        DP_LayerPropsList *props;
    } sub;
    union {
        DP_Tile *const tile;
    } elements[];
};

struct DP_TransientLayerContent {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    struct {
        union {
            DP_LayerList *contents;
            DP_TransientLayerList *transient_contents;
        };
        union {
            DP_LayerPropsList *props;
            DP_TransientLayerPropsList *transient_props;
        };
    } sub;
    union {
        DP_Tile *tile;
        DP_TransientTile *transient_tile;
    } elements[];
};

#else

struct DP_LayerContent {
    DP_Atomic refcount;
    bool transient;
    int width, height;
    struct {
        union {
            DP_LayerList *contents;
            DP_TransientLayerList *transient_contents;
        };
        union {
            DP_LayerPropsList *props;
            DP_TransientLayerPropsList *transient_props;
        };
    } sub;
    union {
        DP_Tile *tile;
        DP_TransientTile *transient_tile;
    } elements[];
};

#endif


DP_LayerContent *DP_layer_content_incref(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_atomic_inc(&lc->refcount);
    return lc;
}

DP_LayerContent *DP_layer_content_incref_nullable(DP_LayerContent *lc_or_null)
{
    return lc_or_null ? DP_layer_content_incref(lc_or_null) : NULL;
}

void DP_layer_content_decref(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    if (DP_atomic_dec(&lc->refcount)) {
        int count = DP_tile_total_round(lc->width, lc->height);
        for (int i = 0; i < count; ++i) {
            DP_tile_decref_nullable(lc->elements[i].tile);
        }
        DP_layer_props_list_decref(lc->sub.props);
        DP_layer_list_decref(lc->sub.contents);
        DP_free(lc);
    }
}

void DP_layer_content_decref_nullable(DP_LayerContent *lc_or_null)
{
    if (lc_or_null) {
        DP_layer_content_decref(lc_or_null);
    }
}

int DP_layer_content_refcount(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    return DP_atomic_get(&lc->refcount);
}

bool DP_layer_content_transient(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    return lc->transient;
}


static bool mark_both(void *data, int tile_index)
{
    DP_ASSERT(data);
    DP_ASSERT(tile_index >= 0);
    DP_LayerContent *a = ((DP_LayerContent **)data)[0];
    DP_LayerContent *b = ((DP_LayerContent **)data)[1];
    DP_ASSERT(tile_index < DP_tile_total_round(a->width, a->height));
    DP_ASSERT(tile_index < DP_tile_total_round(b->width, b->height));
    return a->elements[tile_index].tile || b->elements[tile_index].tile;
}

static void layer_content_diff_mark_both(DP_LayerContent *lc,
                                         DP_LayerContent *prev_lc,
                                         DP_CanvasDiff *diff)
{
    DP_ASSERT(lc);
    DP_ASSERT(prev_lc);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(DP_atomic_get(&prev_lc->refcount) > 0);
    DP_ASSERT(lc->width == prev_lc->width);   // Different sizes could be
    DP_ASSERT(lc->height == prev_lc->height); // supported, but aren't yet.
    DP_canvas_diff_check(diff, mark_both, (DP_LayerContent *[]){lc, prev_lc});
    DP_layer_list_diff_mark(lc->sub.contents, diff);
    DP_layer_list_diff_mark(prev_lc->sub.contents, diff);
}

static bool diff_tile(void *data, int tile_index)
{
    DP_ASSERT(data);
    DP_ASSERT(tile_index >= 0);
    DP_LayerContent *a = ((DP_LayerContent **)data)[0];
    DP_LayerContent *b = ((DP_LayerContent **)data)[1];
    DP_ASSERT(tile_index < DP_tile_total_round(a->width, a->height));
    DP_ASSERT(tile_index < DP_tile_total_round(b->width, b->height));
    return a->elements[tile_index].tile != b->elements[tile_index].tile;
}

static bool diff_tile_both_censored(void *data, int tile_index)
{
    DP_ASSERT(data);
    DP_ASSERT(tile_index >= 0);
    DP_LayerContent *a = ((DP_LayerContent **)data)[0];
    DP_LayerContent *b = ((DP_LayerContent **)data)[1];
    DP_ASSERT(tile_index < DP_tile_total_round(a->width, a->height));
    DP_ASSERT(tile_index < DP_tile_total_round(b->width, b->height));
    // When layers are censored, all non-blank tiles get turned into the
    // same censor tile, so we just have to compare their null-ness.
    return !a->elements[tile_index].tile != !b->elements[tile_index].tile;
}

static void layer_content_diff(DP_LayerContent *lc, bool censored,
                               DP_LayerContent *prev_lc, bool prev_censored,
                               DP_CanvasDiff *diff)
{
    DP_ASSERT(lc);
    DP_ASSERT(prev_lc);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(DP_atomic_get(&prev_lc->refcount) > 0);
    DP_ASSERT(lc->width == prev_lc->width);   // Different sizes could be
    DP_ASSERT(lc->height == prev_lc->height); // supported, but aren't yet.
    if (!censored && !prev_censored) {
        DP_canvas_diff_check(diff, diff_tile,
                             (DP_LayerContent *[]){lc, prev_lc});
        DP_layer_list_diff(lc->sub.contents, lc->sub.props,
                           prev_lc->sub.contents, prev_lc->sub.props, diff);
    }
    else if (censored && prev_censored) {
        DP_canvas_diff_check(diff, diff_tile_both_censored,
                             (DP_LayerContent *[]){lc, prev_lc});
        DP_layer_list_diff(lc->sub.contents, lc->sub.props,
                           prev_lc->sub.contents, prev_lc->sub.props, diff);
    }
    else {
        layer_content_diff_mark_both(lc, prev_lc, diff);
    }
}

void DP_layer_content_diff(DP_LayerContent *lc, DP_LayerProps *lp,
                           DP_LayerContent *prev_lc, DP_LayerProps *prev_lp,
                           DP_CanvasDiff *diff)
{
    DP_ASSERT(lc);
    DP_ASSERT(lp);
    DP_ASSERT(prev_lc);
    DP_ASSERT(prev_lp);
    DP_ASSERT(diff);
    bool visible = DP_layer_props_visible(lp);
    bool prev_visible = DP_layer_props_visible(prev_lp);
    if (visible) {
        if (prev_visible) {
            if (DP_layer_props_differ(lp, prev_lp)) {
                layer_content_diff_mark_both(lc, prev_lc, diff);
            }
            else {
                layer_content_diff(lc, DP_layer_props_censored(lp), prev_lc,
                                   DP_layer_props_censored(prev_lp), diff);
            }
        }
        else {
            DP_layer_content_diff_mark(lc, diff);
        }
    }
    else if (prev_visible) {
        DP_layer_content_diff_mark(prev_lc, diff);
    }
}

static bool mark(void *data, int tile_index)
{
    DP_ASSERT(data);
    DP_ASSERT(tile_index >= 0);
    DP_LayerContent *lc = data;
    DP_ASSERT(tile_index < DP_tile_total_round(lc->width, lc->height));
    return lc->elements[tile_index].tile;
}

static void layer_content_diff_mark(DP_LayerContent *lc, DP_CanvasDiff *diff)
{
    DP_ASSERT(lc);
    DP_ASSERT(diff);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_canvas_diff_check(diff, mark, lc);
}

void DP_layer_content_diff_mark(DP_LayerContent *lc, DP_CanvasDiff *diff)
{
    DP_ASSERT(lc);
    DP_ASSERT(diff);
    layer_content_diff_mark(lc, diff);
    DP_layer_list_diff_mark(lc->sub.contents, diff);
}


int DP_layer_content_width(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    return lc->width;
}

int DP_layer_content_height(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    return lc->height;
}

DP_Tile *DP_layer_content_tile_at_noinc(DP_LayerContent *lc, int x, int y)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < DP_tile_count_round(lc->width));
    DP_ASSERT(y < DP_tile_count_round(lc->height));
    return lc->elements[y * DP_tile_count_round(lc->width) + x].tile;
}

DP_Pixel15 DP_layer_content_pixel_at(DP_LayerContent *lc, int x, int y)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < lc->width);
    DP_ASSERT(y < lc->height);
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int wt = DP_tile_count_round(lc->width);
    DP_Tile *t = lc->elements[yt * wt + xt].tile;
    if (t) {
        return DP_tile_pixel_at(t, x - xt * DP_TILE_SIZE,
                                y - yt * DP_TILE_SIZE);
    }
    else {
        return DP_pixel15_zero();
    }
}

static DP_UPixel15 sample_dab_color(DP_LayerContent *lc, DP_BrushStamp stamp)
{
    uint16_t *weights = stamp.data;
    int diameter = stamp.diameter;
    int right = DP_min_int(stamp.left + diameter, lc->width);
    int bottom = DP_min_int(stamp.top + diameter, lc->height);

    int y = DP_max_int(0, stamp.top);
    int yb = stamp.top < 0 ? -stamp.top : 0; // y in relation to brush origin
    int x0 = DP_max_int(0, stamp.left);
    int xb0 = stamp.left < 0 ? -stamp.left : 0;
    int xtiles = DP_tile_count_round(lc->width);

    float weight = 0.0;
    float red = 0.0;
    float green = 0.0;
    float blue = 0.0;
    float alpha = 0.0;

    // collect weighted color sums
    while (y < bottom) {
        int yindex = y / DP_TILE_SIZE;
        int yt = y - yindex * DP_TILE_SIZE;
        int hb = yt + diameter - yb < DP_TILE_SIZE ? diameter - yb
                                                   : DP_TILE_SIZE - yt;
        int x = x0;
        int xb = xb0; // x in relation to brush origin
        while (x < right) {
            const int xindex = x / DP_TILE_SIZE;
            const int xt = x - xindex * DP_TILE_SIZE;
            const int wb = xt + diameter - xb < DP_TILE_SIZE
                             ? diameter - xb
                             : DP_TILE_SIZE - xt;
            const int i = xtiles * yindex + xindex;

            DP_tile_sample(lc->elements[i].tile, weights + yb * diameter + xb,
                           xt, yt, wb, hb, diameter - wb, &weight, &red, &green,
                           &blue, &alpha);

            x = (xindex + 1) * DP_TILE_SIZE;
            xb = xb + wb;
        }
        y = (yindex + 1) * DP_TILE_SIZE;
        yb = yb + hb;
    }

    // There must be at least some alpha for the results to make sense
    float required_alpha =
        DP_int_to_float(DP_square_int(diameter) * 30) / (float)DP_BIT15;
    if (alpha < required_alpha) {
        return DP_upixel15_zero();
    }

    // Calculate final average
    red /= weight;
    green /= weight;
    blue /= weight;
    alpha /= weight;

    // Unpremultiply, clamp against rounding error.
    red = CLAMP(red / alpha, 0.0f, 1.0f);
    green = CLAMP(green / alpha, 0.0f, 1.0f);
    blue = CLAMP(blue / alpha, 0.0f, 1.0f);

    return (DP_UPixel15){
        .b = DP_channel_float_to_15(blue),
        .g = DP_channel_float_to_15(green),
        .r = DP_channel_float_to_15(red),
        .a = DP_channel_float_to_15(alpha),
    };
}

DP_UPixel15 DP_layer_content_sample_color_at(DP_LayerContent *lc,
                                             uint16_t *stamp_buffer, int x,
                                             int y, int diameter,
                                             int *in_out_last_diameter)
{
    if (x >= 0 && y >= 0 && x < lc->width && y < lc->height) {
        if (diameter < 2) {
            DP_Pixel15 pixel = DP_layer_content_pixel_at(lc, x, y);
            return DP_pixel15_unpremultiply(pixel);
        }
        else {
            int last_diameter;
            if (in_out_last_diameter) {
                last_diameter = *in_out_last_diameter;
                *in_out_last_diameter = diameter;
            }
            else {
                last_diameter = -1;
            }
            return sample_dab_color(
                lc, DP_paint_color_sampling_stamp_make(stamp_buffer, diameter,
                                                       x, y, last_diameter));
        }
    }
    else {
        return DP_upixel15_zero();
    }
}

DP_LayerList *DP_layer_content_sub_contents_noinc(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    return lc->sub.contents;
}

DP_LayerPropsList *DP_layer_content_sub_props_noinc(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    return lc->sub.props;
}

static bool layer_content_tile_bounds(DP_LayerContent *lc, int *out_left,
                                      int *out_top, int *out_right,
                                      int *out_bottom)
{
    DP_TileCounts tile_counts = DP_tile_counts_round(lc->width, lc->height);
    int left = tile_counts.x;
    int top = tile_counts.y;
    int right = 0;
    int bottom = 0;

    // TODO: Crop pixel-perfect instead of only to the nearest tile.
    for (int y = 0; y < tile_counts.y; ++y) {
        for (int x = 0; x < tile_counts.x; ++x) {
            DP_Tile *t = DP_layer_content_tile_at_noinc(lc, x, y);
            if (t && !DP_tile_blank(t)) {
                if (x < left) {
                    left = x;
                }
                if (x > right) {
                    right = x;
                }
                if (y < top) {
                    top = y;
                }
                if (y > bottom) {
                    bottom = y;
                }
            }
        }
    }

    if (top != tile_counts.y) {
        *out_left = left;
        *out_top = top;
        *out_right = right;
        *out_bottom = bottom;
        return true;
    }
    else {
        return false; // Whole layer seems to be blank.
    }
}

static bool get_sublayer_change_bounds(DP_LayerContent *lc, int i, int *out_x,
                                       int *out_y, int *out_width,
                                       int *out_height)
{
    DP_LayerList *ll = DP_layer_content_sub_contents_noinc(lc);
    DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
    DP_ASSERT(!DP_layer_list_entry_is_group(lle));
    DP_LayerContent *slc = DP_layer_list_entry_content_noinc(lle);
    int left, top, right, bottom;
    if (layer_content_tile_bounds(slc, &left, &top, &right, &bottom)) {
        if (out_x) {
            *out_x = left * DP_TILE_SIZE;
        }
        if (out_y) {
            *out_y = top * DP_TILE_SIZE;
        }
        if (out_width) {
            *out_width = (right - left + 1) * DP_TILE_SIZE;
        }
        if (out_height) {
            *out_height = (bottom - top + 1) * DP_TILE_SIZE;
        }
        return true;
    }
    else {
        return false;
    }
}

bool DP_layer_content_search_change_bounds(DP_LayerContent *lc,
                                           unsigned int context_id, int *out_x,
                                           int *out_y, int *out_width,
                                           int *out_height)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_LayerPropsList *lpl = DP_layer_content_sub_props_noinc(lc);
    int count = DP_layer_props_list_count(lpl);
    for (int i = 0; i < count; ++i) {
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        int sublayer_id = DP_layer_props_id(lp);
        if (sublayer_id > 0 && DP_int_to_uint(sublayer_id) == context_id) {
            return get_sublayer_change_bounds(lc, i, out_x, out_y, out_width,
                                              out_height);
        }
    }
    return false;
}

DP_Image *DP_layer_content_to_image(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    int width = lc->width;
    int height = lc->height;
    if (width <= 0 || height <= 0) {
        DP_error_set("Can't create image from a layer with zero pixels");
        return NULL;
    }

    DP_Image *img = DP_image_new(width, height);
    DP_TileCounts tile_counts = DP_tile_counts_round(width, height);
    DP_debug("Layer to image %dx%d tiles", tile_counts.x, tile_counts.y);
    for (int y = 0; y < tile_counts.y; ++y) {
        for (int x = 0; x < tile_counts.x; ++x) {
            DP_tile_copy_to_image(lc->elements[y * tile_counts.x + x].tile, img,
                                  x * DP_TILE_SIZE, y * DP_TILE_SIZE);
        }
    }
    return img;
}

DP_Image *DP_layer_content_to_image_cropped(DP_LayerContent *lc,
                                            int *out_offset_x,
                                            int *out_offset_y)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);

    int left, top, right, bottom;
    if (!layer_content_tile_bounds(lc, &left, &top, &right, &bottom)) {
        return NULL; // Whole layer seems to be blank.
    }

    int width = (right - left + 1) * DP_TILE_SIZE;
    int height = (bottom - top + 1) * DP_TILE_SIZE;
    DP_Image *img = DP_image_new(width, height);
    for (int y = top; y <= bottom; ++y) {
        int target_y = (y - top) * DP_TILE_SIZE;
        for (int x = left; x <= right; ++x) {
            DP_Tile *t = DP_layer_content_tile_at_noinc(lc, x, y);
            int target_x = (x - left) * DP_TILE_SIZE;
            DP_tile_copy_to_image(t, img, target_x, target_y);
        }
    }

    if (out_offset_x) {
        *out_offset_x = left * DP_TILE_SIZE;
    }
    if (out_offset_y) {
        *out_offset_y = top * DP_TILE_SIZE;
    }
    return img;
}

DP_Pixel8 *DP_layer_content_to_pixels8(DP_LayerContent *lc, int x, int y,
                                       int width, int height)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);

    DP_TileIterator ti = DP_tile_iterator_make(
        lc->width, lc->height, DP_rect_make(x, y, width, height));
    DP_Pixel8 *pixels = DP_malloc_zeroed(sizeof(*pixels) * DP_int_to_size(width)
                                         * DP_int_to_size(height));

    while (DP_tile_iterator_next(&ti)) {
        DP_Tile *t = DP_layer_content_tile_at_noinc(lc, ti.col, ti.row);
        if (t) {
            DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
            while (DP_tile_into_dst_iterator_next(&tidi)) {
                pixels[tidi.dst_y * width + tidi.dst_x] = DP_pixel15_to_8(
                    DP_tile_pixel_at(t, tidi.tile_x, tidi.tile_y));
            }
        }
    }

    return pixels;
}

static uint8_t mask_opacity_at(DP_Image *mask, int dst_x, int dst_y)
{
    return mask ? DP_image_pixel_at(mask, dst_x, dst_y).a : 255;
}

DP_Image *DP_layer_content_select(DP_LayerContent *lc, const DP_Rect *rect,
                                  DP_Image *mask)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(rect);

    DP_TileIterator ti = DP_tile_iterator_make(lc->width, lc->height, *rect);
    DP_Image *img = DP_image_new(DP_rect_width(ti.dst), DP_rect_height(ti.dst));

    while (DP_tile_iterator_next(&ti)) {
        DP_Tile *t = DP_layer_content_tile_at_noinc(lc, ti.col, ti.row);
        if (t) {
            DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
            while (DP_tile_into_dst_iterator_next(&tidi)) {
                uint8_t opacity = mask_opacity_at(mask, tidi.dst_x, tidi.dst_y);
                if (opacity != 0) {
                    DP_Pixel15 pixel =
                        DP_tile_pixel_at(t, tidi.tile_x, tidi.tile_y);
                    if (opacity != 255) {
                        DP_UPixel15 upixel = DP_pixel15_unpremultiply(pixel);
                        upixel.a =
                            DP_fix15_mul(upixel.a, DP_channel8_to_15(opacity));
                        pixel = DP_pixel15_premultiply(upixel);
                    }
                    DP_image_pixel_at_set(img, tidi.dst_x, tidi.dst_y,
                                          DP_pixel15_to_8(pixel));
                }
            }
        }
    }

    return img;
}

static DP_Tile *flatten_tile(DP_LayerContent *lc, int tile_index,
                             bool include_sublayers)
{
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(lc->width, lc->height));
    DP_Tile *t = lc->elements[tile_index].tile;
    DP_LayerList *ll = lc->sub.contents;
    if (!include_sublayers || DP_layer_list_count(ll) == 0) {
        return DP_tile_incref_nullable(t);
    }
    else if (t) {
        DP_TransientTile *tt = DP_transient_tile_new(t, 0);
        DP_ViewModeContext vmc = DP_view_mode_context_make_default();
        DP_layer_list_flatten_tile_to(ll, lc->sub.props, tile_index, tt,
                                      DP_BIT15, false, &vmc);
        return DP_transient_tile_persist(tt);
    }
    else {
        DP_ViewModeContext vmc = DP_view_mode_context_make_default();
        DP_TransientTile *tt_or_null = DP_layer_list_flatten_tile_to(
            ll, lc->sub.props, tile_index, NULL, DP_BIT15, false, &vmc);
        return tt_or_null ? DP_transient_tile_persist(tt_or_null) : NULL;
    }
}

static DP_Tile *flatten_censored_tile(DP_LayerContent *lc, int tile_index,
                                      bool include_sublayers)
{
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(lc->width, lc->height));
    DP_Tile *t = lc->elements[tile_index].tile;
    if (t) {
        return DP_tile_censored_inc();
    }
    else if (include_sublayers) {
        DP_LayerList *ll = lc->sub.contents;
        int sublayer_count = DP_layer_list_count(ll);
        for (int i = 0; i < sublayer_count; ++i) {
            DP_LayerContent *sub_lc = DP_layer_list_entry_content_noinc(
                DP_layer_list_at_noinc(ll, i));
            if (sub_lc->elements[tile_index].tile) {
                return DP_tile_censored_inc();
            }
        }
        return NULL;
    }
    else {
        return NULL;
    }
}

DP_TransientTile *DP_layer_content_flatten_tile_to(
    DP_LayerContent *lc, int tile_index, DP_TransientTile *tt_or_null,
    uint16_t opacity, int blend_mode, bool censored, bool include_sublayers)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_Tile *t = censored
                   ? flatten_censored_tile(lc, tile_index, include_sublayers)
                   : flatten_tile(lc, tile_index, include_sublayers);
    if (t) {
        DP_TransientTile *tt = DP_transient_tile_merge_nullable(
            tt_or_null, t, opacity, blend_mode);
        DP_tile_decref(t);
        return tt;
    }
    else {
        return tt_or_null;
    }
}


static bool has_content(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    int count = DP_tile_total_round(lc->width, lc->height);
    for (int i = 0; i < count; ++i) {
        DP_Tile *tile = lc->elements[i].tile;
        if (tile && !DP_tile_blank(tile)) {
            return true;
        }
    }
    return false;
}

static DP_TransientLayerContent *alloc_layer_content(int width, int height)
{
    size_t count = DP_int_to_size(DP_tile_total_round(width, height));
    DP_TransientLayerContent *tlc =
        DP_malloc(DP_FLEX_SIZEOF(DP_TransientLayerContent, elements, count));
    DP_atomic_set(&tlc->refcount, 1);
    tlc->transient = true;
    tlc->width = width;
    tlc->height = height;
    return tlc;
}

static DP_TransientTile *get_transient_tile(DP_TransientLayerContent *tlc,
                                            unsigned int context_id, int i)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < DP_tile_total_round(tlc->width, tlc->height));
    DP_Tile *t = tlc->elements[i].tile;
    DP_ASSERT(t);
    if (DP_tile_transient(t)) {
        return (DP_TransientTile *)t;
    }
    else {
        DP_TransientTile *tt = DP_transient_tile_new(t, context_id);
        tlc->elements[i].transient_tile = tt;
        DP_tile_decref(t);
        return tt;
    }
}

static DP_TransientTile *
get_or_create_transient_tile(DP_TransientLayerContent *tlc,
                             unsigned int context_id, int i)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < DP_tile_total_round(tlc->width, tlc->height));
    DP_Tile *tile = tlc->elements[i].tile;
    if (!tile) {
        tlc->elements[i].transient_tile =
            DP_transient_tile_new_blank(context_id);
    }
    else if (!DP_tile_transient(tile)) {
        tlc->elements[i].transient_tile =
            DP_transient_tile_new(tile, context_id);
        DP_tile_decref(tile);
    }
    return tlc->elements[i].transient_tile;
}

void DP_transient_layer_content_pixel_at_put(DP_TransientLayerContent *tlc,
                                             unsigned int context_id,
                                             int blend_mode, int x, int y,
                                             DP_Pixel15 pixel)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < tlc->width);
    DP_ASSERT(y < tlc->height);
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int wt = DP_tile_count_round(tlc->width);
    DP_TransientTile *tt =
        get_or_create_transient_tile(tlc, context_id, yt * wt + xt);
    DP_transient_tile_pixel_at_put(tt, blend_mode, x - xt * DP_TILE_SIZE,
                                   y - yt * DP_TILE_SIZE, pixel);
}

void DP_transient_layer_content_pixel_at_set(DP_TransientLayerContent *tlc,
                                             unsigned int context_id, int x,
                                             int y, DP_Pixel15 pixel)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(x >= 0);
    DP_ASSERT(y >= 0);
    DP_ASSERT(x < tlc->width);
    DP_ASSERT(y < tlc->height);
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int wt = DP_tile_count_round(tlc->width);
    DP_TransientTile *tt =
        get_or_create_transient_tile(tlc, context_id, yt * wt + xt);
    DP_transient_tile_pixel_at_set(tt, x - xt * DP_TILE_SIZE,
                                   y - yt * DP_TILE_SIZE, pixel);
}


static DP_TransientLayerContent *
resize_layer_content_aligned(DP_LayerContent *lc, int top, int left, int width,
                             int height)
{
    DP_TransientLayerContent *tlc = alloc_layer_content(width, height);
    DP_TileCounts new_counts = DP_tile_counts_round(width, height);
    DP_TileCounts old_counts = DP_tile_counts_round(lc->width, lc->height);
    DP_TileCounts offsets = DP_tile_counts_round(-left, -top);
    for (int y = 0; y < new_counts.y; ++y) {
        for (int x = 0; x < new_counts.x; ++x) {
            int old_x = offsets.x + x;
            int old_y = offsets.y + y;
            bool out_of_bounds = old_x < 0 || old_x >= old_counts.x || old_y < 0
                              || old_y >= old_counts.y;
            DP_Tile *tile = out_of_bounds
                              ? NULL
                              : lc->elements[old_y * old_counts.x + old_x].tile;
            tlc->elements[y * new_counts.x + x].tile =
                DP_tile_incref_nullable(tile);
        }
    }
    tlc->sub.contents = DP_layer_list_new();
    tlc->sub.props = DP_layer_props_list_new();
    return tlc;
}

static void calculate_resize_offsets(int offset, int *out_pos, int *out_crop)
{
    if (offset < 0) {
        *out_pos = 0;
        *out_crop = -offset;
    }
    else {
        *out_pos = offset;
        *out_crop = 0;
    }
}

static DP_TransientLayerContent *
resize_layer_content_copy(DP_LayerContent *lc, unsigned int context_id, int top,
                          int left, int width, int height)
{
    // TODO: This operation is super expensive and obliterates change
    // information. It could be solved by storing an x and y pixel offset in the
    // layer instead. Need more functionality to test that out though.
    DP_Image *img = DP_layer_content_to_image(lc);

    int x, y, crop_x, crop_y;
    calculate_resize_offsets(left, &x, &crop_x);
    calculate_resize_offsets(top, &y, &crop_y);
    if (crop_x != 0 || crop_y != 0) {
        int img_width = DP_image_width(img);
        int img_height = DP_image_height(img);
        if (crop_x < img_width && crop_y < img_height) {
            DP_Image *tmp = DP_image_new_subimage(
                img, crop_x, crop_y, img_width - crop_x, img_height - crop_y);
            DP_image_free(img);
            img = tmp;
        }
        else { // Nothing to do, resulting image is out of bounds.
            DP_image_free(img);
            return DP_transient_layer_content_new_init(width, height, NULL);
        }
    }

    DP_TransientLayerContent *tlc =
        DP_transient_layer_content_new_init(width, height, NULL);
    DP_transient_layer_content_put_image(tlc, context_id, DP_BLEND_MODE_REPLACE,
                                         x, y, img);
    DP_image_free(img);

    return tlc;
}

static DP_TransientLayerContent *resize_layer_content(DP_LayerContent *lc,
                                                      unsigned int context_id,
                                                      int top, int left,
                                                      int width, int height)
{
    if (left % DP_TILE_SIZE == 0 && top % DP_TILE_SIZE == 0) {
        DP_debug("Resize: layer is aligned");
        return resize_layer_content_aligned(lc, top, left, width, height);
    }
    else {
        DP_debug("Resize: layer must be copied");
        return resize_layer_content_copy(lc, context_id, top, left, width,
                                         height);
    }
}

DP_TransientLayerContent *DP_layer_content_resize(DP_LayerContent *lc,
                                                  unsigned int context_id,
                                                  int top, int right,
                                                  int bottom, int left)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);

    int width = lc->width + left + right;
    int height = lc->height + top + bottom;
    DP_TransientLayerContent *tlc;
    if (has_content(lc)) {
        tlc = resize_layer_content(lc, context_id, top, left, width, height);
    }
    else {
        DP_debug("Resize: layer is blank");
        tlc = DP_transient_layer_content_new_init(width, height, NULL);
    }

    DP_LayerList *sub_ll = lc->sub.contents;
    if (DP_layer_list_count(sub_ll) != 0) {
        tlc->sub.transient_contents =
            DP_layer_list_resize(sub_ll, context_id, top, right, bottom, left);
        DP_layer_list_decref(sub_ll);
    }

    return tlc;
}

DP_LayerContent *DP_layer_content_merge_sublayers(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_LayerList *ll = lc->sub.contents;
    int count = DP_layer_list_count(ll);
    if (count > 0) {
        DP_TransientLayerContent *tlc = DP_transient_layer_content_new(lc);
        DP_transient_layer_content_merge_all_sublayers(tlc, 0);
        return DP_transient_layer_content_persist(tlc);
    }
    else {
        return DP_layer_content_incref(lc);
    }
}


DP_TransientLayerContent *DP_transient_layer_content_new(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(!lc->transient);
    int width = lc->width;
    int height = lc->height;
    DP_TransientLayerContent *tlc = alloc_layer_content(width, height);
    int count = DP_tile_total_round(width, height);
    for (int i = 0; i < count; ++i) {
        tlc->elements[i].tile = DP_tile_incref_nullable(lc->elements[i].tile);
    }
    tlc->sub.contents = DP_layer_list_incref(lc->sub.contents);
    tlc->sub.props = DP_layer_props_list_incref(lc->sub.props);
    return tlc;
}

DP_TransientLayerContent *
DP_transient_layer_content_new_init(int width, int height, DP_Tile *tile)
{
    return DP_transient_layer_content_new_init_with_transient_sublayers_noinc(
        width, height, tile, DP_transient_layer_list_new_init(0),
        DP_transient_layer_props_list_new_init(0));
}

DP_TransientLayerContent *
DP_transient_layer_content_new_init_with_transient_sublayers_noinc(
    int width, int height, DP_Tile *tile, DP_TransientLayerList *sub_tll,
    DP_TransientLayerPropsList *sub_tlpl)
{
    DP_ASSERT(width >= 0);
    DP_ASSERT(height >= 0);
    DP_ASSERT(sub_tll);
    DP_ASSERT(sub_tlpl);
    DP_TransientLayerContent *tlc = alloc_layer_content(width, height);
    int tile_count = DP_tile_total_round(width, height);
    DP_tile_incref_by_nullable(tile, tile_count);
    for (int i = 0; i < tile_count; ++i) {
        tlc->elements[i].tile = tile;
    }
    tlc->sub.transient_contents = sub_tll;
    tlc->sub.transient_props = sub_tlpl;
    return tlc;
}

DP_TransientLayerContent *
DP_transient_layer_content_incref(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return (DP_TransientLayerContent *)DP_layer_content_incref(
        (DP_LayerContent *)tlc);
}

void DP_transient_layer_content_decref(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_layer_content_decref((DP_LayerContent *)tlc);
}

int DP_transient_layer_content_refcount(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return DP_layer_content_refcount((DP_LayerContent *)tlc);
}

DP_LayerContent *
DP_transient_layer_content_persist(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    tlc->transient = false;
    int count = DP_tile_total_round(tlc->width, tlc->height);
    for (int i = 0; i < count; ++i) {
        DP_Tile *tile = tlc->elements[i].tile;
        if (tile && DP_tile_transient(tile)) {
            DP_TransientTile *tt = tlc->elements[i].transient_tile;
            if (DP_transient_tile_blank(tt)) {
                DP_transient_tile_decref(tt);
                tlc->elements[i].transient_tile = NULL;
            }
            else {
                DP_transient_tile_persist(tt);
            }
        }
    }
    if (DP_layer_list_transient(tlc->sub.contents)) {
        DP_transient_layer_list_persist(tlc->sub.transient_contents);
    }
    if (DP_layer_props_list_transient(tlc->sub.props)) {
        DP_transient_layer_props_list_persist(tlc->sub.transient_props);
    }
    return (DP_LayerContent *)tlc;
}

int DP_transient_layer_content_width(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return DP_layer_content_width((DP_LayerContent *)tlc);
}

int DP_transient_layer_content_height(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return DP_layer_content_height((DP_LayerContent *)tlc);
}

DP_Tile *DP_transient_layer_content_tile_at_noinc(DP_TransientLayerContent *tlc,
                                                  int x, int y)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return DP_layer_content_tile_at_noinc((DP_LayerContent *)tlc, x, y);
}

void DP_transient_layer_content_transient_tile_at_set_noinc(
    DP_TransientLayerContent *tlc, int x, int y, DP_TransientTile *tt)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    int i = y * DP_tile_count_round(tlc->width) + x;
    DP_tile_decref_nullable(tlc->elements[i].tile);
    tlc->elements[i].transient_tile = tt;
}


DP_LayerList *
DP_transient_layer_content_sub_contents_noinc(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return DP_layer_content_sub_contents_noinc((DP_LayerContent *)tlc);
}

DP_LayerPropsList *
DP_transient_layer_content_sub_props_noinc(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return DP_layer_content_sub_props_noinc((DP_LayerContent *)tlc);
}


DP_TransientLayerContent *
DP_transient_layer_content_resize_to(DP_TransientLayerContent *tlc,
                                     unsigned int context_id, int width,
                                     int height)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    int layer_width = tlc->width;
    int layer_height = tlc->height;
    if (layer_width == width && layer_height == height) {
        return tlc;
    }
    else {
        DP_TransientLayerContent *next = DP_layer_content_resize(
            (DP_LayerContent *)tlc, context_id, 0, width - layer_width,
            height - layer_height, 0);
        DP_transient_layer_content_decref(tlc);
        return next;
    }
}


static DP_TransientTile *create_transient_tile(DP_TransientLayerContent *tlc,
                                               unsigned int context_id, int i)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < DP_tile_total_round(tlc->width, tlc->height));
    DP_ASSERT(!tlc->elements[i].tile);
    DP_TransientTile *tt = DP_transient_tile_new_blank(context_id);
    tlc->elements[i].transient_tile = tt;
    return tt;
}

static bool can_blend_blank(int blend_mode, uint16_t opacity)
{
    return DP_blend_mode_blend_blank(blend_mode) && opacity != 0;
}

void DP_transient_layer_content_merge(DP_TransientLayerContent *tlc,
                                      unsigned int context_id,
                                      DP_LayerContent *lc, uint16_t opacity,
                                      int blend_mode)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(tlc->width == lc->width);
    DP_ASSERT(tlc->height == lc->height);
    int count = DP_tile_total_round(lc->width, lc->height);
    bool blend_blank = can_blend_blank(blend_mode, opacity);
    for (int i = 0; i < count; ++i) {
        DP_Tile *t = lc->elements[i].tile;
        if (t) {
            if (tlc->elements[i].tile) {
                DP_TransientTile *tt = get_transient_tile(tlc, context_id, i);
                DP_ASSERT((void *)tt != (void *)t);
                DP_transient_tile_merge(tt, t, opacity, blend_mode);
            }
            else if (blend_blank) {
                // For blend modes like normal and behind, do regular blending.
                DP_TransientTile *tt =
                    create_transient_tile(tlc, context_id, i);
                DP_transient_tile_merge(tt, t, opacity, blend_mode);
            }
            else {
                // For most other blend modes merging with transparent pixels
                // doesn't do anything. For example erasing nothing or multiply
                // with nothing just leads to more nothing. Skip the empty tile.
            }
        }
    }
}


static DP_TransientLayerList *
get_transient_sub_contents(DP_TransientLayerContent *tlc, int reserve)
{
    DP_LayerList *ll = tlc->sub.contents;
    if (!DP_layer_list_transient(ll)) {
        tlc->sub.transient_contents = DP_transient_layer_list_new(ll, reserve);
        DP_layer_list_decref(ll);
    }
    else if (reserve != 0) {
        tlc->sub.transient_contents = DP_transient_layer_list_reserve(
            tlc->sub.transient_contents, reserve);
    }
    return tlc->sub.transient_contents;
}

static DP_TransientLayerPropsList *
get_transient_sub_props(DP_TransientLayerContent *tlc, int reserve)
{
    DP_LayerPropsList *lpl = tlc->sub.props;
    if (!DP_layer_props_list_transient(lpl)) {
        tlc->sub.transient_props =
            DP_transient_layer_props_list_new(lpl, reserve);
        DP_layer_props_list_decref(lpl);
    }
    else if (reserve != 0) {
        tlc->sub.transient_props = DP_transient_layer_props_list_reserve(
            tlc->sub.transient_props, reserve);
    }
    return tlc->sub.transient_props;
}

static void create_sublayer(DP_TransientLayerContent *tlc, int sublayer_id,
                            DP_TransientLayerContent **out_tlc,
                            DP_TransientLayerProps **out_tlp)
{
    DP_TransientLayerList *tll = get_transient_sub_contents(tlc, 1);
    DP_TransientLayerPropsList *tlpl = get_transient_sub_props(tlc, 1);
    DP_ASSERT(DP_transient_layer_props_list_count(tlpl)
              == DP_transient_layer_list_count(tll));
    DP_ASSERT(DP_transient_layer_props_list_index_by_id(tlpl, sublayer_id)
              == -1);
    int index = DP_transient_layer_list_count(tll) - 1;

    DP_TransientLayerContent *sub_tlc =
        DP_transient_layer_content_new_init(tlc->width, tlc->height, NULL);
    DP_transient_layer_list_insert_transient_content_noinc(tll, sub_tlc, index);

    DP_TransientLayerProps *sub_tlp =
        DP_transient_layer_props_new_init(sublayer_id, false);
    DP_transient_layer_props_list_insert_transient_noinc(tlpl, sub_tlp, index);

    if (out_tlc) {
        *out_tlc = sub_tlc;
    }
    if (out_tlp) {
        *out_tlp = sub_tlp;
    }
}

#define PUT_IMAGE_PIXEL_SKIP 0
#define PUT_IMAGE_PIXEL_PUT  1
#define PUT_IMAGE_PIXEL_SET  2

static int put_image_handle_pixel(int blend_mode, DP_Pixel8 pixel,
                                  DP_Pixel15 *out_dst_pixel)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_ERASE:
        if (pixel.a == 255) {
            *out_dst_pixel = DP_pixel15_zero();
            return PUT_IMAGE_PIXEL_SET;
        }
        else if (pixel.a != 0) {
            uint16_t a = DP_channel8_to_15(pixel.a);
            *out_dst_pixel = (DP_Pixel15){a, a, a, a};
            return PUT_IMAGE_PIXEL_PUT;
        }
        else {
            return PUT_IMAGE_PIXEL_SKIP;
        }
    case DP_BLEND_MODE_NORMAL:
        if (pixel.a == 255) {
            *out_dst_pixel = DP_pixel8_to_15(pixel);
            return PUT_IMAGE_PIXEL_SET;
        }
        else if (pixel.a != 0) {
            *out_dst_pixel = DP_pixel8_to_15(pixel);
            return PUT_IMAGE_PIXEL_PUT;
        }
        else {
            return PUT_IMAGE_PIXEL_SKIP;
        }
    case DP_BLEND_MODE_NORMAL_AND_ERASER:
        *out_dst_pixel = DP_pixel8_to_15(pixel);
        return pixel.a == 255 ? PUT_IMAGE_PIXEL_SET : PUT_IMAGE_PIXEL_PUT;
    case DP_BLEND_MODE_REPLACE:
        *out_dst_pixel = DP_pixel8_to_15(pixel);
        return PUT_IMAGE_PIXEL_SET;
    default:
        if (pixel.a != 0) {
            *out_dst_pixel = DP_pixel8_to_15(pixel);
            return PUT_IMAGE_PIXEL_PUT;
        }
        else {
            return PUT_IMAGE_PIXEL_SKIP;
        }
    }
}

void DP_transient_layer_content_put_image(DP_TransientLayerContent *tlc,
                                          unsigned int context_id,
                                          int blend_mode, int left, int top,
                                          DP_Image *img)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(img);

    int width = tlc->width;
    int wt = DP_tile_count_round(width);
    DP_TileIterator ti = DP_tile_iterator_make(
        width, tlc->height,
        DP_rect_make(left, top, DP_image_width(img), DP_image_height(img)));

    while (DP_tile_iterator_next(&ti)) {
        int i = ti.row * wt + ti.col;
        if (tlc->elements[i].tile || DP_blend_mode_blend_blank(blend_mode)) {
            DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
            DP_TransientTile *tt = NULL;
            while (DP_tile_into_dst_iterator_next(&tidi)) {
                DP_Pixel8 pixel =
                    DP_image_pixel_at(img, tidi.dst_x, tidi.dst_y);
                DP_Pixel15 dst_pixel;
                switch (put_image_handle_pixel(blend_mode, pixel, &dst_pixel)) {
                case PUT_IMAGE_PIXEL_SKIP:
                    break;
                case PUT_IMAGE_PIXEL_PUT:
                    if (!tt) {
                        tt = get_or_create_transient_tile(tlc, context_id, i);
                    }
                    DP_transient_tile_pixel_at_put(tt, blend_mode, tidi.tile_x,
                                                   tidi.tile_y, dst_pixel);
                    break;
                case PUT_IMAGE_PIXEL_SET:
                    if (!tt) {
                        tt = get_or_create_transient_tile(tlc, context_id, i);
                    }
                    DP_transient_tile_pixel_at_set(tt, tidi.tile_x, tidi.tile_y,
                                                   dst_pixel);
                    break;
                }
            }
        }
    }
}


static bool can_blend_blank_pixel(int blend_mode, uint16_t opacity,
                                  DP_UPixel15 pixel)
{
    return can_blend_blank(blend_mode, opacity)
        && ((blend_mode != DP_BLEND_MODE_NORMAL_AND_ERASER
             && blend_mode != DP_BLEND_MODE_REPLACE)
            || pixel.a != 0);
}

// We want to initialize a static buffer with the same value 4096 times, so this
// is a goofy way to achieve that at compile time without spelling it all out.
#define DP_BIT15_4    DP_BIT15, DP_BIT15, DP_BIT15, DP_BIT15
#define DP_BIT15_16   DP_BIT15_4, DP_BIT15_4, DP_BIT15_4, DP_BIT15_4
#define DP_BIT15_64   DP_BIT15_16, DP_BIT15_16, DP_BIT15_16, DP_BIT15_16
#define DP_BIT15_256  DP_BIT15_64, DP_BIT15_64, DP_BIT15_64, DP_BIT15_64
#define DP_BIT15_1024 DP_BIT15_256, DP_BIT15_256, DP_BIT15_256, DP_BIT15_256
#define DP_BIT15_4096 DP_BIT15_1024, DP_BIT15_1024, DP_BIT15_1024, DP_BIT15_1024

static void fill_rect(DP_TransientLayerContent *tlc, unsigned int context_id,
                      int blend_mode, int left, int top, int right, int bottom,
                      DP_UPixel15 pixel)
{
    const uint16_t *mask = DP_tile_opaque_mask();
    uint16_t opacity = pixel.a;
    bool blend_blank = can_blend_blank_pixel(blend_mode, opacity, pixel);

    int tx_min = left / DP_TILE_SIZE;
    int tx_max = (right - 1) / DP_TILE_SIZE;
    int ty_min = top / DP_TILE_SIZE;
    int ty_max = (bottom - 1) / DP_TILE_SIZE;
    int xtiles = DP_tile_counts_round(tlc->width, tlc->height).x;

    for (int ty = ty_min; ty <= ty_max; ++ty) {
        int cy = ty * DP_TILE_SIZE;
        for (int tx = tx_min; tx <= tx_max; ++tx) {
            int cx = tx * DP_TILE_SIZE;
            int x = DP_max_int(cx, left) - cx;
            int y = DP_max_int(cy, top) - cy;
            int w = DP_min_int((tx + 1) * DP_TILE_SIZE, right) - cx - x;
            int h = DP_min_int((ty + 1) * DP_TILE_SIZE, bottom) - cy - y;
            int i = ty * xtiles + tx;

            DP_TransientTile *tt;
            if (tlc->elements[i].tile) {
                tt = get_or_create_transient_tile(tlc, context_id, i);
            }
            else if (blend_blank) {
                tt = DP_transient_tile_new_blank(context_id);
                tlc->elements[i].transient_tile = tt;
            }
            else {
                continue; // Nothing to do on a blank tile.
            }
            DP_transient_tile_brush_apply(tt, pixel, blend_mode, mask, opacity,
                                          x, y, w, h, 0);
        }
    }
}

static void fill_entirely(DP_TransientLayerContent *tld,
                          unsigned int context_id, int blend_mode,
                          DP_UPixel15 pixel)
{
    bool is_replacement =
        blend_mode == DP_BLEND_MODE_REPLACE
        || (blend_mode == DP_BLEND_MODE_NORMAL && pixel.a == DP_BIT15);
    if (is_replacement) {
        DP_Tile *tile = DP_tile_new_from_upixel15(context_id, pixel);
        int tile_count = DP_tile_total_round(tld->width, tld->height);
        DP_tile_incref_by(tile, tile_count - 1);
        for (int i = 0; i < tile_count; ++i) {
            DP_tile_decref_nullable(tld->elements[i].tile);
            tld->elements[i].tile = tile;
        }
    }
    else {
        fill_rect(tld, context_id, blend_mode, 0, 0, tld->width, tld->height,
                  pixel);
    }
}

void DP_transient_layer_content_fill_rect(DP_TransientLayerContent *tlc,
                                          unsigned int context_id,
                                          int blend_mode, int left, int top,
                                          int right, int bottom,
                                          DP_UPixel15 pixel)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    if (left == 0 && top == 0 && right == tlc->width && bottom == tlc->height) {
        fill_entirely(tlc, context_id, blend_mode, pixel);
    }
    else {
        fill_rect(tlc, context_id, blend_mode, left, top, right, bottom, pixel);
    }
}

void DP_transient_layer_content_tile_set_noinc(DP_TransientLayerContent *tlc,
                                               DP_Tile *t, int i)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(i < DP_tile_total_round(tlc->width, tlc->height));
    DP_tile_decref_nullable(tlc->elements[i].tile);
    tlc->elements[i].tile = t;
}

void DP_transient_layer_content_transient_tile_set_noinc(
    DP_TransientLayerContent *tlc, DP_TransientTile *tt, int i)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(i < DP_tile_total_round(tlc->width, tlc->height));
    DP_tile_decref_nullable(tlc->elements[i].tile);
    tlc->elements[i].transient_tile = tt;
}

void DP_transient_layer_content_put_tile_inc(DP_TransientLayerContent *tlc,
                                             DP_Tile *tile, int x, int y,
                                             int repeat)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);

    DP_TileCounts tile_counts = DP_tile_counts_round(tlc->width, tlc->height);
    int tile_total = tile_counts.x * tile_counts.y;
    int start = y * tile_counts.x + x;
    DP_ASSERT(start < tile_total);

    int end = start + repeat + 1;
    if (end > tile_total) {
        DP_warn("Put tile: ending index %d clamped to %d", end, tile_total);
        end = tile_total;
    }

    DP_tile_incref_by(tile, end - start);
    for (int i = start; i < end; ++i) {
        DP_tile_decref_nullable(tlc->elements[i].tile);
        tlc->elements[i].tile = tile;
    }
}


typedef void (*DP_ApplyBrushStampFn)(DP_TransientTile *tt, const uint16_t *mask,
                                     uint16_t opacity, int x, int y, int w,
                                     int h, int skip, void *user);

static void apply_brush_stamp_with(DP_TransientLayerContent *tlc,
                                   unsigned int context_id, uint16_t opacity,
                                   DP_BrushStamp *stamp, bool blend_blank,
                                   DP_ApplyBrushStampFn apply_fn, void *user)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(stamp);

    int width = tlc->width;
    int height = tlc->height;
    int top = stamp->top;
    int left = stamp->left;
    int d = stamp->diameter;
    if (left + d <= 0 || top + d <= 0 || left >= width || top >= height) {
        return; // Out of bounds, nothing to do.
    }

    int bottom = DP_min_int(top + d, height);
    int right = DP_min_int(left + d, width);
    int xtiles = DP_tile_count_round(width);
    uint16_t *mask = stamp->data;

    int y = top < 0 ? 0 : top;
    int yb = top < 0 ? -top : 0;
    int x0 = left < 0 ? 0 : left;
    int xb0 = left < 0 ? -left : 0;
    while (y < bottom) {
        int yindex = y / DP_TILE_SIZE;
        int yt = y - yindex * DP_TILE_SIZE;
        int hb = yt + d - yb < DP_TILE_SIZE ? d - yb : DP_TILE_SIZE - yt;
        int x = x0;
        int xb = xb0;
        while (x < right) {
            int xindex = x / DP_TILE_SIZE;
            int xt = x - xindex * DP_TILE_SIZE;
            int wb = xt + d - xb < DP_TILE_SIZE ? d - xb : DP_TILE_SIZE - xt;
            int mask_offset = yb * d + xb;
            int i = xtiles * yindex + xindex;
            x = (xindex + 1) * DP_TILE_SIZE;
            xb = xb + wb;

            DP_TransientTile *tt;
            if (tlc->elements[i].tile) {
                tt = get_transient_tile(tlc, context_id, i);
            }
            else if (blend_blank) {
                tt = create_transient_tile(tlc, context_id, i);
            }
            else {
                continue;
            }

            apply_fn(tt, mask + mask_offset, opacity, xt, yt, wb, hb, d - wb,
                     user);
        }
        y = (yindex + 1) * DP_TILE_SIZE;
        yb = yb + hb;
    }
}


struct DP_ApplyStampParams {
    DP_UPixel15 pixel;
    int blend_mode;
};

static void apply_stamp(DP_TransientTile *tt, const uint16_t *mask,
                        uint16_t opacity, int x, int y, int w, int h, int skip,
                        void *user)
{
    struct DP_ApplyStampParams *params = user;
    DP_transient_tile_brush_apply(tt, params->pixel, params->blend_mode, mask,
                                  opacity, x, y, w, h, skip);
}

void DP_transient_layer_content_brush_stamp_apply(
    DP_TransientLayerContent *tlc, unsigned int context_id, DP_UPixel15 pixel,
    uint16_t opacity, int blend_mode, DP_BrushStamp *stamp)
{
    struct DP_ApplyStampParams params = {pixel, blend_mode};
    apply_brush_stamp_with(tlc, context_id, opacity, stamp,
                           can_blend_blank_pixel(blend_mode, opacity, pixel),
                           apply_stamp, &params);
}


static void apply_stamp_posterize(DP_TransientTile *tt, const uint16_t *mask,
                                  uint16_t opacity, int x, int y, int w, int h,
                                  int skip, void *user)
{
    int *posterize_num_ptr = user;
    DP_transient_tile_brush_apply_posterize(tt, *posterize_num_ptr, mask,
                                            opacity, x, y, w, h, skip);
}

void DP_transient_layer_content_brush_stamp_apply_posterize(
    DP_TransientLayerContent *tlc, unsigned int context_id, uint16_t opacity,
    int posterize_num, DP_BrushStamp *stamp)
{
    apply_brush_stamp_with(tlc, context_id, opacity, stamp, true,
                           apply_stamp_posterize, &posterize_num);
}


void DP_transient_layer_content_transient_sublayer_at(
    DP_TransientLayerContent *tlc, int sublayer_index,
    DP_TransientLayerContent **out_tlc, DP_TransientLayerProps **out_tlp)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    if (out_tlc) {
        DP_TransientLayerList *tll = get_transient_sub_contents(tlc, 0);
        *out_tlc = DP_transient_layer_list_transient_content_at_noinc(
            tll, sublayer_index);
    }
    if (out_tlp) {
        DP_TransientLayerPropsList *tlpl = get_transient_sub_props(tlc, 0);
        *out_tlp = DP_transient_layer_props_list_transient_at_noinc(
            tlpl, sublayer_index);
    }
}

void DP_transient_layer_content_transient_sublayer(
    DP_TransientLayerContent *tlc, int sublayer_id,
    DP_TransientLayerContent **out_tlc, DP_TransientLayerProps **out_tlp)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    int index = DP_layer_props_list_index_by_id(tlc->sub.props, sublayer_id);
    if (index < 0) {
        create_sublayer(tlc, sublayer_id, out_tlc, out_tlp);
    }
    else {
        DP_transient_layer_content_transient_sublayer_at(tlc, index, out_tlc,
                                                         out_tlp);
    }
}

void DP_transient_layer_content_sublayer_insert_inc(
    DP_TransientLayerContent *tlc, DP_LayerContent *sub_lc,
    DP_LayerProps *sub_lp)
{
    DP_ASSERT(tlc);
    DP_ASSERT(sub_lc);
    DP_ASSERT(sub_lp);

    DP_TransientLayerList *tll = get_transient_sub_contents(tlc, 1);
    DP_TransientLayerPropsList *tlpl = get_transient_sub_props(tlc, 1);
    DP_ASSERT(DP_transient_layer_props_list_count(tlpl)
              == DP_transient_layer_list_count(tll));
    DP_ASSERT(DP_transient_layer_props_list_index_by_id(
                  tlpl, DP_layer_props_id(sub_lp))
              == -1);

    int index = DP_transient_layer_list_count(tll) - 1;
    DP_transient_layer_list_set_content_inc(tll, sub_lc, index);
    DP_transient_layer_props_list_set_inc(tlpl, sub_lp, index);
}

void DP_transient_layer_content_merge_sublayer_at(DP_TransientLayerContent *tlc,
                                                  unsigned int context_id,
                                                  int index)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_TransientLayerList *tll = get_transient_sub_contents(tlc, 0);
    DP_TransientLayerPropsList *tlpl = get_transient_sub_props(tlc, 0);
    DP_LayerListEntry *lle = DP_transient_layer_list_at_noinc(tll, index);
    DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
    DP_LayerProps *lp = DP_transient_layer_props_list_at_noinc(tlpl, index);
    DP_transient_layer_content_merge(tlc, context_id, lc,
                                     DP_layer_props_opacity(lp),
                                     DP_layer_props_blend_mode(lp));
    DP_transient_layer_list_delete_at(tll, index);
    DP_transient_layer_props_list_delete_at(tlpl, index);
}

void DP_transient_layer_content_merge_all_sublayers(
    DP_TransientLayerContent *tlc, unsigned int context_id)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_LayerList *ll = tlc->sub.contents;
    DP_LayerPropsList *lpl = tlc->sub.props;
    int count = DP_layer_list_count(ll);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        DP_transient_layer_content_merge(tlc, context_id, lc,
                                         DP_layer_props_opacity(lp),
                                         DP_layer_props_blend_mode(lp));
    }
    DP_layer_list_decref(ll);
    DP_layer_props_list_decref(lpl);
    tlc->sub.contents = DP_layer_list_new();
    tlc->sub.props = DP_layer_props_list_new();
}

DP_TransientTile *
DP_transient_layer_content_render_tile(DP_TransientLayerContent *tlc,
                                       DP_CanvasState *cs, int tile_index,
                                       const DP_ViewModeFilter *vmf_or_null)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(cs);
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(tlc->width, tlc->height));
    DP_tile_decref_nullable(tlc->elements[tile_index].tile);
    DP_ViewModeFilter vmf =
        vmf_or_null ? *vmf_or_null : DP_view_mode_filter_make_default();
    DP_TransientTile *tt = DP_canvas_state_flatten_tile(
        cs, tile_index, DP_FLAT_IMAGE_RENDER_FLAGS, &vmf);
    tlc->elements[tile_index].transient_tile = tt;
    return tt;
}
