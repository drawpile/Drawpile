/*
 * Copyright (C) 2022 - 2023 askmeaboutloom
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
 *
 * --------------------------------------------------------------------
 *
 * Parts of this code are based on Krita, using it under the GNU General
 * Public License, version 3. See 3rdparty/licenses/krita/COPYING.txt for
 * details.
 */
#include "flood_fill.h"
#include "canvas_state.h"
#include "image.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_routes.h"
#include "pixels.h"
#include "selection.h"
#include "tile.h"
#include "tile_iterator.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/queue.h>
#include <math.h>
#include <helpers.h> // M_PI


// Flood fill algorithm based on: Smith, Alvy Ray (1979). Tint Fill. SIGGRAPH
// '79: Proceedings of the 6th annual conference on Computer graphics and
// interactive techniques. pp. 276–283.
// See https://en.wikipedia.org/wiki/Flood_fill#Span_Filling

#define SOURCE_STATUS_MERGED  1
#define SOURCE_STATUS_DILATED 2
#define SOURCE_STATUS_ERODED  3

#define SOURCE_BLANK_CANVAS_MAX_LAYERS      3
#define SOURCE_BLANK_CANVAS_TOO_MANY_LAYERS (SOURCE_BLANK_CANVAS_MAX_LAYERS + 1)

typedef enum DP_FloodFillContextType {
    DP_FLOOD_FILL_SOURCE_BLANK,
    DP_FLOOD_FILL_SOURCE_LAYER_CONTENT,
    DP_FLOOD_FILL_SOURCE_LAYER_CONTENT_WITH_SUBLAYERS,
    DP_FLOOD_FILL_SOURCE_LAYER_GROUP,
    DP_FLOOD_FILL_SOURCE_LAYER_GROUP_WITH_SUBLAYERS,
    DP_FLOOD_FILL_SOURCE_MERGED,
} DP_FloodFillContextType;

typedef struct DP_FillContext {
    int width, height;
    DP_Rect area;
    int min_x, min_y, max_x, max_y;
    DP_Selection *sel;
    bool cancelled;
    DP_FloodFillShouldCancelFn should_cancel;
    void *user;
} DP_FillContext;

typedef struct DP_FloodFillContext {
    DP_FillContext parent;
    DP_FloodFillContextType type;
    int xtiles;
    int gap;
    float tolerance_squared;
    DP_UPixelFloat reference_color;
    union {
        DP_LayerContent *lc;
        struct {
            DP_LayerGroup *lg;
            DP_LayerProps *lp;
        };
        struct {
            DP_CanvasState *cs;
            DP_ViewModeBuffer vmb;
            DP_ViewModeFilter vmf;
            unsigned int flags;
        };
    };
    unsigned char *flood_map;
    unsigned char *dilate_map;
    unsigned char *erode_map;
    unsigned char *tile_status;
    unsigned char *output;
    DP_Queue queue;
} DP_FloodFillContext;

typedef struct DP_FillSeed {
    int x, y;
} DP_FillSeed;


static unsigned char *buffer_at(unsigned char *buffer, DP_Rect area, int x,
                                int y)
{
    DP_ASSERT(DP_rect_contains(area, x, y));
    int bx = x - area.x1;
    int by = y - area.y1;
    return &buffer[by * DP_rect_width(area) + bx];
}

static unsigned char buffer_get(const unsigned char *buffer, DP_Rect area,
                                int x, int y)
{
    return *buffer_at((unsigned char *)buffer, area, x, y);
}

static void buffer_set(unsigned char *buffer, DP_Rect area, int x, int y,
                       unsigned char value)
{
    *buffer_at(buffer, area, x, y) = value;
}

static bool is_cancelled(DP_FillContext *c)
{
    if (c->cancelled) {
        return true;
    }
    else if (c->should_cancel && c->should_cancel(c->user)) {
        c->cancelled = true;
        return true;
    }
    else {
        return false;
    }
}


static bool source_is_merged(DP_FloodFillContext *c, int tile_index)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    return c->tile_status[tile_index] >= SOURCE_STATUS_MERGED;
}

static bool source_is_dilated(DP_FloodFillContext *c, int tile_index)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    return c->tile_status[tile_index] >= SOURCE_STATUS_DILATED;
}

static bool source_is_eroded(DP_FloodFillContext *c, int tile_index)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    return c->tile_status[tile_index] >= SOURCE_STATUS_ERODED;
}

static void source_set_merged(DP_FloodFillContext *c, int tile_index)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    DP_ASSERT(c->parent.cancelled || c->tile_status[tile_index] == 0);
    c->tile_status[tile_index] = SOURCE_STATUS_MERGED;
}

static void source_set_dilated(DP_FloodFillContext *c, int tile_index)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    DP_ASSERT(c->parent.cancelled
              || c->tile_status[tile_index] == SOURCE_STATUS_MERGED);
    c->tile_status[tile_index] = SOURCE_STATUS_DILATED;
}

static void source_set_eroded(DP_FloodFillContext *c, int tile_index)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    DP_ASSERT(c->parent.cancelled
              || c->tile_status[tile_index] == SOURCE_STATUS_DILATED);
    c->tile_status[tile_index] = SOURCE_STATUS_ERODED;
}

static DP_Tile *source_merge_tile(DP_FloodFillContext *c, int tile_index)
{
    switch (c->type) {
    case DP_FLOOD_FILL_SOURCE_BLANK:
        break;
    case DP_FLOOD_FILL_SOURCE_LAYER_CONTENT:
        return DP_tile_incref_nullable(
            DP_layer_content_tile_at_index_noinc(c->lc, tile_index));
    case DP_FLOOD_FILL_SOURCE_LAYER_CONTENT_WITH_SUBLAYERS:
        return DP_layer_content_flatten_tile(c->lc, tile_index, false, true);
    case DP_FLOOD_FILL_SOURCE_LAYER_GROUP:
        return (DP_Tile *)DP_layer_group_flatten_tile(c->lg, c->lp, tile_index,
                                                      false);
    case DP_FLOOD_FILL_SOURCE_LAYER_GROUP_WITH_SUBLAYERS:
        return (DP_Tile *)DP_layer_group_flatten_tile(c->lg, c->lp, tile_index,
                                                      true);
    case DP_FLOOD_FILL_SOURCE_MERGED:
        return (DP_Tile *)DP_canvas_state_flatten_tile(c->cs, tile_index,
                                                       c->flags, &c->vmf);
    }
    DP_UNREACHABLE();
}

static DP_UPixelFloat source_to_color(DP_Pixel15 pixel)
{
    return DP_upixel15_to_float_round8(DP_pixel15_unpremultiply(pixel));
}

