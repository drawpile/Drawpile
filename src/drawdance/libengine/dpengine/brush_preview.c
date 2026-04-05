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


#define PI_FLOAT   ((float)M_PI)
#define DELTA_MSEC 20


struct DP_BrushPreview {
    DP_BrushEngine *be;
    DP_CanvasState *cs;
    DP_Vector messages;
    uint32_t foreground;
    uint32_t background;
    uint32_t smudge;
    union {
        int dummy; // To stop the compiler from whinging about initialization.
        DP_ClassicBrush classic;
        struct {
            DP_MyPaintBrush brush;
            DP_MyPaintSettings settings;
        } mypaint;
    };
};

static void push_message(void *user, DP_Message *msg)
{
    DP_BrushPreview *bp = user;
    DP_VECTOR_PUSH_TYPE(&bp->messages, DP_Message *, msg);
}

DP_BrushPreview *DP_brush_preview_new(void)
{
    DP_BrushPreview *bp = DP_malloc(sizeof(*bp));
    *bp = (DP_BrushPreview){
        DP_brush_engine_new(NULL, push_message, NULL, NULL, bp),
        NULL,
        DP_VECTOR_NULL,
        0xff000000u,
        0xffffffffu,
        0xffffffffu,
        {0},
    };
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

void DP_brush_preview_palette_set(DP_BrushPreview *bp, uint32_t foreground,
                                  uint32_t background, uint32_t smudge)
{
    DP_ASSERT(bp);
    bp->foreground = foreground;
    bp->background = background;
    bp->smudge = smudge;
}

void DP_brush_preview_size_limit_set(DP_BrushPreview *bp, int limit)
{
    DP_ASSERT(bp);
    DP_brush_engine_size_limit_set(bp->be, limit);
}


static DP_CanvasState *handle_preview_message_dec(DP_CanvasState *cs,
                                                  DP_DrawContext *dc,
                                                  DP_Message *msg)
{
    DP_CanvasState *next = DP_canvas_state_handle(cs, dc, NULL, msg);
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
        DP_canvas_state_handle_multidab(cs, dc, NULL, count, msgs);
    if (next) {
        DP_canvas_state_decref(cs);
        return next;
    }
    else {
        return cs;
    }
}


static uint32_t hsv_to_bgra(float h, float s, float v)
{
    hsv_to_rgb_float(&h, &s, &v);
    DP_UPixelFloat pixel = {v, s, h, 1.0f};
    return DP_pixel8_premultiply(DP_upixel_float_to_8(pixel)).color;
}

static void set_preview_foreground_dab(DP_UNUSED int count, DP_PixelDab *pds,
                                       void *user)
{
    DP_ASSERT(count == 1);
    int *d_ptr = user;
    DP_pixel_dab_init(pds, 0, 0, 0, DP_int_to_uint8(*d_ptr), 255);
}

static DP_CanvasState *draw_foreground_dabs(DP_CanvasState *cs,
                                            DP_DrawContext *dc, int width,
                                            int height)
{
    int d = CLAMP(height * 2 / 3, 10, 255);
    int x0 = d;
    int x1 = width - d;
    int step = d * 70 / 100;
    float huestep =
        359.0f / 360.0f / (DP_int_to_float(x1 - x0) / DP_int_to_float(step));
    float hue = 0.0f;
    for (int x = x0; x < x1; x += step, hue += huestep) {
        cs = handle_preview_message_dec(
            cs, dc,
            DP_msg_draw_dabs_pixel_new(
                0, 0, 1, DP_int_to_int32(x), DP_int_to_int32(height / 2),
                // Set the alpha to zero to draw in direct mode.
                hsv_to_bgra(hue, 0.62f, 0.86f) & 0x00ffffffu,
                DP_BLEND_MODE_NORMAL, set_preview_foreground_dab, 1, &d));
    }
    return cs;
}

static void stroke_to_time(DP_BrushEngine *be, float x, float y, float pressure,
                           DP_CanvasState *cs, long long dt,
                           long long *in_out_time_msec)
{
    DP_BrushPoint bp = {x, y, pressure, 0.0f, 0.0f, 0.0f, *in_out_time_msec};
    DP_brush_engine_stroke_to(be, bp, cs);
    *in_out_time_msec += dt;
}

static void stroke_to(DP_BrushEngine *be, float x, float y, float pressure,
                      DP_CanvasState *cs, long long *in_out_time_msec)
{
    stroke_to_time(be, x, y, pressure, cs, DELTA_MSEC, in_out_time_msec);
}

static void stroke_freehand(DP_BrushEngine *be, DP_CanvasState *cs,
                            DP_Rect rect, long long *in_out_time_msec)
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
        if (p > 0.5f) {
            p = 1.0f - p;
        }
        float raw_pressure = p / 0.5f + 0.1f;
        float pressure = CLAMP(raw_pressure, 0.0f, 1.0f);

        long long dt = DP_float_to_llong((1.0f - pressure) * 20.0f + 0.1f);
        float y = sinf(phase) * h;
        stroke_to_time(be, rxf + xf, offy + y, pressure, cs, dt,
                       in_out_time_msec);
        phase += dphase;
    }
}

