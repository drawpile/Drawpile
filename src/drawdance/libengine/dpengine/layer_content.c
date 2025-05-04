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


#ifdef DP_NO_STRICT_ALIASING

struct DP_LayerContent {
    DP_Atomic refcount;
    const bool transient;
    const int width, height;
    DP_LayerContent *mask;
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
    union {
        DP_LayerContent *mask;
        DP_TransientLayerContent *transient_mask;
    };
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
    union {
        DP_LayerContent *mask;
        DP_TransientLayerContent *transient_mask;
    };
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
        DP_layer_content_decref_nullable(lc->mask);
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
                           prev_lc->sub.contents, prev_lc->sub.props, diff, 0);
    }
    else if (censored && prev_censored) {
        DP_canvas_diff_check(diff, diff_tile_both_censored,
                             (DP_LayerContent *[]){lc, prev_lc});
        DP_layer_list_diff(lc->sub.contents, lc->sub.props,
                           prev_lc->sub.contents, prev_lc->sub.props, diff, 0);
    }
    else {
        layer_content_diff_mark_both(lc, prev_lc, diff);
    }
}

void DP_layer_content_diff(DP_LayerContent *lc, DP_LayerProps *lp,
                           DP_LayerContent *prev_lc, DP_LayerProps *prev_lp,
                           DP_CanvasDiff *diff, int only_layer_id)
{
    DP_ASSERT(lc);
    DP_ASSERT(lp);
    DP_ASSERT(prev_lc);
    DP_ASSERT(prev_lp);
    DP_ASSERT(diff);

    bool visible, prev_visible;
    if (only_layer_id == 0) {
        visible = DP_layer_props_visible(lp);
        prev_visible = DP_layer_props_visible(prev_lp);
    }
    else {
        visible = only_layer_id == DP_layer_props_id(lp);
        prev_visible = only_layer_id == DP_layer_props_id(prev_lp);
    }

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

void DP_layer_content_diff_selection(DP_LayerContent *lc,
                                     DP_LayerContent *prev_lc,
                                     DP_CanvasDiff *diff)
{
    DP_ASSERT(lc);
    DP_ASSERT(prev_lc);
    DP_ASSERT(diff);
    layer_content_diff(lc, false, prev_lc, false, diff);
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

DP_Tile *DP_layer_content_tile_at_index_noinc(DP_LayerContent *lc, int i)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(i >= 0);
    DP_ASSERT(i < DP_tile_total_round(lc->width, lc->height));
    return lc->elements[i].tile;
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

bool DP_layer_content_pick_at(DP_LayerContent *lc, int x, int y,
                              unsigned int *out_context_id)
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
        int xp = x - xt * DP_TILE_SIZE;
        int yp = y - yt * DP_TILE_SIZE;
        if (DP_tile_pixel_at(t, xp, yp).a != 0) {
            if (out_context_id) {
                *out_context_id = DP_tile_context_id(t);
            }
            return true;
        }
    }
    return false;
}

static DP_UPixelFloat sample_dab_color(DP_LayerContent *lc, DP_BrushStamp stamp,
                                       bool opaque, bool pigment)
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

    int sample_interval = diameter <= 4 ? 1 : (diameter * 7) / 2;
    float sample_rate = 1.0f / (14.0f * DP_int_to_float(diameter));

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

            if (pigment) {
                DP_tile_sample_pigment(
                    lc->elements[i].tile, weights + yb * diameter + xb, xt, yt,
                    wb, hb, diameter - wb, opaque, sample_interval, sample_rate,
                    &weight, &red, &green, &blue, &alpha);
            }
            else {
                DP_tile_sample(lc->elements[i].tile,
                               weights + yb * diameter + xb, xt, yt, wb, hb,
                               diameter - wb, opaque, &weight, &red, &green,
                               &blue, &alpha);
            }

            x = (xindex + 1) * DP_TILE_SIZE;
            xb = xb + wb;
        }
        y = (yindex + 1) * DP_TILE_SIZE;
        yb = yb + hb;
    }

    return DP_paint_sample_to_upixel(diameter, opaque, pigment, weight, red,
                                     green, blue, alpha);
}