static bool source_should_flood(DP_Pixel15 pixel,
                                DP_UPixelFloat reference_color,
                                float tolerance_squared)
{
    // TODO: we could use better functions for color distance than this.
    // Guess if we're supposed to fill a transparent-ish pixel.
    if (reference_color.a < 0.05f) {
        float a = DP_channel15_to_float_round8(pixel.a);
        return a * a <= tolerance_squared;
    }
    else {
        DP_UPixelFloat color = source_to_color(pixel);
        float b = color.b - reference_color.b;
        float g = color.g - reference_color.g;
        float r = color.r - reference_color.r;
        float a = color.a - reference_color.a;
        return b * b + g * g + r * r + a * a <= tolerance_squared;
    }
}

static void source_tile_bounds(DP_FloodFillContext *c, int xt, int yt,
                               int *out_canvas_left, int *out_canvas_top,
                               int *out_buffer_left, int *out_buffer_top,
                               int *out_buffer_right, int *out_buffer_bottom)
{
    int canvas_left = xt * DP_TILE_SIZE;
    if (out_canvas_left) {
        *out_canvas_left = canvas_left;
    }

    int canvas_top = yt * DP_TILE_SIZE;
    if (out_canvas_top) {
        *out_canvas_top = canvas_top;
    }

    DP_Rect area = c->parent.area;
    *out_buffer_left = DP_max_int(canvas_left, area.x1);
    *out_buffer_top = DP_max_int(canvas_top, area.y1);
    *out_buffer_right = DP_min_int(canvas_left + DP_TILE_SIZE - 1, area.x2);
    *out_buffer_bottom = DP_min_int(canvas_top + DP_TILE_SIZE - 1, area.y2);
}

static void source_flood_dec(DP_FloodFillContext *c, int xt, int yt, DP_Tile *t)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    int canvas_left, canvas_top, buffer_left, buffer_top, buffer_right,
        buffer_bottom;
    source_tile_bounds(c, xt, yt, &canvas_left, &canvas_top, &buffer_left,
                       &buffer_top, &buffer_right, &buffer_bottom);
    DP_UPixelFloat reference_color = c->reference_color;
    float tolerance_squared = c->tolerance_squared;
    for (int y = buffer_top; y <= buffer_bottom; ++y) {
        for (int x = buffer_left; x <= buffer_right; ++x) {
            if (source_should_flood(
                    DP_tile_pixel_at(t, x - canvas_left, y - canvas_top),
                    reference_color, tolerance_squared)) {
                buffer_set(c->flood_map, c->parent.area, x, y, 1);
            }
        }
    }
    DP_tile_decref(t);
}

static void source_flood_null(DP_FloodFillContext *c, int xt, int yt)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    DP_UPixelFloat reference_color = c->reference_color;
    float tolerance_squared = c->tolerance_squared;
    if (source_should_flood(DP_pixel15_zero(), reference_color,
                            tolerance_squared)) {
        int buffer_left, buffer_top, buffer_right, buffer_bottom;
        source_tile_bounds(c, xt, yt, NULL, NULL, &buffer_left, &buffer_top,
                           &buffer_right, &buffer_bottom);
        size_t stride = DP_int_to_size(buffer_right - buffer_left + 1);
        for (int y = buffer_top; y <= buffer_bottom; ++y) {
            memset(buffer_at(c->flood_map, c->parent.area, buffer_left, y), 1,
                   stride);
        }
    }
}

static void source_flood_nullable_dec(DP_FloodFillContext *c, int xt, int yt,
                                      DP_Tile *t_or_null)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    if (t_or_null) {
        source_flood_dec(c, xt, yt, t_or_null);
    }
    else {
        source_flood_null(c, xt, yt);
    }
}

static void source_init_at(DP_FloodFillContext *c, int x, int y)
{
    DP_ASSERT(DP_rect_contains(c->parent.area, x, y));
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int tile_index = yt * c->xtiles + xt;
    DP_Tile *t = source_merge_tile(c, tile_index);
    source_set_merged(c, tile_index);
    c->reference_color =
        t ? source_to_color(DP_tile_pixel_at(t, x - xt * DP_TILE_SIZE,
                                             y - yt * DP_TILE_SIZE))
          : DP_upixel_float_zero();
    source_flood_nullable_dec(c, xt, yt, t);
}

static unsigned int source_flags_make(bool include_background,
                                      bool include_sublayers)
{
    static_assert(DP_FLAT_IMAGE_RENDER_FLAGS & DP_FLAT_IMAGE_INCLUDE_BACKGROUND,
                  "render flags include background (and we may exclude it)");
    static_assert(DP_FLAT_IMAGE_RENDER_FLAGS & DP_FLAT_IMAGE_INCLUDE_SUBLAYERS,
                  "render flags include sublayers (and we may exclude them)");
    unsigned int flags = DP_FLAT_IMAGE_RENDER_FLAGS;
    if (!include_background) {
        flags &= ~DP_FLAT_IMAGE_INCLUDE_BACKGROUND;
    }
    if (!include_sublayers) {
        flags &= ~DP_FLAT_IMAGE_INCLUDE_SUBLAYERS;
    }
    return flags;
}

static int source_canvas_look_blank_collect_layers(DP_LayerList *ll,
                                                   bool include_sublayers,
                                                   DP_LayerContent **out_lcs,
                                                   int fill)
{
    int count = DP_layer_list_count(ll);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            fill = source_canvas_look_blank_collect_layers(
                DP_layer_group_children_noinc(lg), include_sublayers, out_lcs,
                fill);
            if (fill > SOURCE_BLANK_CANVAS_MAX_LAYERS) {
                return SOURCE_BLANK_CANVAS_TOO_MANY_LAYERS;
            }
        }
        else {
            DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
            if (fill >= SOURCE_BLANK_CANVAS_MAX_LAYERS
                || (include_sublayers && DP_layer_content_has_sublayers(lc))) {
                return SOURCE_BLANK_CANVAS_TOO_MANY_LAYERS;
            }
            else {
                out_lcs[fill] = DP_layer_list_entry_content_noinc(lle);
                ++fill;
            }
        }
    }
    return fill;
}

