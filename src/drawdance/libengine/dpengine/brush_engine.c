// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_engine.h"
#include "brush.h"
#include "canvas_state.h"
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
#include <dpmsg/ids.h>
#include <math.h>
#include <mypaint-brush.h>
#include <mypaint.h>
#include <helpers.h> // RGB <-> HSV conversion, CLAMP, mod_arith

#define DP_PERF_CONTEXT "brush_engine"


#define SPLINE_DISTANCE    8.0f
#define SPLINE_DRAIN_DELAY 100LL
#define SPLINE_POINT_NULL                                    \
    (DP_SplinePoint)                                         \
    {                                                        \
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0}, 0.0f, false \
    }

#define STABILIZER_MIN_QUEUE_SIZE 3
#define STABILIZER_SAMPLE_TIME    1

// Same amount of smudge buckets that MyPaint uses.
#define SMUDGE_BUCKET_COUNT 256
#define MIN_DABS_CAPACITY   1024
#define MAX_XY_DELTA        127

// Based on MyPaint's velocity calculations for fine speed.
#define CLASSIC_VELOCITY_GAMMA    54.598148f
#define CLASSIC_VELOCITY_M        1.493972f
#define CLASSIC_VELOCITY_Q        -6.373980f
#define CLASSIC_VELOCITY_SLOWNESS 0.04f

typedef enum DP_BrushEngineActiveType {
    DP_BRUSH_ENGINE_ACTIVE_PIXEL,
    DP_BRUSH_ENGINE_ACTIVE_SOFT,
    DP_BRUSH_ENGINE_ACTIVE_MYPAINT,
} DP_BrushEngineActiveType;

typedef struct DP_BrushEnginePixelDab {
    int8_t x;
    int8_t y;
    uint16_t size;
    uint8_t opacity;
} DP_BrushEnginePixelDab;

typedef struct DP_BrushEngineClassicDab {
    int8_t x;
    int8_t y;
    uint32_t size;
    uint8_t hardness;
    uint8_t opacity;
} DP_BrushEngineClassicDab;

typedef struct DP_BrushEngineMyPaintDab {
    int8_t x;
    int8_t y;
    uint32_t size;
    uint8_t hardness;
    uint8_t opacity;
    uint8_t angle;
    uint8_t aspect_ratio;
} DP_BrushEngineMyPaintDab;

typedef struct DP_SplinePoint {
    DP_BrushPoint p;
    float length;
    bool drawn;
} DP_SplinePoint;

