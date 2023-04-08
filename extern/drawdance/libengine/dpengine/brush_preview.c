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
 */
#include "brush_preview.h"
#include "brush.h"
#include "brush_engine.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "flood_fill.h"
#include "image.h"
#include "layer_content.h"
#include "layer_routes.h"
#include "paint.h"
#include "pixels.h"
#include "tile.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/vector.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>
#include <math.h>
#include <helpers.h> // CLAMP, M_PI


#define FOREGROUND_SOLID 0
#define FOREGROUND_BARS  1
#define FOREGROUND_DABS  2

#define PI_FLOAT ((float)M_PI)
#define DELTA_MS 20


struct DP_BrushPreview {
    DP_BrushEngine *be;
    DP_CanvasState *cs;
    DP_Vector messages;
};

static void push_message(void *user, DP_Message *msg)
{
    DP_BrushPreview *bp = user;
    DP_VECTOR_PUSH_TYPE(&bp->messages, DP_Message *, msg);
}

DP_BrushPreview *DP_brush_preview_new(void)
{
    DP_BrushPreview *bp = DP_malloc(sizeof(*bp));
    *bp = (DP_BrushPreview){DP_brush_engine_new(push_message, bp), NULL,
                            DP_VECTOR_NULL};
    DP_VECTOR_INIT_TYPE(&bp->messages, DP_Message *, 64);
    return bp;
}

static void dispose_message(void *element)
{
    DP_Message **msg_ptr = element;
    DP_message_decref(*msg_ptr);
}

void DP_brush_preview_free(DP_BrushPreview *bp)
{
    if (bp) {
        DP_VECTOR_CLEAR_DISPOSE_TYPE(&bp->messages, DP_Message *,
                                     dispose_message);
        DP_canvas_state_decref_nullable(bp->cs);
        DP_brush_engine_free(bp->be);
        DP_free(bp);
    }
}


static DP_CanvasState *handle_preview_message_dec(DP_CanvasState *cs,
                                                  DP_DrawContext *dc,
                                                  DP_Message *msg)
{
    DP_CanvasState *next = DP_canvas_state_handle(cs, dc, msg).cs;
    DP_message_decref(msg);
    if (next) {
        DP_canvas_state_decref(cs);
        return next;
    }
    else {
        DP_warn("Preview: %s", DP_error());
        return cs;
    }
}

static DP_CanvasState *handle_preview_messages_multidab(DP_CanvasState *cs,
                                                        DP_DrawContext *dc,
                                                        int count,
                                                        DP_Message **msgs)
{
    DP_CanvasState *next =
        DP_canvas_state_handle_multidab(cs, dc, count, msgs).cs;
    if (next) {
        DP_canvas_state_decref(cs);
        return next;
    }
    else {
        return cs;
    }
}


static bool is_dark_color(DP_UPixelFloat color)
{
    float lum = color.b * 0.0722f + color.g * 0.7152f + color.r * 0.216f;
    return lum <= 0.5f;
}

static uint32_t hsv_to_bgra(float h, float s, float v)
{
    hsv_to_rgb_float(&h, &s, &v);
    DP_UPixelFloat pixel = {v, s, h, 1.0f};
    return DP_pixel8_premultiply(DP_upixel_float_to_8(pixel)).color;
}

static void set_background_color(DP_UNUSED size_t size, unsigned char *out,
                                 void *user)
{
    DP_ASSERT(size == 4);
    DP_write_bigendian_uint32(*(uint32_t *)user, out);
}

static void set_preview_foreground_dab(DP_UNUSED int count, DP_PixelDab *pds,
                                       void *user)
{
    DP_ASSERT(count == 1);
    int *d_ptr = user;
    DP_pixel_dab_init(pds, 0, 0, 0, DP_int_to_uint8(*d_ptr), 255);
}

static void stroke_freehand(DP_BrushEngine *be, DP_CanvasState *cs,
                            DP_Rect rect)
{
    int rw = DP_rect_width(rect);
    int rh = DP_rect_height(rect);
    float rxf = DP_int_to_float(DP_rect_x(rect));
    float ryf = DP_int_to_float(DP_rect_y(rect));
    float rwf = DP_int_to_float(rw);
    float rhf = DP_int_to_float(rh);
    float h = rhf * 0.6f;
    float offy = ryf + DP_int_to_float(rh) / 2.0f;
    float dphase = (2.0f * PI_FLOAT) / rwf;
    float phase = 0.0f;
    for (int x = 0; x < rw; ++x) {
        float xf = DP_int_to_float(x);
        float p = xf / rwf;
        float raw_pressure = (p * p - p * p * p) * 6.756f;
        float pressure = CLAMP(raw_pressure, 0.0f, 1.0f);
        float y = sinf(phase) * h;
        DP_brush_engine_stroke_to(be, rxf + xf, offy + y, pressure, 0.0f, 0.0f,
                                  0.0f, DELTA_MS, cs);
        phase += dphase;
    }
}