static bool source_layers_blank(DP_FloodFillContext *c, int canvas_width,
                                int canvas_height, DP_TileCounts tc,
                                DP_LayerContent **lcs, int layer_count)
{
    if (layer_count > 0) {
        DP_Rect area = c->parent.area;
        if (area.x1 == 0 && area.y1 == 0 && area.x2 == canvas_width - 1
            && area.y2 == canvas_height - 1) {
            int tile_total = tc.x * tc.y;
            for (int i = 0; i < tile_total; ++i) {
                for (int j = 0; j < layer_count; ++j) {
                    if (DP_layer_content_tile_at_index_noinc(lcs[j], i)) {
                        return false;
                    }
                }
            }
        }
        else {
            DP_TileIterator ti =
                DP_tile_iterator_make(canvas_width, canvas_height, area);
            while (DP_tile_iterator_next(&ti)) {
                for (int i = 0; i < layer_count; ++i) {
                    if (DP_layer_content_tile_at_noinc(lcs[i], ti.col,
                                                       ti.row)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static bool source_canvas_looks_blank(DP_FloodFillContext *c,
                                      bool include_sublayers, int canvas_width,
                                      int canvas_height, DP_TileCounts tc,
                                      DP_CanvasState *cs)
{
    // We don't want to waste time checking too many layers for blankness here,
    // so we'll limit it to a small amount under the assumption that a canvas
    // that has any significant amount of layers isn't blank. We could also make
    // this fancier and check if the layers are visible and such, but that's
    // probably too elaborate for what's supposed to be just an optimization for
    // the user misappropriating flood filling to fill an entire area.
    DP_LayerContent *lcs[SOURCE_BLANK_CANVAS_MAX_LAYERS];
    int layer_count = source_canvas_look_blank_collect_layers(
        DP_canvas_state_layers_noinc(cs), include_sublayers, lcs, 0);
    return layer_count <= SOURCE_BLANK_CANVAS_MAX_LAYERS
        && source_layers_blank(c, canvas_width, canvas_height, tc, lcs,
                               layer_count);
}

static size_t source_map_size(DP_FloodFillContext *c)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    DP_Rect area = c->parent.area;
    return DP_int_to_size(DP_rect_width(area))
         * DP_int_to_size(DP_rect_height(area));
}

static bool source_init(DP_FloodFillContext *c, DP_CanvasState *cs,
                        int layer_id, bool include_sublayers,
                        DP_ViewMode view_mode, int active_layer_id,
                        int active_frame_index, double tolerance, int gap,
                        int x, int y)
{
    DP_ASSERT(gap >= 0);
    int canvas_width = DP_canvas_state_width(cs);
    int canvas_height = DP_canvas_state_height(cs);
    DP_TileCounts tc = DP_tile_counts_round(canvas_width, canvas_height);
    if (layer_id <= 0) {
        if (source_canvas_looks_blank(c, include_sublayers, canvas_width,
                                      canvas_height, tc, cs)) {
            c->type = DP_FLOOD_FILL_SOURCE_BLANK;
            return true;
        }
        else {
            c->type = DP_FLOOD_FILL_SOURCE_MERGED;
            c->cs = cs;
            DP_view_mode_buffer_init(&c->vmb);
            c->vmf = DP_view_mode_filter_make(&c->vmb, view_mode, cs,
                                              active_layer_id,
                                              active_frame_index, NULL);
            c->flags = source_flags_make(layer_id == 0, include_sublayers);
        }
    }
    else {
        DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
        DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
        if (!lre) {
            DP_error_set("Flood fill: layer %d not found", layer_id);
            return false;
        }
        else if (DP_layer_routes_entry_is_group(lre)) {
            c->type = include_sublayers
                        ? DP_FLOOD_FILL_SOURCE_LAYER_GROUP_WITH_SUBLAYERS
                        : DP_FLOOD_FILL_SOURCE_LAYER_GROUP;
            c->lg = DP_layer_routes_entry_group(lre, cs);
            c->lp = DP_layer_routes_entry_props(lre, cs);
        }
        else {
            DP_LayerContent *lc = DP_layer_routes_entry_content(lre, cs);
            if (include_sublayers && DP_layer_content_has_sublayers(lc) != 0) {
                c->type = DP_FLOOD_FILL_SOURCE_LAYER_CONTENT_WITH_SUBLAYERS;
                c->lc = lc;
            }
            else if (source_layers_blank(c, canvas_width, canvas_height, tc,
                                         &lc, 1)) {
                c->type = DP_FLOOD_FILL_SOURCE_BLANK;
                return true;
            }
            else {
                c->type = DP_FLOOD_FILL_SOURCE_LAYER_CONTENT;
                c->lc = lc;
            }
        }
    }
    c->xtiles = tc.x;
    c->gap = gap;
    c->tolerance_squared = DP_double_to_float(tolerance * tolerance);
    size_t map_size = source_map_size(c);
    c->flood_map = DP_malloc_zeroed(map_size);
    c->dilate_map = gap == 0 ? NULL : DP_malloc_zeroed(map_size);
    c->erode_map = gap == 0 ? NULL : DP_malloc_zeroed(map_size);
    c->tile_status =
        DP_malloc_zeroed(DP_int_to_size(tc.x) * DP_int_to_size(tc.y));
    source_init_at(c, x, y);
    return true;
}

static void source_move_flood_map_to_output(DP_FloodFillContext *c)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    DP_ASSERT(c->flood_map);
    DP_ASSERT(!c->output);
    c->output = c->flood_map;
    c->flood_map = NULL;
}

static void source_dispose(DP_FloodFillContext *c)
{
    switch (c->type) {
    case DP_FLOOD_FILL_SOURCE_BLANK:
        return;
    case DP_FLOOD_FILL_SOURCE_MERGED:
        DP_view_mode_buffer_dispose(&c->vmb);
        DP_FALLTHROUGH();
    case DP_FLOOD_FILL_SOURCE_LAYER_CONTENT_WITH_SUBLAYERS:
    case DP_FLOOD_FILL_SOURCE_LAYER_GROUP:
    case DP_FLOOD_FILL_SOURCE_LAYER_GROUP_WITH_SUBLAYERS:
    case DP_FLOOD_FILL_SOURCE_LAYER_CONTENT:
        DP_free(c->tile_status);
        DP_free(c->erode_map);
        DP_free(c->dilate_map);
        DP_free(c->flood_map);
        return;
    }
    DP_UNREACHABLE();
}

static unsigned char source_flood_map_at(DP_FloodFillContext *c, int x, int y)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int tile_index = yt * c->xtiles + xt;
    if (!source_is_merged(c, tile_index)) {
        DP_Tile *t = source_merge_tile(c, tile_index);
        source_set_merged(c, tile_index);
        source_flood_nullable_dec(c, xt, yt, t);
    }
    return buffer_get(c->flood_map, c->parent.area, x, y);
}

static void source_dilate_pixel(DP_FloodFillContext *c, int x0, int y0)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    int gap = c->gap;
    DP_Rect area = c->parent.area;
    int x1 = DP_max_int(area.x1, x0 - gap);
    int y1 = DP_max_int(area.y1, y0 - gap);
    int x2 = DP_min_int(area.x2, x0 + gap);
    int y2 = DP_min_int(area.y2, y0 + gap);
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            if (source_flood_map_at(c, x, y) == 0) {
                buffer_set(c->dilate_map, area, x0, y0, 1);
                return;
            }
        }
    }
}

