/*
 * Copyright (C) 2022 askmeaboufoom
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
#include "brush_engine.h"
#include "brush.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "layer_content.h"
#include "layer_routes.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/perf.h>
#include <dpmsg/message.h>
#include <math.h>
#include <mypaint-brush.h>
#include <mypaint.h>
#include <helpers.h> // RGB <-> HSV conversion, CLAMP, mod_arith

#define DP_PERF_CONTEXT "brush_engine"


// Same amount of smudge buckets that MyPaint uses.
#define SMUDGE_BUCKET_COUNT 256
#define MIN_DABS_CAPACITY   1024
#define MAX_XY_DELTA        127

typedef enum DP_BrushEngineActiveType {
    DP_BRUSH_ENGINE_ACTIVE_PIXEL,
    DP_BRUSH_ENGINE_ACTIVE_SOFT,
    DP_BRUSH_ENGINE_ACTIVE_MYPAINT,
} DP_BrushEngineActiveType;

typedef struct DP_BrushEnginePixelDab {
    int8_t x;
    int8_t y;
    uint8_t size;
    uint8_t opacity;
} DP_BrushEnginePixelDab;

typedef struct DP_BrushEngineClassicDab {
    int8_t x;
    int8_t y;
    uint16_t size;
    uint8_t hardness;
    uint8_t opacity;
} DP_BrushEngineClassicDab;

typedef struct DP_BrushEngineMyPaintDab {
    int8_t x;
    int8_t y;
    uint16_t size;
    uint8_t hardness;
    uint8_t opacity;
    uint8_t angle;
    uint8_t aspect_ratio;
} DP_BrushEngineMyPaintDab;

struct DP_BrushEngine {
    int layer_id;
    unsigned int context_id;
    DP_LayerContent *lc;
    DP_CanvasState *cs;
    DP_BrushStampBuffer stamp_buffer;
    int last_diameter;
    DP_BrushEngineActiveType active;
    MyPaintBrush *mypaint_brush;
    MyPaintSurface2 mypaint_surface2;
    bool in_progress;
    union {        // Active type decides which of these is relevant.
        int dummy; // Make this initializable without the compiler whining.
        struct {
            DP_ClassicBrush brush;
            union {
                int pixel_length;
                float soft_length;
            };
            int smudge_distance;
            DP_UPixelFloat brush_color;
            DP_UPixelFloat smudge_color;
            float last_x;
            float last_y;
            float last_pressure;
            int32_t dab_x;
            int32_t dab_y;
            uint32_t dab_color;
        } classic;
        struct {
            bool lock_alpha;
            bool erase;
            uint8_t dab_lock_alpha;
            int32_t dab_x;
            int32_t dab_y;
            uint32_t dab_color;
        } mypaint;
    };
    struct {
        int32_t last_x;
        int32_t last_y;
        int used;
        size_t capacity;
        void *buffer;
    } dabs;
    DP_BrushEnginePushMessageFn push_message;
    void *user;
};


static bool delta_xy(DP_BrushEngine *be, int32_t dab_x, int32_t dab_y,
                     int8_t *out_dx, int8_t *out_dy)
{
    int dx = dab_x - be->dabs.last_x;
    int dy = dab_y - be->dabs.last_y;
    if (abs(dx) <= MAX_XY_DELTA && abs(dy) <= MAX_XY_DELTA) {
        *out_dx = DP_int32_to_int8(dx);
        *out_dy = DP_int32_to_int8(dy);
        return true;
    }
    else {
        return false;
    }
}

static void *get_dab_buffer(DP_BrushEngine *be, size_t element_size)
{
    size_t capacity = be->dabs.capacity;
    int used = be->dabs.used;
    int max_used = DP_size_to_int(capacity / element_size);
    if (used == max_used) {
        size_t new_capacity = DP_max_size(capacity * 2, MIN_DABS_CAPACITY);
        DP_ASSERT(DP_size_to_int(new_capacity / element_size) > max_used);
        be->dabs.buffer = DP_realloc(be->dabs.buffer, new_capacity);
        be->dabs.capacity = new_capacity;
    }
    else {
        DP_ASSERT(used < max_used);
    }
    return be->dabs.buffer;
}

static uint32_t combine_rgba(float r, float g, float b, float a)
{
    return DP_float_to_uint32(a * 255.0f + 0.5f) << 24u
         | DP_float_to_uint32(r * 255.0f + 0.5f) << 16u
         | DP_float_to_uint32(g * 255.0f + 0.5f) << 8u
         | DP_float_to_uint32(b * 255.0f + 0.5f);
}

static uint32_t combine_upixel_float(DP_UPixelFloat pixel)
{
    return combine_rgba(pixel.r, pixel.g, pixel.b, pixel.a);
}

static uint8_t get_uint8(float input)
{
    float value = input * 255.0f + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 0, UINT8_MAX));
}


static void add_dab_pixel(DP_BrushEngine *be, DP_ClassicBrush *cb, int x, int y,
                          float pressure)
{
    uint8_t dab_size = DP_classic_brush_pixel_dab_size_at(cb, pressure);
    uint8_t dab_opacity = DP_classic_brush_dab_opacity_at(cb, pressure);
    if (dab_size > 0 && dab_opacity > 0) {
        int32_t dab_x = DP_int_to_int32(x);
        int32_t dab_y = DP_int_to_int32(y);
        uint32_t dab_color = combine_upixel_float(be->classic.smudge_color);

        int used = be->dabs.used;
        int8_t dx, dy;
        bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_PIXEL_DABS_MAX
                       && be->classic.dab_color == dab_color
                       && delta_xy(be, dab_x, dab_y, &dx, &dy);
        be->dabs.last_x = dab_x;
        be->dabs.last_y = dab_y;

        if (!can_append) {
            DP_brush_engine_dabs_flush(be);
            be->classic.dab_x = dab_x;
            be->classic.dab_y = dab_y;
            be->classic.dab_color = dab_color;
            dx = 0;
            dy = 0;
        }

        DP_BrushEnginePixelDab *dabs = get_dab_buffer(be, sizeof(*dabs));
        dabs[be->dabs.used++] =
            (DP_BrushEnginePixelDab){dx, dy, dab_size, dab_opacity};
    }
}


static void add_dab_soft(DP_BrushEngine *be, DP_ClassicBrush *cb, float x,
                         float y, float pressure)
{
    uint16_t dab_size = DP_classic_brush_soft_dab_size_at(cb, pressure);
    uint8_t dab_opacity = DP_classic_brush_dab_opacity_at(cb, pressure);
    // Disregard infinitesimal or fully opaque dabs. 26 is a radius of 0.1.
    if (dab_size >= 26 && dab_opacity > 0) {
        int32_t dab_x = DP_float_to_int32(x * 4.0f);
        int32_t dab_y = DP_float_to_int32(y * 4.0f);
        uint32_t dab_color = combine_upixel_float(be->classic.smudge_color);

        int used = be->dabs.used;
        int8_t dx, dy;
        bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_CLASSIC_DABS_MAX
                       && be->classic.dab_color == dab_color
                       && delta_xy(be, dab_x, dab_y, &dx, &dy);
        be->dabs.last_x = dab_x;
        be->dabs.last_y = dab_y;

        if (!can_append) {
            DP_brush_engine_dabs_flush(be);
            be->classic.dab_x = dab_x;
            be->classic.dab_y = dab_y;
            be->classic.dab_color = dab_color;
            dx = 0;
            dy = 0;
        }

        DP_BrushEngineClassicDab *dabs = get_dab_buffer(be, sizeof(*dabs));
        dabs[be->dabs.used++] = (DP_BrushEngineClassicDab){
            dx, dy, dab_size, DP_classic_brush_dab_hardness_at(cb, pressure),
            dab_opacity};
    }
}


static void init_mypaint_once(void)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(lock);
    static bool init = false;
    if (!init) {
        DP_atomic_lock(&lock);
        if (!init) {
            mypaint_init();
            init = true;
        }
        DP_atomic_unlock(&lock);
    }
}

static DP_BrushEngine *get_mypaint_surface_brush_engine(MyPaintSurface2 *self)
{
    unsigned char *bytes = (unsigned char *)self;
    return (DP_BrushEngine *)(bytes
                              - offsetof(DP_BrushEngine, mypaint_surface2));
}

static uint32_t get_mypaint_dab_color(DP_BrushEngine *be, float color_r,
                                      float color_g, float color_b,
                                      float alpha_eraser)
{
    if (be->mypaint.erase) {
        return 0;
    }
    else {
        return combine_rgba(color_r, color_g, color_b, alpha_eraser);
    }
}

static uint8_t get_mypaint_dab_lock_alpha(DP_BrushEngine *be, float lock_alpha)
{
    if (be->mypaint.erase) {
        return 0;
    }
    else if (be->mypaint.lock_alpha) {
        return 255;
    }
    else {
        return DP_float_to_uint8(lock_alpha * 255.0f + 0.5f);
    }
}

static uint16_t get_mypaint_dab_size(float radius)
{
    float value = radius * 512.0f + 0.5f;
    return DP_float_to_uint16(CLAMP(value, 0, UINT16_MAX));
}

// Reduce an arbitrary angle in degrees to a value between 0 and 255.
// That puts the angle resolution at around 1.4 degrees, which is good enough.
static uint8_t get_mypaint_dab_angle(float angle)
{
    float value = mod_arith(angle, 360.0f) / 360.0f * 255.0f + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 0, UINT8_MAX));
}

// Spread the aspect ratio into a range between 0.1 and 10.0, then into a byte.
static uint8_t get_mypaint_dab_aspect_ratio(float aspect_ratio)
{
    return DP_float_to_uint8((CLAMP(aspect_ratio, 0.1f, 10.0f) - 0.1f) * 25.755f
                             + 0.5f);
}

static int
add_dab_mypaint_pigment(MyPaintSurface2 *self, float x, float y, float radius,
                        float color_r, float color_g, float color_b,
                        float opaque, float hardness, float alpha_eraser,
                        float aspect_ratio, float angle, float lock_alpha,
                        DP_UNUSED float colorize, DP_UNUSED float posterize,
                        DP_UNUSED float posterize_num, DP_UNUSED float paint)
{
    // A radius less than 0.1 pixels is infinitesimally small. A hardness or
    // opacity of zero means the mask is completely blank. Disregard the dab
    // in those cases. MyPaint uses the same logic for this.
    if (radius >= 0.1f && hardness > 0.0f && opaque > 0.0f) {
        DP_BrushEngine *be = get_mypaint_surface_brush_engine(self);
        int32_t dab_x = DP_float_to_int32(x * 4.0f);
        int32_t dab_y = DP_float_to_int32(y * 4.0f);
        uint32_t dab_color =
            get_mypaint_dab_color(be, color_r, color_g, color_b, alpha_eraser);
        uint8_t dab_lock_alpha = get_mypaint_dab_lock_alpha(be, lock_alpha);

        int used = be->dabs.used;
        int8_t dx, dy;
        bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_MYPAINT_DABS_MAX
                       && be->mypaint.dab_color == dab_color
                       && be->mypaint.dab_lock_alpha == dab_lock_alpha
                       && delta_xy(be, dab_x, dab_y, &dx, &dy);
        be->dabs.last_x = dab_x;
        be->dabs.last_y = dab_y;

        if (!can_append) {
            DP_brush_engine_dabs_flush(be);
            be->mypaint.dab_x = dab_x;
            be->mypaint.dab_y = dab_y;
            be->mypaint.dab_color = dab_color;
            be->mypaint.dab_lock_alpha = dab_lock_alpha;
            dx = 0;
            dy = 0;
        }

        DP_BrushEngineMyPaintDab *dabs = get_dab_buffer(be, sizeof(*dabs));
        dabs[be->dabs.used++] = (DP_BrushEngineMyPaintDab){
            dx,
            dy,
            get_mypaint_dab_size(radius),
            get_uint8(hardness),
            get_uint8(opaque),
            get_mypaint_dab_angle(angle),
            get_mypaint_dab_aspect_ratio(aspect_ratio)};
        return 1;
    }
    else {
        return 0;
    }
}

static int add_dab_mypaint(MyPaintSurface *self, float x, float y, float radius,
                           float color_r, float color_g, float color_b,
                           float opaque, float hardness, float alpha_eraser,
                           float aspect_ratio, float angle, float lock_alpha,
                           float colorize)
{
    return add_dab_mypaint_pigment((MyPaintSurface2 *)self, x, y, radius,
                                   color_r, color_g, color_b, opaque, hardness,
                                   alpha_eraser, aspect_ratio, angle,
                                   lock_alpha, colorize, 0.0f, 0.0f, 0.0f);
}

static void get_color_mypaint_pigment(MyPaintSurface2 *self, float x, float y,
                                      float radius, float *color_r,
                                      float *color_g, float *color_b,
                                      float *color_a, DP_UNUSED float paint)
{
    DP_BrushEngine *be = get_mypaint_surface_brush_engine(self);
    DP_LayerContent *lc = be->lc;
    if (lc) {
        int diameter = DP_min_int(DP_float_to_int(radius * 2.0f + 0.5f), 255);
        DP_UPixel15 color = DP_layer_content_sample_color_at(
            lc, be->stamp_buffer, DP_float_to_int(x + 0.5f),
            DP_float_to_int(y + 0.5f), diameter, &be->last_diameter);
        *color_r = DP_channel15_to_float(color.r);
        *color_g = DP_channel15_to_float(color.g);
        *color_b = DP_channel15_to_float(color.b);
        *color_a = DP_channel15_to_float(color.a);
    }
    else {
        *color_r = 0.0f;
        *color_g = 0.0f;
        *color_b = 0.0f;
        *color_a = 0.0f;
    }
}

static void get_color_mypaint(MyPaintSurface *self, float x, float y,
                              float radius, float *color_r, float *color_g,
                              float *color_b, float *color_a)
{
    get_color_mypaint_pigment((MyPaintSurface2 *)self, x, y, radius, color_r,
                              color_g, color_b, color_a, 0.0f);
}


DP_BrushEngine *DP_brush_engine_new(DP_BrushEnginePushMessageFn push_message,
                                    void *user)
{
    init_mypaint_once();
    DP_BrushEngine *be = DP_malloc(sizeof(*be));
    *be = (DP_BrushEngine){
        0,
        0,
        NULL,
        NULL,
        {0},
        -1,
        DP_BRUSH_ENGINE_ACTIVE_PIXEL,
        mypaint_brush_new_with_buckets(SMUDGE_BUCKET_COUNT),
        {{add_dab_mypaint, get_color_mypaint, NULL, NULL, NULL, NULL, 0},
         add_dab_mypaint_pigment,
         get_color_mypaint_pigment,
         NULL},
        false,
        {0},
        {0, 0, 0, 0, NULL},
        push_message,
        user};
    return be;
}

void DP_brush_engine_free(DP_BrushEngine *be)
{
    if (be) {
        DP_free(be->dabs.buffer);
        mypaint_brush_unref(be->mypaint_brush);
        DP_layer_content_decref_nullable(be->lc);
        DP_free(be);
    }
}


void DP_brush_engine_classic_brush_set(DP_BrushEngine *be,
                                       const DP_ClassicBrush *brush,
                                       int layer_id, DP_UPixelFloat color)
{
    DP_ASSERT(be);
    DP_ASSERT(brush);
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);

    be->layer_id = layer_id;

    switch (brush->shape) {
    case DP_CLASSIC_BRUSH_SHAPE_PIXEL_ROUND:
    case DP_CLASSIC_BRUSH_SHAPE_PIXEL_SQUARE:
        be->active = DP_BRUSH_ENGINE_ACTIVE_PIXEL;
        break;
    case DP_CLASSIC_BRUSH_SHAPE_SOFT_ROUND:
        be->active = DP_BRUSH_ENGINE_ACTIVE_SOFT;
        break;
    }

    be->classic.brush = *brush;
    DP_ClassicBrush *cb = &be->classic.brush;
    if (cb->incremental || cb->smudge.max > 0.0f) {
        // Incremental mode must be used when smudging, because color is not
        // picked up from sublayers
        color.a = 0.0f;
    }
    else {
        // If brush color alpha is nonzero, indirect drawing mode
        // is used and the alpha is used as the overall transparency
        // of the entire stroke.
        // TODO this doesn't work right. We should use alpha-darken mode
        // and set the opacity range properly
        color.a = cb->opacity.max;
        cb->opacity.max = 1.0f;
        if (cb->opacity_pressure) {
            cb->opacity.min = 0.0f;
        }
    }
    be->classic.brush_color = color;
}

void DP_brush_engine_mypaint_brush_set(DP_BrushEngine *be,
                                       const DP_MyPaintBrush *brush,
                                       const DP_MyPaintSettings *settings,
                                       int layer_id, bool freehand,
                                       DP_UPixelFloat color)
{
    DP_ASSERT(be);
    DP_ASSERT(brush);
    DP_ASSERT(settings);
    DP_ASSERT(layer_id >= 0);
    DP_ASSERT(layer_id <= UINT16_MAX);
    be->layer_id = layer_id;
    be->active = DP_BRUSH_ENGINE_ACTIVE_MYPAINT;

    MyPaintBrush *mb = be->mypaint_brush;
    for (MyPaintBrushSetting s = 0; s < MYPAINT_BRUSH_SETTINGS_COUNT; ++s) {
        const DP_MyPaintMapping *mapping = &settings->mappings[s];
        mypaint_brush_set_base_value(mb, s, mapping->base_value);
        for (MyPaintBrushInput i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i) {
            const DP_MyPaintControlPoints *input = &mapping->inputs[i];
            int n = input->n;
            mypaint_brush_set_mapping_n(mb, s, i, n);
            for (int c = 0; c < n; ++c) {
                mypaint_brush_set_mapping_point(mb, s, i, c, input->xvalues[c],
                                                input->yvalues[c]);
            }
        }
    }

    float r_h = color.r;
    float g_s = color.g;
    float b_v = color.b;
    rgb_to_hsv_float(&r_h, &g_s, &b_v);
    mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_COLOR_H, r_h);
    mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_COLOR_S, g_s);
    mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_COLOR_V, b_v);

    be->mypaint.lock_alpha = brush->lock_alpha;
    be->mypaint.erase = brush->erase;

    // Slow tracking interferes with automated drawing of lines, shapes et
    // cetera, so we turn it off when it's not a living creature drawing.
    if (!freehand) {
        mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_SLOW_TRACKING,
                                     0.0f);
    }

    // We don't support spectral painting (aka Pigment mode), so we'll turn
    // that off at the source here. It's like turning the Pigment slider in
    // MyPaint all the way to zero.
    mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_PAINT_MODE, 0.0f);
}


static void set_pixel_dabs(int count, DP_PixelDab *out, void *user)
{
    DP_BrushEnginePixelDab *dabs = user;
    for (int i = 0; i < count; ++i) {
        DP_BrushEnginePixelDab *dab = &dabs[i];
        DP_pixel_dab_init(out, i, dab->x, dab->y, dab->size, dab->opacity);
    }
}

static void flush_pixel_dabs(DP_BrushEngine *be, int used)
{
    DP_Message *(*new_fn)(unsigned int, uint16_t, int32_t, int32_t, uint32_t,
                          uint8_t, void (*)(int, DP_PixelDab *, void *), int,
                          void *);
    if (be->classic.brush.shape == DP_CLASSIC_BRUSH_SHAPE_PIXEL_ROUND) {
        new_fn = DP_msg_draw_dabs_pixel_new;
    }
    else {
        new_fn = DP_msg_draw_dabs_pixel_square_new;
    }
    be->push_message(
        be->user, new_fn(be->context_id, DP_int_to_uint16(be->layer_id),
                         be->classic.dab_x, be->classic.dab_y,
                         be->classic.dab_color, (uint8_t)be->classic.brush.mode,
                         set_pixel_dabs, used, be->dabs.buffer));
}

static void set_soft_dabs(int count, DP_ClassicDab *out, void *user)
{
    DP_BrushEngineClassicDab *dabs = user;
    for (int i = 0; i < count; ++i) {
        DP_BrushEngineClassicDab *dab = &dabs[i];
        DP_classic_dab_init(out, i, dab->x, dab->y, dab->size, dab->hardness,
                            dab->opacity);
    }
}

static void flush_soft_dabs(DP_BrushEngine *be, int used)
{
    be->push_message(be->user,
                     DP_msg_draw_dabs_classic_new(
                         be->context_id, DP_int_to_uint16(be->layer_id),
                         be->classic.dab_x, be->classic.dab_y,
                         be->classic.dab_color, (uint8_t)be->classic.brush.mode,
                         set_soft_dabs, used, be->dabs.buffer));
}

static void set_mypaint_dabs(int count, DP_MyPaintDab *out, void *user)
{
    DP_BrushEngineMyPaintDab *dabs = user;
    for (int i = 0; i < count; ++i) {
        DP_BrushEngineMyPaintDab *dab = &dabs[i];
        DP_mypaint_dab_init(out, i, dab->x, dab->y, dab->size, dab->hardness,
                            dab->opacity, dab->angle, dab->aspect_ratio);
    }
}

static void flush_mypaint_dabs(DP_BrushEngine *be, int used)
{
    be->push_message(be->user,
                     DP_msg_draw_dabs_mypaint_new(
                         be->context_id, DP_int_to_uint16(be->layer_id),
                         be->mypaint.dab_x, be->mypaint.dab_y,
                         be->mypaint.dab_color, be->mypaint.dab_lock_alpha,
                         set_mypaint_dabs, used, be->dabs.buffer));
}

void DP_brush_engine_dabs_flush(DP_BrushEngine *be)
{
    DP_ASSERT(be);
    int used = be->dabs.used;
    if (used != 0) {
        switch (be->active) {
        case DP_BRUSH_ENGINE_ACTIVE_PIXEL:
            flush_pixel_dabs(be, used);
            break;
        case DP_BRUSH_ENGINE_ACTIVE_SOFT:
            flush_soft_dabs(be, used);
            break;
        case DP_BRUSH_ENGINE_ACTIVE_MYPAINT:
            flush_mypaint_dabs(be, used);
            break;
        default:
            DP_UNREACHABLE();
        }
    }
    be->dabs.used = 0;
}


void DP_brush_engine_stroke_begin(DP_BrushEngine *be, unsigned int context_id,
                                  bool push_undo_point)
{
    DP_ASSERT(be);
    DP_ASSERT(!be->in_progress);
    DP_ASSERT(be->dabs.used == 0);
    DP_PERF_BEGIN_DETAIL(fn, "stroke_begin", "active=%d", (int)be->active);

    be->context_id = context_id;
    if (push_undo_point) {
        be->push_message(be->user, DP_msg_undo_point_new(context_id));
    }

    DP_PERF_END(fn);
}


static int get_classic_smudge_diameter(const DP_ClassicBrush *cb,
                                       float pressure)
{
    int diameter =
        DP_float_to_int(DP_classic_brush_size_at(cb, pressure) * 2.0f + 0.5f);
    return CLAMP(diameter, 2, 255);
}

static DP_UPixelFloat sample_classic_smudge(DP_BrushEngine *be,
                                            DP_ClassicBrush *cb,
                                            DP_LayerContent *lc, float x,
                                            float y, float pressure)
{
    int diameter = get_classic_smudge_diameter(cb, pressure);
    return DP_upixel15_to_float(DP_layer_content_sample_color_at(
        lc, be->stamp_buffer, DP_float_to_int(x), DP_float_to_int(y), diameter,
        &be->last_diameter));
}

static void update_classic_smudge(DP_BrushEngine *be, DP_ClassicBrush *cb,
                                  DP_LayerContent *lc, float x, float y,
                                  float pressure)
{
    float smudge = DP_classic_brush_smudge_at(cb, pressure);
    int smudge_distance = ++be->classic.smudge_distance;
    if (smudge > 0.0f && smudge_distance > cb->resmudge && lc) {
        DP_UPixelFloat sample =
            sample_classic_smudge(be, cb, lc, x, y, pressure);
        if (sample.a > 0.0f) {
            float a = sample.a * smudge;
            DP_UPixelFloat *sp = &be->classic.smudge_color;
            sp->r = sp->r * (1.0f - a) + sample.r * a;
            sp->g = sp->g * (1.0f - a) + sample.g * a;
            sp->b = sp->b * (1.0f - a) + sample.b * a;
        }
        be->classic.smudge_distance = 0;
    }
}

static void first_dab_pixel(DP_BrushEngine *be, DP_ClassicBrush *cb, float x,
                            float y, float pressure)
{
    be->classic.pixel_length = 0;
    add_dab_pixel(be, cb, DP_float_to_int(x), DP_float_to_int(y), pressure);
}

static void stroke_pixel(DP_BrushEngine *be, DP_ClassicBrush *cb,
                         DP_LayerContent *lc, float x, float y, float pressure)
{
    float last_x = be->classic.last_x;
    float last_y = be->classic.last_y;
    float last_pressure = be->classic.last_pressure;
    float dp = (pressure - last_pressure) / hypotf(x - last_x, y - last_y);

    int x0 = DP_float_to_int(last_x);
    int y0 = DP_float_to_int(last_y);
    int x1 = DP_float_to_int(x);
    int y1 = DP_float_to_int(y);

    int diff_x = x1 - x0;
    int diff_y = y1 - y0;
    int step_x = diff_x < 0 ? -1 : 1;
    int step_y = diff_y < 0 ? -1 : 1;
    int dx = diff_x * step_x * 2;
    int dy = diff_y * step_y * 2;

    int da, db, *a0, *b0, a1, step_a, step_b;
    if (dx > dy) {
        da = dx;
        db = dy;
        a0 = &x0;
        b0 = &y0;
        a1 = x1;
        step_a = step_x;
        step_b = step_y;
    }
    else {
        da = dy;
        db = dx;
        a0 = &y0;
        b0 = &x0;
        a1 = y1;
        step_a = step_y;
        step_b = step_x;
    }

    int distance = be->classic.pixel_length;
    float p = last_pressure;
    int fraction = db - (da / 2);
    while (*a0 != a1) {
        int spacing = DP_float_to_int(DP_classic_brush_spacing_at(cb, p));
        if (fraction >= 0) {
            *b0 += step_b;
            fraction -= da;
        }
        *a0 += step_a;
        fraction += db;
        distance += 1;
        if (distance >= spacing) {
            update_classic_smudge(be, cb, lc, DP_int_to_float(x0),
                                  DP_int_to_float(y0), p);
            add_dab_pixel(be, cb, x0, y0, p);
            distance = 0;
        }
        p += dp;
    }

    be->classic.pixel_length = distance;
}

static void first_dab_soft(DP_BrushEngine *be, DP_ClassicBrush *cb, float x,
                           float y, float pressure)
{
    be->classic.soft_length = 0.0f;
    add_dab_soft(be, cb, x, y, pressure);
}

static void stroke_soft(DP_BrushEngine *be, DP_ClassicBrush *cb,
                        DP_LayerContent *lc, float x, float y, float pressure)
{
    float last_x = be->classic.last_x;
    float last_y = be->classic.last_y;
    float diff_x = x - last_x;
    float diff_y = y - last_y;
    float dist = hypotf(diff_x, diff_y);
    if (dist >= 0.25f) {
        float dx = diff_x / dist;
        float dy = diff_y / dist;
        float last_pressure = be->classic.last_pressure;
        float dp = (pressure - last_pressure) / dist;

        float spacing0 =
            DP_max_float(DP_classic_brush_spacing_at(cb, last_pressure), 1.0f);

        float length = be->classic.soft_length;
        float i = length > spacing0 ? 0.0f : length == 0.0f ? spacing0 : length;

        float dab_x = last_x + dx * i;
        float dab_y = last_y + dy * i;
        float dab_p = CLAMP(last_pressure + dp * i, 0.0f, 1.0f);

        while (i <= dist) {
            update_classic_smudge(be, cb, lc, dab_x, dab_y, dab_p);
            add_dab_soft(be, cb, dab_x, dab_y, dab_p);

            float spacing =
                DP_max_float(DP_classic_brush_spacing_at(cb, dab_p), 1.0f);
            dab_x += dx * spacing;
            dab_y += dy * spacing;
            dab_p = CLAMP(dab_p + dp * spacing, 0.0f, 1.0f);
            i += spacing;
        }

        be->classic.soft_length = i - dist;
    }
}

static void stroke_to_classic(
    DP_BrushEngine *be, float x, float y, float pressure,
    void (*first_dab)(DP_BrushEngine *, DP_ClassicBrush *, float, float, float),
    void (*stroke)(DP_BrushEngine *, DP_ClassicBrush *, DP_LayerContent *,
                   float, float, float))
{
    DP_ClassicBrush *cb = &be->classic.brush;
    DP_LayerContent *lc = be->lc;
    if (be->in_progress) {
        stroke(be, cb, lc, x, y, pressure);
    }
    else {
        be->in_progress = true;
        if (cb->colorpick && cb->mode != DP_BLEND_MODE_ERASE && lc) {
            be->classic.smudge_color =
                sample_classic_smudge(be, cb, lc, x, y, pressure);
            be->classic.smudge_distance = -1;
        }
        else {
            be->classic.smudge_color = be->classic.brush_color;
            be->classic.smudge_distance = 0;
        }
        first_dab(be, cb, x, y, pressure);
    }
    be->classic.last_x = x;
    be->classic.last_y = y;
    be->classic.last_pressure = pressure;
}


static void stroke_to_mypaint(DP_BrushEngine *be, float x, float y,
                              float pressure, float xtilt, float ytilt,
                              float rotation, long long delta_msec)
{
    MyPaintBrush *mb = be->mypaint_brush;
    MyPaintSurface2 *surface = &be->mypaint_surface2;

    if (!be->in_progress) {
        be->in_progress = true;
        mypaint_brush_reset(mb);
        mypaint_brush_new_stroke(mb);
        // Generate a phantom stroke point with zero pressure and a really
        // high delta time to separate this stroke from the last.  This is
        // because libmypaint actually expects you to transmit strokes even
        // when the pen is in the air, which is not how Drawpile works.
        // Generating this singular pen-in-air event makes it behave
        // properly though. Or arguably better than in MyPaint, since when
        // you crank up the smoothing really high there and move the pen
        // really fast, you can get strokes from when your pen wasn't on the
        // tablet, which is just weird.
        mypaint_brush_stroke_to_2(mb, surface, x, y, 0.0f, xtilt, ytilt,
                                  1000.0f, 1.0f, 0.0f, rotation);
    }

    double delta_sec = ((double)delta_msec) / 1000.0f;
    mypaint_brush_stroke_to_2(mb, surface, x, y, pressure, xtilt, ytilt,
                              delta_sec, 1.0f, 0.0f, rotation);
}

static DP_LayerContent *search_layer(DP_CanvasState *cs, int layer_id)
{
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
    if (lre) {
        DP_LayerContent *lc = DP_layer_routes_entry_content(lre, cs);
        return DP_layer_content_incref(lc);
    }
    else {
        return NULL;
    }
}

void DP_brush_engine_stroke_to(DP_BrushEngine *be, float x, float y,
                               float pressure, float xtilt, float ytilt,
                               float rotation, long long delta_msec,
                               DP_CanvasState *cs_or_null)
{
    DP_ASSERT(be);
    DP_BrushEngineActiveType active = be->active;
    DP_PERF_BEGIN_DETAIL(fn, "stroke_to", "active=%d", (int)active);

    if (cs_or_null != be->cs) {
        DP_PERF_BEGIN(search_layer, "stroke_to:search_layer");
        be->cs = cs_or_null;
        DP_layer_content_decref_nullable(be->lc);
        be->lc = cs_or_null ? search_layer(cs_or_null, be->layer_id) : NULL;
        DP_PERF_END(search_layer);
    }

    switch (active) {
    case DP_BRUSH_ENGINE_ACTIVE_PIXEL:
        stroke_to_classic(be, x, y, pressure, first_dab_pixel, stroke_pixel);
        break;
    case DP_BRUSH_ENGINE_ACTIVE_SOFT:
        stroke_to_classic(be, x, y, pressure, first_dab_soft, stroke_soft);
        break;
    case DP_BRUSH_ENGINE_ACTIVE_MYPAINT:
        stroke_to_mypaint(be, x, y, pressure, xtilt, ytilt, rotation,
                          delta_msec);
        break;
    default:
        DP_UNREACHABLE();
    }

    DP_PERF_END(fn);
}

void DP_brush_engine_stroke_end(DP_BrushEngine *be, bool push_pen_up)
{
    DP_ASSERT(be);
    DP_PERF_BEGIN_DETAIL(fn, "stroke_end", "active=%d", (int)be->active);

    DP_layer_content_decref_nullable(be->lc);
    be->lc = NULL;
    be->cs = NULL;

    DP_brush_engine_dabs_flush(be);

    if (push_pen_up) {
        be->push_message(be->user, DP_msg_pen_up_new(be->context_id));
    }
    be->in_progress = false;

    DP_PERF_END(fn);
}

void DP_brush_engine_offset_add(DP_BrushEngine *be, float x, float y)
{
    switch (be->active) {
    case DP_BRUSH_ENGINE_ACTIVE_PIXEL:
    case DP_BRUSH_ENGINE_ACTIVE_SOFT:
        be->classic.last_x += x;
        be->classic.last_y += y;
        break;
    case DP_BRUSH_ENGINE_ACTIVE_MYPAINT:
        // MyPaint brushes don't support the canvas getting moved or resized
        // while a stroke is active, so we can't apply the offset here. But
        // it's not a terribly useful feature to begin with, so it's fine.
        break;
    default:
        DP_UNREACHABLE();
    }
}