static void stroke_line(DP_BrushEngine *be, DP_CanvasState *cs, DP_Rect rect)
{
    DP_brush_engine_stroke_to(be, DP_int_to_float(DP_rect_x(rect)),
                              DP_int_to_float(DP_rect_y(rect)), 1.0f, 0.0f,
                              0.0f, 0.0f, DELTA_MS, cs);
    DP_brush_engine_stroke_to(be, DP_int_to_float(DP_rect_right(rect)),
                              DP_int_to_float(DP_rect_bottom(rect)), 1.0f, 0.0f,
                              0.0f, 0.0f, DELTA_MS, cs);
}

static void stroke_rectangle(DP_BrushEngine *be, DP_CanvasState *cs,
                             DP_Rect rect)
{
    float l = DP_int_to_float(DP_rect_left(rect));
    float r = DP_int_to_float(DP_rect_right(rect));
    float t = DP_int_to_float(DP_rect_top(rect));
    float b = DP_int_to_float(DP_rect_bottom(rect));
    DP_brush_engine_stroke_to(be, l, t, 1.0f, 0.0f, 0.0f, 0.0f, DELTA_MS, cs);
    DP_brush_engine_stroke_to(be, r, t, 1.0f, 0.0f, 0.0f, 0.0f, DELTA_MS, cs);
    DP_brush_engine_stroke_to(be, r, b, 1.0f, 0.0f, 0.0f, 0.0f, DELTA_MS, cs);
    DP_brush_engine_stroke_to(be, l, b, 1.0f, 0.0f, 0.0f, 0.0f, DELTA_MS, cs);
    DP_brush_engine_stroke_to(be, l, t, 1.0f, 0.0f, 0.0f, 0.0f, DELTA_MS, cs);
}

static void stroke_ellipse(DP_BrushEngine *be, DP_CanvasState *cs, DP_Rect rect)
{
    float a = DP_int_to_float(DP_rect_width(rect)) / 2.0f;
    float b = DP_int_to_float(DP_rect_height(rect)) / 2.0f;
    float cx = DP_int_to_float(DP_rect_x(rect)) + a;
    float cy = DP_int_to_float(DP_rect_y(rect)) + b;
    for (int i = 0; i <= 40; ++i) {
        float t = PI_FLOAT / 20.0f * DP_int_to_float(i);
        DP_brush_engine_stroke_to(be, cx + a * cosf(t), cy + b * sinf(t), 1.0f,
                                  0.0f, 0.0f, 0.0f, DELTA_MS, cs);
    }
}

static void stroke_flood_fill_blob(DP_BrushEngine *be, DP_CanvasState *cs,
                                   DP_Rect rect)
{
    int rx = DP_rect_x(rect);
    int ry = DP_rect_y(rect);
    int rw = DP_rect_width(rect);
    int rh = DP_rect_height(rect);
    float rxf = DP_int_to_float(rx);
    float rwf = DP_int_to_float(rw);
    float rrf = DP_int_to_float(DP_rect_right(rect));
    float mid = DP_int_to_float(ry + rh / 2);
    float h = DP_int_to_float(rh) * 0.8f;

    float a = 0.0f;
    while (a < PI_FLOAT) {
        float x = rxf + a / PI_FLOAT * rwf;
        float y = powf(sinf(a), 0.5f) * 0.7f + sinf(a * 3.0f) * 0.3f;
        DP_brush_engine_stroke_to(be, x, mid - y * h, 1.0f, 0.0f, 0.0f, 0.0f,
                                  DELTA_MS, cs);
        a += 0.1f;
    }

    a = 0.1f;
    while (a < PI_FLOAT) {
        float x = rrf - (a / PI_FLOAT * rwf);
        float y = powf(sinf(a), 0.5f) * 0.7f + sinf(a * 2.8f) * 0.2f;
        DP_brush_engine_stroke_to(be, x, mid + y * h, 1.0f, 0.0f, 0.0f, 0.0f,
                                  DELTA_MS, cs);
        a += 0.1f;
    }

    a = 0.0f;
    float x = rxf + a / PI_FLOAT * rwf;
    float y = powf(sinf(a), 0.5f) * 0.7f + sinf(a * 3.0f) * 0.3f;
    DP_brush_engine_stroke_to(be, x, mid - y * h, 1.0f, 0.0f, 0.0f, 0.0f,
                              DELTA_MS, cs);
}