static void stroke_line(DP_BrushEngine *be, DP_CanvasState *cs, DP_Rect rect,
                        long long *in_out_time_msec)
{
    stroke_to(be, DP_int_to_float(DP_rect_left(rect)),
              DP_int_to_float(DP_rect_bottom(rect)), 1.0f, cs,
              in_out_time_msec);
    stroke_to(be, DP_int_to_float(DP_rect_right(rect)),
              DP_int_to_float(DP_rect_top(rect)), 1.0f, cs, in_out_time_msec);
}

static void stroke_rectangle(DP_BrushEngine *be, DP_CanvasState *cs,
                             DP_Rect rect, long long *in_out_time_msec)
{
    float l = DP_int_to_float(DP_rect_left(rect));
    float r = DP_int_to_float(DP_rect_right(rect));
    float t = DP_int_to_float(DP_rect_top(rect));
    float b = DP_int_to_float(DP_rect_bottom(rect));
    stroke_to(be, l, t, 1.0f, cs, in_out_time_msec);
    stroke_to(be, r, t, 1.0f, cs, in_out_time_msec);
    stroke_to(be, r, b, 1.0f, cs, in_out_time_msec);
    stroke_to(be, l, b, 1.0f, cs, in_out_time_msec);
    stroke_to(be, l, t, 1.0f, cs, in_out_time_msec);
}

static void stroke_ellipse(DP_BrushEngine *be, DP_CanvasState *cs, DP_Rect rect,
                           long long *in_out_time_msec)
{
    float a = DP_int_to_float(DP_rect_width(rect)) / 2.0f;
    float b = DP_int_to_float(DP_rect_height(rect)) / 2.0f;
    float cx = DP_int_to_float(DP_rect_x(rect)) + a;
    float cy = DP_int_to_float(DP_rect_y(rect)) + b;
    for (int i = 0; i <= 40; ++i) {
        float t = PI_FLOAT / 20.0f * DP_int_to_float(i);
        stroke_to(be, cx + a * cosf(t), cy + b * sinf(t), 1.0f, cs,
                  in_out_time_msec);
    }
}

static void canvas_background_image_set(DP_UNUSED size_t size,
                                        unsigned char *data, void *user)
{
    DP_ASSERT(size == 4);
    DP_UPixel8 p = {*(uint32_t *)user};
    data[0] = p.b;
    data[1] = p.g;
    data[2] = p.r;
    data[3] = p.a;
}