DP_UPixelFloat DP_layer_content_sample_color_at(
    DP_LayerContent *lc, uint16_t *stamp_buffer, int x, int y, int diameter,
    bool opaque, bool pigment, int *in_out_last_diameter, bool *out_in_bounds)
{
    int radius = diameter < 2 ? 0 : diameter / 2;
    bool in_bounds = x + radius >= 0 && y + radius >= 0
                  && x - radius < lc->width && y - radius < lc->height;
    if (out_in_bounds) {
        *out_in_bounds = in_bounds;
    }

    if (in_bounds) {
        if (diameter < 2) {
            DP_Pixel15 pixel = DP_layer_content_pixel_at(lc, x, y);
            return DP_upixel15_to_float(DP_pixel15_unpremultiply(pixel));
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
            DP_BrushStamp stamp = DP_paint_color_sampling_stamp_make(
                stamp_buffer, diameter, x, y, last_diameter);
            return sample_dab_color(lc, stamp, opaque, pigment);
        }
    }
    else {
        return DP_upixel_float_zero();
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

static bool tile_same_pixel(DP_LayerContent *lc, DP_Pixel15 pixel,
                            DP_TileIterator *ti)
{
    DP_Tile *t = DP_layer_content_tile_at_noinc(lc, ti->col, ti->row);
    if (t) {
        DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(ti);
        while (DP_tile_into_dst_iterator_next(&tidi)) {
            DP_Pixel15 q = DP_tile_pixel_at(t, tidi.tile_x, tidi.tile_y);
            if (!DP_pixel15_equal(pixel, q)) {
                return false;
            }
        }
        return true;
    }
    else {
        return pixel.a == 0;
    }
}

bool DP_layer_content_same_pixel(DP_LayerContent *lc, DP_Pixel15 *out_pixel)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    int width = lc->width;
    int height = lc->height;
    if (width > 0 && height > 0) {
        DP_Pixel15 pixel = DP_layer_content_pixel_at(lc, 0, 0);
        DP_TileIterator ti = DP_tile_iterator_make(
            width, height, DP_rect_make(0, 0, width, height));
        while (DP_tile_iterator_next(&ti)) {
            if (!tile_same_pixel(lc, pixel, &ti)) {
                return false;
            }
        }

        if (out_pixel) {
            *out_pixel = pixel;
        }
        return true;
    }
    else {
        return false;
    }
}

bool DP_layer_content_has_content(DP_LayerContent *lc)
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

static bool layer_content_tile_bounds(DP_LayerContent *lc, int *out_left,
                                      int *out_top, int *out_right,
                                      int *out_bottom)
{
    DP_TileCounts tile_counts = DP_tile_counts_round(lc->width, lc->height);
    int left = tile_counts.x;
    int top = tile_counts.y;
    int right = 0;
    int bottom = 0;

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

static bool layer_content_crop_top(DP_LayerContent *lc, DP_Rect rect,
                                   int *out_top)
{
    for (int y = rect.y1; y <= rect.y2; ++y) {
        for (int x = rect.x1; x <= rect.x2; ++x) {
            if (DP_layer_content_pixel_at(lc, x, y).a != 0) {
                *out_top = y;
                return true;
            }
        }
    }
    return false;
}

static bool layer_content_crop_bottom(DP_LayerContent *lc, DP_Rect rect,
                                      int *out_bottom)
{
    for (int y = rect.y2; y >= rect.y1; --y) {
        for (int x = rect.x1; x <= rect.x2; ++x) {
            if (DP_layer_content_pixel_at(lc, x, y).a != 0) {
                *out_bottom = y;
                return true;
            }
        }
    }
    return false;
}

static bool layer_content_crop_left(DP_LayerContent *lc, DP_Rect rect,
                                    int *out_left)
{
    for (int x = rect.x1; x <= rect.x2; ++x) {
        for (int y = rect.y1; y <= rect.y2; ++y) {
            if (DP_layer_content_pixel_at(lc, x, y).a != 0) {
                *out_left = x;
                return true;
            }
        }
    }
    return false;
}

static bool layer_content_crop_right(DP_LayerContent *lc, DP_Rect rect,
                                     int *out_right)
{
    for (int x = rect.x2; x >= rect.x1; --x) {
        for (int y = rect.y1; y <= rect.y2; ++y) {
            if (DP_layer_content_pixel_at(lc, x, y).a != 0) {
                *out_right = x;
                return true;
            }
        }
    }
    return false;
}

static bool layer_content_crop(DP_LayerContent *lc, int *out_x, int *out_y,
                               int *out_width, int *out_height)
{
    int tile_left, tile_top, tile_right, tile_bottom;
    if (!layer_content_tile_bounds(lc, &tile_left, &tile_top, &tile_right,
                                   &tile_bottom)) {
        return false;
    }

    DP_Rect rect =
        DP_rect_make(tile_left * DP_TILE_SIZE, tile_top * DP_TILE_SIZE,
                     (tile_right - tile_left + 1) * DP_TILE_SIZE,
                     (tile_bottom - tile_top + 1) * DP_TILE_SIZE);

    int max_x = lc->width - 1;
    if (rect.x2 > max_x) {
        rect.x2 = max_x;
    }

    int max_y = lc->height - 1;
    if (rect.y2 > max_y) {
        rect.y2 = max_y;
    }

    bool crop_ok = layer_content_crop_top(lc, rect, &rect.y1)
                && layer_content_crop_bottom(lc, rect, &rect.y2)
                && layer_content_crop_left(lc, rect, &rect.x1)
                && layer_content_crop_right(lc, rect, &rect.x2);

    if (crop_ok) {
        *out_x = rect.x1;
        *out_y = rect.y1;
        *out_width = DP_rect_width(rect);
        *out_height = DP_rect_height(rect);
        return true;
    }
    else {
        return false;
    }
}

static bool layer_content_crop_prev(DP_LayerContent *lc, DP_Rect prev,
                                    int *out_x, int *out_y, int *out_width,
                                    int *out_height)
{
    int tile_left, tile_top, tile_right, tile_bottom;
    if (!layer_content_tile_bounds(lc, &tile_left, &tile_top, &tile_right,
                                   &tile_bottom)) {
        return false;
    }

    DP_Rect rect =
        DP_rect_make(tile_left * DP_TILE_SIZE, tile_top * DP_TILE_SIZE,
                     (tile_right - tile_left + 1) * DP_TILE_SIZE,
                     (tile_bottom - tile_top + 1) * DP_TILE_SIZE);

    int max_x = lc->width - 1;
    if (rect.x2 > max_x) {
        rect.x2 = max_x;
    }

    int max_y = lc->height - 1;
    if (rect.y2 > max_y) {
        rect.y2 = max_y;
    }

    bool crop_ok =
        (prev.y1 <= rect.y1 || layer_content_crop_top(lc, rect, &rect.y1))
        && (prev.y2 >= rect.y2 || layer_content_crop_bottom(lc, rect, &rect.y2))
        && (prev.x1 <= rect.x1 || layer_content_crop_left(lc, rect, &rect.x1))
        && (prev.x2 >= rect.x2 || layer_content_crop_right(lc, rect, &rect.x2));

    if (crop_ok) {
        *out_x = rect.x1;
        *out_y = rect.y1;
        *out_width = DP_rect_width(rect);
        *out_height = DP_rect_height(rect);
        return true;
    }
    else {
        return false;
    }
}

static void layer_content_union_bounds(DP_LayerContent *lc, bool *in_out_valid,
                                       DP_Rect *in_out_bounds)
{
    int x, y, width, height;
    bool prev_valid = *in_out_valid;
    bool valid = prev_valid ? layer_content_crop_prev(lc, *in_out_bounds, &x,
                                                      &y, &width, &height)
                            : layer_content_crop(lc, &x, &y, &width, &height);
    if (valid) {
        DP_Rect bounds = DP_rect_make(x, y, width, height);
        if (prev_valid) {
            *in_out_bounds = DP_rect_union(*in_out_bounds, bounds);
        }
        else {
            *in_out_valid = true;
            *in_out_bounds = bounds;
        }
    }
}

bool DP_layer_content_bounds(DP_LayerContent *lc, bool include_sublayers,
                             DP_Rect *out_bounds)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);

    bool valid = false;
    DP_Rect bounds;
    layer_content_union_bounds(lc, &valid, &bounds);
    if (include_sublayers) {
        DP_LayerList *sub_ll = lc->sub.contents;
        int sub_count = DP_layer_list_count(sub_ll);
        for (int i = 0; i < sub_count; ++i) {
            layer_content_union_bounds(
                DP_layer_list_content_at_noinc(sub_ll, i), &valid, &bounds);
        }
    }

    if (valid && out_bounds) {
        *out_bounds = bounds;
    }
    return valid;
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

bool DP_layer_content_is_blank_in_bounds(DP_LayerContent *lc,
                                         const DP_Rect *rect)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(rect);
    DP_TileIterator ti = DP_tile_iterator_make(lc->width, lc->height, *rect);
    while (DP_tile_iterator_next(&ti)) {
        DP_Tile *t = DP_layer_content_tile_at_noinc(lc, ti.col, ti.row);
        if (t && !DP_tile_blank(t)) {
            return false;
        }
    }
    return true;
}