void render_brush_preview(
    DP_BrushPreview *bp, DP_DrawContext *dc, int width, int height,
    DP_BrushPreviewShape shape, DP_UPixelFloat initial_color, bool smudge,
    DP_BlendMode mode,
    void (*set_brush)(void *, DP_BrushEngine *, DP_UPixelFloat), void *user)
{
    DP_ASSERT(bp);
    DP_ASSERT(dc);
    DP_canvas_state_decref_nullable(bp->cs);
    if (width <= 0 || height <= 0) {
        bp->cs = NULL;
        return;
    }

    uint32_t color =
        DP_pixel8_premultiply(DP_upixel_float_to_8(initial_color)).color;
    uint32_t background_color =
        is_dark_color(initial_color) ? 0xfffafafau : 0xff202020u;

    uint32_t layer_color = 0;
    int foreground_style = smudge ? FOREGROUND_BARS : FOREGROUND_SOLID;

    if (mode == DP_BLEND_MODE_ERASE) {
        layer_color = 0xff202020u;
        background_color = 0;
    }
    else if (mode == DP_BLEND_MODE_COLOR_ERASE) {
        layer_color = color;
        background_color = 0;
    }
    else if (mode == DP_BLEND_MODE_NORMAL_AND_ERASER
             || !DP_blend_mode_can_increase_opacity((int)mode)) {
        foreground_style = FOREGROUND_DABS;
        background_color = 0;
    }

    DP_UPixelFloat brush_color;
    switch (shape) {
    case DP_BRUSH_PREVIEW_FLOOD_FILL:
        brush_color = (DP_UPixelFloat){0.0f, 0.0f, 0.0f, 0.0f};
        background_color = 0;
        break;
    case DP_BRUSH_PREVIEW_FLOOD_ERASE:
        layer_color = color;
        brush_color = DP_upixel8_to_float(
            DP_pixel8_unpremultiply((DP_Pixel8){background_color}));
        background_color = 0;
        break;
    default:
        brush_color = initial_color;
        break;
    }

    DP_CanvasState *cs = DP_canvas_state_new();
    cs = handle_preview_message_dec(
        cs, dc,
        DP_msg_canvas_background_new(0, set_background_color, 4,
                                     &background_color));
    cs = handle_preview_message_dec(
        cs, dc,
        DP_msg_canvas_resize_new(0, 0, DP_int_to_int32(width),
                                 DP_int_to_int32(height), 0));
    cs = handle_preview_message_dec(
        cs, dc, DP_msg_layer_create_new(0, 1, 0, 0, layer_color, 0, "", 0));

    switch (foreground_style) {
    case FOREGROUND_SOLID:
        if (mode == DP_BLEND_MODE_BEHIND) {
            // Put something on the layer to draw behind.
            uint32_t w = DP_int_to_uint32(width);
            uint32_t h = DP_int_to_uint32(height);
            uint32_t b = DP_int_to_uint32(DP_max_int(width / 20, 2));
            cs = handle_preview_message_dec(
                cs, dc,
                DP_msg_fill_rect_new(0, 1, DP_BLEND_MODE_NORMAL,
                                     w / 4u - b / 2u, 0, b, h, 0xff000000));
            cs = handle_preview_message_dec(
                cs, dc,
                DP_msg_fill_rect_new(0, 1, DP_BLEND_MODE_NORMAL,
                                     w / 4u * 2 - b / 2u, 0, b, h, 0xff000000));
            cs = handle_preview_message_dec(
                cs, dc,
                DP_msg_fill_rect_new(0, 1, DP_BLEND_MODE_NORMAL,
                                     w / 4u * 3 - b / 2u, 0, b, h, 0xff000000));
        }
        break;
    case FOREGROUND_DABS: {
        int d = CLAMP(height * 2 / 3, 10, 255);
        int x0 = d;
        int x1 = width - d;
        int step = d * 70 / 100;
        float huestep = 359.0f / 360.0f
                      / (DP_int_to_float(x1 - x0) / DP_int_to_float(step));
        float hue = 0.0f;
        for (int x = x0; x < x1; x += step, hue += huestep) {
            cs = handle_preview_message_dec(
                cs, dc,
                DP_msg_draw_dabs_pixel_new(
                    0, 1, DP_int_to_int32(x), DP_int_to_int32(height / 2),
                    // Set the alpha to zero to draw in direct mode.
                    hsv_to_bgra(hue, 0.62f, 0.86f) & 0x00ffffffu,
                    DP_BLEND_MODE_NORMAL, set_preview_foreground_dab, 1, &d));
        }
        break;
    }
    case FOREGROUND_BARS: {
        uint32_t w = DP_int_to_uint32(width);
        uint32_t h = DP_int_to_uint32(height);
        uint32_t bars = 5;
        uint32_t bw = w / bars;
        for (uint32_t i = 0; i < bars; ++i) {
            float hue = DP_uint32_to_float(i) / DP_uint32_to_float(bars);
            cs = handle_preview_message_dec(
                cs, dc,
                DP_msg_fill_rect_new(0, 1, DP_BLEND_MODE_NORMAL, i * bw, 0, bw,
                                     h, hsv_to_bgra(hue, 0.62f, 0.86f)));
        }
        break;
    }
    default:
        DP_UNREACHABLE();
    }

    DP_Rect rect = DP_rect_make(width / 8, height / 4, width - width / 4,
                                height - height / 2);
    DP_BrushEngine *be = bp->be;
    set_brush(user, be, brush_color);
    DP_brush_engine_stroke_begin(be, 1, false);
    switch (shape) {
    case DP_BRUSH_PREVIEW_STROKE:
        stroke_freehand(be, cs, rect);
        break;
    case DP_BRUSH_PREVIEW_LINE:
        stroke_line(be, cs, rect);
        break;
    case DP_BRUSH_PREVIEW_RECTANGLE:
        stroke_rectangle(be, cs, rect);
        break;
    case DP_BRUSH_PREVIEW_ELLIPSE:
        stroke_ellipse(be, cs, rect);
        break;
    case DP_BRUSH_PREVIEW_FLOOD_FILL:
    case DP_BRUSH_PREVIEW_FLOOD_ERASE:
        stroke_flood_fill_blob(be, cs, rect);
        break;
    default:
        DP_warn("Unknown preview shape %d", (int)shape);
        break;
    }
    DP_brush_engine_stroke_end(be, false);
    DP_brush_engine_dabs_flush(be);

    cs = handle_preview_messages_multidab(
        cs, dc, DP_size_to_int(bp->messages.used), bp->messages.elements);
    DP_VECTOR_CLEAR_TYPE(&bp->messages, DP_Message *, dispose_message);

    bp->cs = handle_preview_message_dec(cs, dc, DP_msg_pen_up_new(1));
}