static void source_dilate_tile(DP_FloodFillContext *c, int xt, int yt)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    int buffer_left, buffer_top, buffer_right, buffer_bottom;
    source_tile_bounds(c, xt, yt, NULL, NULL, &buffer_left, &buffer_top,
                       &buffer_right, &buffer_bottom);
    for (int y = buffer_top; y <= buffer_bottom; ++y) {
        for (int x = buffer_left; x <= buffer_right; ++x) {
            if (is_cancelled(&c->parent)) {
                return;
            }
            source_dilate_pixel(c, x, y);
        }
    }
}

static unsigned char source_dilate_map_at(DP_FloodFillContext *c, int x, int y)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int tile_index = yt * c->xtiles + xt;
    if (!source_is_dilated(c, tile_index)) {
        source_dilate_tile(c, xt, yt);
        source_set_dilated(c, tile_index);
    }
    return buffer_get(c->dilate_map, c->parent.area, x, y);
}

static void source_erode_pixel(DP_FloodFillContext *c, int x0, int y0)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    int gap = c->gap;
    DP_Rect area = c->parent.area;
    int x1 = DP_max_int(area.x1, x0 - gap);
    int y1 = DP_max_int(area.y1, y0 - gap);
    int x2 = DP_min_int(area.x2, x0 + gap);
    int y2 = DP_min_int(area.y2, y0 + gap);
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            if (source_dilate_map_at(c, x, y) == 0) {
                buffer_set(c->erode_map, area, x0, y0, 1);
                return;
            }
        }
    }
}

static void source_erode_tile(DP_FloodFillContext *c, int xt, int yt)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    int buffer_left, buffer_top, buffer_right, buffer_bottom;
    source_tile_bounds(c, xt, yt, NULL, NULL, &buffer_left, &buffer_top,
                       &buffer_right, &buffer_bottom);
    for (int y = buffer_top; y <= buffer_bottom; ++y) {
        for (int x = buffer_left; x <= buffer_right; ++x) {
            if (is_cancelled(&c->parent)) {
                return;
            }
            source_erode_pixel(c, x, y);
        }
    }
}

static unsigned char source_erode_map_at(DP_FloodFillContext *c, int x, int y)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    int xt = x / DP_TILE_SIZE;
    int yt = y / DP_TILE_SIZE;
    int tile_index = yt * c->xtiles + xt;
    if (!source_is_eroded(c, tile_index)) {
        source_erode_tile(c, xt, yt);
        source_set_eroded(c, tile_index);
    }
    return buffer_get(c->erode_map, c->parent.area, x, y);
}

static unsigned char source_at(DP_FloodFillContext *c, int x, int y)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    return c->gap == 0 ? source_flood_map_at(c, x, y)
                       : source_erode_map_at(c, x, y);
}

static void source_flood_all(DP_FloodFillContext *c)
{
    DP_ASSERT(c->type != DP_FLOOD_FILL_SOURCE_BLANK);
    DP_ASSERT(c->gap == 0);
    DP_Rect area = c->parent.area;
    int tile_left = area.x1 / DP_TILE_SIZE;
    int tile_top = area.y1 / DP_TILE_SIZE;
    int tile_right = area.x2 / DP_TILE_SIZE;
    int tile_bottom = area.y2 / DP_TILE_SIZE;
    int xtiles = c->xtiles;
    for (int yt = tile_top; yt <= tile_bottom; ++yt) {
        for (int xt = tile_left; xt <= tile_right; ++xt) {
            if (is_cancelled(&c->parent)) {
                return;
            }
            int tile_index = yt * xtiles + xt;
            if (!source_is_merged(c, tile_index)) {
                DP_Tile *t = source_merge_tile(c, tile_index);
                source_set_merged(c, tile_index);
                source_flood_nullable_dec(c, xt, yt, t);
            }
        }
    }
}


static void init_selection(DP_FillContext *c, DP_CanvasState *cs,
                           unsigned int context_id, int selection_id)
{
    if (selection_id > 0) {
        DP_Selection *sel = DP_canvas_state_selection_search_noinc(
            cs, context_id, selection_id);
        if (sel) {
            DP_Rect sel_bounds = *DP_selection_bounds(sel);
            if (!DP_rect_empty(sel_bounds)) {
                c->sel = sel;
                c->area = DP_rect_intersection(c->area, sel_bounds);
            }
        }
    }
}

static void add_seed(DP_Queue *s, int x, int y)
{
    DP_FillSeed *fs = DP_queue_push(s, sizeof(DP_FillSeed));
    (*fs) = (DP_FillSeed){x, y};
}

static void shift_seed(DP_Queue *s, int *out_x, int *out_y)
{
    DP_FillSeed *fs = DP_queue_peek(s, sizeof(DP_FillSeed));
    *out_x = fs->x;
    *out_y = fs->y;
    DP_queue_shift(s);
}

static bool inside(DP_FloodFillContext *c, int x, int y)
{
    DP_Rect area = c->parent.area;
    return DP_rect_contains(area, x, y)
        && buffer_get(c->output, area, x, y) == 0 && source_at(c, x, y) != 0;
}

static void update_bounds(DP_FillContext *c, int x, int y)
{
    if (x < c->min_x) {
        c->min_x = x;
    }
    if (x > c->max_x) {
        c->max_x = x;
    }
    if (y < c->min_y) {
        c->min_y = y;
    }
    if (y > c->max_y) {
        c->max_y = y;
    }
}

static void set_pixel(DP_FloodFillContext *c, int x, int y)
{
    buffer_set(c->output, c->parent.area, x, y, 1);
    update_bounds(&c->parent, x, y);
}

static void scan(DP_FloodFillContext *c, DP_Queue *s, int lx, int rx, int y)
{
    bool added = false;
    for (int x = lx; x <= rx; ++x) {
        if (!inside(c, x, y)) {
            added = false;
        }
        else if (!added) {
            add_seed(s, x, y);
            added = true;
        }
    }
}