struct DP_BrushEngine {
    DP_LayerContent *lc;
    DP_CanvasState *cs;
    uint16_t *stamp_buffer;
    int layer_id;
    int last_diameter;
    DP_Atomic size_limit;
    DP_BrushEngineActiveType active;
    MyPaintBrush *mypaint_brush;
    MyPaintSurface2 mypaint_surface2;
    struct {
        unsigned int context_id;
        float zoom;
        float angle_rad;
        bool mirror;
        bool flip;
        bool in_progress;
        long long last_time_msec;
    } stroke;
    struct {
        bool enabled;
        int fill;
        int offset;
        DP_SplinePoint points[4];
    } spline;
    struct {
        int size;
        bool finish_strokes;
        int capacity;
        int fill;
        int offset;
        DP_BrushPoint *points;
    } smoother;
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
            float last_velocity;
            float last_distance;
            bool last_left;
            bool last_up;
            uint8_t dab_flags;
            int32_t dab_x;
            int32_t dab_y;
            uint32_t dab_color;
        } classic;
        struct {
            DP_PaintMode paint_mode;
            DP_BlendMode blend_mode;
            uint8_t dab_flags;
            uint8_t dab_lock_alpha;
            uint8_t dab_colorize;
            uint8_t dab_posterize;
            uint8_t dab_posterize_num;
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


static void set_poll_control_enabled(DP_BrushEngine *be, bool enabled)
{
    DP_BrushEnginePollControlFn poll_control = be->poll_control;
    if (poll_control) {
        poll_control(be->user, enabled);
    }
}


// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: stabilizer implementation from Krita

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

// SPDX-SnippetEnd


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
                          float pressure, float velocity, float distance)
{
    uint16_t dab_size =
        DP_classic_brush_pixel_dab_size_at(cb, pressure, velocity, distance);
    uint8_t dab_opacity =
        DP_classic_brush_dab_opacity_at(cb, pressure, velocity, distance);
    if (dab_size > 0 && dab_opacity > 0) {
        uint8_t dab_flags = (uint8_t)cb->paint_mode;
        int32_t dab_x = DP_int_to_int32(x);
        int32_t dab_y = DP_int_to_int32(y);
        uint32_t dab_color = combine_upixel_float(be->classic.smudge_color);

        int used = be->dabs.used;
        int8_t dx, dy;
        bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_PIXEL_DABS_MAX
                       && be->classic.dab_flags == dab_flags
                       && be->classic.dab_color == dab_color
                       && delta_xy(be, dab_x, dab_y, &dx, &dy);
        be->dabs.last_x = dab_x;
        be->dabs.last_y = dab_y;

        if (!can_append) {
            DP_brush_engine_dabs_flush(be);
            be->classic.dab_flags = dab_flags;
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
                         float y, float pressure, float velocity,
                         float distance, bool left, bool up)
{
    uint32_t dab_size =
        DP_classic_brush_soft_dab_size_at(cb, pressure, velocity, distance);
    uint8_t dab_opacity =
        DP_classic_brush_dab_opacity_at(cb, pressure, velocity, distance);
    // Disregard infinitesimal or fully transparent dabs. 26 is a radius of 0.1.
    if (dab_size >= 26 && dab_opacity > 0) {
        // The brush mask rendering has some unsightly discontinuities between
        // fractions of a radius and the next full step, causing an unsightly
        // jagged look between them. We can't fix the brush rendering directly
        // because that would break compatibility, so instead we fudge the
        // positioning to compensate.
        float rrem = fmodf(DP_uint32_to_float(dab_size) / 256.0f, 1.0f);
        if (rrem < 0.00001f) {
            rrem = 1.0f;
        }

        float full_fudge = -0.72f;
        float half_fudge = -0.36f;
        float fudge_x, fudge_y;
        if (left && up) {
            fudge_x = fudge_y = rrem * full_fudge;
        }
        else if (!left && up) {
            fudge_x = rrem * half_fudge;
            fudge_y = rrem * full_fudge;
        }
        else if (!left && !up) {
            fudge_x = rrem * full_fudge;
            fudge_y = rrem * half_fudge;
        }
        else {
            fudge_x = fudge_y = rrem * half_fudge;
        }

        uint8_t dab_flags = (uint8_t)cb->paint_mode;
        int32_t dab_x = DP_float_to_int32((x + fudge_x) * 4.0f) + (int32_t)3;
        int32_t dab_y = DP_float_to_int32((y + fudge_y) * 4.0f) + (int32_t)2;
        uint32_t dab_color = combine_upixel_float(be->classic.smudge_color);

        int used = be->dabs.used;
        int8_t dx, dy;
        bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_CLASSIC_DABS_MAX
                       && be->classic.dab_flags == dab_flags
                       && be->classic.dab_color == dab_color
                       && delta_xy(be, dab_x, dab_y, &dx, &dy);
        be->dabs.last_x = dab_x;
        be->dabs.last_y = dab_y;

        if (!can_append) {
            DP_brush_engine_dabs_flush(be);
            be->classic.dab_flags = dab_flags;
            be->classic.dab_x = dab_x;
            be->classic.dab_y = dab_y;
            be->classic.dab_color = dab_color;
            dx = 0;
            dy = 0;
        }

        DP_BrushEngineClassicDab *dabs = get_dab_buffer(be, sizeof(*dabs));
        dabs[be->dabs.used++] = (DP_BrushEngineClassicDab){
            dx, dy, dab_size,
            DP_classic_brush_dab_hardness_at(cb, pressure, velocity, distance),
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
    return (void *)(bytes - offsetof(DP_BrushEngine, mypaint_surface2));
}

static bool is_normal_mypaint_mode(DP_BlendMode blend_mode)
{
    return blend_mode == DP_BLEND_MODE_NORMAL
        || blend_mode == DP_BLEND_MODE_PIGMENT_ALPHA;
}

static bool should_ignore_full_alpha_mypaint_dab(DP_PaintMode paint_mode,
                                                 DP_BlendMode blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_NORMAL:
    case DP_BLEND_MODE_PIGMENT_ALPHA:
        return paint_mode != DP_PAINT_MODE_DIRECT;
    case DP_BLEND_MODE_ERASE:
    case DP_BLEND_MODE_ERASE_PRESERVE:
        return false;
    default:
        return true;
    }
}

static uint32_t get_mypaint_dab_color(float color_r, float color_g,
                                      float color_b, float alpha_eraser)
{
    return combine_rgba(color_r, color_g, color_b, alpha_eraser);
}

static uint8_t get_mypaint_dab_lock_alpha(float lock_alpha)
{
    return DP_float_to_uint8(lock_alpha * 255.0f + 0.5f);
}

static uint8_t get_mypaint_dab_colorize(float colorize)
{
    return DP_float_to_uint8(colorize * 255.0f + 0.5f);
}

static uint8_t get_mypaint_dab_posterize(float posterize)
{
    return DP_float_to_uint8(posterize * 255.0f + 0.5f);
}

static uint8_t get_mypaint_dab_posterize_num(uint8_t dab_posterize,
                                             float posterize_num)
{
    if (dab_posterize == 0) {
        return 0;
    }
    else {
        float value = posterize_num * 100.0f + 0.5f - 1.0f;
        return DP_float_to_uint8(CLAMP(value, 0.0f, 127.0f));
    }
}

static uint32_t get_mypaint_dab_size(float radius)
{
    float value = radius * 512.0f + 0.5f;
    return DP_float_to_uint32(
        CLAMP(value, 0, (float)DP_BRUSH_SIZE_MAX * 256.0f));
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
    if (radius < 0.1f && hardness <= 0.0f && opaque <= 0.0f) {
        return 0;
    }

    DP_BrushEngine *be = get_mypaint_surface_brush_engine(self);
    DP_PaintMode paint_mode = be->mypaint.paint_mode;
    DP_BlendMode blend_mode = be->mypaint.blend_mode;
    // Disregard colors with zero alpha if the paint and/or blend modes can't
    // deal with it. MyPaint brushes do this when they want to smudge at full
    // alpha, which will use an undefined (in practice: fully red) color.
    if (alpha_eraser <= 0.0f
        && should_ignore_full_alpha_mypaint_dab(paint_mode, blend_mode)) {
        return 0;
    }

    int32_t dab_x = DP_float_to_int32(x * 4.0f);
    int32_t dab_y = DP_float_to_int32(y * 4.0f);
    uint32_t dab_color;
    uint8_t dab_flags, dab_lock_alpha, dab_colorize, dab_posterize,
        dab_posterize_num;

    if (paint_mode == DP_PAINT_MODE_DIRECT
        && is_normal_mypaint_mode(blend_mode)) {
        dab_color =
            get_mypaint_dab_color(color_r, color_g, color_b, alpha_eraser);
        dab_flags = 0;
        dab_lock_alpha = get_mypaint_dab_lock_alpha(lock_alpha);
        dab_colorize = get_mypaint_dab_colorize(colorize);
        dab_posterize = get_mypaint_dab_posterize(posterize);
        dab_posterize_num =
            get_mypaint_dab_posterize_num(dab_posterize, posterize_num);
        DP_ASSERT((dab_posterize_num & 0x80) == 0);
        if (blend_mode == DP_BLEND_MODE_PIGMENT_ALPHA) {
            dab_posterize_num |= (uint8_t)0x80;
        }
    }
    else {
        dab_color = get_mypaint_dab_color(color_r, color_g, color_b, 1.0f);
        dab_flags = (uint8_t)paint_mode;
        dab_lock_alpha = 0;
        dab_colorize = 0;
        dab_posterize = 0;
        dab_posterize_num = 0;
    }

    int used = be->dabs.used;
    int8_t dx, dy;
    bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_MYPAINT_DABS_MAX
                   && be->mypaint.dab_color == dab_color
                   && be->mypaint.dab_flags == dab_flags
                   && be->mypaint.dab_lock_alpha == dab_lock_alpha
                   && be->mypaint.dab_colorize == dab_colorize
                   && be->mypaint.dab_posterize == dab_posterize
                   && be->mypaint.dab_posterize_num == dab_posterize_num
                   && delta_xy(be, dab_x, dab_y, &dx, &dy);
    be->dabs.last_x = dab_x;
    be->dabs.last_y = dab_y;

    if (!can_append) {
        DP_brush_engine_dabs_flush(be);
        be->mypaint.dab_x = dab_x;
        be->mypaint.dab_y = dab_y;
        be->mypaint.dab_color = dab_color;
        be->mypaint.dab_flags = dab_flags;
        be->mypaint.dab_lock_alpha = dab_lock_alpha;
        be->mypaint.dab_colorize = dab_colorize;
        be->mypaint.dab_posterize = dab_posterize;
        be->mypaint.dab_posterize_num = dab_posterize_num;
        dx = 0;
        dy = 0;
    }

    DP_BrushEngineMyPaintDab *dabs = get_dab_buffer(be, sizeof(*dabs));
    dabs[be->dabs.used++] =
        (DP_BrushEngineMyPaintDab){dx,
                                   dy,
                                   get_mypaint_dab_size(radius),
                                   get_uint8(hardness),
                                   get_uint8(opaque),
                                   get_mypaint_dab_angle(angle),
                                   get_mypaint_dab_aspect_ratio(aspect_ratio)};
    return 1;
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

static uint16_t *get_stamp_buffer(DP_BrushEngine *be)
{
    if (!be->stamp_buffer) {
        be->stamp_buffer = DP_malloc_simd(
            DP_square_size(sizeof(*be->stamp_buffer) * (size_t)260));
    }
    return be->stamp_buffer;
}

static bool is_pigment_mode(DP_BlendMode blend_mode)
{
    return blend_mode == DP_BLEND_MODE_PIGMENT
        || blend_mode == DP_BLEND_MODE_PIGMENT_ALPHA;
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
        DP_UPixelFloat color = DP_layer_content_sample_color_at(
            lc, get_stamp_buffer(be), DP_float_to_int(x + 0.5f),
            DP_float_to_int(y + 0.5f), diameter, false,
            is_pigment_mode(be->mypaint.blend_mode), &be->last_diameter);
        *color_r = color.r;
        *color_g = color.g;
        *color_b = color.b;
        *color_a = color.a;
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


// The MyPaint brush engine sometimes ignores the first stroke issued. We do a
// dummy stroke to begin with to get that out of the way, these dummy functions
// here just accept the stroke and throw it away.

static int add_dab_mypaint_pigment_dummy(
    DP_UNUSED MyPaintSurface2 *self, DP_UNUSED float x, DP_UNUSED float y,
    DP_UNUSED float radius, DP_UNUSED float color_r, DP_UNUSED float color_g,
    DP_UNUSED float color_b, DP_UNUSED float opaque, DP_UNUSED float hardness,
    DP_UNUSED float alpha_eraser, DP_UNUSED float aspect_ratio,
    DP_UNUSED float angle, DP_UNUSED float lock_alpha, DP_UNUSED float colorize,
    DP_UNUSED float posterize, DP_UNUSED float posterize_num,
    DP_UNUSED float paint)
{
    return 1;
}

static int add_dab_mypaint_dummy(MyPaintSurface *self, float x, float y,
                                 float radius, float color_r, float color_g,
                                 float color_b, float opaque, float hardness,
                                 float alpha_eraser, float aspect_ratio,
                                 float angle, float lock_alpha, float colorize)
{
    return add_dab_mypaint_pigment_dummy(
        (MyPaintSurface2 *)self, x, y, radius, color_r, color_g, color_b,
        opaque, hardness, alpha_eraser, aspect_ratio, angle, lock_alpha,
        colorize, 0.0f, 0.0f, 0.0f);
}

static void get_color_mypaint_pigment_dummy(
    DP_UNUSED MyPaintSurface2 *self, DP_UNUSED float x, DP_UNUSED float y,
    DP_UNUSED float radius, float *color_r, float *color_g, float *color_b,
    float *color_a, DP_UNUSED float paint)
{
    *color_r = 0.0f;
    *color_g = 0.0f;
    *color_b = 0.0f;
    *color_a = 0.0f;
}

static void get_color_mypaint_dummy(MyPaintSurface *self, float x, float y,
                                    float radius, float *color_r,
                                    float *color_g, float *color_b,
                                    float *color_a)
{
    get_color_mypaint_pigment_dummy((MyPaintSurface2 *)self, x, y, radius,
                                    color_r, color_g, color_b, color_a, 0.0f);
}

static void issue_dummy_mypaint_stroke(MyPaintBrush *mb)
{
    MyPaintSurface2 surface = {{add_dab_mypaint_dummy, get_color_mypaint_dummy,
                                NULL, NULL, NULL, NULL, 0},
                               add_dab_mypaint_pigment_dummy,
                               get_color_mypaint_pigment_dummy,
                               NULL};
    mypaint_brush_reset(mb);
    mypaint_brush_new_stroke(mb);
    mypaint_brush_stroke_to_2(mb, &surface, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                              1000.0f, 1.0f, 0.0f, 0.0f);
    mypaint_brush_stroke_to_2(mb, &surface, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0,
                              1.0f, 0.0f, 0.0f);
}


DP_BrushEngine *
DP_brush_engine_new(DP_BrushEnginePushMessageFn push_message,
                    DP_BrushEnginePollControlFn poll_control_or_null,
                    void *user)
{
    init_mypaint_once();
    DP_BrushEngine *be = DP_malloc(sizeof(*be));
    *be = (DP_BrushEngine){
        NULL,
        NULL,
        NULL,
        0,
        -1,
        DP_ATOMIC_INIT(-1),
        DP_BRUSH_ENGINE_ACTIVE_PIXEL,
        mypaint_brush_new_with_buckets(SMUDGE_BUCKET_COUNT),
        {{add_dab_mypaint, get_color_mypaint, NULL, NULL, NULL, NULL, 0},
         add_dab_mypaint_pigment,
         get_color_mypaint_pigment,
         NULL},
        {0, 1.0f, 0.0f, false, false, false, 0},
        {false,
         0,
         0,
         {SPLINE_POINT_NULL, SPLINE_POINT_NULL, SPLINE_POINT_NULL,
          SPLINE_POINT_NULL}},
        {0, false, 0, 0, 0, NULL},
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
    issue_dummy_mypaint_stroke(be->mypaint_brush);
    return be;
}

void DP_brush_engine_free(DP_BrushEngine *be)
{
    if (be) {
        DP_free(be->dabs.buffer);
        mypaint_brush_unref(be->mypaint_brush);
        DP_layer_content_decref_nullable(be->lc);
        DP_free(be->stamp_buffer);
        DP_free(be->smoother.points);
        DP_vector_dispose(&be->stabilizer.points);
        DP_queue_dispose(&be->stabilizer.queue);
        DP_free(be);
    }
}

void DP_brush_engine_size_limit_set(DP_BrushEngine *be, int size_limit)
{
    DP_ASSERT(be);
    DP_atomic_set(&be->size_limit, size_limit);
}

static void set_common_stroke_params(DP_BrushEngine *be,
                                     const DP_StrokeParams *stroke)
{
    be->layer_id = stroke->layer_id;
    be->spline.enabled = stroke->interpolate;
    be->stabilizer.sample_count = stroke->stabilizer_sample_count;
    be->stabilizer.finish_strokes = stroke->stabilizer_finish_strokes;

    int smoothing = DP_max_int(0, stroke->smoothing);
    be->smoother.size = smoothing;
    be->smoother.finish_strokes = stroke->smoothing_finish_strokes;
    if (be->smoother.capacity < smoothing) {
        be->smoother.points =
            DP_realloc(be->smoother.points, sizeof(*be->smoother.points)
                                                * DP_int_to_size(smoothing));
        be->smoother.capacity = smoothing;
    }
}

void DP_brush_engine_classic_brush_set(DP_BrushEngine *be,
                                       const DP_ClassicBrush *brush,
                                       const DP_StrokeParams *stroke,
                                       const DP_UPixelFloat *color_override,
                                       bool eraser_override)
{
    DP_ASSERT(be);
    DP_ASSERT(brush);
    DP_ASSERT(stroke);
    DP_ASSERT(stroke->layer_id >= 0);
    DP_ASSERT(stroke->layer_id <= DP_MESSAGE_LAYER_ID_MAX);

    set_common_stroke_params(be, stroke);

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
    if (eraser_override) {
        cb->erase = true;
        cb->erase_mode = DP_BLEND_MODE_ERASE;
    }

    if (DP_blend_mode_direct_only((int)DP_classic_brush_blend_mode(cb))) {
        cb->paint_mode = DP_PAINT_MODE_DIRECT;
    }

    DP_UPixelFloat color = color_override ? *color_override : brush->color;
    if (cb->paint_mode == DP_PAINT_MODE_DIRECT || cb->smudge.max > 0.0f) {
        // Incremental mode must be used when smudging, because color is not
        // picked up from sublayers
        cb->paint_mode = DP_PAINT_MODE_DIRECT;
        color.a = 0.0f;
    }
    else {
        // For indirect drawing modes, the alpha is used as the overall opacity
        // of the entire stroke.
        color.a = cb->opacity.max;
        cb->opacity.max = 1.0f;
        if (cb->opacity_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE) {
            cb->opacity.min = cb->opacity.min / color.a;
        }
    }
    be->classic.brush_color = color;
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
                                       const DP_UPixelFloat *color_override,
                                       bool eraser_override)
{
    DP_ASSERT(be);
    DP_ASSERT(brush);
    DP_ASSERT(settings);
    DP_ASSERT(stroke);
    DP_ASSERT(stroke->layer_id >= DP_LAYER_ID_MIN);
    DP_ASSERT(stroke->layer_id <= DP_LAYER_ID_MAX);

    set_common_stroke_params(be, stroke);
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

    be->mypaint.blend_mode = eraser_override
                               ? DP_BLEND_MODE_ERASE
                               : DP_mypaint_brush_blend_mode(brush);
    be->mypaint.paint_mode =
        DP_blend_mode_direct_only((int)be->mypaint.blend_mode)
            ? DP_PAINT_MODE_DIRECT
            : brush->paint_mode;

    // We don't support partial pigment mode for performance and usability
    // reasons, you either enable the pigment blend mode or you don't.
    disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_PAINT_MODE);

    // We have our own, better stabilizer, so turn the MyPaint one off.
    disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SLOW_TRACKING);

    bool indirect = be->mypaint.paint_mode != DP_PAINT_MODE_DIRECT;
    DP_BlendMode blend_mode = be->mypaint.blend_mode;
    if (indirect || DP_blend_mode_compares_alpha((int)blend_mode)) {
        // Opacity linearization is supposed to compensate for direct mode
        // drawing, in indirect mode it just causes really wrong behavior. The
        // same goes for blend modes like Greater, which effectively do an
        // indirect mode on top of the existing color.
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE);
    }

    if (indirect) {
        // Indirect mode can't smudge.
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_RADIUS_LOG);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH_LOG);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_BUCKET);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_TRANSPARENCY);
    }

    if (indirect || !is_normal_mypaint_mode(blend_mode)) {
        // Indirect mode also can't do any of these. They're also basically
        // blend modes, so they only work when that hasn't been altered.
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_ERASER);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_LOCK_ALPHA);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_COLORIZE);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_POSTERIZE);
    }
}