bool DP_layer_content_is_blank_in_mask(DP_LayerContent *lc, const DP_Rect *rect,
                                       const DP_Pixel8 *mask)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(rect);
    DP_ASSERT(mask);
    int mask_width = DP_rect_width(*rect);
    DP_TileIterator ti = DP_tile_iterator_make(lc->width, lc->height, *rect);
    while (DP_tile_iterator_next(&ti)) {
        DP_Tile *t = DP_layer_content_tile_at_noinc(lc, ti.col, ti.row);
        if (t) {
            DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
            while (DP_tile_into_dst_iterator_next(&tidi)) {
                if (mask[tidi.dst_y * mask_width + tidi.dst_x].a != 0
                    && DP_tile_pixel_at(t, tidi.tile_x, tidi.tile_y).a != 0) {
                    return false;
                }
            }
        }
    }
    return true;
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

static bool layer_content_crop_censored(DP_LayerContent *lc, int *out_x,
                                        int *out_y, int *out_width,
                                        int *out_height)
{
    int tile_left, tile_top, tile_right, tile_bottom;
    if (!layer_content_tile_bounds(lc, &tile_left, &tile_top, &tile_right,
                                   &tile_bottom)) {
        return false;
    }

    DP_Rect crop =
        DP_rect_make(tile_left * DP_TILE_SIZE, tile_top * DP_TILE_SIZE,
                     (tile_right - tile_left + 1) * DP_TILE_SIZE,
                     (tile_bottom - tile_top + 1) * DP_TILE_SIZE);
    *out_x = crop.x1;
    *out_y = crop.y1;
    *out_width = DP_rect_width(crop);
    *out_height = DP_rect_height(crop);
    return true;
}