static void flood_fill(DP_FloodFillContext *c, int x0, int y0)
{
    DP_Queue *s = &c->queue;
    add_seed(s, x0, y0);
    while (s->used != 0 && !is_cancelled(&c->parent)) {
        int x, y;
        shift_seed(s, &x, &y);
        int lx = x;
        while (inside(c, lx - 1, y)) {
            set_pixel(c, lx - 1, y);
            lx = lx - 1;
        }
        while (inside(c, x, y)) {
            set_pixel(c, x, y);
            x = x + 1;
        }
        scan(c, s, lx, x - 1, y + 1);
        scan(c, s, lx, x - 1, y - 1);
    }
}

static void tighten_output_bounds(DP_FloodFillContext *c)
{
    DP_Rect area = c->parent.area;
    for (int y = area.y1; y <= area.y2; ++y) {
        for (int x = area.x1; x <= area.x2; ++x) {
            if (buffer_get(c->output, area, x, y)) {
                update_bounds(&c->parent, x, y);
            }
        }
    }
}

static int get_kernel_diameter(int radius)
{
    return radius * 2 + 1;
}

static unsigned char *generate_expansion_kernel(DP_FloodFillKernel kernel_shape,
                                                int expand)
{
    if (kernel_shape == DP_FLOOD_FILL_KERNEL_SQUARE) {
        return NULL;
    }
    else {
        int diameter = get_kernel_diameter(expand);
        size_t kernel_size = DP_square_size(DP_int_to_size(diameter));
        unsigned char *kernel = DP_malloc(kernel_size);
        int rr = DP_square_int(expand);
        for (int y = 0; y < diameter; ++y) {
            for (int x = 0; x < diameter; ++x) {
                kernel[y * diameter + x] =
                    DP_square_int(x - expand) + DP_square_int(y - expand) <= rr;
            }
        }
        return kernel;
    }
}

static void apply_expansion_kernel(float *mask, int mask_width, float value,
                                   const unsigned char *kernel, int width,
                                   int height, int expand, int feather_radius,
                                   int expand_min_x, int expand_min_y, int x0,
                                   int y0)
{
    int start_x0 = x0 - expand;
    int start_y0 = y0 - expand;
    int start_x = DP_max_int(start_x0, 0);
    int start_y = DP_max_int(start_y0, 0);
    int end_x = DP_min_int(x0 + expand, width - 1);
    int end_y = DP_min_int(y0 + expand, height - 1);
    int diameter = kernel ? get_kernel_diameter(expand) : 0;
    for (int y = start_y; y <= end_y; ++y) {
        for (int x = start_x; x <= end_x; ++x) {
            if (!kernel || kernel[(y - start_y0) * diameter + (x - start_x0)]) {
                int mx = x - expand_min_x + feather_radius;
                int my = y - expand_min_y + feather_radius;
                int index = my * mask_width + mx;
                if (value == 1.0f || mask[index] < value) {
                    mask[index] = value;
                }
            }
        }
    }
}

static float erode_mask_pixel(const float *in_mask, int mask_width,
                              const unsigned char *kernel, int shrink,
                              int diameter, int x0, int y0)
{
    int start_x = x0 - shrink;
    int start_y = y0 - shrink;
    int end_x = x0 + shrink;
    int end_y = y0 + shrink;
    float value = 1.0f;
    for (int y = start_y; y <= end_y; ++y) {
        for (int x = start_x; x <= end_x; ++x) {
            if (!kernel || kernel[(y - start_y) * diameter + (x - start_x)]) {
                value = DP_min_float(value, in_mask[y * mask_width + x]);
                if (value == 0.0f) {
                    return 0.0f;
                }
            }
        }
    }
    return value;
}

static void erode_mask(DP_FillContext *c, const float *in_mask, int in_width,
                       int in_height, float *out_mask, int out_width,
                       int feather_radius, int shrink,
                       const unsigned char *kernel)
{
    int diameter = kernel ? get_kernel_diameter(shrink) : 0;
    for (int y = shrink; y < in_height - shrink && !is_cancelled(c); ++y) {
        int out_y = y - shrink + feather_radius;
        for (int x = shrink; x < in_width - shrink; ++x) {
            int out_x = x - shrink + feather_radius;
            out_mask[out_y * out_width + out_x] = erode_mask_pixel(
                in_mask, in_width, kernel, shrink, diameter, x, y);
        }
    }
}

static bool expand_left_edge(int expand_min_x, int feather_radius, float *mask,
                             int img_width, int img_height)
{
    int left = expand_min_x - feather_radius;
    if (left < 0) {
        for (int y = feather_radius; y < img_height - feather_radius; ++y) {
            float edge = mask[y * img_width - left];
            if (edge != 0.0f) {
                for (int x = left; x < 0; ++x) {
                    mask[y * img_width + x - left] = edge;
                }
            }
        }
        return true;
    }
    else {
        return false;
    }
}

static bool expand_right_edge(int canvas_width, int expand_max_x,
                              int feather_radius, float *mask, int img_width,
                              int img_height)
{
    int right = expand_max_x + feather_radius;
    int canvas_right = canvas_width - 1;
    if (right > canvas_right) {
        int r = img_width - (right - canvas_right) - 1;
        for (int y = feather_radius; y < img_height - feather_radius; ++y) {
            float edge = mask[y * img_width + r];
            if (edge != 0.0f) {
                for (int x = right; x > canvas_right; --x) {
                    mask[y * img_width + img_width - (x - canvas_right)] = edge;
                }
            }
        }
        return true;
    }
    else {
        return false;
    }
}

static void expand_top_edge(int expand_min_y, int feather_radius, float *mask,
                            int img_width, int x_start, int width)
{
    int top = expand_min_y - feather_radius;
    if (top < 0) {
        for (int x = x_start; x < width; ++x) {
            float edge = mask[-top * img_width + x];
            if (edge != 0.0f) {
                for (int y = top; y < 0; ++y) {
                    mask[(y - top) * img_width + x] = edge;
                }
            }
        }
    }
}

static void expand_bottom_edge(int canvas_height, int expand_max_y,
                               int feather_radius, float *mask, int img_width,
                               int img_height, int x_start, int width)
{
    int bottom = expand_max_y + feather_radius;
    int canvas_bottom = canvas_height - 1;
    if (bottom > canvas_bottom) {
        int b = img_height - (bottom - canvas_bottom) - 1;
        for (int x = x_start; x < width; ++x) {
            float edge = mask[b * img_width + x];
            if (edge != 0.0f) {
                for (int y = bottom; y > canvas_bottom; --y) {
                    mask[(img_height - (y - canvas_bottom)) * img_width + x] =
                        edge;
                }
            }
        }
    }
}