static uint8_t get_dab_blend_mode(DP_PaintMode paint_mode,
                                  DP_BlendMode blend_mode)
{
    // Marker and Greater look better in wash mode, so switch when using direct
    // painting. The soft indirect mode is there if the user really wants it.
    if (paint_mode == DP_PAINT_MODE_DIRECT) {
        switch (blend_mode) {
        case DP_BLEND_MODE_MARKER:
            return (uint8_t)DP_BLEND_MODE_MARKER_WASH;
        case DP_BLEND_MODE_MARKER_ALPHA:
            return (uint8_t)DP_BLEND_MODE_MARKER_ALPHA_WASH;
        case DP_BLEND_MODE_GREATER:
            return (uint8_t)DP_BLEND_MODE_GREATER_WASH;
        case DP_BLEND_MODE_GREATER_ALPHA:
            return (uint8_t)DP_BLEND_MODE_GREATER_ALPHA_WASH;
        default:
            break;
        }
    }
    return (uint8_t)blend_mode;
}

static void set_pixel_dabs(int count, DP_PixelDab *out, void *user)
{
    DP_BrushEngine *be = user;
    DP_BrushEnginePixelDab *dabs = be->dabs.buffer;
    int size_limit = DP_atomic_get(&be->size_limit);
    static_assert(DP_BRUSH_SIZE_MAX <= UINT16_MAX,
                  "Maximum brush size fits into a u16");
    uint16_t max_size = size_limit < 0 || size_limit > DP_BRUSH_SIZE_MAX
                          ? (uint16_t)DP_BRUSH_SIZE_MAX
                          : DP_int_to_uint16(size_limit);
    for (int i = 0; i < count; ++i) {
        DP_BrushEnginePixelDab *dab = &dabs[i];
        uint16_t size = dab->size;
        if (size > max_size) {
            size = max_size;
        }
        DP_pixel_dab_init(out, i, dab->x, dab->y, size, dab->opacity);
    }
}