DP_UPixel8 *
DP_layer_content_to_upixels8_cropped(DP_LayerContent *lc, bool censored,
                                     int *out_offset_x, int *out_offset_y,
                                     int *out_width, int *out_height)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);

    int x, y, width, height;
    DP_UPixel8 *pixels;
    if (censored) {
        if (layer_content_crop_censored(lc, &x, &y, &width, &height)) {
            pixels =
                DP_layer_content_to_upixels8_censored(lc, x, y, width, height);
        }
        else {
            return NULL;
        }
    }
    else {
        if (layer_content_crop(lc, &x, &y, &width, &height)) {
            pixels = DP_layer_content_to_upixels8(lc, x, y, width, height);
        }
        else {
            return NULL;
        }
    }

    if (out_offset_x) {
        *out_offset_x = x;
    }
    if (out_offset_y) {
        *out_offset_y = y;
    }
    if (out_width) {
        *out_width = width;
    }
    if (out_height) {
        *out_height = height;
    }
    return pixels;
}

DP_Pixel8 *DP_layer_content_to_pixels8_cropped(DP_LayerContent *lc,
                                               int *out_offset_x,
                                               int *out_offset_y,
                                               int *out_width, int *out_height)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    int x, y, width, height;
    if (layer_content_crop(lc, &x, &y, &width, &height)) {
        if (out_offset_x) {
            *out_offset_x = x;
        }
        if (out_offset_y) {
            *out_offset_y = y;
        }
        if (out_width) {
            *out_width = width;
        }
        if (out_height) {
            *out_height = height;
        }
        return DP_layer_content_to_pixels8(lc, x, y, width, height);
    }
    else {
        return NULL;
    }
}

DP_UPixel8 *DP_layer_content_to_upixels8(DP_LayerContent *lc, int x, int y,
                                         int width, int height)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);

    DP_TileIterator ti = DP_tile_iterator_make(
        lc->width, lc->height, DP_rect_make(x, y, width, height));
    DP_UPixel8 *pixels = DP_malloc_zeroed(
        sizeof(*pixels) * DP_int_to_size(width) * DP_int_to_size(height));

    while (DP_tile_iterator_next(&ti)) {
        DP_Tile *t = DP_layer_content_tile_at_noinc(lc, ti.col, ti.row);
        if (t) {
            DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
            while (DP_tile_into_dst_iterator_next(&tidi)) {
                pixels[tidi.dst_y * width + tidi.dst_x] =
                    DP_upixel15_to_8(DP_pixel15_unpremultiply(
                        DP_tile_pixel_at(t, tidi.tile_x, tidi.tile_y)));
            }
        }
    }

    return pixels;
}

DP_UPixel8 *DP_layer_content_to_upixels8_censored(DP_LayerContent *lc, int x,
                                                  int y, int width, int height)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);

    DP_TileIterator ti = DP_tile_iterator_make(
        lc->width, lc->height, DP_rect_make(x, y, width, height));
    DP_UPixel8 *pixels = DP_malloc_zeroed(
        sizeof(*pixels) * DP_int_to_size(width) * DP_int_to_size(height));

    DP_Tile *censor_tile = DP_tile_censored_noinc();
    while (DP_tile_iterator_next(&ti)) {
        if (DP_layer_content_tile_at_noinc(lc, ti.col, ti.row)) {
            DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
            while (DP_tile_into_dst_iterator_next(&tidi)) {
                pixels[tidi.dst_y * width + tidi.dst_x] =
                    DP_upixel15_to_8(DP_pixel15_unpremultiply(DP_tile_pixel_at(
                        censor_tile, tidi.tile_x, tidi.tile_y)));
            }
        }
    }

    return pixels;
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

