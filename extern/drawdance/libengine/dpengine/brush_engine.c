/*
 * Copyright (C) 2022-2023 askmeaboutloom
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
 * The stabilizer implementation is based on Krita, using it under the GNU
 * General Public License, version 3. See 3rdparty/licenses/krita/COPYING.txt
 * for details.
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
#include <dpcommon/event_log.h>
#include <dpcommon/perf.h>
#include <dpcommon/queue.h>
#include <dpcommon/vector.h>
#include <dpmsg/message.h>
#include <math.h>
#include <mypaint-brush.h>
#include <mypaint.h>
#include <helpers.h> // RGB <-> HSV conversion, CLAMP, mod_arith

#define DP_PERF_CONTEXT "brush_engine"


#define STABILIZER_MIN_QUEUE_SIZE 3
#define STABILIZER_SAMPLE_TIME    1

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
    DP_LayerContent *lc;
    DP_CanvasState *cs;
    DP_BrushStampBuffer stamp_buffer;
    int last_diameter;
    DP_BrushEngineActiveType active;
    MyPaintBrush *mypaint_brush;
    MyPaintSurface2 mypaint_surface2;
    struct {
        unsigned int context_id;
        float zoom;
        bool in_progress;
        long long last_time_msec;
    } stroke;
    struct {
        int sample_count;
        bool finish_strokes;
        bool active;
        long long last_time_msec;
        DP_Queue queue;
        DP_Vector points;
        DP_BrushPoint last_point;
    } stabilizer;
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
            uint8_t mode;
            uint8_t dab_lock_alpha;
            uint8_t dab_colorize;
            uint8_t dab_posterize;
            uint8_t dab_mode;
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
    DP_BrushEnginePollControlFn poll_control;
    void *user;
};


static void stabilizer_queue_push(DP_BrushEngine *be, DP_BrushPoint bp)
{
    *((DP_BrushPoint *)DP_queue_push(&be->stabilizer.queue,
                                     sizeof(DP_BrushPoint))) = bp;
}

static DP_BrushPoint stabilizer_queue_at(DP_BrushEngine *be, size_t i)
{
    return *(DP_BrushPoint *)DP_queue_at(&be->stabilizer.queue,
                                         sizeof(DP_BrushPoint), i);
}

static void stabilizer_queue_replace(DP_BrushEngine *be, DP_BrushPoint bp)
{
    DP_queue_shift(&be->stabilizer.queue);
    stabilizer_queue_push(be, bp);
}

static void stabilizer_points_clear(DP_BrushEngine *be, long long time_msec)
{
    DP_BrushPoint *last_point_or_null =
        DP_vector_last(&be->stabilizer.points, sizeof(DP_BrushPoint));
    if (last_point_or_null) {
        be->stabilizer.last_point = *last_point_or_null;
    }
    be->stabilizer.points.used = 0;
    be->stabilizer.last_time_msec = time_msec;
}

static void stabilizer_points_push(DP_BrushEngine *be, DP_BrushPoint bp)
{
    *((DP_BrushPoint *)DP_vector_push(&be->stabilizer.points,
                                      sizeof(DP_BrushPoint))) = bp;
}

static DP_BrushPoint stabilizer_points_get(DP_BrushEngine *be, long long i,
                                           double alpha)
{
    size_t k = DP_double_to_size(alpha * DP_llong_to_double(i));
    if (k < be->stabilizer.points.used) {
        return *(DP_BrushPoint *)DP_vector_at(&be->stabilizer.points,
                                              sizeof(DP_BrushPoint), k);
    }
    else {
        return be->stabilizer.last_point;
    }
}

static void stabilizer_init(DP_BrushEngine *be, DP_BrushPoint bp)
{
    be->stabilizer.active = true;

    int queue_size =
        DP_max_int(STABILIZER_MIN_QUEUE_SIZE, be->stabilizer.sample_count);
    be->stabilizer.queue.used = 0;
    for (int i = 0; i < queue_size; ++i) {
        stabilizer_queue_push(be, bp);
    }

    stabilizer_points_clear(be, bp.time_msec);
    stabilizer_points_push(be, bp);

    be->poll_control(be->user, true);
}

static void stabilizer_stroke_to(DP_BrushEngine *be, DP_BrushPoint bp)
{
    if (be->stabilizer.active) {
        stabilizer_points_push(be, bp);
    }
    else {
        stabilizer_init(be, bp);
    }
}

static DP_BrushPoint stabilizer_stabilize(DP_BrushEngine *be, DP_BrushPoint bp)
{
    size_t used = be->stabilizer.queue.used;
    // First queue entry is stale, since bp is the one that's replacing it.
    for (size_t i = 1; i < used; ++i) {
        // Uniform averaging.
        float k = DP_size_to_float(i) / DP_size_to_float(i + 1);
        float k1 = 1.0f - k;
        DP_BrushPoint sample = stabilizer_queue_at(be, i);
        bp.x = k1 * sample.x + k * bp.x;
        bp.y = k1 * sample.y + k * bp.y;
        bp.pressure = k1 * sample.pressure + k * bp.pressure;
        bp.xtilt = k1 * sample.xtilt + k * bp.xtilt;
        bp.ytilt = k1 * sample.ytilt + k * bp.ytilt;
        float a1 = bp.rotation;
        float a2 = sample.rotation;
        if (a1 != a2) {
            float distance = smallest_angular_difference(a1, a2);
            float inc = k * distance;
            float b1 = a1 + inc;
            float b2 = a1 - inc;
            float d1 = smallest_angular_difference(b1, a2);
            float d2 = smallest_angular_difference(b2, a2);
            bp.rotation = d1 < d2 ? b1 : b2;
        }
    }
    return bp;
}

static void stabilizer_finish(DP_BrushEngine *be, long long time_msec,
                              DP_CanvasState *cs_or_null)
{
    be->poll_control(be->user, false);
    if (be->stabilizer.finish_strokes) {
        // Flush existing events.
        DP_brush_engine_poll(be, time_msec, cs_or_null);
        // Drain the queue with a delay matching its size.
        stabilizer_points_push(be, be->stabilizer.last_point);
        long long queue_size_samples = DP_size_to_llong(
            be->stabilizer.queue.used * STABILIZER_SAMPLE_TIME);
        DP_brush_engine_poll(
            be, be->stabilizer.last_time_msec + queue_size_samples, cs_or_null);
    }
    be->stabilizer.active = false;
}


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

static uint8_t get_mypaint_dab_colorize(DP_BrushEngine *be, float colorize)
{
    if (be->mypaint.erase || be->mypaint.lock_alpha) {
        return 0;
    }
    else {
        return DP_float_to_uint8(colorize * 255.0f + 0.5f);
    }
}

static uint8_t get_mypaint_dab_posterize(DP_BrushEngine *be, float posterize)
{
    if (be->mypaint.erase || be->mypaint.lock_alpha) {
        return 0;
    }
    else {
        return DP_float_to_uint8(posterize * 255.0f + 0.5f);
    }
}

static uint8_t get_mypaint_dab_posterize_num(uint8_t dab_posterize,
                                             float posterize_num)
{
    if (dab_posterize == 0) {
        return 0;
    }
    else {
        float value = posterize_num * 100.0f + 0.5f;
        return DP_float_to_uint8(CLAMP(value, 1.0f, 128.0f)) - (uint8_t)1;
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

// Spread the aspect ratio into a range between 1.0 and 10.0, then into a byte.
static uint8_t get_mypaint_dab_aspect_ratio(float aspect_ratio)
{
    float value = (aspect_ratio - 1.0f) * 28.333f + 0.5f;
    return DP_float_to_uint8(CLAMP(value, 0, UINT8_MAX));
}

static int add_dab_mypaint_pigment(MyPaintSurface2 *self, float x, float y,
                                   float radius, float color_r, float color_g,
                                   float color_b, float opaque, float hardness,
                                   float alpha_eraser, float aspect_ratio,
                                   float angle, float lock_alpha,
                                   float colorize, float posterize,
                                   float posterize_num, DP_UNUSED float paint)
{
    // A radius less than 0.1 pixels is infinitesimally small. A hardness or
    // opacity of zero means the mask is completely blank. Disregard the dab
    // in those cases. MyPaint uses the same logic for this.
    if (radius >= 0.1f && hardness > 0.0f && opaque > 0.0f) {
        DP_BrushEngine *be = get_mypaint_surface_brush_engine(self);
        int32_t dab_x = DP_float_to_int32(x * 4.0f);
        int32_t dab_y = DP_float_to_int32(y * 4.0f);
        uint32_t dab_color;
        uint8_t dab_lock_alpha, dab_colorize, dab_posterize, dab_mode;

        uint8_t mode = be->mypaint.mode;
        if (mode == 0) {
            dab_color = get_mypaint_dab_color(be, color_r, color_g, color_b,
                                              alpha_eraser);
            dab_lock_alpha = get_mypaint_dab_lock_alpha(be, lock_alpha);
            dab_colorize = get_mypaint_dab_colorize(be, colorize);
            dab_posterize = get_mypaint_dab_posterize(be, posterize);
            dab_mode =
                get_mypaint_dab_posterize_num(dab_posterize, posterize_num);
        }
        else {
            dab_color =
                get_mypaint_dab_color(be, color_r, color_g, color_b, 1.0);
            dab_lock_alpha = 0;
            dab_colorize = 0;
            dab_posterize = 0;
            dab_mode = DP_MYPAINT_BRUSH_MODE_FLAG | mode;
        }

        int used = be->dabs.used;
        int8_t dx, dy;
        bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_MYPAINT_DABS_MAX
                       && be->mypaint.dab_color == dab_color
                       && be->mypaint.dab_lock_alpha == dab_lock_alpha
                       && be->mypaint.dab_colorize == dab_colorize
                       && be->mypaint.dab_posterize == dab_posterize
                       && be->mypaint.dab_mode == dab_mode
                       && delta_xy(be, dab_x, dab_y, &dx, &dy);
        be->dabs.last_x = dab_x;
        be->dabs.last_y = dab_y;

        if (!can_append) {
            DP_brush_engine_dabs_flush(be);
            be->mypaint.dab_x = dab_x;
            be->mypaint.dab_y = dab_y;
            be->mypaint.dab_color = dab_color;
            be->mypaint.dab_lock_alpha = dab_lock_alpha;
            be->mypaint.dab_colorize = dab_colorize;
            be->mypaint.dab_posterize = dab_posterize;
            be->mypaint.dab_mode = dab_mode;
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


DP_BrushEngine *
DP_brush_engine_new(DP_BrushEnginePushMessageFn push_message,
                    DP_BrushEnginePollControlFn poll_control_or_null,
                    void *user)
{
    init_mypaint_once();
    DP_BrushEngine *be = DP_malloc(sizeof(*be));
    *be = (DP_BrushEngine){
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
        {0, 1.0f, false, 0},
        {0,
         true,
         false,
         0,
         DP_QUEUE_NULL,
         DP_VECTOR_NULL,
         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0}},
        {0},
        {0, 0, 0, 0, NULL},
        push_message,
        poll_control_or_null,
        user};
    DP_queue_init(&be->stabilizer.queue, 128, sizeof(DP_BrushPoint));
    DP_vector_init(&be->stabilizer.points, 128, sizeof(DP_BrushPoint));
    return be;
}

void DP_brush_engine_free(DP_BrushEngine *be)
{
    if (be) {
        DP_free(be->dabs.buffer);
        mypaint_brush_unref(be->mypaint_brush);
        DP_layer_content_decref_nullable(be->lc);
        DP_vector_dispose(&be->stabilizer.points);
        DP_queue_dispose(&be->stabilizer.queue);
        DP_free(be);
    }
}


void DP_brush_engine_classic_brush_set(DP_BrushEngine *be,
                                       const DP_ClassicBrush *brush,
                                       const DP_StrokeParams *stroke,
                                       const DP_UPixelFloat *color_override)
{
    DP_ASSERT(be);
    DP_ASSERT(brush);
    DP_ASSERT(stroke);
    DP_ASSERT(stroke->layer_id >= 0);
    DP_ASSERT(stroke->layer_id <= UINT16_MAX);

    be->layer_id = stroke->layer_id;

    switch (brush->shape) {
    case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
    case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
        be->active = DP_BRUSH_ENGINE_ACTIVE_PIXEL;
        break;
    default:
        be->active = DP_BRUSH_ENGINE_ACTIVE_SOFT;
        break;
    }

    be->classic.brush = *brush;
    DP_ClassicBrush *cb = &be->classic.brush;
    DP_UPixelFloat color = color_override ? *color_override : brush->color;
    if (cb->incremental || cb->smudge.max > 0.0f) {
        // Incremental mode must be used when smudging, because color is not
        // picked up from sublayers
        color.a = 0.0f;
    }
    else {
        // If brush color alpha is nonzero, indirect drawing mode
        // is used and the alpha is used as the overall transparency
        // of the entire stroke.
        color.a = cb->opacity.max;
        cb->opacity.max = 1.0f;
        if (cb->opacity_pressure) {
            cb->opacity.min = cb->opacity.min / color.a;
        }
    }
    be->classic.brush_color = color;

    be->stabilizer.sample_count = stroke->stabilizer_sample_count;
    be->stabilizer.finish_strokes = stroke->stabilizer_finish_strokes;
}

static void disable_mypaint_dynamics(MyPaintBrush *mb, MyPaintBrushSetting s)
{
    for (MyPaintBrushInput i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i) {
        mypaint_brush_set_mapping_n(mb, s, i, 0);
    }
}

static void disable_mypaint_setting(MyPaintBrush *mb, MyPaintBrushSetting s)
{
    mypaint_brush_set_base_value(mb, s, 0.0f);
    disable_mypaint_dynamics(mb, s);
}

void DP_brush_engine_mypaint_brush_set(DP_BrushEngine *be,
                                       const DP_MyPaintBrush *brush,
                                       const DP_MyPaintSettings *settings,
                                       const DP_StrokeParams *stroke,
                                       const DP_UPixelFloat *color_override)
{
    DP_ASSERT(be);
    DP_ASSERT(brush);
    DP_ASSERT(settings);
    DP_ASSERT(stroke);
    DP_ASSERT(stroke->layer_id >= 0);
    DP_ASSERT(stroke->layer_id <= UINT16_MAX);
    be->layer_id = stroke->layer_id;
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

    DP_UPixelFloat color = color_override ? *color_override : brush->color;
    float r_h = color.r;
    float g_s = color.g;
    float b_v = color.b;
    rgb_to_hsv_float(&r_h, &g_s, &b_v);
    mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_COLOR_H, r_h);
    mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_COLOR_S, g_s);
    mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_COLOR_V, b_v);

    be->mypaint.lock_alpha = brush->lock_alpha;
    be->mypaint.erase = brush->erase;
    be->mypaint.mode = brush->incremental ? DP_MYPAINT_BRUSH_MODE_INCREMENTAL
                     : brush->erase       ? DP_MYPAINT_BRUSH_MODE_ERASE
                     : brush->lock_alpha  ? DP_MYPAINT_BRUSH_MODE_RECOLOR
                                          : DP_MYPAINT_BRUSH_MODE_NORMAL;

    // We don't support spectral painting (aka Pigment mode), so we'll turn
    // that off at the source here. It's like turning the Pigment slider in
    // MyPaint all the way to zero.
    disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_PAINT_MODE);

    // We have our own, better stabilizer, so turn the MyPaint one off.
    disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SLOW_TRACKING);

    if (!brush->incremental) {
        // Opacity linearization is supposed to compensate for direct mode
        // drawing, in indirect mode it just causes really wrong behavior.
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE);
        // Indirect mode can't smudge, erase, colorize or posterize.
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_RADIUS_LOG);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_ERASER);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_LOCK_ALPHA);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_COLORIZE);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH_LOG);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_BUCKET);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_TRANSPARENCY);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_POSTERIZE);
        // Can't change the color during an indirect stroke.
        disable_mypaint_dynamics(mb, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_H);
        disable_mypaint_dynamics(mb, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_L);
        disable_mypaint_dynamics(mb, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSL_S);
        disable_mypaint_dynamics(mb, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_V);
        disable_mypaint_dynamics(mb, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSV_S);
    }

    be->stabilizer.sample_count = stroke->stabilizer_sample_count;
    be->stabilizer.finish_strokes = stroke->stabilizer_finish_strokes;
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
    if (be->classic.brush.shape == DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND) {
        new_fn = DP_msg_draw_dabs_pixel_new;
    }
    else {
        new_fn = DP_msg_draw_dabs_pixel_square_new;
    }
    be->push_message(
        be->user,
        new_fn(be->stroke.context_id, DP_int_to_uint16(be->layer_id),
               be->classic.dab_x, be->classic.dab_y, be->classic.dab_color,
               (uint8_t)DP_classic_brush_blend_mode(&be->classic.brush),
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
    be->push_message(
        be->user,
        DP_msg_draw_dabs_classic_new(
            be->stroke.context_id, DP_int_to_uint16(be->layer_id),
            be->classic.dab_x, be->classic.dab_y, be->classic.dab_color,
            (uint8_t)DP_classic_brush_blend_mode(&be->classic.brush),
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
                         be->stroke.context_id, DP_int_to_uint16(be->layer_id),
                         be->mypaint.dab_x, be->mypaint.dab_y,
                         be->mypaint.dab_color, be->mypaint.dab_lock_alpha,
                         be->mypaint.dab_colorize, be->mypaint.dab_posterize,
                         be->mypaint.dab_mode, set_mypaint_dabs, used,
                         be->dabs.buffer));
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
                                  bool push_undo_point, float zoom)
{
    DP_ASSERT(be);
    DP_ASSERT(!be->stroke.in_progress);
    DP_ASSERT(be->dabs.used == 0);
    DP_PERF_BEGIN_DETAIL(fn, "stroke_begin", "active=%d", (int)be->active);
    DP_EVENT_LOG("stroke_begin");

    be->stroke.context_id = context_id;
    be->stroke.zoom = zoom;
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
    if (dist >= 0.001f) {
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
    if (be->stroke.in_progress) {
        stroke(be, cb, lc, x, y, pressure);
    }
    else {
        be->stroke.in_progress = true;
        bool colorpick = cb->colorpick
                      && DP_classic_brush_blend_mode(cb) != DP_BLEND_MODE_ERASE
                      && lc;
        if (colorpick) {
            be->classic.smudge_color =
                sample_classic_smudge(be, cb, lc, x, y, pressure);
            be->classic.smudge_color.a = be->classic.brush_color.a;
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


static void stroke_to_mypaint(DP_BrushEngine *be, DP_BrushPoint bp)
{
    MyPaintBrush *mb = be->mypaint_brush;
    MyPaintSurface2 *surface = &be->mypaint_surface2;
    float zoom = be->stroke.zoom;

    double delta_sec;
    if (be->stroke.in_progress) {
        delta_sec = DP_llong_to_float(bp.time_msec - be->stroke.last_time_msec)
                  / 1000.0f;
    }
    else {
        be->stroke.in_progress = true;
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
        mypaint_brush_stroke_to_2(mb, surface, bp.x, bp.y, 0.0f, bp.xtilt,
                                  bp.ytilt, 1000.0f, zoom, 0.0f, bp.rotation);
        delta_sec = 0.0f;
    }

    mypaint_brush_stroke_to_2(mb, surface, bp.x, bp.y, bp.pressure, bp.xtilt,
                              bp.ytilt, delta_sec, zoom, 0.0f, bp.rotation);
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

static void stroke_to(DP_BrushEngine *be, DP_BrushPoint bp,
                      DP_CanvasState *cs_or_null)
{
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
        DP_EVENT_LOG("stroke_to active=pixel x=%f y=%f pressure=%f", bp.x, bp.y,
                     bp.pressure);
        stroke_to_classic(be, bp.x, bp.y, bp.pressure, first_dab_pixel,
                          stroke_pixel);
        break;
    case DP_BRUSH_ENGINE_ACTIVE_SOFT:
        DP_EVENT_LOG("stroke_to active=soft x=%f y=%f pressure=%f", bp.x, bp.y,
                     bp.pressure);
        stroke_to_classic(be, bp.x, bp.y, bp.pressure, first_dab_soft,
                          stroke_soft);
        break;
    case DP_BRUSH_ENGINE_ACTIVE_MYPAINT:
        DP_EVENT_LOG("stroke_to active=mypaint x=%f y=%f pressure=%f xtilt=%f "
                     "ytilt=%f rotation=%f time_msec=%lld",
                     bp.x, bp.y, bp.pressure, bp.xtilt, bp.ytilt, bp.rotation,
                     bp.time_msec);
        stroke_to_mypaint(be, bp);
        break;
    default:
        DP_UNREACHABLE();
    }

    be->stroke.last_time_msec = bp.time_msec;
    DP_PERF_END(fn);
}

void DP_brush_engine_stroke_to(DP_BrushEngine *be, DP_BrushPoint bp,
                               DP_CanvasState *cs_or_null)
{
    DP_ASSERT(be);
    if (be->poll_control && be->stabilizer.sample_count > 0) {
        stabilizer_stroke_to(be, bp);
    }
    else {
        stroke_to(be, bp, cs_or_null);
    }
}

void DP_brush_engine_poll(DP_BrushEngine *be, long long time_msec,
                          DP_CanvasState *cs_or_null)
{
    DP_ASSERT(be);
    if (be->stabilizer.active) {
        long long elapsed_msec = time_msec - be->stabilizer.last_time_msec;
        long long elapsed = elapsed_msec / STABILIZER_SAMPLE_TIME;
        double alpha = DP_size_to_double(be->stabilizer.points.used)
                     / DP_llong_to_double(elapsed);

        for (long long i = 0; i < elapsed; ++i) {
            DP_BrushPoint bp = stabilizer_points_get(be, i, alpha);
            stroke_to(be, stabilizer_stabilize(be, bp), cs_or_null);
            stabilizer_queue_replace(be, bp);
        }

        stabilizer_points_clear(be, time_msec);
    }
}

void DP_brush_engine_stroke_end(DP_BrushEngine *be, long long time_msec,
                                DP_CanvasState *cs_or_null, bool push_pen_up)
{
    DP_ASSERT(be);
    DP_PERF_BEGIN_DETAIL(fn, "stroke_end", "active=%d", (int)be->active);
    DP_EVENT_LOG("stroke_end");

    if (be->stabilizer.active) {
        stabilizer_finish(be, time_msec, cs_or_null);
    }

    DP_layer_content_decref_nullable(be->lc);
    be->lc = NULL;
    be->cs = NULL;

    DP_brush_engine_dabs_flush(be);

    if (push_pen_up) {
        be->push_message(be->user, DP_msg_pen_up_new(be->stroke.context_id));
    }
    be->stroke.in_progress = false;

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