static void set_preview_classic_brush(void *user, DP_BrushEngine *be,
                                      DP_UPixelFloat color)
{
    DP_brush_engine_classic_brush_set(be, user, 1, color);
}

void DP_brush_preview_render_classic(DP_BrushPreview *bp, DP_DrawContext *dc,
                                     int width, int height,
                                     const DP_ClassicBrush *brush,
                                     DP_BrushPreviewShape shape)
{
    DP_ASSERT(bp);
    DP_ASSERT(dc);
    DP_ASSERT(brush);
    render_brush_preview(bp, dc, width, height, shape, brush->color,
                         brush->smudge.max > 0.0f,
                         DP_classic_brush_blend_mode(brush),
                         set_preview_classic_brush, (void *)brush);
}

static void set_preview_mypaint_brush(void *user, DP_BrushEngine *be,
                                      DP_UPixelFloat color)
{
    const DP_MyPaintBrush *brush = ((void **)user)[0];
    const DP_MyPaintSettings *settings = ((void **)user)[1];
    DP_brush_engine_mypaint_brush_set(be, brush, settings, 1, false, color);
}

void DP_brush_preview_render_mypaint(DP_BrushPreview *bp, DP_DrawContext *dc,
                                     int width, int height,
                                     const DP_MyPaintBrush *brush,
                                     const DP_MyPaintSettings *settings,
                                     DP_BrushPreviewShape shape)
{
    DP_ASSERT(bp);
    DP_ASSERT(dc);
    DP_ASSERT(brush);
    DP_ASSERT(settings);
    DP_BlendMode mode = brush->erase      ? DP_BLEND_MODE_ERASE
                      : brush->lock_alpha ? DP_BLEND_MODE_RECOLOR
                                          : DP_BLEND_MODE_NORMAL_AND_ERASER;
    render_brush_preview(bp, dc, width, height, shape, brush->color, false,
                         mode, set_preview_mypaint_brush,
                         (void *[]){(void *)brush, (void *)settings});
}


