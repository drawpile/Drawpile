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
#include "layer_routes.h"
#include "pixels.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/queue.h>
#include <limits.h>
#include <math.h>
#include <helpers.h> // M_PI


// Flood fill algorithm based on: Smith, Alvy Ray (1979). Tint Fill. SIGGRAPH
// '79: Proceedings of the 6th annual conference on Computer graphics and
// interactive techniques. pp. 276–283.
// See https://en.wikipedia.org/wiki/Flood_fill#Span_Filling

typedef struct DP_FillContext {
    unsigned char *buffer;
    int width, height;
    DP_LayerContent *lc;
    DP_UPixelFloat reference_color;
    double tolerance_squared;
    int min_x, min_y, max_x, max_y;
    int pixels_left;
    DP_Queue queue;
} DP_FillContext;

typedef struct DP_FillSeed {
    int x, y;
} DP_FillSeed;

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

static unsigned char *buffer_at(DP_FillContext *c, int x, int y)
{
    return &c->buffer[y * c->width + x];
}

static bool is_blank(DP_FillContext *c, int x, int y)
{
    return *buffer_at(c, x, y) == 0;
}

static DP_UPixelFloat get_color_at(DP_LayerContent *lc, int x, int y)
{
    return DP_upixel15_to_float(
        DP_pixel15_unpremultiply(DP_layer_content_pixel_at(lc, x, y)));
}

static bool should_fill(DP_FillContext *c, int x, int y)
{
    DP_UPixelFloat color = get_color_at(c->lc, x, y);
    // TODO: we could use better functions for color distance than this.
    DP_UPixelFloat reference_color = c->reference_color;
    double b = color.b - reference_color.b;
    double g = color.g - reference_color.g;
    double r = color.r - reference_color.r;
    double a = color.a - reference_color.a;
    return b * b + g * g + r * r + a * a <= c->tolerance_squared;
}

static bool inside(DP_FillContext *c, int x, int y)
{
    return x >= 0 && x < c->width && y >= 0 && y < c->height
        && is_blank(c, x, y) && should_fill(c, x, y);
}

static bool set_pixel(DP_FillContext *c, int x, int y)
{
    if (--c->pixels_left > 0) {
        *buffer_at(c, x, y) = 1;
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
        return true;
    }
    else {
        return false;
    }
}

static void scan(DP_FillContext *c, DP_Queue *s, int lx, int rx, int y)
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

static bool fill(DP_FillContext *c, int x0, int y0)
{
    DP_Queue *s = &c->queue;
    add_seed(s, x0, y0);
    while (s->used != 0) {
        int x, y;
        shift_seed(s, &x, &y);
        int lx = x;
        while (inside(c, lx - 1, y)) {
            if (!set_pixel(c, lx - 1, y)) {
                return false;
            }
            lx = lx - 1;
        }
        while (inside(c, x, y)) {
            if (!set_pixel(c, x, y)) {
                return false;
            }
            x = x + 1;
        }
        scan(c, s, lx, x - 1, y + 1);
        scan(c, s, lx, x - 1, y - 1);
    }
    return true;
}

static int get_kernel_diameter(int radius)
{
    return radius * 2 + 1;
}

static unsigned char *generate_expansion_kernel(int expand)
{
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

static void apply_expansion_kernel(float *mask, int mask_width,
                                   unsigned char *kernel, int width, int height,
                                   int expand, int feather_radius,
                                   int expand_min_x, int expand_min_y, int x0,
                                   int y0)
{
    int start_x0 = x0 - expand;
    int start_y0 = y0 - expand;
    int start_x = DP_max_int(start_x0, 0);
    int start_y = DP_max_int(start_y0, 0);
    int end_x = DP_min_int(x0 + expand, width - 1);
    int end_y = DP_min_int(y0 + expand, height - 1);
    int diameter = get_kernel_diameter(expand);
    for (int y = start_y; y <= end_y; ++y) {
        for (int x = start_x; x <= end_x; ++x) {
            int kernel_x = x - start_x0;
            int kernel_y = y - start_y0;
            if (kernel[kernel_y * diameter + kernel_x]) {
                int mx = x - expand_min_x + feather_radius;
                int my = y - expand_min_y + feather_radius;
                mask[my * mask_width + mx] = 1.0f;
            }
        }
    }
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

static void feather_mask(float *mask, float *tmp, int width, int height,
                         int radius)
{
    // This is a classic two-pass gaussian blur. We create a one-dimensional
    // gaussian kernel, then blur once horizontally to a temporary buffer and
    // then vertically back into the original image.
    float *kernel = generate_gaussian_kernel(radius);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            blur_horizontally(tmp, mask, x, y, width, kernel, radius);
        }
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            blur_vertically(mask, tmp, x, y, width, height, kernel, radius);
        }
    }
    DP_free(kernel);
}