static void expand_edges(int canvas_width, int canvas_height, int expand_min_x,
                         int expand_min_y, int expand_max_x, int expand_max_y,
                         int feather_radius, float *mask, int img_width,
                         int img_height)
{
    bool left_expanded = expand_left_edge(expand_min_x, feather_radius, mask,
                                          img_width, img_height);
    bool right_expanded =
        expand_right_edge(canvas_width, expand_max_x, feather_radius, mask,
                          img_width, img_height);

    int x_start = left_expanded ? 0 : feather_radius;
    int width = img_width - (right_expanded ? 0 : feather_radius);
    expand_top_edge(expand_min_y, feather_radius, mask, img_width, x_start,
                    width);
    expand_bottom_edge(canvas_height, expand_max_y, feather_radius, mask,
                       img_width, img_height, x_start, width);
}

static float *generate_gaussian_kernel(int radius)
{
    // Based on Krita's Gaussian kernel generation, see license above.
    int diameter = get_kernel_diameter(radius);
    float *kernel = DP_malloc(DP_int_to_size(diameter) * sizeof(*kernel));
    float r = DP_int_to_float(radius);
    float r_squared = r * r;
    float multiplicand = 1.0f / (2.0f * (float)M_PI * r_squared);
    float exponent_multiplicand = 1.0f / (2.0f * r_squared);
    float total = 0.0f;

    for (int x = 0; x < diameter; ++x) {
        int distance = abs(radius - x);
        float d_squared = DP_int_to_float(distance * distance);
        float k = multiplicand
                * expf(-(d_squared + r_squared) * exponent_multiplicand);
        kernel[x] = k;
        total += k;
    }

    for (int i = 0; i < diameter; ++i) {
        kernel[i] /= total;
    }

    return kernel;
}

static void blur_horizontally(float *dst, const float *src, int x0, int y0,
                              int width, const float *kernel, int radius)
{
    int left = DP_max_int(x0 - radius, 0);
    int right = DP_min_int(x0 + radius, width - 1);
    float result = 0.0f;
    for (int x = left; x <= right; ++x) {
        result += src[y0 * width + x] * kernel[x - x0 + radius];
    }
    dst[y0 * width + x0] = result;
}

static void blur_vertically(float *dst, const float *src, int x0, int y0,
                            int width, int height, const float *kernel,
                            int radius)
{
    int top = DP_max_int(y0 - radius, 0);
    int bottom = DP_min_int(y0 + radius, height - 1);
    float result = 0.0f;
    for (int y = top; y <= bottom; ++y) {
        result += src[y * width + x0] * kernel[y - y0 + radius];
    }
    dst[y0 * width + x0] = result;
}

static void feather_mask(DP_FillContext *c, float *mask, float *tmp, int width,
                         int height, int radius)
{
    // This is a classic two-pass gaussian blur. We create a one-dimensional
    // gaussian kernel, then blur once horizontally to a temporary buffer and
    // then vertically back into the original image.
    float *kernel = generate_gaussian_kernel(radius);
    for (int y = 0; y < height; ++y) {
        if (is_cancelled(c)) {
            DP_free(kernel);
            return;
        }
        for (int x = 0; x < width; ++x) {
            blur_horizontally(tmp, mask, x, y, width, kernel, radius);
        }
    }
    for (int y = 0; y < height; ++y) {
        if (is_cancelled(c)) {
            DP_free(kernel);
            return;
        }
        for (int x = 0; x < width; ++x) {
            blur_vertically(mask, tmp, x, y, width, height, kernel, radius);
        }
    }
    DP_free(kernel);
}

static float *make_mask(DP_FillContext *c,
                        float (*get_output)(void *, int, int), int expand,
                        DP_FloodFillKernel kernel_shape, int feather_radius,
                        bool from_edge, int *out_img_x, int *out_img_y,
                        int *out_img_width, int *out_img_height)
{
    int min_x = c->min_x, min_y = c->min_y;
    int max_x = c->max_x, max_y = c->max_y;
    int positive_expand = DP_max_int(expand, 0);
    int expand_min_x = DP_max_int(min_x - positive_expand, 0);
    int expand_min_y = DP_max_int(min_y - positive_expand, 0);
    int expand_max_x = DP_min_int(max_x + positive_expand, c->width - 1);
    int expand_max_y = DP_min_int(max_y + positive_expand, c->height - 1);
    int img_width = expand_max_x - expand_min_x + feather_radius * 2 + 1;
    int img_height = expand_max_y - expand_min_y + feather_radius * 2 + 1;
    size_t mask_size = DP_int_to_size(img_width) * DP_int_to_size(img_height);
    size_t mask_bytes = mask_size * sizeof(float);
    float *mask = DP_malloc_zeroed(mask_bytes);
    *out_img_x = expand_min_x - feather_radius;
    *out_img_y = expand_min_y - feather_radius;
    *out_img_width = img_width;
    *out_img_height = img_height;

    if (expand == 0) {
        for (int y = min_y; y <= max_y; ++y) {
            if (is_cancelled(c)) {
                return mask;
            }
            for (int x = min_x; x <= max_x; ++x) {
                float value = get_output ? get_output(c, x, y) : 1.0f;
                if (value > 0.0f) {
                    int mx = x - min_x + feather_radius;
                    int my = y - min_y + feather_radius;
                    mask[my * img_width + mx] = value;
                }
            }
        }
    }
    else if (expand > 0) {
        unsigned char *kernel = generate_expansion_kernel(kernel_shape, expand);
        for (int y = min_y; y <= max_y; ++y) {
            if (is_cancelled(c)) {
                DP_free(kernel);
                return mask;
            }
            for (int x = min_x; x <= max_x; ++x) {
                float value = get_output ? get_output(c, x, y) : 1.0f;
                if (value > 0.0f) {
                    apply_expansion_kernel(mask, img_width, value, kernel,
                                           c->width, c->height, expand,
                                           feather_radius, expand_min_x,
                                           expand_min_y, x, y);
                }
            }
        }
        DP_free(kernel);
    }
    else {
        int shrink = -expand;
        int tmp_width = expand_max_x - expand_min_x + shrink * 2 + 1;
        int tmp_height = expand_max_y - expand_min_y + shrink * 2 + 1;
        size_t tmp_size =
            DP_int_to_size(tmp_width) * DP_int_to_size(tmp_height);
        float *tmp = DP_malloc_zeroed(tmp_size * sizeof(float));

        for (int y = min_y; y <= max_y; ++y) {
            if (is_cancelled(c)) {
                DP_free(tmp);
                return mask;
            }
            for (int x = min_x; x <= max_x; ++x) {
                float value = get_output ? get_output(c, x, y) : 1.0f;
                if (value > 0.0f) {
                    int mx = x - min_x + shrink;
                    int my = y - min_y + shrink;
                    tmp[my * tmp_width + mx] = value;
                }
            }
        }

        if (!from_edge) {
            expand_edges(c->width, c->height, expand_min_x, expand_min_y,
                         expand_max_x, expand_max_y, shrink, tmp, tmp_width,
                         tmp_height);
        }

        unsigned char *kernel = generate_expansion_kernel(kernel_shape, shrink);
        erode_mask(c, tmp, tmp_width, tmp_height, mask, img_width,
                   feather_radius, shrink, kernel);
        DP_free(kernel);
        DP_free(tmp);
    }

    if (!is_cancelled(c) && feather_radius != 0) {
        if (!from_edge) {
            expand_edges(c->width, c->height, expand_min_x, expand_min_y,
                         expand_max_x, expand_max_y, feather_radius, mask,
                         img_width, img_height);
        }
        float *tmp = DP_malloc(mask_bytes);
        feather_mask(c, mask, tmp, img_width, img_height, feather_radius);
        DP_free(tmp);
    }

    return mask;
}