void DP_brush_preview_render_flood_fill(DP_BrushPreview *bp,
                                        DP_UPixelFloat fill_color,
                                        double tolerance, int expand,
                                        int feather_radius, int blend_mode)
{
    DP_CanvasState *cs = bp->cs;
    if (!cs) {
        DP_warn("Preview flood fill: no canvas state");
        return;
    }

    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, 1);
    if (!lre) {
        DP_warn("Preview flood fill: layer not found");
        return;
    }

    int width = DP_canvas_state_width(cs);
    int height = DP_canvas_state_height(cs);
    int x, y;
    DP_Image *img;
    DP_FloodFillResult result = DP_flood_fill(
        cs, width / 2, height / 2, fill_color, tolerance, 1, false,
        width * height, expand, feather_radius, &img, &x, &y);
    if (result != DP_FLOOD_FILL_SUCCESS) {
        DP_warn("Preview flood fill: %s", DP_error());
        return;
    }

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
    DP_canvas_state_decref(cs);

    DP_TransientLayerContent *tlc =
        DP_layer_routes_entry_transient_content(lre, tcs);
    DP_transient_layer_content_put_image(tlc, 0, blend_mode, x, y, img);
    DP_image_free(img);

    bp->cs = DP_transient_canvas_state_persist(tcs);
}


DP_Image *DP_brush_preview_to_image(DP_BrushPreview *bp)
{
    DP_ASSERT(bp);
    DP_CanvasState *cs = bp->cs;
    return cs ? DP_canvas_state_to_flat_image(cs, DP_FLAT_IMAGE_RENDER_FLAGS,
                                              NULL, NULL)
              : NULL;
}


static void set_preview_pixel_dab(DP_UNUSED int count, DP_PixelDab *pds,
                                  void *user)
{
    DP_ASSERT(count == 1);
    const DP_ClassicBrush *cb = user;
    DP_pixel_dab_init(pds, 0, 0, 0,
                      DP_classic_brush_pixel_dab_size_at(cb, 1.0f),
                      DP_classic_brush_dab_opacity_at(cb, 1.0f));
}

static void set_preview_classic_dab(DP_UNUSED int count, DP_ClassicDab *cds,
                                    void *user)
{
    DP_ASSERT(count == 1);
    const DP_ClassicBrush *cb = user;
    DP_classic_dab_init(cds, 0, 0, 0,
                        DP_classic_brush_soft_dab_size_at(cb, 1.0f),
                        DP_classic_brush_dab_hardness_at(cb, 1.0f),
                        DP_classic_brush_dab_opacity_at(cb, 1.0f));
}

static DP_Message *get_preview_draw_dab_message(const DP_ClassicBrush *cb,
                                                int width, int height,
                                                uint32_t color)
{
    switch (cb->shape) {
    case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
        return DP_msg_draw_dabs_pixel_new(
            0, 1, DP_int_to_int32(width / 2), DP_int_to_int32(height / 2),
            color, DP_BLEND_MODE_NORMAL, set_preview_pixel_dab, 1, (void *)cb);
    case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
        return DP_msg_draw_dabs_pixel_square_new(
            0, 1, DP_int_to_int32(width / 2), DP_int_to_int32(height / 2),
            color, DP_BLEND_MODE_NORMAL, set_preview_pixel_dab, 1, (void *)cb);
    default:
        DP_ASSERT(cb->shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND);
        return DP_msg_draw_dabs_classic_new(
            0, 1, DP_int_to_int32(width * 4 / 2),
            DP_int_to_int32(height * 4 / 2), color, DP_BLEND_MODE_NORMAL,
            set_preview_classic_dab, 1, (void *)cb);
    }
}

DP_Image *DP_classic_brush_preview_dab(const DP_ClassicBrush *cb,
                                       DP_DrawContext *dc, int width,
                                       int height, uint32_t color)
{
    DP_ASSERT(cb);

    DP_CanvasState *cs = DP_canvas_state_new();
    cs = handle_preview_message_dec(
        cs, dc, DP_msg_canvas_resize_new(0, 0, width, height, 0));
    cs = handle_preview_message_dec(
        cs, dc, DP_msg_layer_create_new(0, 1, 0, 0, 0, 0, "", 0));
    cs = handle_preview_message_dec(
        cs, dc, get_preview_draw_dab_message(cb, width, height, color));

    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
    DP_canvas_state_decref(cs);
    return img;
}