DP_Pixel8 *DP_layer_content_to_pixels8_mask(DP_LayerContent *lc, int x, int y,
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
                uint16_t a = DP_tile_pixel_at(t, tidi.tile_x, tidi.tile_y).a;
                if (a != 0) {
                    pixels[tidi.dst_y * width + tidi.dst_x] = (DP_Pixel8){
                        .b = 0,
                        .g = 0,
                        .r = 0,
                        .a = DP_channel15_to_8(a),
                    };
                }
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
                                  DP_Image *mask, uint16_t opacity)
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
                uint8_t mask_opacity =
                    mask_opacity_at(mask, tidi.dst_x, tidi.dst_y);
                if (mask_opacity != 0) {
                    DP_Pixel15 pixel =
                        DP_tile_pixel_at(t, tidi.tile_x, tidi.tile_y);
                    if (opacity != DP_BIT15 || mask_opacity != 255) {
                        uint16_t a = DP_fix15_mul(
                            opacity, DP_channel8_to_15(mask_opacity));
                        pixel.b = DP_fix15_mul(pixel.b, a);
                        pixel.g = DP_fix15_mul(pixel.g, a);
                        pixel.r = DP_fix15_mul(pixel.r, a);
                        pixel.a = DP_fix15_mul(pixel.a, a);
                    }
                    DP_image_pixel_at_set(img, tidi.dst_x, tidi.dst_y,
                                          DP_pixel15_to_8(pixel));
                }
            }
        }
    }

    return img;
}

static bool get_mask_tile(DP_LayerContent *mask, int i, DP_Tile **out_mt)
{
    if (mask) {
        DP_Tile *mt = mask->elements[i].tile;
        if (mt) {
            *out_mt = mt;
            return true;
        }
        else {
            *out_mt = NULL;
            return false;
        }
    }
    else {
        *out_mt = NULL;
        return true;
    }
}

static bool tile_needs_masking(DP_Tile *mt)
{
    return mt && !DP_tile_opaque_ident(mt);
}

static DP_Tile *flatten_tile(DP_LayerContent *lc, int tile_index,
                             DP_UPixel8 tint, bool include_sublayers)
{
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(lc->width, lc->height));
    DP_Tile *mt;
    if (get_mask_tile(lc->mask, tile_index, &mt)) {
        DP_Tile *t = lc->elements[tile_index].tile;
        DP_LayerList *ll = lc->sub.contents;
        if (!include_sublayers || DP_layer_list_count(ll) == 0) {
            if (t) {
                bool needs_masking = tile_needs_masking(mt);
                if (tint.a == 0 && !needs_masking) {
                    return DP_tile_incref(t);
                }
                else {
                    DP_TransientTile *tt =
                        needs_masking ? DP_transient_tile_new_masked(t, mt, 0)
                                      : DP_transient_tile_new(t, 0);
                    if (tint.a != 0) {
                        DP_transient_tile_tint(tt, tint);
                    }
                    return DP_transient_tile_persist(tt);
                }
            }
        }
        else if (t) {
            DP_TransientTile *tt = DP_transient_tile_new(t, 0);
            DP_ViewModeContext vmc = DP_view_mode_context_make_default();
            DP_layer_list_flatten_tile_to(ll, lc->sub.props, tile_index, tt,
                                          DP_BIT15, (DP_UPixel8){.color = 0},
                                          false, false, false, &vmc);
            if (tile_needs_masking(mt)) {
                DP_transient_tile_mask_in_place(tt, mt);
            }
            if (tint.a != 0) {
                DP_transient_tile_tint(tt, tint);
            }
            return DP_transient_tile_persist(tt);
        }
        else {
            DP_ViewModeContext vmc = DP_view_mode_context_make_default();
            DP_TransientTile *tt_or_null = DP_layer_list_flatten_tile_to(
                ll, lc->sub.props, tile_index, NULL, DP_BIT15,
                (DP_UPixel8){.color = 0}, false, false, false, &vmc);
            if (tt_or_null) {
                if (tile_needs_masking(mt)) {
                    DP_transient_tile_mask_in_place(tt_or_null, mt);
                }
                if (tint.a != 0) {
                    DP_transient_tile_tint(tt_or_null, tint);
                }
                return DP_transient_tile_persist(tt_or_null);
            }
        }
    }
    return NULL;
}