static void flush_pixel_dabs(DP_BrushEngine *be, int used)
{
    DP_Message *(*new_fn)(unsigned int, uint8_t, uint32_t, int32_t, int32_t,
                          uint32_t, uint8_t,
                          void (*)(int, DP_PixelDab *, void *), int, void *);
    if (be->classic.brush.shape == DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND) {
        new_fn = DP_msg_draw_dabs_pixel_new;
    }
    else {
        new_fn = DP_msg_draw_dabs_pixel_square_new;
    }
    be->push_message(
        be->user, new_fn(be->stroke.context_id, (uint8_t)be->classic.dab_flags,
                         DP_int_to_uint32(be->layer_id), be->classic.dab_x,
                         be->classic.dab_y, be->classic.dab_color,
                         get_dab_blend_mode(
                             be->classic.brush.paint_mode,
                             DP_classic_brush_blend_mode(&be->classic.brush)),
                         set_pixel_dabs, used, be));
}

static uint32_t get_subpixel_max_size(DP_BrushEngine *be)
{
    int size_limit = DP_atomic_get(&be->size_limit);
    return (uint32_t)256
         * (size_limit < 0 || size_limit > DP_BRUSH_SIZE_MAX
                ? (uint32_t)DP_BRUSH_SIZE_MAX
                : DP_int_to_uint32(size_limit));
}