void render_brush_preview(DP_BrushPreview *bp, DP_DrawContext *dc, int width,
                          int height, DP_BrushPreviewStyle style,
                          DP_BrushPreviewShape shape,
                          DP_UPixelFloat initial_color,
                          void (*set_brush)(DP_BrushPreview *, DP_UPixelFloat,
                                            bool))
{
    DP_ASSERT(bp);
    DP_ASSERT(dc);
    DP_canvas_state_decref_nullable(bp->cs);
    if (width <= 0 || height <= 0) {
        bp->cs = NULL;
        return;
    }

    uint32_t background_color;
    DP_UPixelFloat stroke_color;
    DP_UPixelFloat override_smudge_color;
    bool want_foreground_dabs;
    bool have_override_smudge_color;
    DP_BrushPreviewShape effective_shape;
    switch (style) {
    case DP_BRUSH_PREVIEW_STYLE_FULL:
        background_color = 0u;
        stroke_color = initial_color;
        want_foreground_dabs = true;
        have_override_smudge_color = false;
        effective_shape = shape;
        break;
    default:
        background_color = bp->background;
        stroke_color = DP_upixel8_to_float((DP_UPixel8){bp->foreground});
        override_smudge_color = DP_upixel8_to_float((DP_UPixel8){bp->smudge});
        have_override_smudge_color = true;
        want_foreground_dabs = false;
        effective_shape = DP_BRUSH_PREVIEW_STROKE;
        break;
    }

    DP_CanvasState *cs = DP_canvas_state_new();
    cs = handle_preview_message_dec(
        cs, dc,
        DP_msg_canvas_resize_new(0, 0, DP_int_to_int32(width),
                                 DP_int_to_int32(height), 0));
    if (background_color != 0u) {
        cs = handle_preview_message_dec(
            cs, dc,
            DP_msg_canvas_background_zstd_new(0, canvas_background_image_set, 4,
                                              &background_color));
    }
    cs = handle_preview_message_dec(
        cs, dc, DP_msg_layer_tree_create_new(0, 1, 0, 0, 0u, 0, "", 0));

    if (want_foreground_dabs) {
        cs = draw_foreground_dabs(cs, dc, width, height);
    }

    set_brush(bp, stroke_color, shape == DP_BRUSH_PREVIEW_STROKE);

    DP_BrushEngine *be = bp->be;
    DP_brush_engine_random_seed_set(be, 1L);
    DP_brush_engine_override_smudge_color_set(
        be, have_override_smudge_color ? &override_smudge_color : NULL);
    DP_brush_engine_stroke_begin(be, cs, 1, false, false, false, false, 1.0f,
                                 0.0f);

    int padding_x = DP_min_int(width / 8, 24);
    int padding_y = DP_min_int(height / 3, 24);
    DP_Rect rect = DP_rect_make(padding_x, padding_y, width - (padding_x * 2),
                                height - (padding_y * 2));

    long long time_msec = 0;
    switch (effective_shape) {
    case DP_BRUSH_PREVIEW_STROKE:
        stroke_freehand(be, cs, rect, &time_msec);
        break;
    case DP_BRUSH_PREVIEW_LINE:
        stroke_line(be, cs, rect, &time_msec);
        break;
    case DP_BRUSH_PREVIEW_RECTANGLE:
        stroke_rectangle(be, cs, rect, &time_msec);
        break;
    case DP_BRUSH_PREVIEW_ELLIPSE:
        stroke_ellipse(be, cs, rect, &time_msec);
        break;
    default:
        DP_warn("Unknown preview shape %d", (int)shape);
        break;
    }
    DP_brush_engine_stroke_end(be, time_msec, cs, false);
    DP_brush_engine_dabs_flush(be);

    cs = handle_preview_messages_multidab(
        cs, dc, DP_size_to_int(bp->messages.used), bp->messages.elements);
    DP_VECTOR_CLEAR_TYPE(&bp->messages, DP_Message *, dispose_message);

    bp->cs = handle_preview_message_dec(cs, dc, DP_msg_pen_up_new(1, 1));
}