static DP_Tile *mask_censor_tile(DP_Tile *mt)
{
    if (mt) {
        DP_TransientTile *tt =
            DP_transient_tile_new_masked(DP_tile_censored_noinc(), mt, 0);
        return DP_transient_tile_persist(tt);
    }
    else {
        return DP_tile_censored_inc();
    }
}

static DP_Tile *flatten_censored_tile(DP_LayerContent *lc, int tile_index,
                                      bool include_sublayers)
{
    DP_ASSERT(tile_index >= 0);
    DP_ASSERT(tile_index < DP_tile_total_round(lc->width, lc->height));
    DP_Tile *mt;
    if (get_mask_tile(lc->mask, tile_index, &mt)) {
        DP_Tile *t = lc->elements[tile_index].tile;
        if (t) {
            return mask_censor_tile(mt);
        }
        else if (include_sublayers) {
            DP_LayerList *ll = lc->sub.contents;
            int sublayer_count = DP_layer_list_count(ll);
            for (int i = 0; i < sublayer_count; ++i) {
                DP_LayerContent *sub_lc = DP_layer_list_entry_content_noinc(
                    DP_layer_list_at_noinc(ll, i));
                if (sub_lc->elements[tile_index].tile) {
                    return mask_censor_tile(mt);
                }
            }
        }
    }
    return NULL;
}

DP_Tile *DP_layer_content_flatten_tile(DP_LayerContent *lc, int tile_index,
                                       bool censored, bool include_sublayers)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    return censored ? flatten_censored_tile(lc, tile_index, include_sublayers)
                    : flatten_tile(lc, tile_index, (DP_UPixel8){.color = 0},
                                   include_sublayers);
}

DP_TransientTile *
DP_layer_content_flatten_tile_to(DP_LayerContent *lc, int tile_index,
                                 DP_TransientTile *tt_or_null, uint16_t opacity,
                                 int blend_mode, DP_UPixel8 tint, bool censored,
                                 bool include_sublayers)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_Tile *t = censored
                   ? flatten_censored_tile(lc, tile_index, include_sublayers)
                   : flatten_tile(lc, tile_index, tint, include_sublayers);
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


static DP_TransientLayerContent *alloc_layer_content(int width, int height)
{
    size_t count = DP_int_to_size(DP_tile_total_round(width, height));
    DP_TransientLayerContent *tlc =
        DP_malloc(DP_FLEX_SIZEOF(DP_TransientLayerContent, elements, count));
    DP_atomic_set(&tlc->refcount, 1);
    tlc->transient = true;
    tlc->width = width;
    tlc->height = height;
    tlc->mask = NULL;
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
    if (DP_layer_content_has_content(lc)) {
        tlc = resize_layer_content(lc, context_id, top, left, width, height);
    }
    else {
        DP_debug("Resize: layer is blank");
        tlc = DP_transient_layer_content_new_init(width, height, NULL);
    }

    DP_LayerContent *mask = lc->mask;
    if (mask) {
        tlc->transient_mask =
            DP_layer_content_resize(mask, context_id, top, right, bottom, left);
    }

    DP_LayerList *sub_ll = lc->sub.contents;
    if (DP_layer_list_count(sub_ll) != 0) {
        DP_layer_list_decref(tlc->sub.contents);
        tlc->sub.transient_contents =
            DP_layer_list_resize(sub_ll, context_id, top, right, bottom, left);
        DP_layer_props_list_decref(tlc->sub.props);
        tlc->sub.props = DP_layer_props_list_incref(lc->sub.props);
    }

    return tlc;
}

bool DP_layer_content_has_sublayers(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    int count = DP_layer_list_count(lc->sub.contents);
    DP_ASSERT(count == DP_layer_props_list_count(lc->sub.props));
    return count != 0;
}