static float *make_mask(DP_FillContext *c, int expand, int feather_radius,
                        int *out_img_x, int *out_img_y, int *out_img_width,
                        int *out_img_height)
{
    int min_x = c->min_x, min_y = c->min_y;
    int max_x = c->max_x, max_y = c->max_y;
    int expand_min_x = DP_max_int(min_x - expand, 0);
    int expand_min_y = DP_max_int(min_y - expand, 0);
    int expand_max_x = DP_min_int(max_x + expand, c->width - 1);
    int expand_max_y = DP_min_int(max_y + expand, c->height - 1);
    int img_width = expand_max_x - expand_min_x + feather_radius * 2 + 1;
    int img_height = expand_max_y - expand_min_y + feather_radius * 2 + 1;
    size_t mask_size = DP_int_to_size(img_width) * DP_int_to_size(img_height);
    float *mask = DP_malloc_zeroed(mask_size * sizeof(*mask));

    if (expand == 0) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                if (!is_blank(c, x, y)) {
                    int mx = x - min_x + feather_radius;
                    int my = y - min_y + feather_radius;
                    mask[my * img_width + mx] = 1.0f;
                }
            }
        }
    }
    else {
        unsigned char *kernel = generate_expansion_kernel(expand);
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                if (!is_blank(c, x, y)) {
                    apply_expansion_kernel(mask, img_width, kernel, c->width,
                                           c->height, expand, feather_radius,
                                           expand_min_x, expand_min_y, x, y);
                }
            }
        }
        DP_free(kernel);
    }

    if (feather_radius != 0) {
        float *tmp = DP_malloc(mask_size * sizeof(*mask));
        feather_mask(mask, tmp, img_width, img_height, feather_radius);
        DP_free(tmp);
    }

    *out_img_x = expand_min_x - feather_radius;
    *out_img_y = expand_min_y - feather_radius;
    *out_img_width = img_width;
    *out_img_height = img_height;
    return mask;
}

DP_Image *mask_to_image(const float *mask, int img_width, int img_height,
                        DP_UPixelFloat fill_color)
{
    DP_Image *img = DP_image_new(img_width, img_height);
    DP_Pixel8 *pixels = DP_image_pixels(img);
    DP_Pixel8 opaque = DP_pixel8_premultiply(DP_upixel_float_to_8(fill_color));

    size_t count = DP_int_to_size(img_width) * DP_int_to_size(img_height);
    for (size_t i = 0; i < count; ++i) {
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

    return img;
}

DP_FloodFillResult DP_flood_fill(DP_CanvasState *cs, int x, int y,
                                 DP_UPixelFloat fill_color, double tolerance,
                                 int layer_id, bool sample_merged,
                                 int size_limit, int expand, int feather_radius,
                                 DP_Image **out_img, int *out_x, int *out_y)
{
    DP_ASSERT(cs);
    int width = DP_canvas_state_width(cs);
    int height = DP_canvas_state_height(cs);
    if (x < 0 || y < 0 || x >= width || y >= height) {
        DP_error_set("Flood fill: initial point out of bounds");
        return DP_FLOOD_FILL_OUT_OF_BOUNDS;
    }

    DP_LayerContent *lc;
    if (sample_merged) {
        lc = (DP_LayerContent *)DP_canvas_state_to_flat_layer(
            cs, DP_FLAT_IMAGE_RENDER_FLAGS);
    }
    else {
        DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
        DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
        if (!lre) {
            DP_error_set("Flood fill: layer %d not found", layer_id);
            return DP_FLOOD_FILL_INVALID_LAYER;
        }
        lc = DP_layer_content_incref(DP_layer_routes_entry_content(lre, cs));
    }

    size_t buffer_size = DP_int_to_size(width) * DP_int_to_size(height);
    unsigned char *buffer = DP_malloc_zeroed(buffer_size);

    DP_FillContext c = {
        buffer,
        width,
        height,
        lc,
        get_color_at(lc, x, y),
        tolerance * tolerance,
        INT_MAX,
        INT_MAX,
        INT_MIN,
        INT_MIN,
        size_limit,
        DP_QUEUE_NULL,
    };

    DP_queue_init(&c.queue, 1024, sizeof(DP_FillSeed));
    bool fill_done = fill(&c, x, y);
    DP_queue_dispose(&c.queue);
    DP_layer_content_decref(lc);
    if (!fill_done) {
        DP_error_set("Flood fill: size limit %d exceeded", size_limit);
        DP_free(buffer);
        return DP_FLOOD_FILL_SIZE_LIMIT_EXCEEDED;
    }

    if (c.min_x > c.max_x || c.min_y > c.max_y) {
        DP_error_set("Flood fill: nothing to fill");
        DP_free(buffer);
        return DP_FLOOD_FILL_NOTHING_TO_FILL;
    }

    int img_x, img_y, img_width, img_height;
    float *mask =
        make_mask(&c, DP_max_int(expand, 0), DP_max_int(feather_radius, 0),
                  &img_x, &img_y, &img_width, &img_height);
    DP_free(buffer);

    DP_Image *img = mask_to_image(mask, img_width, img_height, fill_color);
    DP_free(mask);

    if (out_x) {
        *out_x = img_x;
    }
    if (out_y) {
        *out_y = img_y;
    }
    if (out_img) {
        *out_img = img;
    }
    else {
        DP_free(img);
    }
    return DP_FLOOD_FILL_SUCCESS;
}