static DP_BrushEngineStrokeParams
preview_stroke_params(bool allow_pixel_perfect)
{
    return (DP_BrushEngineStrokeParams){
        {0, 0, false, false, false}, NULL, 0.0, 0, 1, 0, false, false,
        allow_pixel_perfect};
}

static void set_preview_classic_brush(DP_BrushPreview *bp, DP_UPixelFloat color,
                                      bool allow_pixel_perfect)
{
    DP_BrushEngineStrokeParams besp =
        preview_stroke_params(allow_pixel_perfect);
    DP_brush_engine_classic_brush_set(bp->be, &bp->classic, &besp, &color,
                                      false);
}

static DP_BlendMode plainify_blend_mode(DP_BlendMode blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_ERASE:
    case DP_BLEND_MODE_ERASE_PRESERVE:
    case DP_BLEND_MODE_COLOR_ERASE:
    case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
        return DP_BLEND_MODE_ERASE;
    case DP_BLEND_MODE_NORMAL_AND_ERASER:
    case DP_BLEND_MODE_PIGMENT_AND_ERASER:
    case DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER:
        return DP_BLEND_MODE_NORMAL_AND_ERASER;
    case DP_BLEND_MODE_MARKER:
    case DP_BLEND_MODE_MARKER_WASH:
    case DP_BLEND_MODE_MARKER_ALPHA:
    case DP_BLEND_MODE_MARKER_ALPHA_WASH:
        return DP_BLEND_MODE_MARKER_ALPHA;
    case DP_BLEND_MODE_GREATER:
    case DP_BLEND_MODE_GREATER_WASH:
    case DP_BLEND_MODE_GREATER_ALPHA:
    case DP_BLEND_MODE_GREATER_ALPHA_WASH:
        return DP_BLEND_MODE_GREATER_ALPHA;
    default:
        return DP_BLEND_MODE_NORMAL;
    }
}

void DP_brush_preview_render_classic(DP_BrushPreview *bp, DP_DrawContext *dc,
                                     int width, int height,
                                     const DP_ClassicBrush *brush,
                                     DP_BrushPreviewStyle style,
                                     DP_BrushPreviewShape shape)
{
    DP_ASSERT(bp);
    DP_ASSERT(dc);
    DP_ASSERT(brush);

    DP_classic_brush_clone(&bp->classic, brush);
    if (style == DP_BRUSH_PREVIEW_STYLE_PLAIN) {
        bp->classic.brush_mode =
            plainify_blend_mode(DP_classic_brush_blend_mode(&bp->classic));
        bp->classic.erase = false;
    }

    render_brush_preview(bp, dc, width, height, style, shape, brush->color,
                         set_preview_classic_brush);
}

static void set_preview_mypaint_brush(DP_BrushPreview *bp, DP_UPixelFloat color,
                                      bool allow_pixel_perfect)
{
    DP_BrushEngineStrokeParams besp =
        preview_stroke_params(allow_pixel_perfect);
    DP_brush_engine_mypaint_brush_set(bp->be, &bp->mypaint.brush,
                                      &bp->mypaint.settings, &besp, &color,
                                      false);
}

static void preview_disable_mypaint_setting(DP_BrushPreview *bp, int setting,
                                            float base_value)
{
    DP_MyPaintMapping *mapping = &bp->mypaint.settings.mappings[setting];
    mapping->base_value = base_value;
    for (int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i) {
        mapping->inputs[i].n = 0;
    }
}