DP_LayerContent *DP_layer_content_merge_sublayers(DP_LayerContent *lc)
{
    DP_ASSERT(lc);
    DP_ASSERT(DP_atomic_get(&lc->refcount) > 0);
    DP_LayerList *ll = lc->sub.contents;
    int count = DP_layer_list_count(ll);
    if (count > 0) {
        DP_TransientLayerContent *tlc;
        if (DP_layer_content_transient(lc)) {
            tlc = DP_transient_layer_content_new_transient(
                (DP_TransientLayerContent *)lc);
        }
        else {
            tlc = DP_transient_layer_content_new(lc);
        }
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
    tlc->mask = DP_layer_content_incref_nullable(lc->mask);
    tlc->sub.contents = DP_layer_list_incref(lc->sub.contents);
    tlc->sub.props = DP_layer_props_list_incref(lc->sub.props);
    return tlc;
}

DP_TransientLayerContent *
DP_transient_layer_content_new_transient(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    int width = tlc->width;
    int height = tlc->height;
    DP_TransientLayerContent *new_tlc = alloc_layer_content(width, height);
    int count = DP_tile_total_round(width, height);
    for (int i = 0; i < count; ++i) {
        DP_Tile *t = tlc->elements[i].tile;
        if (t) {
            if (DP_tile_transient(t)) {
                new_tlc->elements[i].transient_tile =
                    DP_transient_tile_new_transient((DP_TransientTile *)t, 0);
            }
            else {
                new_tlc->elements[i].tile = DP_tile_incref(t);
            }
        }
        else {
            new_tlc->elements[i].tile = NULL;
        }
    }
    new_tlc->mask = DP_layer_content_incref_nullable(tlc->mask);
    new_tlc->sub.contents = DP_layer_list_incref(tlc->sub.contents);
    new_tlc->sub.props = DP_layer_props_list_incref(tlc->sub.props);
    return new_tlc;
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

static DP_LayerContent *persist_with(DP_TransientLayerContent *tlc, bool mask)
{
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
            else if (mask && DP_transient_tile_opaque(tt)) {
                DP_transient_tile_decref(tt);
                tlc->elements[i].tile = DP_tile_opaque_inc();
            }
            else {
                DP_transient_tile_persist(tt);
            }
        }
    }
    if (tlc->mask && DP_layer_content_transient(tlc->mask)) {
        persist_with(tlc->transient_mask, true);
    }
    if (DP_layer_list_transient(tlc->sub.contents)) {
        DP_transient_layer_list_persist(tlc->sub.transient_contents);
    }
    if (DP_layer_props_list_transient(tlc->sub.props)) {
        DP_transient_layer_props_list_persist(tlc->sub.transient_props);
    }
    return (DP_LayerContent *)tlc;
}

DP_LayerContent *
DP_transient_layer_content_persist(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return persist_with(tlc, false);
}

DP_LayerContent *
DP_transient_layer_content_persist_mask(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return persist_with(tlc, true);
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

DP_Tile *
DP_transient_layer_content_tile_at_index_noinc(DP_TransientLayerContent *tlc,
                                               int i)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return DP_layer_content_tile_at_index_noinc((DP_LayerContent *)tlc, i);
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

DP_LayerContent *
DP_transient_layer_content_mask_noinc_nullable(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return tlc->mask;
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

bool DP_transient_layer_content_has_content(DP_TransientLayerContent *tlc)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    return DP_layer_content_has_content((DP_LayerContent *)tlc);
}

bool DP_transient_layer_content_bounds(DP_TransientLayerContent *tlc,
                                       bool include_sublayers,
                                       DP_Rect *out_bounds)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(out_bounds);
    return DP_layer_content_bounds((DP_LayerContent *)tlc, include_sublayers,
                                   out_bounds);
}

void DP_transient_layer_content_mask_set_noinc_nullable(
    DP_TransientLayerContent *tlc, DP_LayerContent *mask_or_null)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_layer_content_decref_nullable(tlc->mask);
    tlc->mask = mask_or_null;
}

void DP_transient_layer_content_mask_set_inc_nullable(
    DP_TransientLayerContent *tlc, DP_LayerContent *mask_or_null)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_transient_layer_content_mask_set_noinc_nullable(
        tlc, DP_layer_content_incref_nullable(mask_or_null));
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

static DP_TransientTile *merge_tile(DP_TransientTile *DP_RESTRICT tt,
                                    DP_Tile *DP_RESTRICT t,
                                    DP_Tile *DP_RESTRICT mt,
                                    DP_TransientTile *DP_RESTRICT tmp_tt,
                                    uint16_t opacity, int blend_mode)
{
    if (tile_needs_masking(mt)) {
        if (tmp_tt) {
            DP_transient_tile_mask(tmp_tt, t, mt);
        }
        else {
            tmp_tt = DP_transient_tile_new_masked(t, mt, 0);
        }
        DP_transient_tile_merge(tt, (DP_Tile *)tmp_tt, opacity, blend_mode);
        return tmp_tt;
    }
    else {
        DP_transient_tile_merge(tt, t, opacity, blend_mode);
        return NULL;
    }
}