static float get_flood_mask_value(void *user, int x, int y)
{
    DP_FloodFillContext *c = user;
    unsigned char *output = c->output;
    return buffer_get(output, c->parent.area, x, y) == 0 ? 0.0f : 1.0f;
}

static void merge_mask_with_selection(float *mask, int img_x, int img_y,
                                      int img_width, int img_height,
                                      DP_Selection *sel)
{
    DP_LayerContent *lc = DP_selection_content_noinc(sel);
    int layer_width = DP_layer_content_width(lc);
    int layer_height = DP_layer_content_height(lc);
    for (int y = 0; y < img_height; ++y) {
        for (int x = 0; x < img_width; ++x) {
            int index = y * img_width + x;
            float value = mask[index];
            if (value > 0.0f) {
                int layer_x = x + img_x;
                int layer_y = y + img_y;
                if (layer_x >= 0 && layer_x < layer_width && layer_y >= 0
                    && layer_y < layer_height) {
                    DP_Pixel15 pixel =
                        DP_layer_content_pixel_at(lc, x + img_x, y + img_y);
                    mask[index] = value * DP_channel15_to_float(pixel.a);
                }
                else {
                    mask[index] = 0.0f;
                }
            }
        }
    }
}

static bool is_mask_empty(float *mask, int img_width, int img_height)
{
    size_t count = DP_int_to_size(img_width) * DP_int_to_size(img_height);
    for (size_t i = 0; i < count; ++i) {
        if (mask[i] != 0.0f) {
            return false;
        }
    }
    return true;
}

static DP_Image *mask_to_image(DP_FillContext *c, const float *mask,
                               int img_width, int img_height,
                               DP_UPixelFloat fill_color)
{
    DP_Image *img = DP_image_new(img_width, img_height);
    DP_Pixel8 *pixels = DP_image_pixels(img);
    DP_Pixel8 opaque = DP_pixel8_premultiply(DP_upixel_float_to_8(fill_color));

    for (int y = 0; y < img_height; ++y) {
        if (is_cancelled(c)) {
            return img;
        }
        for (int x = 0; x < img_width; ++x) {
            int i = y * img_width + x;
            float m = mask[i];
            if (m >= 1.0f) {
                pixels[i] = opaque;
            }
            else if (m > 0.0f) {
                DP_UPixelFloat p = fill_color;
                p.a *= m;
                pixels[i] = DP_pixel8_premultiply(DP_upixel_float_to_8(p));
            }
        }
    }

    return img;
}

static DP_FloodFillResult finish_fill(DP_FillContext *c, float *mask, int img_x,
                                      int img_y, int img_width, int img_height,
                                      DP_UPixelFloat fill_color,
                                      DP_Image **out_img, int *out_x,
                                      int *out_y)
{
    DP_Image *img = mask_to_image(c, mask, img_width, img_height, fill_color);
    DP_free(mask);

    if (out_img) {
        *out_img = img;
    }
    else {
        DP_image_free(img);
    }

    if (out_x) {
        *out_x = img_x;
    }

    if (out_y) {
        *out_y = img_y;
    }

    return DP_FLOOD_FILL_SUCCESS;
}