void DP_brush_preview_render_mypaint(DP_BrushPreview *bp, DP_DrawContext *dc,
                                     int width, int height,
                                     const DP_MyPaintBrush *brush,
                                     const DP_MyPaintSettings *settings,
                                     DP_BrushPreviewStyle style,
                                     DP_BrushPreviewShape shape)
{
    DP_ASSERT(bp);
    DP_ASSERT(dc);
    DP_ASSERT(brush);
    DP_ASSERT(settings);

    DP_mypaint_brush_clone(&bp->mypaint.brush, brush);
    DP_mypaint_settings_clone(&bp->mypaint.settings, settings);
    if (style == DP_BRUSH_PREVIEW_STYLE_PLAIN) {
        bp->mypaint.brush.brush_mode = plainify_blend_mode(
            DP_mypaint_brush_blend_mode(&bp->mypaint.brush));
        bp->mypaint.brush.erase = false;

        preview_disable_mypaint_setting(bp, MYPAINT_BRUSH_SETTING_ERASER, 0.0f);
        preview_disable_mypaint_setting(bp, MYPAINT_BRUSH_SETTING_LOCK_ALPHA,
                                        0.0f);
        preview_disable_mypaint_setting(bp, MYPAINT_BRUSH_SETTING_COLORIZE,
                                        0.0f);
        preview_disable_mypaint_setting(bp, MYPAINT_BRUSH_SETTING_POSTERIZE,
                                        0.0f);
        preview_disable_mypaint_setting(
            bp, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_H, 0.0f);
        preview_disable_mypaint_setting(
            bp, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSV_S, 0.0f);
        preview_disable_mypaint_setting(
            bp, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSL_S, 0.0f);
        preview_disable_mypaint_setting(
            bp, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_V, 0.0f);
        preview_disable_mypaint_setting(
            bp, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_L, 0.0f);
    }

    render_brush_preview(bp, dc, width, height, style, shape, brush->color,
                         set_preview_mypaint_brush);
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
    DP_pixel_dab_init(
        pds, 0, 0, 0,
        DP_classic_brush_pixel_dab_size_at(cb, 1.0f, HUGE_VALF, HUGE_VALF,
                                           false),
        DP_classic_brush_dab_opacity_at(cb, 1.0f, HUGE_VALF, HUGE_VALF));
}

static void set_preview_classic_dab(DP_UNUSED int count, DP_ClassicDab *cds,
                                    void *user)
{
    DP_ASSERT(count == 1);
    const DP_ClassicBrush *cb = user;
    DP_classic_dab_init(
        cds, 0, 0, 0,
        DP_classic_brush_soft_dab_size_at(cb, 1.0f, HUGE_VALF, HUGE_VALF,
                                          false),
        DP_classic_brush_dab_hardness_at(cb, 1.0f, HUGE_VALF, HUGE_VALF),
        DP_classic_brush_dab_opacity_at(cb, 1.0f, HUGE_VALF, HUGE_VALF));
}

static DP_Message *get_preview_draw_dab_message(const DP_ClassicBrush *cb,
                                                int width, int height,
                                                uint32_t color)
{
    switch (cb->shape) {
    case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
        return DP_msg_draw_dabs_pixel_new(
            0, (uint8_t)cb->paint_mode, 1, DP_int_to_int32(width / 2),
            DP_int_to_int32(height / 2), color, DP_BLEND_MODE_NORMAL,
            set_preview_pixel_dab, 1, (void *)cb);
    case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
        return DP_msg_draw_dabs_pixel_square_new(
            0, (uint8_t)cb->paint_mode, 1, DP_int_to_int32(width / 2),
            DP_int_to_int32(height / 2), color, DP_BLEND_MODE_NORMAL,
            set_preview_pixel_dab, 1, (void *)cb);
    default:
        DP_ASSERT(cb->shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND);
        return DP_msg_draw_dabs_classic_new(
            0, (uint8_t)cb->paint_mode, 1, DP_int_to_int32(width * 4 / 2),
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
        cs, dc, DP_msg_layer_tree_create_new(0, 1, 0, 0, 0, 0, "", 0));
    cs = handle_preview_message_dec(
        cs, dc, get_preview_draw_dab_message(cb, width, height, color));

    DP_Image *img = DP_canvas_state_to_flat_image(
        cs, DP_FLAT_IMAGE_RENDER_FLAGS, NULL, NULL);
    DP_canvas_state_decref(cs);
    return img;
}