void DP_transient_layer_content_merge(DP_TransientLayerContent *tlc,
                                      unsigned int context_id,
                                      DP_LayerContent *lc, uint16_t opacity,
                                      int blend_mode, bool censored)
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
    DP_Tile *censor_tile = censored ? DP_tile_censored_noinc() : NULL;
    DP_LayerContent *mask = lc->mask;
    DP_TransientTile *tmp_tt = NULL;
    for (int i = 0; i < count; ++i) {
        DP_Tile *t = lc->elements[i].tile;
        DP_Tile *mt;
        if (t && get_mask_tile(mask, i, &mt)) {
            if (tlc->elements[i].tile) {
                DP_TransientTile *tt = get_transient_tile(tlc, context_id, i);
                DP_ASSERT((void *)tt != (void *)t);
                tmp_tt = merge_tile(tt, censored ? censor_tile : t, mt, tmp_tt,
                                    opacity, blend_mode);
            }
            else if (blend_blank) {
                // For blend modes like normal and behind, do regular blending.
                DP_TransientTile *tt =
                    create_transient_tile(tlc, context_id, i);
                tmp_tt = merge_tile(tt, censored ? censor_tile : t, mt, tmp_tt,
                                    opacity, blend_mode);
            }
            else {
                // For most other blend modes merging with transparent pixels
                // doesn't do anything. For example erasing nothing or multiply
                // with nothing just leads to more nothing. Skip the empty tile.
            }
        }
    }
    DP_transient_tile_decref_nullable(tmp_tt);
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

void DP_transient_layer_content_put_pixels(DP_TransientLayerContent *tlc,
                                           unsigned int context_id,
                                           int blend_mode, int left, int top,
                                           int width, int height,
                                           const DP_Pixel8 *pixels)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(pixels);

    int canvas_width = tlc->width;
    int wt = DP_tile_count_round(canvas_width);
    DP_TileIterator ti = DP_tile_iterator_make(
        canvas_width, tlc->height, DP_rect_make(left, top, width, height));

    while (DP_tile_iterator_next(&ti)) {
        int i = ti.row * wt + ti.col;
        if (tlc->elements[i].tile || DP_blend_mode_blend_blank(blend_mode)) {
            DP_TileIntoDstIterator tidi = DP_tile_into_dst_iterator_make(&ti);
            DP_TransientTile *tt = NULL;
            while (DP_tile_into_dst_iterator_next(&tidi)) {
                DP_Pixel8 pixel = pixels[tidi.dst_y * width + tidi.dst_x];
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

void DP_transient_layer_content_put_image(DP_TransientLayerContent *tlc,
                                          unsigned int context_id,
                                          int blend_mode, int left, int top,
                                          DP_Image *img)
{
    DP_ASSERT(tlc);
    DP_ASSERT(DP_atomic_get(&tlc->refcount) > 0);
    DP_ASSERT(tlc->transient);
    DP_ASSERT(img);
    DP_transient_layer_content_put_pixels(
        tlc, context_id, blend_mode, left, top, DP_image_width(img),
        DP_image_height(img), DP_image_pixels(img));
}


static bool can_blend_blank_pixel(int blend_mode, uint16_t opacity,
                                  DP_UPixel15 pixel)
{
    return can_blend_blank(blend_mode, opacity)
        && ((blend_mode != DP_BLEND_MODE_NORMAL_AND_ERASER
             && blend_mode != DP_BLEND_MODE_REPLACE)
            || pixel.a != 0);
}

static void fill_rect(DP_TransientLayerContent *tlc, unsigned int context_id,
                      int blend_mode, int left, int top, int right, int bottom,
                      DP_UPixel15 pixel)
{
    const uint16_t *mask = DP_tile_opaque_mask();
    uint16_t opacity = pixel.a;
    bool blend_blank = can_blend_blank_pixel(blend_mode, opacity, pixel);

    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    int width = tlc->width;
    if (right > width) {
        right = width;
    }
    int height = tlc->height;
    if (bottom > height) {
        bottom = height;
    }

    int tx_min = left / DP_TILE_SIZE;
    int tx_max = (right - 1) / DP_TILE_SIZE;
    int ty_min = top / DP_TILE_SIZE;
    int ty_max = (bottom - 1) / DP_TILE_SIZE;
    int xtiles = DP_tile_counts_round(width, height).x;

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
                                   const DP_BrushStamp *stamp, bool blend_blank,
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
    uint16_t opacity, int blend_mode, const DP_BrushStamp *stamp)
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
    int posterize_num, const DP_BrushStamp *stamp)
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
                                     DP_layer_props_blend_mode(lp), false);
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
                                         DP_layer_props_blend_mode(lp), false);
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