static void set_soft_dabs(int count, DP_ClassicDab *out, void *user)
{
    DP_BrushEngine *be = user;
    DP_BrushEngineClassicDab *dabs = be->dabs.buffer;
    uint32_t max_size = get_subpixel_max_size(be);
    for (int i = 0; i < count; ++i) {
        DP_BrushEngineClassicDab *dab = &dabs[i];
        uint32_t size = dab->size;
        if (size > max_size) {
            size = max_size;
        }
        DP_classic_dab_init(out, i, dab->x, dab->y, size, dab->hardness,
                            dab->opacity);
    }
}

static void flush_soft_dabs(DP_BrushEngine *be, int used)
{
    be->push_message(
        be->user,
        DP_msg_draw_dabs_classic_new(
            be->stroke.context_id, be->classic.dab_flags,
            DP_int_to_uint32(be->layer_id), be->classic.dab_x,
            be->classic.dab_y, be->classic.dab_color,
            get_dab_blend_mode(be->classic.brush.paint_mode,
                               DP_classic_brush_blend_mode(&be->classic.brush)),
            set_soft_dabs, used, be));
}

static void set_mypaint_dabs(int count, DP_MyPaintDab *out, void *user)
{
    DP_BrushEngine *be = user;
    DP_BrushEngineMyPaintDab *dabs = be->dabs.buffer;
    uint32_t max_size = get_subpixel_max_size(be);
    for (int i = 0; i < count; ++i) {
        DP_BrushEngineMyPaintDab *dab = &dabs[i];
        uint32_t size = dab->size;
        if (size > max_size) {
            size = max_size;
        }
        DP_mypaint_dab_init(out, i, dab->x, dab->y, size, dab->hardness,
                            dab->opacity, dab->angle, dab->aspect_ratio);
    }
}

static void set_mypaint_blend_dabs(int count, DP_MyPaintBlendDab *out,
                                   void *user)
{
    DP_BrushEngine *be = user;
    DP_BrushEngineMyPaintDab *dabs = be->dabs.buffer;
    uint32_t max_size = get_subpixel_max_size(be);
    for (int i = 0; i < count; ++i) {
        DP_BrushEngineMyPaintDab *dab = &dabs[i];
        uint32_t size = dab->size;
        if (size > max_size) {
            size = max_size;
        }
        DP_mypaint_blend_dab_init(out, i, dab->x, dab->y, size, dab->hardness,
                                  dab->opacity, dab->angle, dab->aspect_ratio);
    }
}

static void flush_mypaint_dabs(DP_BrushEngine *be, int used)
{
    DP_Message *msg;
    DP_PaintMode paint_mode = be->mypaint.paint_mode;
    DP_BlendMode blend_mode = be->mypaint.blend_mode;
    if (paint_mode == DP_PAINT_MODE_DIRECT
        && is_normal_mypaint_mode(blend_mode)) {
        msg = DP_msg_draw_dabs_mypaint_new(
            be->stroke.context_id, DP_int_to_uint32(be->layer_id),
            be->mypaint.dab_x, be->mypaint.dab_y, be->mypaint.dab_color,
            be->mypaint.dab_lock_alpha, be->mypaint.dab_colorize,
            be->mypaint.dab_posterize, be->mypaint.dab_posterize_num,
            set_mypaint_dabs, used, be);
    }
    else {
        msg = DP_msg_draw_dabs_mypaint_blend_new(
            be->stroke.context_id, be->mypaint.dab_flags,
            DP_int_to_uint32(be->layer_id), be->mypaint.dab_x,
            be->mypaint.dab_y, be->mypaint.dab_color,
            get_dab_blend_mode(paint_mode, blend_mode), set_mypaint_blend_dabs,
            used, be);
    }
    be->push_message(be->user, msg);
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
                                  bool push_undo_point, bool mirror, bool flip,
                                  float zoom, float angle)
{
    DP_ASSERT(be);
    DP_ASSERT(!be->stroke.in_progress);
    DP_ASSERT(be->dabs.used == 0);
    DP_PERF_BEGIN_DETAIL(fn, "stroke_begin", "active=%d", (int)be->active);
    DP_EVENT_LOG("stroke_begin");

    be->stroke.context_id = context_id;
    be->stroke.zoom = zoom;
    // We currently only use the view angle for MyPaint brushes, which take it
    // as radians (and then convert it back to degrees again internally.)
    be->stroke.angle_rad = DP_double_to_float(angle * M_PI / 180.0);
    be->stroke.mirror = mirror;
    be->stroke.flip = flip;
    if (push_undo_point) {
        be->push_message(be->user, DP_msg_undo_point_new(context_id));
    }
    be->spline.fill = 0;
    be->spline.offset = 0;
    be->smoother.fill = 0;
    be->smoother.offset = 0;
    set_poll_control_enabled(be, true);

    DP_PERF_END(fn);
}


static int get_classic_smudge_diameter(const DP_ClassicBrush *cb,
                                       float pressure, float velocity,
                                       float distance)
{
    int diameter = DP_float_to_int(
        DP_classic_brush_size_at(cb, pressure, velocity, distance) + 0.5f);
    return CLAMP(diameter, 2, 255);
}

static DP_UPixelFloat sample_classic_smudge(DP_BrushEngine *be,
                                            DP_ClassicBrush *cb,
                                            DP_LayerContent *lc, float x,
                                            float y, float pressure,
                                            float velocity, float distance)
{
    int diameter =
        get_classic_smudge_diameter(cb, pressure, velocity, distance);
    return DP_layer_content_sample_color_at(
        lc, get_stamp_buffer(be), DP_float_to_int(x), DP_float_to_int(y),
        diameter, true, is_pigment_mode(DP_classic_brush_blend_mode(cb)),
        &be->last_diameter);
}