DP_FloodFillResult
DP_flood_fill(DP_CanvasState *cs, unsigned int context_id, int selection_id,
              int x, int y, DP_UPixelFloat fill_color, double tolerance,
              int layer_id, int size, int gap, int expand,
              DP_FloodFillKernel kernel_shape, int feather_radius,
              bool from_edge, bool continuous, bool include_sublayers,
              DP_ViewMode view_mode, int active_layer_id,
              int active_frame_index, DP_Image **out_img, int *out_x,
              int *out_y, DP_FloodFillShouldCancelFn should_cancel, void *user)
{
    DP_ASSERT(cs);

    DP_FloodFillContext c = {
        {
            0,
            0,
            {0, 0, 0, 0},
            INT_MAX,
            INT_MAX,
            INT_MIN,
            INT_MIN,
            NULL,
            false,
            should_cancel,
            user,
        },
        (DP_FloodFillContextType)0,
        0,
        0,
        0.0,
        {0.0f, 0.0f, 0.0f, 0.0f},
        {NULL},
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        DP_QUEUE_NULL,
    };
    if (is_cancelled(&c.parent)) {
        return DP_FLOOD_FILL_CANCELLED;
    }

    c.parent.width = DP_canvas_state_width(cs);
    c.parent.height = DP_canvas_state_height(cs);
    if (size < 0) {
        c.parent.area.x1 = 0;
        c.parent.area.y1 = 0;
        c.parent.area.x2 = c.parent.width - 1;
        c.parent.area.y2 = c.parent.height - 1;
    }
    else {
        c.parent.area.x1 = DP_max_int(0, x - size);
        c.parent.area.y1 = DP_max_int(0, y - size);
        c.parent.area.x2 = DP_min_int(c.parent.width - 1, x + size);
        c.parent.area.y2 = DP_min_int(c.parent.height - 1, y + size);
    }

    init_selection(&c.parent, cs, context_id, selection_id);

    if (x < 0 || y < 0 || x >= c.parent.width || y >= c.parent.height
        || DP_rect_empty(c.parent.area)
        || !DP_rect_contains(c.parent.area, x, y)) {
        DP_error_set("Flood fill: initial point out of bounds");
        return DP_FLOOD_FILL_OUT_OF_BOUNDS;
    }

    float fallback_mask;
    if (c.parent.sel) {
        DP_LayerContent *sel_lc = DP_selection_content_noinc(c.parent.sel);
        uint16_t alpha = DP_layer_content_pixel_at(sel_lc, x, y).a;
        fallback_mask = DP_channel15_to_float_round8(alpha);
        if (fallback_mask == 0.0f) {
            DP_error_set("Flood fill: initial point outside of selection");
            return DP_FLOOD_FILL_OUT_OF_BOUNDS;
        }
    }
    else {
        fallback_mask = 1.0f;
    }

    if (!source_init(&c, cs, layer_id, include_sublayers, view_mode,
                     active_layer_id, active_frame_index, tolerance,
                     continuous ? DP_max_int(0, gap) : 0, x, y)) {
        return DP_FLOOD_FILL_INVALID_LAYER;
    }

    if (is_cancelled(&c.parent)) {
        source_dispose(&c);
        return DP_FLOOD_FILL_CANCELLED;
    }

    float (*get_output)(void *, int, int);
    if (c.type == DP_FLOOD_FILL_SOURCE_BLANK) {
        source_dispose(&c);
        get_output = NULL;
        c.parent.min_x = c.parent.area.x1;
        c.parent.max_x = c.parent.area.x2;
        c.parent.min_y = c.parent.area.y1;
        c.parent.max_y = c.parent.area.y2;
    }
    else {
        get_output = get_flood_mask_value;
        if (continuous) {
            c.output = DP_malloc_zeroed(source_map_size(&c));
            DP_queue_init(&c.queue, 1024, sizeof(DP_FillSeed));
            flood_fill(&c, x, y);
            DP_queue_dispose(&c.queue);
            source_dispose(&c);
        }
        else {
            source_flood_all(&c);
            source_move_flood_map_to_output(&c);
            source_dispose(&c);
            tighten_output_bounds(&c);
        }
    }

    if (is_cancelled(&c.parent)) {
        DP_free(c.output);
        return DP_FLOOD_FILL_CANCELLED;
    }

    if (c.parent.min_x > c.parent.max_x || c.parent.min_y > c.parent.max_y) {
        DP_error_set("Flood fill: nothing to fill");
        DP_free(c.output);
        return DP_FLOOD_FILL_NOTHING_TO_FILL;
    }

    int img_x, img_y, img_width, img_height;
    float *mask = make_mask(&c.parent, get_output, expand, kernel_shape,
                            DP_max_int(feather_radius, 0), from_edge, &img_x,
                            &img_y, &img_width, &img_height);
    DP_free(c.output);
    if (is_cancelled(&c.parent)) {
        DP_free(mask);
        return DP_FLOOD_FILL_CANCELLED;
    }

    if (c.parent.sel) {
        merge_mask_with_selection(mask, img_x, img_y, img_width, img_height,
                                  c.parent.sel);
    }
    if (is_cancelled(&c.parent)) {
        DP_free(mask);
        return DP_FLOOD_FILL_CANCELLED;
    }

    // Filling nothing can happen because of erosion from either a negative
    // expand value or gap filling. That result is not useful though, since the
    // user obviously didn't issue a fill for no reason. It also makes clicking
    // and dragging to increase the tolerance not work, which is not useful. So
    // in that case, we just fill a single pixel.
    if (is_mask_empty(mask, img_width, img_height)) {
        mask[0] = fallback_mask;
        img_x = x;
        img_y = y;
        img_width = 1;
        img_height = 1;
    }

    return finish_fill(&c.parent, mask, img_x, img_y, img_width, img_height,
                       fill_color, out_img, out_x, out_y);
}


static bool selection_fill_get_bounds(DP_FillContext *c, DP_CanvasState *cs,
                                      unsigned int context_id, int selection_id)
{
    c->width = DP_canvas_state_width(cs);
    c->height = DP_canvas_state_height(cs);
    if (c->width <= 0 || c->height <= 0) {
        DP_error_set("Canvas has zero size");
        return false;
    }

    c->sel =
        DP_canvas_state_selection_search_noinc(cs, context_id, selection_id);
    if (!c->sel) {
        DP_error_set("Selection %d of user %u not found", selection_id,
                     context_id);
        return false;
    }

    DP_Rect sel_bounds = *DP_selection_bounds(c->sel);
    if (DP_rect_empty(sel_bounds)) {
        DP_error_set("Selection %d of user %u has empty bounds", selection_id,
                     context_id);
        return false;
    }

    c->area = DP_rect_intersection(DP_rect_make(0, 0, c->width, c->height),
                                   sel_bounds);
    if (DP_rect_empty(c->area)) {
        DP_error_set("Intersection between canvas and bounds of selection %d "
                     "of user %u is empty (shouldn't happen)",
                     selection_id, context_id);
        return false;
    }

    c->min_x = c->area.x1;
    c->min_y = c->area.y1;
    c->max_x = c->area.x2;
    c->max_y = c->area.y2;
    return true;
}

static float get_selection_mask_value(void *user, int x, int y)
{
    DP_FillContext *c = user;
    DP_LayerContent *lc = DP_selection_content_noinc(c->sel);
    DP_Pixel15 pixel = DP_layer_content_pixel_at(lc, x, y);
    return DP_channel15_to_float(pixel.a);
}

DP_FloodFillResult
DP_selection_fill(DP_CanvasState *cs, unsigned int context_id, int selection_id,
                  DP_UPixelFloat fill_color, int expand,
                  DP_FloodFillKernel kernel_shape, int feather_radius,
                  bool from_edge, DP_Image **out_img, int *out_x, int *out_y,
                  DP_FloodFillShouldCancelFn should_cancel, void *user)
{
    DP_ASSERT(cs);

    DP_FillContext c = {
        0,       0,    {0, 0, 0, 0}, INT_MAX,       INT_MAX, INT_MIN,
        INT_MIN, NULL, false,        should_cancel, user,
    };
    if (is_cancelled(&c)) {
        return DP_FLOOD_FILL_CANCELLED;
    }

    if (!selection_fill_get_bounds(&c, cs, context_id, selection_id)) {
        return DP_FLOOD_FILL_NOTHING_TO_FILL;
    }

    int img_x, img_y, img_width, img_height;
    float *mask = make_mask(&c, get_selection_mask_value, expand, kernel_shape,
                            DP_max_int(feather_radius, 0), from_edge, &img_x,
                            &img_y, &img_width, &img_height);
    if (is_cancelled(&c)) {
        DP_free(mask);
        return DP_FLOOD_FILL_CANCELLED;
    }

    if (is_mask_empty(mask, img_width, img_height)) {
        DP_error_set("Fill result is blank");
        DP_free(mask);
        return DP_FLOOD_FILL_NOTHING_TO_FILL;
    }

    return finish_fill(&c, mask, img_x, img_y, img_width, img_height,
                       fill_color, out_img, out_x, out_y);
}