static void update_classic_smudge(DP_BrushEngine *be, DP_ClassicBrush *cb,
                                  DP_LayerContent *lc, float x, float y,
                                  float pressure, float velocity,
                                  float distance)
{
    float smudge = DP_classic_brush_smudge_at(cb, pressure, velocity, distance);
    int smudge_distance = ++be->classic.smudge_distance;
    if (smudge > 0.0f && smudge_distance > cb->resmudge && lc) {
        DP_UPixelFloat sample = sample_classic_smudge(
            be, cb, lc, x, y, pressure, velocity, distance);
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

// From libmypaint. Not in a header file, but declared extern.
float exp_decay(float T_const, float t);

static float classic_velocity(float last_velocity)
{
    // From libmypaint's speed calculation.
    float velocity =
        logf(CLASSIC_VELOCITY_GAMMA + last_velocity) * CLASSIC_VELOCITY_M
        + CLASSIC_VELOCITY_Q;
    return DP_max_float(velocity, 0.0f);
}

static float classic_update_velocity(DP_BrushEngine *be, float norm_dist,
                                     float dt)
{
    float last_velocity = be->classic.last_velocity;
    float velocity = classic_velocity(last_velocity);
    // From libmypaint's speed calculation.
    be->classic.last_velocity =
        last_velocity
        + (norm_dist - last_velocity)
              * (1.0f - exp_decay(CLASSIC_VELOCITY_SLOWNESS, dt));
    return velocity;
}

static void first_dab_pixel(DP_BrushEngine *be, DP_ClassicBrush *cb, float x,
                            float y, float pressure)
{
    be->classic.pixel_length = 0;
    add_dab_pixel(be, cb, DP_float_to_int(x), DP_float_to_int(y), pressure,
                  0.0f, 0.0f);
}

static void stroke_pixel(DP_BrushEngine *be, DP_ClassicBrush *cb,
                         DP_LayerContent *lc, float x, float y, float pressure,
                         float delta_sec)
{
    float last_x = be->classic.last_x;
    float last_y = be->classic.last_y;
    float last_pressure = be->classic.last_pressure;
    float diff_x = x - last_x;
    float diff_y = y - last_y;
    float dist = hypotf(diff_x, diff_y);
    float dp = (pressure - last_pressure) / dist;

    int x0 = DP_float_to_int(last_x);
    int y0 = DP_float_to_int(last_y);
    int x1 = DP_float_to_int(x);
    int y1 = DP_float_to_int(y);

    int pixel_diff_x = x1 - x0;
    int pixel_diff_y = y1 - y0;
    int step_x = pixel_diff_x < 0 ? -1 : 1;
    int step_y = pixel_diff_y < 0 ? -1 : 1;
    int dx = pixel_diff_x * step_x * 2;
    int dy = pixel_diff_y * step_y * 2;

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

    int length = be->classic.pixel_length;
    float dab_p = last_pressure;
    float zoom = be->stroke.zoom;
    float norm_dist =
        hypotf(diff_x / delta_sec * zoom, diff_y / delta_sec * zoom);
    float dab_v = classic_update_velocity(be, norm_dist, delta_sec);
    float dab_d = be->classic.last_distance;
    int fraction = db - (da / 2);
    while (*a0 != a1) {
        float spacing = DP_classic_brush_spacing_at(cb, dab_p, dab_v, dab_d);
        int pixel_spacing = DP_float_to_int(spacing);
        if (fraction >= 0) {
            *b0 += step_b;
            fraction -= da;
        }
        *a0 += step_a;
        fraction += db;
        length += 1;
        dab_d += 1.0f;
        if (length >= pixel_spacing) {
            update_classic_smudge(be, cb, lc, DP_int_to_float(x0),
                                  DP_int_to_float(y0), dab_p, dab_v, dab_d);
            add_dab_pixel(be, cb, x0, y0, dab_p, dab_v, dab_d);
            length = 0;
        }
        dab_p = CLAMP(dab_p + dp, 0.0f, 1.0f);
    }

    be->classic.pixel_length = length;
    be->classic.last_distance = dab_d;
}

static void first_dab_soft(DP_BrushEngine *be, DP_ClassicBrush *cb, float x,
                           float y, float pressure)
{
    be->classic.soft_length = 0.0f;
    add_dab_soft(be, cb, x, y, pressure, 0.0f, 0.0f, false, false);
}

static void stroke_soft(DP_BrushEngine *be, DP_ClassicBrush *cb,
                        DP_LayerContent *lc, float x, float y, float pressure,
                        float delta_sec)
{
    float last_x = be->classic.last_x;
    float last_y = be->classic.last_y;
    float diff_x = x - last_x;
    float diff_y = y - last_y;
    float dist = hypotf(diff_x, diff_y);

    bool left;
    if (x == last_x) {
        left = be->classic.last_left;
    }
    else {
        left = x < last_x;
        be->classic.last_left = left;
    }

    bool up;
    if (y == last_y) {
        up = be->classic.last_up;
    }
    else {
        up = y < last_y;
        be->classic.last_up = up;
    }

    if (dist >= 0.001f) {
        float dx = diff_x / dist;
        float dy = diff_y / dist;
        float last_pressure = be->classic.last_pressure;
        float dp = (pressure - last_pressure) / dist;

        float dab_d = be->classic.last_distance;
        float spacing0 = DP_max_float(
            DP_classic_brush_spacing_at(
                cb, last_pressure, classic_velocity(be->classic.last_velocity),
                dab_d),
            1.0f);

        float length = be->classic.soft_length;
        float i = length > spacing0 ? 0.0f : length == 0.0f ? spacing0 : length;

        float dab_x = last_x + dx * i;
        float dab_y = last_y + dy * i;
        float dab_p = CLAMP(last_pressure + dp * i, 0.0f, 1.0f);
        float zoom = be->stroke.zoom;
        float norm_dist =
            hypotf(diff_x / delta_sec * zoom, diff_y / delta_sec * zoom);
        float dab_v = classic_update_velocity(be, norm_dist, delta_sec);

        while (i <= dist) {
            float spacing = DP_max_float(
                DP_classic_brush_spacing_at(cb, dab_p, dab_v, dab_d), 1.0f);
            dab_d += spacing;

            update_classic_smudge(be, cb, lc, dab_x, dab_y, dab_p, dab_v,
                                  dab_d);
            add_dab_soft(be, cb, dab_x, dab_y, dab_p, dab_v, dab_d, left, up);

            dab_x += dx * spacing;
            dab_y += dy * spacing;
            dab_p = CLAMP(dab_p + dp * spacing, 0.0f, 1.0f);
            i += spacing;
        }

        be->classic.soft_length = i - dist;
        be->classic.last_distance = dab_d;
    }
}

static void stroke_to_classic(
    DP_BrushEngine *be, float x, float y, float pressure, long long time_msec,
    void (*first_dab)(DP_BrushEngine *, DP_ClassicBrush *, float, float, float),
    void (*stroke)(DP_BrushEngine *, DP_ClassicBrush *, DP_LayerContent *,
                   float, float, float, float))
{
    DP_ClassicBrush *cb = &be->classic.brush;
    DP_LayerContent *lc = be->lc;
    if (be->stroke.in_progress) {
        float delta_sec = DP_max_float(
            DP_llong_to_float(time_msec - be->stroke.last_time_msec) / 1000.0f,
            0.0001f);
        stroke(be, cb, lc, x, y, pressure, delta_sec);
    }
    else {
        be->classic.last_velocity = 0.0f;
        be->classic.last_distance = 0.0f;
        be->classic.last_left = false;
        be->classic.last_up = false;
        be->stroke.in_progress = true;
        bool colorpick = cb->colorpick
                      && DP_classic_brush_blend_mode(cb) != DP_BLEND_MODE_ERASE
                      && lc;
        if (colorpick) {
            be->classic.smudge_color =
                sample_classic_smudge(be, cb, lc, x, y, pressure, 0.0f, 0.0f);
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


static void tilt_to_mypaint(float xtilt, float ytilt, float angle, bool mirror,
                            bool flip, float *out_xtilt, float *out_ytilt)
{
    if (xtilt == 0.0f && ytilt == 0.0f) {
        *out_xtilt = 0.0f;
        *out_ytilt = 0.0f;
    }
    else {
        float x, y;
        if (angle == 0.0f) {
            x = xtilt;
            y = ytilt;
        }
        else {
            float orientation = atan2f(ytilt, xtilt);
            float magnitude = hypotf(xtilt, ytilt);
            if (mirror == flip) {
                orientation -= angle;
            }
            else {
                orientation += angle;
            }
            x = cosf(orientation) * magnitude;
            y = sinf(orientation) * magnitude;
        }
        *out_xtilt = CLAMP(x, -60.0f, 60.0f) / (mirror ? -60.0f : 60.0f);
        *out_ytilt = CLAMP(y, -60.0f, 60.0f) / (flip ? -60.0f : 60.0f);
    }
}

static void stroke_to_mypaint(DP_BrushEngine *be, DP_BrushPoint bp)
{
    MyPaintBrush *mb = be->mypaint_brush;
    MyPaintSurface2 *surface = &be->mypaint_surface2;
    float zoom = be->stroke.zoom;
    float angle = be->stroke.angle_rad;
    float rotation = bp.rotation / 360.0f;
    float xtilt, ytilt;
    tilt_to_mypaint(bp.xtilt, bp.ytilt, angle, be->stroke.mirror,
                    be->stroke.flip, &xtilt, &ytilt);

    double delta_sec;
    if (be->stroke.in_progress) {
        delta_sec = DP_llong_to_double(bp.time_msec - be->stroke.last_time_msec)
                  / 1000.0;
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
        mypaint_brush_stroke_to_2(mb, surface, bp.x, bp.y, 0.0f, xtilt, ytilt,
                                  1000.0f, zoom, angle, rotation);
        delta_sec = 0.0;
    }

    mypaint_brush_stroke_to_2(mb, surface, bp.x, bp.y, bp.pressure, xtilt,
                              ytilt, delta_sec, zoom, angle, rotation);
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
        DP_EVENT_LOG(
            "stroke_to active=pixel x=%f y=%f pressure=%f time_msec=%lld", bp.x,
            bp.y, bp.pressure, bp.time_msec);
        stroke_to_classic(be, bp.x, bp.y, bp.pressure, bp.time_msec,
                          first_dab_pixel, stroke_pixel);
        break;
    case DP_BRUSH_ENGINE_ACTIVE_SOFT:
        DP_EVENT_LOG(
            "stroke_to active=soft x=%f y=%f pressure=%f time_msec=%lld", bp.x,
            bp.y, bp.pressure, bp.time_msec);
        stroke_to_classic(be, bp.x, bp.y, bp.pressure, bp.time_msec,
                          first_dab_soft, stroke_soft);
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


static void handle_stroke_stabilizer(DP_BrushEngine *be, DP_BrushPoint bp,
                                     DP_CanvasState *cs_or_null)
{
    if (be->stabilizer.sample_count > 0) {
        stabilizer_stroke_to(be, bp);
    }
    else {
        stroke_to(be, bp, cs_or_null);
    }
}


static DP_BrushPoint smoother_get(DP_BrushPoint *points, int size)
{
    DP_ASSERT(size > 0);
    // A simple unweighted sliding-average smoother
    DP_BrushPoint first = points[0];
    float x = first.x;
    float y = first.y;
    float pressure = first.pressure;
    float xtilt = first.xtilt;
    float ytilt = first.ytilt;
    float rotation = first.rotation;
    double time_msec = DP_llong_to_double(first.time_msec);

    for (int i = 1; i < size; ++i) {
        DP_BrushPoint bp = points[i];
        x += bp.x;
        y += bp.y;
        pressure += bp.pressure;
        xtilt += bp.xtilt;
        ytilt += bp.ytilt;
        rotation += bp.rotation;
        time_msec += DP_llong_to_double(bp.time_msec);
    }

    float fsize = DP_int_to_float(size);
    return (DP_BrushPoint){
        x / fsize,
        y / fsize,
        pressure / fsize,
        xtilt / fsize,
        ytilt / fsize,
        rotation / fsize,
        DP_double_to_llong(time_msec / DP_int_to_double(size) + 0.5),
    };
}

static void smoother_stroke_to(DP_BrushEngine *be, DP_BrushPoint bp,
                               DP_CanvasState *cs_or_null)
{
    DP_ASSERT(be->smoother.size > 0);
    int size = be->smoother.size;
    int fill = be->smoother.fill;
    int offset = be->smoother.offset;
    DP_BrushPoint *points = be->smoother.points;

    if (fill == 0) {
        // Pad the buffer with this point, so we blend away from it gradually as
        // we gain more points. We still only count this as one point so we know
        // how much real data we have to drain if it was a very short stroke.
        for (int i = 0; i < size; ++i) {
            points[i] = bp;
        }
    }
    else {
        if (--offset < 0) {
            offset = size - 1;
        }
        points[offset] = bp;
        be->smoother.offset = offset;
    }

    if (fill < size) {
        be->smoother.fill = fill + 1;
    }

    handle_stroke_stabilizer(be, smoother_get(points, size), cs_or_null);
}

static void handle_stroke_smoother(DP_BrushEngine *be, DP_BrushPoint bp,
                                   DP_CanvasState *cs_or_null)
{
    if (be->smoother.size > 0) {
        smoother_stroke_to(be, bp, cs_or_null);
    }
    else {
        handle_stroke_stabilizer(be, bp, cs_or_null);
    }
}

static void smoother_remove(DP_BrushPoint *points, int size, int fill,
                            int offset)
{
    // Pad the buffer with the final point, overwriting the oldest first, for
    // symmetry with starting. For very short strokes this should really set all
    // points between --fill and size - 1.
    points[(offset + fill - 1) % size] = points[offset];
}

static void smoother_drain(DP_BrushEngine *be, DP_CanvasState *cs_or_null)
{
    int size = be->smoother.size;
    if (size > 0 && be->smoother.finish_strokes) {
        DP_BrushPoint *points = be->smoother.points;
        int fill = be->smoother.fill;
        int offset = be->smoother.offset;
        if (fill > 1) {
            smoother_remove(points, size, fill, offset);
            --fill;
            while (fill > 0) {
                handle_stroke_stabilizer(be, smoother_get(points, size),
                                         cs_or_null);
                smoother_remove(points, size, fill, offset);
                --fill;
            }
        }
    }
}


// Points are interpolated along a spline when the distance between two points
// is sufficiently large. We keep 4 points around for this, which seems to be
// enough for getting good results.

static DP_SplinePoint *spline_push(DP_BrushEngine *be, DP_BrushPoint bp,
                                   float length)
{
    DP_ASSERT(be->spline.fill >= 0);
    DP_ASSERT(be->spline.fill < 4);
    int fill = be->spline.fill++;
    DP_SplinePoint *sp = &be->spline.points[(be->spline.offset + fill) % 4];
    *sp = (DP_SplinePoint){bp, length, false};
    return sp;
}

static void spline_shift(DP_BrushEngine *be)
{
    DP_ASSERT(be->spline.fill == 4);
    DP_ASSERT(be->spline.points[be->spline.offset].drawn);
    be->spline.fill = 3;
    be->spline.offset = (be->spline.offset + 1) % 4;
}

static DP_SplinePoint *spline_at(DP_BrushEngine *be, int index)
{
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < be->spline.fill);
    DP_ASSERT(be->spline.fill <= 4);
    return &be->spline.points[(be->spline.offset + index) % 4];
}

static float lerpf(float a, float b, float t)
{
    return a + t * (b - a);
}

static long long lerpll(long long a, long long b, float t)
{
    return a + DP_float_to_llong(t * DP_llong_to_float(b - a));
}

static void spline_interpolate(DP_BrushEngine *be, DP_CanvasState *cs_or_null,
                               DP_BrushPoint p2, float length, DP_BrushPoint p3)
{
    DP_ASSERT(length > SPLINE_DISTANCE);
    DP_ASSERT(be->spline.fill > 1);
    DP_BrushPoint p0 = spline_at(be, 0)->p;
    DP_BrushPoint p1 = spline_at(be, 1)->p;
    float countf = ceilf(length / SPLINE_DISTANCE);
    int count = DP_float_to_int(countf);
    for (int i = 1; i < count; ++i) {
        // SPDX-SnippetBegin
        // SPDX-License-Identifier: MIT
        // SDPX—SnippetName: spline interpolation from perfect-cursors
        float t = DP_int_to_float(i) / countf;
        float tt = t * t;
        float ttt = tt * t;
        float q1 = -ttt + 2.0f * tt - t;
        float q2 = 3.0f * ttt - 5.0f * tt + 2.0f;
        float q3 = -3.0f * ttt + 4.0f * tt + t;
        float q4 = ttt - tt;
        float x = (p0.x * q1 + p1.x * q2 + p2.x * q3 + p3.x * q4) / 2.0f;
        float y = (p0.y * q1 + p1.y * q2 + p2.y * q3 + p3.y * q4) / 2.0f;
        // SPDX-SnippetEnd
        handle_stroke_smoother(
            be,
            (DP_BrushPoint){x, y, lerpf(p1.pressure, p2.pressure, t),
                            lerpf(p1.xtilt, p2.xtilt, t),
                            lerpf(p1.ytilt, p2.ytilt, t),
                            lerpf(p1.rotation, p2.rotation, t),
                            lerpll(p1.time_msec, p2.time_msec, t)},
            cs_or_null);
    }
    handle_stroke_smoother(be, p2, cs_or_null);
}

static void handle_stroke_spline(DP_BrushEngine *be, DP_BrushPoint bp,
                                 DP_CanvasState *cs_or_null)
{
    int prev_fill = be->spline.fill;
    DP_ASSERT(prev_fill >= 0);
    DP_ASSERT(prev_fill < 4);
    if (prev_fill == 0) {
        DP_SplinePoint *sp = spline_push(be, bp, 0.0f);
        handle_stroke_smoother(be, bp, cs_or_null);
        sp->drawn = true;
    }
    else {
        DP_SplinePoint *prev_sp = spline_at(be, prev_fill - 1);
        DP_BrushPoint prev_bp = prev_sp->p;
        float length = hypotf(prev_bp.y - bp.y, prev_bp.x - bp.x);
        DP_SplinePoint *sp = spline_push(be, bp, length);

        if (prev_fill == 3) {
            if (!prev_sp->drawn) {
                spline_interpolate(be, cs_or_null, prev_bp, prev_sp->length,
                                   bp);
                prev_sp->drawn = true;
            }
            spline_shift(be);
        }

        if (prev_fill == 1 || length <= SPLINE_DISTANCE) {
            handle_stroke_smoother(be, bp, cs_or_null);
            sp->drawn = true;
        }
    }
}

static void spline_drain(DP_BrushEngine *be, DP_CanvasState *cs_or_null)
{
    if (be->spline.fill == 3) {
        DP_SplinePoint *sp = spline_at(be, 2);
        if (!sp->drawn) {
            spline_interpolate(be, cs_or_null, sp->p, sp->length, sp->p);
        }
    }
}


void DP_brush_engine_stroke_to(DP_BrushEngine *be, DP_BrushPoint bp,
                               DP_CanvasState *cs_or_null)
{
    DP_ASSERT(be);
    if (be->spline.enabled) {
        handle_stroke_spline(be, bp, cs_or_null);
    }
    else {
        handle_stroke_smoother(be, bp, cs_or_null);
    }
}

void DP_brush_engine_poll(DP_BrushEngine *be, long long time_msec,
                          DP_CanvasState *cs_or_null)
{
    DP_ASSERT(be);

    if (be->spline.fill == 3) {
        DP_SplinePoint *sp = spline_at(be, 2);
        if (!sp->drawn && time_msec - sp->p.time_msec > SPLINE_DRAIN_DELAY) {
            spline_interpolate(be, cs_or_null, sp->p, sp->length, sp->p);
            sp->drawn = true;
        }
    }

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

    set_poll_control_enabled(be, false);
    spline_drain(be, cs_or_null);
    smoother_drain(be, cs_or_null);
    if (be->stabilizer.active) {
        stabilizer_finish(be, time_msec, cs_or_null);
    }

    DP_layer_content_decref_nullable(be->lc);
    be->lc = NULL;
    be->cs = NULL;

    DP_brush_engine_dabs_flush(be);

    if (push_pen_up) {
        be->push_message(be->user,
                         DP_msg_pen_up_new(be->stroke.context_id,
                                           DP_int_to_uint32(be->layer_id)));
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
