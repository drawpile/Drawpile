// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_engine.h"
#include "brush.h"
#include "canvas_state.h"
#include "compress.h"
#include "flood_fill.h"
#include "layer_content.h"
#include "layer_routes.h"
#include "selection.h"
#include "selection_set.h"
#include "tile.h"
#include "tile_iterator.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/event_log.h>
#include <dpcommon/geom.h>
#include <dpcommon/perf.h>
#include <dpcommon/queue.h>
#include <dpcommon/vector.h>
#include <dpmsg/ids.h>
#include <dpmsg/message.h>
#include <math.h>
#include <mypaint-brush.h>
#include <mypaint.h>
#include <rng-double.h>
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

typedef enum DP_BrushEngineFloodTarget {
    DP_BRUSH_ENGINE_FLOOD_TARGET_NONE,
    DP_BRUSH_ENGINE_FLOOD_TARGET_LAYER,
    DP_BRUSH_ENGINE_FLOOD_TARGET_SELECTION,
} DP_BrushEngineFloodTarget;

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

typedef struct DP_PixelDabParams {
    uint8_t flags;
    uint8_t opacity;
    uint16_t size;
    int32_t x;
    int32_t y;
    uint32_t color;
} DP_PixelDabParams;

typedef struct DP_ClassicDabParams {
    uint8_t flags;
    uint8_t opacity;
    uint8_t hardness;
    int32_t x;
    int32_t y;
    uint32_t size;
    uint32_t color;
} DP_ClassicDabParams;

typedef struct DP_MyPaintDabParams {
    uint8_t flags;
    uint8_t mode;
    uint8_t lock_alpha;
    uint8_t colorize;
    uint8_t posterize;
    uint8_t opacity;
    uint8_t hardness;
    uint8_t angle;
    uint8_t aspect_ratio;
    int32_t x;
    int32_t y;
    uint32_t size;
    uint32_t color;
} DP_MyPaintDabParams;

struct DP_MaskSync {
    DP_Atomic refcount;
    DP_Atomic sync_id;
    int last_sync_id;
};

struct DP_StrokeEngine {
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
    DP_StrokeEnginePushPointFn push_point;
    DP_StrokeEnginePollControlFn poll_control;
    void *user;
};

struct DP_BrushEngine {
    DP_StrokeEngine se;
    DP_CanvasState *cs;
    DP_LayerContent *lc;
    bool lc_valid;
    void *buffer;
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
        bool active;
        bool mirror;
        bool flip;
        bool in_progress;
        bool sync_samples;
        bool compatibility_mode;
        long long last_time_msec;
    } stroke;
    struct {
        bool active;
        bool preview;
        int next_selection_id;
        int current_selection_id;
        int sync_id;
        DP_MaskSync *ms;
        DP_LayerContent *lc;
        size_t capacity;
        bool *map;
        ZSTD_CCtx *zstd_cctx;
    } mask;
    struct {
        DP_LayerContent *lc;
        DP_TransientLayerContent *state;
        double tolerance;
        int expand;
        DP_BrushEngineFloodTarget target;
        struct {
            int x;
            int y;
            int shape;
            int size;
            int size_available;
            uint8_t aspect_ratio;
            uint8_t angle;
            unsigned char *buffer;
        } mask;
    } flood;
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
            float indirect_alpha;
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
    struct {
        bool snap;
        bool perfect;
        bool have_last;
        bool have_tentative;
        int last_x;
        int last_y;
        int tentative_x;
        int tentative_y;
        union {
            int dummy; // Make this initializable without the compiler whining.
            DP_PixelDabParams pixel;
            DP_MyPaintDabParams mypaint;
        } tentative_dab;
    } pixel;
    DP_BrushEnginePushMessageFn push_message;
    DP_BrushEnginePollControlFn poll_control;
    DP_BrushEngineSyncFn sync;
    void *user;
};


DP_MaskSync *DP_mask_sync_new(void)
{
    DP_MaskSync *ms = DP_malloc(sizeof(*ms));
    *ms = (DP_MaskSync){DP_ATOMIC_INIT(1), DP_ATOMIC_INIT(0), 0};
    return ms;
}

DP_MaskSync *DP_mask_sync_incref(DP_MaskSync *ms)
{
    DP_ASSERT(ms);
    DP_ASSERT(DP_atomic_get(&ms->refcount) > 0);
    DP_atomic_inc(&ms->refcount);
    return ms;
}

DP_MaskSync *DP_mask_sync_incref_nullable(DP_MaskSync *ms_or_null)
{
    return ms_or_null ? DP_mask_sync_incref(ms_or_null) : NULL;
}

void DP_mask_sync_decref(DP_MaskSync *ms)
{
    DP_ASSERT(ms);
    DP_ASSERT(DP_atomic_get(&ms->refcount) > 0);
    if (DP_atomic_dec(&ms->refcount)) {
        DP_free(ms);
    }
}

void DP_mask_sync_decref_nullable(DP_MaskSync *ms_or_null)
{
    if (ms_or_null) {
        DP_mask_sync_decref(ms_or_null);
    }
}


static DP_PaintMode convert_paint_mode(DP_BrushEngine *be,
                                       DP_PaintMode paint_mode)
{
    if (be->stroke.compatibility_mode) {
        return (DP_PaintMode)DP_paint_mode_to_compatible((uint8_t)paint_mode);
    }
    else {
        return paint_mode;
    }
}

static DP_PaintMode get_classic_brush_paint_mode(DP_BrushEngine *be,
                                                 const DP_ClassicBrush *cb)
{
    return convert_paint_mode(be, cb->paint_mode);
}

static DP_PaintMode get_classic_paint_mode(DP_BrushEngine *be)
{
    return get_classic_brush_paint_mode(be, &be->classic.brush);
}

static DP_PaintMode get_mypaint_paint_mode(DP_BrushEngine *be)
{
    return convert_paint_mode(be, be->mypaint.paint_mode);
}


static DP_BlendMode convert_blend_mode(DP_BrushEngine *be,
                                       DP_BlendMode blend_mode)
{
    if (be->stroke.compatibility_mode) {
        return (DP_BlendMode)DP_blend_mode_to_compatible((uint8_t)blend_mode);
    }
    else {
        return blend_mode;
    }
}

static DP_BlendMode get_classic_brush_blend_mode(DP_BrushEngine *be,
                                                 const DP_ClassicBrush *cb)
{
    return convert_blend_mode(be, DP_classic_brush_blend_mode(cb));
}

static DP_BlendMode get_classic_blend_mode(DP_BrushEngine *be)
{
    return get_classic_brush_blend_mode(be, &be->classic.brush);
}

static DP_BlendMode get_mypaint_blend_mode(DP_BrushEngine *be)
{
    return convert_blend_mode(be, be->mypaint.blend_mode);
}


static float brush_engine_randf(DP_BrushEngine *be)
{
    return fabsf(fmodf(DP_double_to_float(rng_double_next(
                           mypaint_brush_rng(be->mypaint_brush))),
                       1.0f));
}


static DP_LayerContent *search_layer(DP_CanvasState *cs, int layer_id)
{
    if (cs && layer_id > 0) {
        DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
        DP_LayerRoutesSelEntry lrse =
            DP_layer_routes_search_sel(lr, cs, layer_id);
        if (DP_layer_routes_sel_entry_is_valid_source(&lrse)) {
            DP_LayerContent *lc = DP_layer_routes_sel_entry_content(&lrse, cs);
            return lc;
        }
    }
    return NULL;
}

static DP_LayerContent *update_sample_layer_content(DP_BrushEngine *be)
{
    if (be->stroke.sync_samples) {
        DP_ASSERT(be->sync); // Checked when setting sync_samples.
        DP_EVENT_LOG("update_sample_layer_content sync");
        DP_CanvasState *cs = be->sync(be->user);
        DP_EVENT_LOG("update_sample_layer_content synced");
        if (cs) {
            if (cs == be->cs) {
                DP_canvas_state_decref(cs);
            }
            else {
                DP_canvas_state_decref_nullable(be->cs);
                be->cs = cs;
                be->lc_valid = false;
            }
        }
    }

    if (!be->lc_valid) {
        be->lc = search_layer(be->cs, be->layer_id);
        be->lc_valid = true;
    }

    return be->lc;
}


// SPDX-SnippetBegin
// SPDX-License-Identifier: GPL-3.0-or-later
// SDPX—SnippetName: stabilizer implementation from Krita

static void stabilizer_queue_push(DP_StrokeEngine *se, DP_BrushPoint bp)
{
    *((DP_BrushPoint *)DP_queue_push(&se->stabilizer.queue,
                                     sizeof(DP_BrushPoint))) = bp;
}

static DP_BrushPoint stabilizer_queue_at(DP_StrokeEngine *se, size_t i)
{
    return *(DP_BrushPoint *)DP_queue_at(&se->stabilizer.queue,
                                         sizeof(DP_BrushPoint), i);
}

static void stabilizer_queue_replace(DP_StrokeEngine *se, DP_BrushPoint bp)
{
    DP_queue_shift(&se->stabilizer.queue);
    stabilizer_queue_push(se, bp);
}

static void stabilizer_points_clear(DP_StrokeEngine *se, long long time_msec)
{
    DP_BrushPoint *last_point_or_null =
        DP_vector_last(&se->stabilizer.points, sizeof(DP_BrushPoint));
    if (last_point_or_null) {
        se->stabilizer.last_point = *last_point_or_null;
    }
    se->stabilizer.points.used = 0;
    se->stabilizer.last_time_msec = time_msec;
}

static void stabilizer_points_push(DP_StrokeEngine *se, DP_BrushPoint bp)
{
    *((DP_BrushPoint *)DP_vector_push(&se->stabilizer.points,
                                      sizeof(DP_BrushPoint))) = bp;
}

static DP_BrushPoint stabilizer_points_get(DP_StrokeEngine *se, long long i,
                                           double alpha)
{
    size_t k = DP_double_to_size(alpha * DP_llong_to_double(i));
    if (k < se->stabilizer.points.used) {
        return *(DP_BrushPoint *)DP_vector_at(&se->stabilizer.points,
                                              sizeof(DP_BrushPoint), k);
    }
    else {
        return se->stabilizer.last_point;
    }
}

static void stabilizer_init(DP_StrokeEngine *se, DP_BrushPoint bp)
{
    se->stabilizer.active = true;

    int queue_size =
        DP_max_int(STABILIZER_MIN_QUEUE_SIZE, se->stabilizer.sample_count);
    se->stabilizer.queue.used = 0;
    for (int i = 0; i < queue_size; ++i) {
        stabilizer_queue_push(se, bp);
    }

    stabilizer_points_clear(se, bp.time_msec);
    stabilizer_points_push(se, bp);
}

static void stabilizer_stroke_to(DP_StrokeEngine *se, DP_BrushPoint bp)
{
    if (se->stabilizer.active) {
        stabilizer_points_push(se, bp);
    }
    else {
        stabilizer_init(se, bp);
    }
}

static DP_BrushPoint stabilizer_stabilize(DP_StrokeEngine *se, DP_BrushPoint bp)
{
    size_t used = se->stabilizer.queue.used;
    // First queue entry is stale, since bp is the one that's replacing it.
    for (size_t i = 1; i < used; ++i) {
        // Uniform averaging.
        float k = DP_size_to_float(i) / DP_size_to_float(i + 1);
        float k1 = 1.0f - k;
        DP_BrushPoint sample = stabilizer_queue_at(se, i);
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

static void stabilizer_finish(DP_StrokeEngine *se, long long time_msec,
                              DP_CanvasState *cs_or_null)
{
    if (se->stabilizer.finish_strokes) {
        // Flush existing events.
        DP_stroke_engine_poll(se, time_msec, cs_or_null);
        // Drain the queue with a delay matching its size.
        stabilizer_points_push(se, se->stabilizer.last_point);
        long long queue_size_samples = DP_size_to_llong(
            se->stabilizer.queue.used * STABILIZER_SAMPLE_TIME);
        DP_stroke_engine_poll(
            se, se->stabilizer.last_time_msec + queue_size_samples, cs_or_null);
    }
    se->stabilizer.active = false;
}

// SPDX-SnippetEnd


static DP_StrokeEngine
stroke_engine_init(DP_StrokeEnginePushPointFn push_point,
                   DP_StrokeEnginePollControlFn poll_control_or_null,
                   void *user)
{
    DP_StrokeEngine se = {
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
        push_point,
        poll_control_or_null,
        user,
    };
    DP_queue_init(&se.stabilizer.queue, 128, sizeof(DP_BrushPoint));
    DP_vector_init(&se.stabilizer.points, 128, sizeof(DP_BrushPoint));
    return se;
}

DP_StrokeEngine *
DP_stroke_engine_new(DP_StrokeEnginePushPointFn push_point,
                     DP_StrokeEnginePollControlFn poll_control_or_null,
                     void *user)
{
    DP_ASSERT(push_point);
    DP_StrokeEngine *se = DP_malloc(sizeof(*se));
    *se = stroke_engine_init(push_point, poll_control_or_null, user);
    return se;
}

static void stroke_engine_dispose(DP_StrokeEngine *se)
{
    DP_free(se->smoother.points);
    DP_vector_dispose(&se->stabilizer.points);
    DP_queue_dispose(&se->stabilizer.queue);
}

void DP_stroke_engine_free(DP_StrokeEngine *se)
{
    if (se) {
        stroke_engine_dispose(se);
        DP_free(se);
    }
}

void DP_stroke_engine_params_set(DP_StrokeEngine *se,
                                 const DP_StrokeEngineStrokeParams *sesp)
{
    DP_ASSERT(se);
    DP_ASSERT(sesp);
    se->spline.enabled = sesp->interpolate;
    se->stabilizer.sample_count = sesp->stabilizer_sample_count;
    se->stabilizer.finish_strokes = sesp->stabilizer_finish_strokes;

    int smoothing = DP_max_int(0, sesp->smoothing);
    se->smoother.size = smoothing;
    se->smoother.finish_strokes = sesp->smoothing_finish_strokes;
    if (se->smoother.capacity < smoothing) {
        se->smoother.points =
            DP_realloc(se->smoother.points, sizeof(*se->smoother.points)
                                                * DP_int_to_size(smoothing));
        se->smoother.capacity = smoothing;
    }
}

static void stroke_engine_poll_control(DP_StrokeEngine *se, bool enable)
{
    DP_StrokeEnginePollControlFn poll_control = se->poll_control;
    if (poll_control) {
        poll_control(se->user, enable);
    }
}

void DP_stroke_engine_stroke_begin(DP_StrokeEngine *se)
{
    DP_ASSERT(se);
    se->spline.fill = 0;
    se->spline.offset = 0;
    se->smoother.fill = 0;
    se->smoother.offset = 0;
    stroke_engine_poll_control(se, true);
}

static void stroke_engine_push(DP_StrokeEngine *se, DP_BrushPoint bp,
                               DP_CanvasState *cs_or_null)
{
    se->push_point(se->user, bp, cs_or_null);
}


static void handle_stroke_stabilizer(DP_StrokeEngine *se, DP_BrushPoint bp,
                                     DP_CanvasState *cs_or_null)
{
    if (se->stabilizer.sample_count > 0) {
        stabilizer_stroke_to(se, bp);
    }
    else {
        stroke_engine_push(se, bp, cs_or_null);
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

static void smoother_stroke_to(DP_StrokeEngine *se, DP_BrushPoint bp,
                               DP_CanvasState *cs_or_null)
{
    DP_ASSERT(se->smoother.size > 0);
    int size = se->smoother.size;
    int fill = se->smoother.fill;
    int offset = se->smoother.offset;
    DP_BrushPoint *points = se->smoother.points;

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
        se->smoother.offset = offset;
    }

    if (fill < size) {
        se->smoother.fill = fill + 1;
    }

    handle_stroke_stabilizer(se, smoother_get(points, size), cs_or_null);
}

static void handle_stroke_smoother(DP_StrokeEngine *se, DP_BrushPoint bp,
                                   DP_CanvasState *cs_or_null)
{
    if (se->smoother.size > 0) {
        smoother_stroke_to(se, bp, cs_or_null);
    }
    else {
        handle_stroke_stabilizer(se, bp, cs_or_null);
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

static void smoother_drain(DP_StrokeEngine *se, DP_CanvasState *cs_or_null)
{
    int size = se->smoother.size;
    if (size > 0 && se->smoother.finish_strokes) {
        DP_BrushPoint *points = se->smoother.points;
        int fill = se->smoother.fill;
        int offset = se->smoother.offset;
        if (fill > 1) {
            smoother_remove(points, size, fill, offset);
            --fill;
            while (fill > 0) {
                handle_stroke_stabilizer(se, smoother_get(points, size),
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

static DP_SplinePoint *spline_push(DP_StrokeEngine *se, DP_BrushPoint bp,
                                   float length)
{
    DP_ASSERT(se->spline.fill >= 0);
    DP_ASSERT(se->spline.fill < 4);
    int fill = se->spline.fill++;
    DP_SplinePoint *sp = &se->spline.points[(se->spline.offset + fill) % 4];
    *sp = (DP_SplinePoint){bp, length, false};
    return sp;
}

static void spline_shift(DP_StrokeEngine *se)
{
    DP_ASSERT(se->spline.fill == 4);
    DP_ASSERT(se->spline.points[se->spline.offset].drawn);
    se->spline.fill = 3;
    se->spline.offset = (se->spline.offset + 1) % 4;
}

static DP_SplinePoint *spline_at(DP_StrokeEngine *se, int index)
{
    DP_ASSERT(index >= 0);
    DP_ASSERT(index < se->spline.fill);
    DP_ASSERT(se->spline.fill <= 4);
    return &se->spline.points[(se->spline.offset + index) % 4];
}

static float lerpf(float a, float b, float t)
{
    return a + t * (b - a);
}

static long long lerpll(long long a, long long b, float t)
{
    return a + DP_float_to_llong(t * DP_llong_to_float(b - a));
}

static void spline_interpolate(DP_StrokeEngine *se, DP_CanvasState *cs_or_null,
                               DP_BrushPoint p2, float length, DP_BrushPoint p3)
{
    DP_ASSERT(length > SPLINE_DISTANCE);
    DP_ASSERT(se->spline.fill > 1);
    DP_BrushPoint p0 = spline_at(se, 0)->p;
    DP_BrushPoint p1 = spline_at(se, 1)->p;
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
            se,
            (DP_BrushPoint){x, y, lerpf(p1.pressure, p2.pressure, t),
                            lerpf(p1.xtilt, p2.xtilt, t),
                            lerpf(p1.ytilt, p2.ytilt, t),
                            lerpf(p1.rotation, p2.rotation, t),
                            lerpll(p1.time_msec, p2.time_msec, t)},
            cs_or_null);
    }
    handle_stroke_smoother(se, p2, cs_or_null);
}

static void handle_stroke_spline(DP_StrokeEngine *se, DP_BrushPoint bp,
                                 DP_CanvasState *cs_or_null)
{
    int prev_fill = se->spline.fill;
    DP_ASSERT(prev_fill >= 0);
    DP_ASSERT(prev_fill < 4);
    if (prev_fill == 0) {
        DP_SplinePoint *sp = spline_push(se, bp, 0.0f);
        handle_stroke_smoother(se, bp, cs_or_null);
        sp->drawn = true;
    }
    else {
        DP_SplinePoint *prev_sp = spline_at(se, prev_fill - 1);
        DP_BrushPoint prev_bp = prev_sp->p;
        float length = hypotf(prev_bp.y - bp.y, prev_bp.x - bp.x);
        DP_SplinePoint *sp = spline_push(se, bp, length);

        if (prev_fill == 3) {
            if (!prev_sp->drawn) {
                spline_interpolate(se, cs_or_null, prev_bp, prev_sp->length,
                                   bp);
                prev_sp->drawn = true;
            }
            spline_shift(se);
        }

        if (prev_fill == 1 || length <= SPLINE_DISTANCE) {
            handle_stroke_smoother(se, bp, cs_or_null);
            sp->drawn = true;
        }
    }
}

static void spline_drain(DP_StrokeEngine *se, DP_CanvasState *cs_or_null)
{
    if (se->spline.fill == 3) {
        DP_SplinePoint *sp = spline_at(se, 2);
        if (!sp->drawn) {
            spline_interpolate(se, cs_or_null, sp->p, sp->length, sp->p);
        }
    }
}

void DP_stroke_engine_stroke_to(DP_StrokeEngine *se, DP_BrushPoint bp,
                                DP_CanvasState *cs_or_null)
{
    DP_ASSERT(se);
    if (se->spline.enabled) {
        handle_stroke_spline(se, bp, cs_or_null);
    }
    else {
        handle_stroke_smoother(se, bp, cs_or_null);
    }
}

void DP_stroke_engine_poll(DP_StrokeEngine *se, long long time_msec,
                           DP_CanvasState *cs_or_null)
{
    DP_ASSERT(se);
    if (se->spline.fill == 3) {
        DP_SplinePoint *sp = spline_at(se, 2);
        if (!sp->drawn && time_msec - sp->p.time_msec > SPLINE_DRAIN_DELAY) {
            spline_interpolate(se, cs_or_null, sp->p, sp->length, sp->p);
            sp->drawn = true;
        }
    }

    if (se->stabilizer.active) {
        long long elapsed_msec = time_msec - se->stabilizer.last_time_msec;
        long long elapsed = elapsed_msec / STABILIZER_SAMPLE_TIME;
        double alpha = DP_size_to_double(se->stabilizer.points.used)
                     / DP_llong_to_double(elapsed);

        for (long long i = 0; i < elapsed; ++i) {
            DP_BrushPoint bp = stabilizer_points_get(se, i, alpha);
            stroke_engine_push(se, stabilizer_stabilize(se, bp), cs_or_null);
            stabilizer_queue_replace(se, bp);
        }

        stabilizer_points_clear(se, time_msec);
    }
}

void DP_stroke_engine_stroke_end(DP_StrokeEngine *se, long long time_msec,
                                 DP_CanvasState *cs_or_null)
{
    DP_ASSERT(se);
    stroke_engine_poll_control(se, false);
    spline_drain(se, cs_or_null);
    smoother_drain(se, cs_or_null);
    if (se->stabilizer.active) {
        stabilizer_finish(se, time_msec, cs_or_null);
    }
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

static DP_UPixelFloat blend_classic_color(DP_BrushEngine *be,
                                          DP_ClassicBrush *cb, float pressure,
                                          float velocity, float distance)
{
    if (be->classic.indirect_alpha == 0.0f) {
        float a =
            cb->smudge_alpha && !be->stroke.compatibility_mode
                ? DP_classic_brush_smudge_at(cb, pressure, velocity, distance)
                : 1.0f;
        if (a <= 0.0f) {
            return be->classic.brush_color;
        }
        else if (a >= 1.0f) {
            return be->classic.smudge_color;
        }
        else {
            float a1 = 1.0f - a;
            return (DP_UPixelFloat){
                be->classic.smudge_color.b * a + be->classic.brush_color.b * a1,
                be->classic.smudge_color.g * a + be->classic.brush_color.g * a1,
                be->classic.smudge_color.r * a + be->classic.brush_color.r * a1,
                be->classic.smudge_color.a * a + a1,
            };
        }
    }
    else {
        return (DP_UPixelFloat){
            be->classic.smudge_color.b,
            be->classic.smudge_color.g,
            be->classic.smudge_color.r,
            be->classic.indirect_alpha,
        };
    }
}

#define MASK_ALL_BLANK  0
#define MASK_ALL_OPAQUE 1
#define MASK_MIXED      2

static int check_mask_state(DP_LayerContent *mask_lc, DP_PaintMode paint_mode,
                            int width, int height, DP_Rect dab_bounds)
{
    DP_TileIterator ti = DP_tile_iterator_make(width, height, dab_bounds);
    bool have_non_blank = false;
    bool have_non_opaque = false;
    while ((!have_non_blank || !have_non_opaque)
           && DP_tile_iterator_next(&ti)) {
        DP_Tile *t = DP_layer_content_tile_at_noinc(mask_lc, ti.col, ti.row);
        if (DP_tile_opaque_ident(t)) {
            have_non_blank = true;
        }
        else if (t) {
            return MASK_MIXED;
        }
        else {
            have_non_opaque = true;
        }
    }

    if (have_non_blank) {
        if (have_non_opaque || paint_mode != DP_PAINT_MODE_DIRECT) {
            return MASK_MIXED;
        }
        else {
            return MASK_ALL_OPAQUE;
        }
    }
    else {
        return MASK_ALL_BLANK;
    }
}

static void *allocate_buffer(DP_BrushEngine *be)
{
    if (!be->buffer) {
        be->buffer = DP_malloc_simd(DP_max_size(
            DP_square_size(sizeof(uint16_t) * (size_t)260),
            DP_TILE_LENGTH + DP_compress_deflate_bound(DP_TILE_LENGTH)));
    }
    return be->buffer;
}

static unsigned char *get_mask_buffer(DP_BrushEngine *be)
{
    be->last_diameter = -1;
    return allocate_buffer(be);
}

static unsigned char *get_mask_compress_buffer(DP_UNUSED size_t size,
                                               void *user)
{
    return user;
}

static void set_mask(size_t size, unsigned char *out, void *user)
{
    if (size != 0) {
        memcpy(out, user, size);
    }
}

static void push_selection_sync(DP_BrushEngine *be, uint8_t remote_selection_id,
                                int col, int row, size_t mask_size,
                                unsigned char *mask)
{
    unsigned int context_id = be->stroke.context_id;
    be->push_message(be->user,
                     DP_msg_sync_selection_tile_new(
                         context_id, DP_uint_to_uint8(context_id),
                         remote_selection_id, DP_int_to_uint16(col),
                         DP_int_to_uint16(row), set_mask, mask_size, mask));
}

static void push_selection_sync_clear(DP_BrushEngine *be,
                                      uint8_t remote_selection_id)
{
    push_selection_sync(be, remote_selection_id, 0xffff, 0xffff, 0, NULL);
}

static void push_mask_selection_sync(DP_BrushEngine *be, int col, int row,
                                     size_t mask_size, unsigned char *mask)
{
    int selection_id = be->mask.current_selection_id;
    if (selection_id > 0) {
        uint8_t remote_selection_id =
            DP_int_to_uint8(selection_id - 1 + DP_SELECTION_ID_FIRST_REMOTE);
        push_selection_sync(be, remote_selection_id, col, row, mask_size, mask);
    }
}

static void push_mask_selection_sync_clear(DP_BrushEngine *be)
{
    push_mask_selection_sync(be, 0xffff, 0xffff, 0, NULL);
}

static bool sync_mask_state(DP_BrushEngine *be, DP_LayerContent *mask_lc,
                            int width, int height, DP_Rect dab_bounds)
{
    DP_TileIterator ti = DP_tile_iterator_make(width, height, dab_bounds);
    int xtiles = DP_tile_count_round(width);
    while (DP_tile_iterator_next(&ti)) {
        int index = ti.row * xtiles + ti.col;
        if (!be->mask.map[index]) {
            size_t mask_size;
            unsigned char *mask;
            DP_Tile *t =
                DP_layer_content_tile_at_noinc(mask_lc, ti.col, ti.row);
            if (t) {
                unsigned char *buffer = get_mask_buffer(be);
                mask = buffer + DP_TILE_LENGTH;
                mask_size = DP_tile_compress_mask_delta_zstd8le(
                    t, &be->mask.zstd_cctx, (uint8_t *)buffer,
                    get_mask_compress_buffer, mask);
                if (mask_size == 0) {
                    DP_warn("Error compressing selection tile: %s", DP_error());
                    return false;
                }
            }
            else {
                mask_size = 0;
                mask = NULL;
            }

            push_mask_selection_sync(be, ti.col, ti.row, mask_size, mask);
            be->mask.map[index] = true;
        }
    }

    return true;
}

static bool handle_selection_mask(DP_BrushEngine *be, DP_PaintMode paint_mode,
                                  DP_Rect dab_bounds, uint8_t *out_mask_flags)
{
    if (be->mask.active) {
        if (be->mask.preview) {
            *out_mask_flags = DP_int_to_uint8(be->mask.next_selection_id << 3);
            return true;
        }
        else {
            DP_LayerContent *mask_lc = be->mask.lc;
            int width = DP_layer_content_width(mask_lc);
            int height = DP_layer_content_height(mask_lc);
            int mask_state = check_mask_state(mask_lc, paint_mode, width,
                                              height, dab_bounds);
            if (mask_state == MASK_ALL_OPAQUE) {
                // Don't bother with a fully opaque mask in direct paint mode.
                *out_mask_flags = 0;
                return true;
            }
            else if (mask_state == MASK_MIXED
                     && sync_mask_state(be, mask_lc, width, height,
                                        dab_bounds)) {
                *out_mask_flags =
                    DP_int_to_uint8(be->mask.current_selection_id << 3);
                return true;
            }
            else {
                // Drawing outside of mask or sync error, bail out.
                return false;
            }
        }
    }
    else {
        *out_mask_flags = 0;
        return true;
    }
}


static bool in_dab_mask(void *user, int x, int y)
{
    DP_BrushEngine *be = user;
    int diameter = be->flood.mask.size;
    int radius = diameter / 2;
    int mask_x = x - be->flood.mask.x + radius;
    int mask_y = y - be->flood.mask.y + radius;
    if (mask_x >= 0 && mask_x < diameter && mask_y >= 0 && mask_y < diameter) {
        int i = mask_y * diameter + mask_x;
        return be->flood.mask.buffer[i];
    }
    else {
        return false;
    }
}

static unsigned char *allocate_flood_dab_mask(DP_BrushEngine *be, int diameter)
{
    if (be->flood.mask.size_available < diameter) {
        be->flood.mask.size_available = diameter;
        free(be->flood.mask.buffer);
        be->flood.mask.buffer =
            malloc(DP_square_size(DP_int_to_size(diameter)));
    }
    return be->flood.mask.buffer;
}

static void dilate_flood_mask_buffer(unsigned char *buffer, int diameter)
{
    int full_diameter = diameter + 2;
    int last_index = full_diameter - 1;

#define DILATE_AT(X, Y)      (buffer[(Y) * full_diameter + (X)])
#define DILATE_TEST(X, Y)    (DILATE_AT((X), (Y)) & 1)
#define DILATE_TEST_N(X, Y)  DILATE_TEST((X), (Y) - 1)
#define DILATE_TEST_E(X, Y)  DILATE_TEST((X) + 1, (Y))
#define DILATE_TEST_S(X, Y)  DILATE_TEST((X), (Y) + 1)
#define DILATE_TEST_W(X, Y)  DILATE_TEST((X) - 1, (Y))
#define DILATE_TEST_NE(X, Y) DILATE_TEST((X) + 1, (Y) - 1)
#define DILATE_TEST_NW(X, Y) DILATE_TEST((X) - 1, (Y) - 1)
#define DILATE_TEST_SE(X, Y) DILATE_TEST((X) + 1, (Y) + 1)
#define DILATE_TEST_SW(X, Y) DILATE_TEST((X) - 1, (Y) + 1)
#define DILATE_OR(X, Y, COND)     \
    do {                          \
        if ((COND)) {             \
            DILATE_AT(X, Y) |= 2; \
        }                         \
    } while (0)
#define DILATE_SET(X, Y, COND)            \
    do {                                  \
        DILATE_AT(X, Y) = (COND) ? 2 : 0; \
    } while (0)
#define DILATE_SET2(X, Y, P1, P2) \
    DILATE_SET((X), (Y),          \
               DILATE_TEST_##P1((X), (Y)) || DILATE_TEST_##P2((X), (Y)))
#define DILATE_SET3(X, Y, P1, P2, P3)                                   \
    DILATE_SET((X), (Y),                                                \
               DILATE_TEST_##P1((X), (Y)) || DILATE_TEST_##P2((X), (Y)) \
                   || DILATE_TEST_##P3((X), (Y)))
#define DILATE_APPLY(X, Y)                                                   \
    DILATE_OR((X), (Y),                                                      \
              DILATE_AT((X), (Y)) == 0                                       \
                  && (DILATE_TEST_NW((X), (Y)) || DILATE_TEST_N((X), (Y))    \
                      || DILATE_TEST_NE((X), (Y)) || DILATE_TEST_W((X), (Y)) \
                      || DILATE_TEST_E((X), (Y)) || DILATE_TEST_SW((X), (Y)) \
                      || DILATE_TEST_S((X), (Y)) || DILATE_TEST_SE((X), (Y))))

    // Corners are always blank.
    DILATE_AT(0, 0) = 0;
    DILATE_AT(last_index, 0) = 0;
    DILATE_AT(0, last_index) = 0;
    DILATE_AT(last_index, last_index) = 0;

    // Corner-adjacent pixels can only check inwards.
    DILATE_SET2(0, 1, E, SE);
    DILATE_SET2(1, 0, S, SE);
    DILATE_SET2(last_index, 1, W, SW);
    DILATE_SET2(last_index - 1, 0, S, SW);
    DILATE_SET2(0, last_index - 1, E, NE);
    DILATE_SET2(1, last_index, N, NE);
    DILATE_SET2(last_index, last_index - 1, W, NW);
    DILATE_SET2(last_index - 1, last_index, N, NW);

    // Top and bottom rows.
    for (int x = 2; x < last_index; ++x) {
        DILATE_SET3(x, 0, SW, S, SE);
        DILATE_SET3(x, last_index, NW, N, NE);
    }

    // Left and right sides.
    for (int y = 2; y < last_index; ++y) {
        DILATE_SET3(0, y, NE, E, SE);
        DILATE_SET3(last_index, y, NW, W, SW);
    }

    // Dilate the rest of the owl.
    for (int y = 1; y < last_index; ++y) {
        for (int x = 1; x < last_index; ++x) {
            DILATE_APPLY(x, y);
        }
    }
}

static int make_pixel_round_flood_mask_buffer(DP_BrushEngine *be, int diameter)
{
    unsigned char *buffer = allocate_flood_dab_mask(be, diameter);
    float r = DP_int_to_float(diameter) / 2.0f;
    float rr = DP_square_float(r);
    int i = 0;
    for (int y = 0; y < diameter; ++y) {
        float yy = DP_square_float(DP_int_to_float(y) - r + 0.5f);
        for (int x = 0; x < diameter; ++x) {
            float xx = DP_square_float(DP_int_to_float(x) - r + 0.5f);
            buffer[i++] = xx + yy <= rr ? 1 : 0;
        }
    }
    return diameter;
}

static int make_soft_round_flood_mask_buffer(DP_BrushEngine *be, int diameter)
{
    int full_diameter = diameter + 2;
    unsigned char *buffer = allocate_flood_dab_mask(be, full_diameter);
    float r = DP_int_to_float(diameter) / 2.0f;
    float rr = DP_square_float(r);

    int i = full_diameter;
    for (int y = 0; y < diameter; ++y) {
        float yy = DP_square_float(DP_int_to_float(y) - r + 0.5f);
        ++i;
        for (int x = 0; x < diameter; ++x) {
            float xx = DP_square_float(DP_int_to_float(x) - r + 0.5f);
            buffer[i++] = xx + yy <= rr ? 1 : 0;
        }
        ++i;
    }

    dilate_flood_mask_buffer(buffer, diameter);
    return full_diameter;
}

static int make_mypaint_flood_mask_buffer(DP_BrushEngine *be, int diameter,
                                          float aspect_ratio, float angle_rad)
{
    int full_diameter = diameter + 2;
    unsigned char *buffer = allocate_flood_dab_mask(be, full_diameter);
    float cs = cosf(angle_rad);
    float sn = sinf(angle_rad);
    float radius = DP_int_to_float(diameter) / 2.0f;
    float one_over_radius2 = 1.0f / (radius * radius);

    int i = full_diameter;
    for (int y = 0; y < diameter; ++y) {
        ++i;
        for (int x = 0; x < diameter; ++x) {
            float yy = DP_int_to_float(y) + 0.5f - radius;
            float xx = DP_int_to_float(x) + 0.5f - radius;
            float yyr = (yy * cs - xx * sn) * aspect_ratio;
            float xxr = yy * sn + xx * cs;
            float rr = (yyr * yyr + xxr * xxr) * one_over_radius2;
            buffer[i++] = rr <= 1.0f ? 1 : 0;
        }
        ++i;
    }

    dilate_flood_mask_buffer(buffer, diameter);
    return full_diameter;
}

static DP_FloodInDabFn
make_flood_dab_mask_pixel_square(DP_BrushEngine *be,
                                 const DP_PixelDabParams *dab)
{
    int diameter = dab->size;
    bool needs_new_mask =
        be->flood.mask.shape != DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE
        || be->flood.mask.size < diameter;
    if (needs_new_mask) {
        unsigned char *buffer = allocate_flood_dab_mask(be, diameter);
        be->flood.mask.shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE;
        be->flood.mask.size = diameter;
        int count = DP_square_int(diameter);
        for (int i = 0; i < count; ++i) {
            buffer[i] = 1;
        }
    }
    be->flood.mask.x = dab->x;
    be->flood.mask.y = dab->y;
    return in_dab_mask;
}

static DP_FloodInDabFn
make_flood_dab_mask_pixel_round(DP_BrushEngine *be,
                                const DP_PixelDabParams *dab)
{
    int diameter = dab->size;
    bool needs_new_mask =
        be->flood.mask.shape != DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND
        || be->flood.mask.size != diameter;
    if (needs_new_mask) {
        be->flood.mask.shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND;
        be->flood.mask.size = make_soft_round_flood_mask_buffer(be, diameter);
    }
    be->flood.mask.x = dab->x;
    be->flood.mask.y = dab->y;
    return in_dab_mask;
}

static DP_FloodInDabFn make_flood_dab_mask_soft(DP_BrushEngine *be,
                                                const DP_ClassicDabParams *dab)
{
    be->flood.mask.shape = DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;

    // Fudge the diameter to actually cover the entire dab.
    int diameter = DP_uint_to_int(dab->size / 256u);
    if (diameter < 20) {
        diameter += 4;
    }
    else if (diameter < 50) {
        diameter += 5;
    }
    else {
        diameter += 4 + diameter / 50;
    }

    bool needs_new_mask =
        be->flood.mask.shape != DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND
        || be->flood.mask.size != diameter;
    if (needs_new_mask) {
        be->flood.mask.shape = DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;
        be->flood.mask.size = make_pixel_round_flood_mask_buffer(be, diameter);
    }
    be->flood.mask.x = (dab->x + 1) / 4;
    be->flood.mask.y = (dab->y + 1) / 4;
    return in_dab_mask;
}

static DP_FloodInDabFn
make_flood_dab_mask_mypaint(DP_BrushEngine *be, const DP_MyPaintDabParams *dab)
{
    be->flood.mask.shape = DP_BRUSH_SHAPE_MYPAINT;
    int diameter = DP_uint_to_int(dab->size / 256u) + 4;
    uint8_t angle = dab->angle;
    uint8_t aspect_ratio = dab->aspect_ratio;
    uint8_t aspect_ratio_fudge = 64;
    if (aspect_ratio <= aspect_ratio_fudge) {
        aspect_ratio = 0;
    }
    else {
        aspect_ratio -= aspect_ratio_fudge;
    }

    bool needs_new_mask = be->flood.mask.shape != DP_BRUSH_SHAPE_MYPAINT
                       || be->flood.mask.size != diameter
                       || be->flood.mask.aspect_ratio != aspect_ratio
                       || be->flood.mask.angle != angle;
    if (needs_new_mask) {
        be->flood.mask.shape = DP_BRUSH_SHAPE_MYPAINT;
        be->flood.mask.aspect_ratio = aspect_ratio;
        be->flood.mask.angle = angle;
        be->flood.mask.size = make_mypaint_flood_mask_buffer(
            be, diameter,
            DP_mypaint_brush_aspect_ratio_from_uint8(aspect_ratio),
            DP_uint8_to_float(angle) / 255.0f * (((float)M_PI) * 2.0f));
    }
    be->flood.mask.x = (dab->x + 2) / 4;
    be->flood.mask.y = (dab->y + 2) / 4;
    return in_dab_mask;
}

static DP_FloodInDabFn make_flood_dab_mask(DP_BrushEngine *be, void *dab)
{
    switch (be->active) {
    case DP_BRUSH_ENGINE_ACTIVE_PIXEL:
        if (be->classic.brush.shape == DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE) {
            return make_flood_dab_mask_pixel_square(be, dab);
        }
        else {
            return make_flood_dab_mask_pixel_round(be, dab);
        }
    case DP_BRUSH_ENGINE_ACTIVE_SOFT:
        return make_flood_dab_mask_soft(be, dab);
    case DP_BRUSH_ENGINE_ACTIVE_MYPAINT:
        return make_flood_dab_mask_mypaint(be, dab);
    }
    DP_UNREACHABLE();
}

static void flood_mask_flush(void *user)
{
    DP_BrushEngine *be = user;
    DP_brush_engine_dabs_flush(be);
}

static void flood_mask_clear(void *user)
{
    DP_BrushEngine *be = user;
    push_selection_sync_clear(be, DP_SELECTION_ID_FIRST_FLOOD_REMOTE);
}

static bool flood_mask_put(void *user, int col, int row, DP_Tile *t)
{
    DP_BrushEngine *be = user;

    size_t mask_size;
    unsigned char *mask;
    if (t) {
        unsigned char *buffer = get_mask_buffer(be);
        mask = buffer + DP_TILE_LENGTH;
        if (DP_tile_opaque(t)) {
            mask_size = DP_tile_compress_mask_delta_zstd8le_opaque(
                get_mask_compress_buffer, mask);
            DP_ASSERT(mask_size == 1);
        }
        else {
            mask_size = DP_tile_compress_mask_delta_zstd8le_normal(
                t, &be->mask.zstd_cctx, (uint8_t *)buffer,
                get_mask_compress_buffer, mask);
            if (mask_size == 0) {
                DP_warn("Error compressing anti overflow tile: %s", DP_error());
                return false;
            }
        }
    }
    else {
        mask_size = 0;
        mask = NULL;
    }

    push_selection_sync(be, DP_SELECTION_ID_FIRST_FLOOD_REMOTE, col, row,
                        mask_size, mask);
    return true;
}

static bool handle_flood_mask(DP_BrushEngine *be, int origin_x, int origin_y,
                              DP_Rect bounds, void *dab,
                              uint8_t *out_flood_flags)
{
    if (be->stroke.compatibility_mode || !be->flood.lc) {
        *out_flood_flags = 0;
        return true;
    }

    DP_BrushEngineFloodTarget target =
        DP_layer_id_selection(be->layer_id)
            ? DP_BRUSH_ENGINE_FLOOD_TARGET_SELECTION
            : DP_BRUSH_ENGINE_FLOOD_TARGET_LAYER;
    if (be->flood.target != target) {
        if (be->flood.state) {
            flood_mask_clear(be);
            be->flood.state = NULL;
        }
        be->flood.target = target;
    }

    DP_FloodInDabFn in_dab = make_flood_dab_mask(be, dab);
    bool should_mask;
    if (!DP_flood_fill_dab(
            &be->flood.state, origin_x, origin_y, be->flood.tolerance,
            be->flood.expand, &bounds, be->flood.lc, in_dab, flood_mask_flush,
            flood_mask_clear, flood_mask_put, be, &should_mask)) {
        return false;
    }

    if (should_mask) {
        *out_flood_flags = DP_int_to_uint8(1 << 6);
    }
    else {
        *out_flood_flags = 0;
    }
    return true;
}


static bool is_tentative_pixel(int x1, int y1, int x2, int y2)
{
    return abs(x2 - x1) <= 1 && abs(y2 - y1) <= 1;
}

static void pixel_perfect_apply(DP_BrushEngine *be, int x, int y,
                                void (*append_dab)(DP_BrushEngine *, void *),
                                void (*append_tentative)(DP_BrushEngine *),
                                void (*assign_tentative)(DP_BrushEngine *,
                                                         void *),
                                void *user)
{
    // For "pixel-perfect" drawing, we want to avoid doubling up dabs that are
    // one pixel away from the previous dab. For that, we hold onto at most one
    // tentative dab and clobber it if the subsequent pixel would again only be
    // one pixel from the previous one.
    if (be->pixel.perfect && !be->flood.lc) {
        if (be->pixel.have_last) {
            // Check if we need to flush a currently stashed tentative dab.
            bool last_tentative =
                is_tentative_pixel(x, y, be->pixel.last_x, be->pixel.last_y);
            if (be->pixel.have_tentative && !last_tentative) {
                be->pixel.have_tentative = false;
                be->pixel.last_x = be->pixel.tentative_x;
                be->pixel.last_y = be->pixel.tentative_y;
                last_tentative = is_tentative_pixel(x, y, be->pixel.last_x,
                                                    be->pixel.last_y);
                append_tentative(be);
            }
            // Consider stashing the dab if it's too close to the previous one
            // to be sure if it's perfect and bail out int that case. If there
            // was a prior tentative dab, it will be clobbered. If it is far
            // enough away from the previously placed dab then just flush it.
            if (last_tentative) {
                be->pixel.have_tentative = true;
                be->pixel.tentative_x = x;
                be->pixel.tentative_y = y;
                assign_tentative(be, user);
                return;
            }
            else {
                be->pixel.last_x = x;
                be->pixel.last_y = y;
            }
        }
        else {
            // First dab is always perfect.
            be->pixel.have_last = true;
            be->pixel.last_x = x;
            be->pixel.last_y = y;
        }
    }
    // Not a tentative dab, put it down.
    append_dab(be, user);
}


static DP_Rect pixel_dab_bounds(int x, int y, int dab_size)
{
    int radius = (dab_size + 1) / 2;
    return (DP_Rect){x - radius, y - radius, x + radius, y + radius};
}

static void append_pixel_dab(DP_BrushEngine *be, const DP_PixelDabParams *dab)
{
    int used = be->dabs.used;
    int8_t dx, dy;
    bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_PIXEL_DABS_MAX
                   && be->classic.dab_flags == dab->flags
                   && be->classic.dab_color == dab->color
                   && delta_xy(be, dab->x, dab->y, &dx, &dy);
    be->dabs.last_x = dab->x;
    be->dabs.last_y = dab->y;

    if (!can_append) {
        DP_brush_engine_dabs_flush(be);
        be->classic.dab_flags = dab->flags;
        be->classic.dab_x = dab->x;
        be->classic.dab_y = dab->y;
        be->classic.dab_color = dab->color;
        dx = 0;
        dy = 0;
    }

    DP_BrushEnginePixelDab *dabs = get_dab_buffer(be, sizeof(*dabs));
    dabs[be->dabs.used++] =
        (DP_BrushEnginePixelDab){dx, dy, dab->size, dab->opacity};
}

static void pixel_perfect_append_pixel(DP_BrushEngine *be, void *user)
{
    append_pixel_dab(be, user);
}

static void pixel_perfect_append_tentative_pixel(DP_BrushEngine *be)
{
    append_pixel_dab(be, &be->pixel.tentative_dab.pixel);
}

static void pixel_perfect_assign_tentative_pixel(DP_BrushEngine *be, void *user)
{
    DP_PixelDabParams *dab = user;
    be->pixel.tentative_dab.pixel = *dab;
}

static void add_dab_pixel(DP_BrushEngine *be, DP_ClassicBrush *cb, int origin_x,
                          int origin_y, float pressure, float velocity,
                          float distance)
{
    DP_PixelDabParams dab;
    dab.size = DP_classic_brush_pixel_dab_size_at(
        cb, pressure, velocity, distance, be->stroke.compatibility_mode);
    dab.opacity =
        DP_classic_brush_dab_opacity_at(cb, pressure, velocity, distance);

    int x = origin_x;
    int y = origin_y;
    float jitter = DP_classic_brush_jitter_at(cb, pressure, velocity, distance);
    if (jitter > 0.0f) {
        float length = brush_engine_randf(be) * DP_uint16_to_float(dab.size)
                     * 2.0f * jitter;
        float angle = brush_engine_randf(be) * (float)M_PI * 2.0f;
        x += DP_float_to_int(roundf(length * cosf(angle)));
        y += DP_float_to_int(roundf(length * sinf(angle)));
    }
    dab.x = DP_int_to_int32(x);
    dab.y = DP_int_to_int32(y);

    uint8_t mask_flags;
    uint8_t flood_flags;
    DP_PaintMode paint_mode = get_classic_brush_paint_mode(be, cb);
    DP_Rect bounds = pixel_dab_bounds(x, y, dab.size);
    if (dab.size > 0 && dab.opacity > 0
        && handle_selection_mask(be, paint_mode, bounds, &mask_flags)
        && handle_flood_mask(be, origin_x, origin_y, bounds, &dab,
                             &flood_flags)) {
        dab.flags = (uint8_t)paint_mode | mask_flags | flood_flags;
        dab.color = combine_upixel_float(
            blend_classic_color(be, cb, pressure, velocity, distance));

        pixel_perfect_apply(be, x, y, pixel_perfect_append_pixel,
                            pixel_perfect_append_tentative_pixel,
                            pixel_perfect_assign_tentative_pixel, &dab);
    }
}


static DP_Rect subpixel_dab_bounds(float x, float y, float diameter)
{
    int radius = DP_float_to_int((diameter + 1.0f) / 2.0f + 0.5f) + 1;
    int xi = DP_float_to_int(x + 0.5f);
    int yi = DP_float_to_int(y + 0.5f);
    return (DP_Rect){xi - radius, yi - radius, xi + radius, yi + radius};
}

static void add_dab_soft(DP_BrushEngine *be, DP_ClassicBrush *cb,
                         float origin_x, float origin_y, float pressure,
                         float velocity, float distance, bool left, bool up)
{
    DP_ClassicDabParams dab;
    dab.size = DP_classic_brush_soft_dab_size_at(
        cb, pressure, velocity, distance, be->stroke.compatibility_mode);
    dab.opacity =
        DP_classic_brush_dab_opacity_at(cb, pressure, velocity, distance);
    float diameter = DP_uint32_to_float(dab.size) / 256.0f;

    float x = origin_x;
    float y = origin_y;
    float jitter = DP_classic_brush_jitter_at(cb, pressure, velocity, distance);
    if (jitter > 0.0f) {
        float length = brush_engine_randf(be) * diameter * jitter;
        float angle = brush_engine_randf(be) * (float)M_PI * 2.0f;
        x += length * cosf(angle);
        y += length * sinf(angle);
    }

    // The brush mask rendering has some unsightly discontinuities between
    // fractions of a radius and the next full step, causing an unsightly
    // jagged look between them. We can't fix the brush rendering directly
    // because that would break compatibility, so instead we fudge the
    // positioning to compensate.
    float rrem = fmodf(diameter, 1.0f);
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

    dab.x = DP_float_to_int32((x + fudge_x) * 4.0f) + (int32_t)3;
    dab.y = DP_float_to_int32((y + fudge_y) * 4.0f) + (int32_t)2;

    uint8_t mask_flags;
    uint8_t flood_flags;
    DP_PaintMode paint_mode = get_classic_brush_paint_mode(be, cb);
    DP_Rect bounds = subpixel_dab_bounds(x, y, diameter);
    // Disregard infinitesimal or fully transparent dabs. 26 is a radius of 0.1.
    if (dab.size >= 26 && dab.opacity > 0
        && handle_selection_mask(be, paint_mode, bounds, &mask_flags)
        && handle_flood_mask(be, DP_float_to_int(origin_x + 0.5f),
                             DP_float_to_int(origin_y + 0.5f), bounds, &dab,
                             &flood_flags)) {
        dab.flags = (uint8_t)paint_mode | mask_flags | flood_flags;
        dab.color = combine_upixel_float(
            blend_classic_color(be, cb, pressure, velocity, distance));

        int used = be->dabs.used;
        int8_t dx, dy;
        bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_CLASSIC_DABS_MAX
                       && be->classic.dab_flags == dab.flags
                       && be->classic.dab_color == dab.color
                       && delta_xy(be, dab.x, dab.y, &dx, &dy);
        be->dabs.last_x = dab.x;
        be->dabs.last_y = dab.y;

        if (!can_append) {
            DP_brush_engine_dabs_flush(be);
            be->classic.dab_flags = dab.flags;
            be->classic.dab_x = dab.x;
            be->classic.dab_y = dab.y;
            be->classic.dab_color = dab.color;
            dx = 0;
            dy = 0;
        }

        DP_BrushEngineClassicDab *dabs = get_dab_buffer(be, sizeof(*dabs));
        dabs[be->dabs.used++] = (DP_BrushEngineClassicDab){
            dx, dy, dab.size,
            DP_classic_brush_dab_hardness_at(cb, pressure, velocity, distance),
            dab.opacity};
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
        || blend_mode == DP_BLEND_MODE_PIGMENT_ALPHA
        || blend_mode == DP_BLEND_MODE_OKLAB_NORMAL;
}

static bool should_ignore_full_alpha_mypaint_dab(DP_PaintMode paint_mode,
                                                 DP_BlendMode blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_NORMAL:
    case DP_BLEND_MODE_PIGMENT_ALPHA:
    case DP_BLEND_MODE_OKLAB_NORMAL:
        return paint_mode != DP_PAINT_MODE_DIRECT;
    case DP_BLEND_MODE_ERASE:
    case DP_BLEND_MODE_ERASE_PRESERVE:
        return false;
    default:
        return true;
    }
}

static uint8_t get_mypaint_dab_blend_mode_flag(DP_BlendMode blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_PIGMENT_ALPHA:
        return DP_MYPAINT_BRUSH_PIGMENT_FLAG;
    case DP_BLEND_MODE_OKLAB_NORMAL:
        return DP_MYPAINT_BRUSH_OKLAB_FLAG;
    default:
        return 0;
    }
}

static uint8_t get_mypaint_dab_mode(DP_BlendMode blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_NORMAL:
        return DP_MYPAINT_BRUSH_MODE_NORMAL;
    case DP_BLEND_MODE_RECOLOR:
        return DP_MYPAINT_BRUSH_MODE_RECOLOR;
    case DP_BLEND_MODE_ERASE:
        return DP_MYPAINT_BRUSH_MODE_ERASE;
    default:
        DP_warn("Unexpected MyPaint dab blend mode %d", (int)blend_mode);
        return DP_BLEND_MODE_NORMAL;
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

static uint32_t get_mypaint_dab_size(float radius, bool compatibility_mode)
{
    float value = radius * 512.0f + 0.5f;
    return DP_float_to_uint32(
        CLAMP(value, 0, DP_brush_size_maxf(compatibility_mode) * 256.0f));
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

static void append_mypaint_dab(DP_BrushEngine *be,
                               const DP_MyPaintDabParams *dab)
{
    int used = be->dabs.used;
    int8_t dx, dy;
    bool can_append = used != 0 && used < DP_MSG_DRAW_DABS_MYPAINT_DABS_MAX
                   && be->mypaint.dab_color == dab->color
                   && be->mypaint.dab_flags == dab->flags
                   && be->mypaint.dab_lock_alpha == dab->lock_alpha
                   && be->mypaint.dab_colorize == dab->colorize
                   && be->mypaint.dab_posterize == dab->posterize
                   && be->mypaint.dab_mode == dab->mode
                   && delta_xy(be, dab->x, dab->y, &dx, &dy);
    be->dabs.last_x = dab->x;
    be->dabs.last_y = dab->y;

    if (!can_append) {
        DP_brush_engine_dabs_flush(be);
        be->mypaint.dab_x = dab->x;
        be->mypaint.dab_y = dab->y;
        be->mypaint.dab_color = dab->color;
        be->mypaint.dab_flags = dab->flags;
        be->mypaint.dab_lock_alpha = dab->lock_alpha;
        be->mypaint.dab_colorize = dab->colorize;
        be->mypaint.dab_posterize = dab->posterize;
        be->mypaint.dab_mode = dab->mode;
        dx = 0;
        dy = 0;
    }

    DP_BrushEngineMyPaintDab *dabs = get_dab_buffer(be, sizeof(*dabs));
    dabs[be->dabs.used++] = (DP_BrushEngineMyPaintDab){
        dx,           dy,         dab->size,        dab->hardness,
        dab->opacity, dab->angle, dab->aspect_ratio};
}

static void pixel_perfect_append_mypaint(DP_BrushEngine *be, void *user)
{
    append_mypaint_dab(be, user);
}

static void pixel_perfect_append_tentative_mypaint(DP_BrushEngine *be)
{
    append_mypaint_dab(be, &be->pixel.tentative_dab.mypaint);
}

static void pixel_perfect_assign_tentative_mypaint(DP_BrushEngine *be,
                                                   void *user)
{
    DP_MyPaintDabParams *dab = user;
    be->pixel.tentative_dab.mypaint = *dab;
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
    DP_PaintMode paint_mode = get_mypaint_paint_mode(be);
    DP_BlendMode blend_mode = get_mypaint_blend_mode(be);
    // Disregard colors with zero alpha if the paint and/or blend modes can't
    // deal with it. MyPaint brushes do this when they want to smudge at full
    // alpha, which will use an undefined (in practice: fully red) color.
    if (alpha_eraser <= 0.0f
        && should_ignore_full_alpha_mypaint_dab(paint_mode, blend_mode)) {
        return 0;
    }

    DP_MyPaintDabParams dab;
    dab.size = get_mypaint_dab_size(radius, be->stroke.compatibility_mode);

    DP_Rect bounds =
        subpixel_dab_bounds(x, y, DP_uint32_to_float(dab.size) / 256.0f);
    uint8_t mask_flags;
    if (!handle_selection_mask(be, paint_mode, bounds, &mask_flags)) {
        return 0;
    }

    dab.x = DP_float_to_int32(x * 4.0f);
    dab.y = DP_float_to_int32(y * 4.0f);
    dab.size = get_mypaint_dab_size(radius, be->stroke.compatibility_mode);
    dab.angle = get_mypaint_dab_angle(angle);
    dab.aspect_ratio = get_mypaint_dab_aspect_ratio(aspect_ratio);

    float origin_x = mypaint_brush_get_state(be->mypaint_brush,
                                             MYPAINT_BRUSH_STATE_ACTUAL_X);
    float origin_y = mypaint_brush_get_state(be->mypaint_brush,
                                             MYPAINT_BRUSH_STATE_ACTUAL_Y);
    uint8_t flood_flags;
    if (!handle_flood_mask(be, DP_float_to_int(origin_x + 0.5f),
                           DP_float_to_int(origin_y + 0.5f), bounds, &dab,
                           &flood_flags)) {
        return 0;
    }

    if (be->stroke.compatibility_mode) {
        if (paint_mode == DP_PAINT_MODE_DIRECT) {
            dab.flags = 0;
            switch (blend_mode) {
            case DP_BLEND_MODE_ERASE:
            case DP_BLEND_MODE_ERASE_PRESERVE:
                dab.color = 0;
                dab.lock_alpha = 0;
                dab.colorize = 0;
                dab.posterize = 0;
                dab.mode = 0;
                break;
            case DP_BLEND_MODE_RECOLOR:
                dab.color = get_mypaint_dab_color(color_r, color_g, color_b,
                                                  alpha_eraser);
                dab.lock_alpha = 255;
                dab.colorize = 0;
                dab.posterize = 0;
                dab.mode = 0;
                break;
            case DP_BLEND_MODE_COLOR:
            case DP_BLEND_MODE_COLOR_ALPHA:
                dab.color = get_mypaint_dab_color(color_r, color_g, color_b,
                                                  alpha_eraser);
                dab.lock_alpha = 0;
                dab.colorize = 255;
                dab.posterize = 0;
                dab.mode = 0;
                break;
            default:
                dab.color = get_mypaint_dab_color(color_r, color_g, color_b,
                                                  alpha_eraser);
                dab.lock_alpha = get_mypaint_dab_lock_alpha(lock_alpha);
                dab.colorize = get_mypaint_dab_colorize(colorize);
                dab.posterize = get_mypaint_dab_posterize(posterize);
                dab.mode =
                    get_mypaint_dab_posterize_num(dab.posterize, posterize_num);
                break;
            }
        }
        else {
            dab.color = get_mypaint_dab_color(color_r, color_g, color_b, 1.0f);
            dab.flags = 0;
            dab.lock_alpha = 0;
            dab.colorize = 0;
            dab.posterize = 0;
            dab.mode =
                DP_MYPAINT_BRUSH_MODE_FLAG | get_mypaint_dab_mode(blend_mode);
        }
    }
    else if (paint_mode == DP_PAINT_MODE_DIRECT
             && is_normal_mypaint_mode(blend_mode)) {
        dab.color =
            get_mypaint_dab_color(color_r, color_g, color_b, alpha_eraser);
        dab.flags = get_mypaint_dab_blend_mode_flag(blend_mode) | mask_flags
                  | flood_flags;
        dab.lock_alpha = get_mypaint_dab_lock_alpha(lock_alpha);
        dab.colorize = get_mypaint_dab_colorize(colorize);
        dab.posterize = get_mypaint_dab_posterize(posterize);
        dab.mode = get_mypaint_dab_posterize_num(dab.posterize, posterize_num);
    }
    else {
        dab.color = get_mypaint_dab_color(color_r, color_g, color_b, 1.0f);
        dab.flags = (uint8_t)paint_mode | mask_flags | flood_flags;
        dab.lock_alpha = 0;
        dab.colorize = 0;
        dab.posterize = 0;
        dab.mode = 0;
    }

    dab.hardness = get_uint8(hardness);
    dab.opacity = get_uint8(opaque);
    pixel_perfect_apply(be, dab.x / 4, dab.y / 4, pixel_perfect_append_mypaint,
                        pixel_perfect_append_tentative_mypaint,
                        pixel_perfect_assign_tentative_mypaint, &dab);
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
    return allocate_buffer(be);
}

static bool is_pigment_mode(DP_BlendMode blend_mode)
{
    return blend_mode == DP_BLEND_MODE_PIGMENT
        || blend_mode == DP_BLEND_MODE_PIGMENT_ALPHA;
}

static int get_color_mypaint_pigment(MyPaintSurface2 *self, float x, float y,
                                     float radius, float *color_r,
                                     float *color_g, float *color_b,
                                     float *color_a, DP_UNUSED float paint)
{
    DP_BrushEngine *be = get_mypaint_surface_brush_engine(self);
    DP_LayerContent *lc = update_sample_layer_content(be);
    bool in_bounds;
    if (lc) {
        int diameter = DP_min_int(DP_float_to_int(radius * 2.0f + 0.5f), 255);
        DP_UPixelFloat color = DP_layer_content_sample_color_at(
            lc, get_stamp_buffer(be), DP_float_to_int(x + 0.5f),
            DP_float_to_int(y + 0.5f), diameter, false,
            is_pigment_mode(be->mypaint.blend_mode), &be->last_diameter,
            &in_bounds);
        *color_r = color.r;
        *color_g = color.g;
        *color_b = color.b;
        *color_a = color.a;
    }
    else {
        in_bounds = false;
        *color_r = 0.0f;
        *color_g = 0.0f;
        *color_b = 0.0f;
        *color_a = 0.0f;
    }
    return in_bounds;
}

static int get_color_mypaint(MyPaintSurface *self, float x, float y,
                             float radius, float *color_r, float *color_g,
                             float *color_b, float *color_a)
{
    return get_color_mypaint_pigment((MyPaintSurface2 *)self, x, y, radius,
                                     color_r, color_g, color_b, color_a, 0.0f);
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

static int get_color_mypaint_pigment_dummy(DP_UNUSED MyPaintSurface2 *self,
                                           DP_UNUSED float x, DP_UNUSED float y,
                                           DP_UNUSED float radius,
                                           float *color_r, float *color_g,
                                           float *color_b, float *color_a,
                                           DP_UNUSED float paint)
{
    *color_r = 0.0f;
    *color_g = 0.0f;
    *color_b = 0.0f;
    *color_a = 0.0f;
    return 0;
}

static int get_color_mypaint_dummy(MyPaintSurface *self, float x, float y,
                                   float radius, float *color_r, float *color_g,
                                   float *color_b, float *color_a)
{
    return get_color_mypaint_pigment_dummy((MyPaintSurface2 *)self, x, y,
                                           radius, color_r, color_g, color_b,
                                           color_a, 0.0f);
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


static void stroke_to(DP_BrushEngine *be, DP_BrushPoint bp,
                      DP_CanvasState *cs_or_null);

static void brush_engine_handle_stroke_engine_push(void *user, DP_BrushPoint bp,
                                                   DP_CanvasState *cs_or_null)
{
    stroke_to(user, bp, cs_or_null);
}

static void brush_engine_handle_stroke_engine_poll_control(void *user,
                                                           bool enable)
{
    DP_BrushEngine *be = user;
    DP_BrushEnginePollControlFn poll_control = be->poll_control;
    if (poll_control) {
        poll_control(be->user, enable);
    }
}


DP_BrushEngine *
DP_brush_engine_new(DP_MaskSync *ms_or_null,
                    DP_BrushEnginePushMessageFn push_message,
                    DP_BrushEnginePollControlFn poll_control_or_null,
                    DP_BrushEngineSyncFn sync_or_null, void *user)
{
    init_mypaint_once();
    DP_BrushEngine *be = DP_malloc(sizeof(*be));
    *be = (DP_BrushEngine){
        stroke_engine_init(brush_engine_handle_stroke_engine_push,
                           brush_engine_handle_stroke_engine_poll_control, be),
        NULL,
        NULL,
        false,
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
        {0, 1.0f, 0.0f, false, false, false, false, false, false, 0},
        {false, false, 0, 0, ms_or_null ? ++ms_or_null->last_sync_id : 0,
         DP_mask_sync_incref_nullable(ms_or_null), NULL, 0, NULL, NULL},
        {NULL,
         NULL,
         0.0,
         0,
         DP_BRUSH_ENGINE_FLOOD_TARGET_NONE,
         {0, 0, -1, 0, 0, 0, 0, NULL}},
        {0},
        {0, 0, 0, 0, NULL},
        {false, false, false, false, 0, 0, 0, 0, {0}},
        push_message,
        poll_control_or_null,
        sync_or_null,
        user};
    issue_dummy_mypaint_stroke(be->mypaint_brush);
    return be;
}

void DP_brush_engine_free(DP_BrushEngine *be)
{
    if (be) {
        DP_free(be->dabs.buffer);
        mypaint_brush_unref(be->mypaint_brush);
        DP_compress_zstd_free(&be->mask.zstd_cctx);
        DP_free(be->flood.mask.buffer);
        DP_transient_layer_content_decref_nullable(be->flood.state);
        DP_layer_content_decref_nullable(be->flood.lc);
        DP_free(be->mask.map);
        DP_layer_content_decref_nullable(be->mask.lc);
        DP_mask_sync_decref_nullable(be->mask.ms);
        DP_canvas_state_decref_nullable(be->cs);
        DP_free_simd(be->buffer);
        stroke_engine_dispose(&be->se);
        DP_free(be);
    }
}

void DP_brush_engine_size_limit_set(DP_BrushEngine *be, int size_limit)
{
    DP_ASSERT(be);
    DP_atomic_set(&be->size_limit, size_limit);
}

static void set_common_stroke_params(DP_BrushEngine *be,
                                     const DP_BrushEngineStrokeParams *besp)
{
    be->layer_id = besp->layer_id;
    be->stroke.sync_samples = besp->sync_samples && be->sync;
    DP_stroke_engine_params_set(&be->se, &besp->se);
    DP_ASSERT(besp->selection_id < 32); // Only have 5 bits for the id.
    be->mask.next_selection_id = besp->selection_id;
    DP_layer_content_decref_nullable(be->flood.lc);
    be->flood.lc = DP_layer_content_incref_nullable(besp->flood_lc);
    be->flood.tolerance = DP_max_double(besp->flood_tolerance, 0.0);
    be->flood.expand =
        DP_clamp_int(besp->flood_expand, 0, DP_ANTI_OVERFLOW_EXPAND_MAX);
}

static void apply_layer_alpha_lock(DP_BlendMode *in_out_blend_mode,
                                   bool layer_alpha_lock)
{
    if (layer_alpha_lock) {
        *in_out_blend_mode = (DP_BlendMode)DP_blend_mode_to_alpha_preserving(
            (int)*in_out_blend_mode);
    }
}

void DP_brush_engine_classic_brush_set(DP_BrushEngine *be,
                                       const DP_ClassicBrush *brush,
                                       const DP_BrushEngineStrokeParams *besp,
                                       const DP_UPixelFloat *color_override,
                                       bool eraser_override)
{
    DP_ASSERT(be);
    DP_ASSERT(brush);
    DP_ASSERT(besp);
    DP_ASSERT(besp->layer_id == 0
              || DP_layer_id_normal_or_selection(besp->layer_id));

    set_common_stroke_params(be, besp);

    switch (brush->shape) {
    case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
    case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
        be->active = DP_BRUSH_ENGINE_ACTIVE_PIXEL;
        be->pixel.snap = brush->pixel_art_input;
        be->pixel.perfect = besp->allow_pixel_perfect && brush->pixel_perfect;
        break;
    default:
        be->active = DP_BRUSH_ENGINE_ACTIVE_SOFT;
        be->pixel.snap = false;
        be->pixel.perfect = false;
        break;
    }

    be->classic.brush = *brush;
    DP_ClassicBrush *cb = &be->classic.brush;
    if (eraser_override) {
        cb->erase = true;
        cb->erase_mode = DP_BLEND_MODE_ERASE;
    }

    apply_layer_alpha_lock(cb->erase ? &cb->erase_mode : &cb->brush_mode,
                           besp->layer_alpha_lock);

    if (DP_blend_mode_direct_only((int)DP_classic_brush_blend_mode(cb))) {
        cb->paint_mode = DP_PAINT_MODE_DIRECT;
    }

    DP_UPixelFloat color = color_override ? *color_override : brush->color;
    if (cb->paint_mode == DP_PAINT_MODE_DIRECT || cb->smudge.max > 0.0f) {
        // Incremental mode must be used when smudging, because color is not
        // picked up from sublayers
        cb->paint_mode = DP_PAINT_MODE_DIRECT;
        be->classic.indirect_alpha = 0.0f;
    }
    else {
        // For indirect drawing modes, the alpha is used as the overall opacity
        // of the entire stroke.
        float opacity = cb->opacity.max;
        be->classic.indirect_alpha = opacity;
        cb->opacity.max = 1.0f;
        if (cb->opacity_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE) {
            cb->opacity.min = opacity > 0.0f ? cb->opacity.min / color.a : 1.0f;
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

static void disable_mypaint_setting(MyPaintBrush *mb, MyPaintBrushSetting s,
                                    float base_value)
{
    mypaint_brush_set_base_value(mb, s, base_value);
    disable_mypaint_dynamics(mb, s);
}

void DP_brush_engine_mypaint_brush_set(DP_BrushEngine *be,
                                       const DP_MyPaintBrush *brush,
                                       const DP_MyPaintSettings *settings,
                                       const DP_BrushEngineStrokeParams *besp,
                                       const DP_UPixelFloat *color_override,
                                       bool eraser_override)
{
    DP_ASSERT(be);
    DP_ASSERT(brush);
    DP_ASSERT(settings);
    DP_ASSERT(besp);
    DP_ASSERT(besp->layer_id >= DP_LAYER_ID_MIN);
    DP_ASSERT(besp->layer_id <= DP_LAYER_ID_MAX);

    set_common_stroke_params(be, besp);
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
    apply_layer_alpha_lock(&be->mypaint.blend_mode, besp->layer_alpha_lock);

    DP_BlendMode blend_mode = be->mypaint.blend_mode;
    be->mypaint.paint_mode = DP_blend_mode_direct_only((int)blend_mode)
                               ? DP_PAINT_MODE_DIRECT
                               : brush->paint_mode;

    // We don't support partial pigment mode for performance and usability
    // reasons, you either enable the pigment blend mode or you don't.
    mypaint_brush_set_base_value(mb, MYPAINT_BRUSH_SETTING_PAINT_MODE,
                                 is_pigment_mode(blend_mode) ? 1.0f : 0.0f);
    disable_mypaint_dynamics(mb, MYPAINT_BRUSH_SETTING_PAINT_MODE);

    // We have our own, better stabilizer, so turn the MyPaint one off.
    disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SLOW_TRACKING, 0.0f);

    bool indirect = be->mypaint.paint_mode != DP_PAINT_MODE_DIRECT;
    if (indirect || DP_blend_mode_compares_alpha((int)blend_mode)) {
        // Opacity linearization is supposed to compensate for direct mode
        // drawing, in indirect mode it just causes really wrong behavior. The
        // same goes for blend modes like Greater, which effectively do an
        // indirect mode on top of the existing color.
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE,
                                0.0f);
    }

    if (indirect) {
        // Indirect mode can't smudge.
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE, 0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH, 0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_RADIUS_LOG,
                                0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH_LOG,
                                0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_BUCKET, 0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SMUDGE_TRANSPARENCY,
                                0.0f);
    }

    if (indirect || !is_normal_mypaint_mode(blend_mode)) {
        // Indirect mode also can't do any of these. They're also basically
        // blend modes, so they only work when that hasn't been altered.
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_ERASER, 0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_LOCK_ALPHA, 0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_COLORIZE, 0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_POSTERIZE, 0.0f);
    }

    bool pixel_perfect = brush->pixel_perfect;
    be->pixel.snap = false;
    be->pixel.perfect = besp->allow_pixel_perfect && pixel_perfect;
    if (pixel_perfect) {
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_HARDNESS, 1.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_ANTI_ALIASING, 0.0f);
        disable_mypaint_setting(mb, MYPAINT_BRUSH_SETTING_SNAP_TO_PIXEL, 1.0f);
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

static uint8_t get_effective_classic_blend_mode(DP_BrushEngine *be,
                                                DP_BlendMode opaque_mode,
                                                DP_BlendMode alpha_mode)
{
    unsigned int alpha = (be->classic.dab_color & 0xff000000u) >> 24u;
    if (alpha == 0) {
        return (uint8_t)DP_BLEND_MODE_ERASE;
    }
    else if (alpha < 255) {
        return (uint8_t)alpha_mode;
    }
    else {
        return (uint8_t)opaque_mode;
    }
}

static uint8_t get_classic_dab_blend_mode(DP_BrushEngine *be)
{
    DP_PaintMode paint_mode = get_classic_paint_mode(be);
    DP_BlendMode blend_mode = get_classic_blend_mode(be);
    if (paint_mode == DP_PAINT_MODE_DIRECT) {
        switch (blend_mode) {
        case DP_BLEND_MODE_NORMAL:
            return get_effective_classic_blend_mode(
                be, DP_BLEND_MODE_NORMAL, DP_BLEND_MODE_NORMAL_AND_ERASER);
        case DP_BLEND_MODE_PIGMENT_ALPHA:
            return get_effective_classic_blend_mode(
                be, DP_BLEND_MODE_PIGMENT_ALPHA,
                DP_BLEND_MODE_PIGMENT_AND_ERASER);
        case DP_BLEND_MODE_OKLAB_NORMAL:
            return get_effective_classic_blend_mode(
                be, DP_BLEND_MODE_OKLAB_NORMAL,
                DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER);
        default:
            break;
        }
    }
    return get_dab_blend_mode(paint_mode, blend_mode);
}

static uint32_t get_classic_dab_color(DP_BrushEngine *be,
                                      uint8_t dab_blend_mode)
{
    uint32_t color = be->classic.dab_color;
    // In direct mode, alpha on the color is meaningless unless it's an "and
    // eraser" blend mode that actually makes use of it, so we clamp off the
    // alpha in the cases where it doesn't matter.
    if (be->classic.indirect_alpha == 0.0f
        && dab_blend_mode != DP_BLEND_MODE_NORMAL_AND_ERASER
        && dab_blend_mode != DP_BLEND_MODE_PIGMENT_AND_ERASER
        && dab_blend_mode != DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER) {
        color &= (uint32_t)0xffffff;
    }
    return color;
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
    uint8_t dab_blend_mode = get_classic_dab_blend_mode(be);
    be->push_message(
        be->user,
        new_fn(be->stroke.context_id, (uint8_t)be->classic.dab_flags,
               DP_int_to_uint32(be->layer_id), be->classic.dab_x,
               be->classic.dab_y, get_classic_dab_color(be, dab_blend_mode),
               dab_blend_mode, set_pixel_dabs, used, be));
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
    uint8_t dab_blend_mode = get_classic_dab_blend_mode(be);
    be->push_message(be->user, DP_msg_draw_dabs_classic_new(
                                   be->stroke.context_id, be->classic.dab_flags,
                                   DP_int_to_uint32(be->layer_id),
                                   be->classic.dab_x, be->classic.dab_y,
                                   get_classic_dab_color(be, dab_blend_mode),
                                   dab_blend_mode, set_soft_dabs, used, be));
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
    DP_PaintMode paint_mode = get_mypaint_paint_mode(be);
    DP_BlendMode blend_mode = get_mypaint_blend_mode(be);
    if (be->stroke.compatibility_mode
        || (paint_mode == DP_PAINT_MODE_DIRECT
            && is_normal_mypaint_mode(blend_mode))) {
        msg = DP_msg_draw_dabs_mypaint_new(
            be->stroke.context_id, be->mypaint.dab_flags,
            DP_int_to_uint32(be->layer_id), be->mypaint.dab_x,
            be->mypaint.dab_y, be->mypaint.dab_color,
            be->mypaint.dab_lock_alpha, be->mypaint.dab_colorize,
            be->mypaint.dab_posterize, be->mypaint.dab_mode, set_mypaint_dabs,
            used, be);
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

void DP_brush_engine_message_push_noinc(DP_BrushEngine *be, DP_Message *msg)
{
    DP_ASSERT(be);
    DP_ASSERT(msg);
    be->push_message(be->user, msg);
}


static DP_LayerContent *search_sel_lc(DP_CanvasState *cs_or_null,
                                      unsigned int context_id, int selection_id)
{
    if (cs_or_null && selection_id > 0) {
        DP_SelectionSet *ss =
            DP_canvas_state_selections_noinc_nullable(cs_or_null);
        if (ss) {
            DP_Selection *sel =
                DP_selection_set_search_noinc(ss, context_id, selection_id);
            if (sel) {
                return DP_selection_content_noinc(sel);
            }
        }
    }
    return NULL;
}

static bool update_mask_sync_id(DP_BrushEngine *be)
{
    if (be->mask.ms) {
        int sync_id = be->mask.sync_id;
        int prev_sync_id = DP_atomic_xch(&be->mask.ms->sync_id, sync_id);
        return sync_id != prev_sync_id;
    }
    return false;
}

void DP_brush_engine_stroke_begin(DP_BrushEngine *be,
                                  DP_CanvasState *cs_or_null,
                                  unsigned int context_id,
                                  bool compatibility_mode, bool push_undo_point,
                                  bool mirror, bool flip, float zoom,
                                  float angle)
{
    DP_ASSERT(be);
    DP_ASSERT(!be->stroke.in_progress);
    DP_ASSERT(be->dabs.used == 0);
    DP_PERF_BEGIN_DETAIL(fn, "stroke_begin", "active=%d", (int)be->active);
    DP_EVENT_LOG("stroke_begin");

    be->stroke.active = true;
    be->stroke.context_id = context_id;
    be->stroke.zoom = zoom;
    // We currently only use the view angle for MyPaint brushes, which take it
    // as radians (and then convert it back to degrees again internally.)
    be->stroke.angle_rad = DP_double_to_float(angle * M_PI / 180.0);
    be->stroke.mirror = mirror;
    be->stroke.flip = flip;
    be->stroke.compatibility_mode = compatibility_mode;
    if (push_undo_point) {
        be->push_message(be->user, DP_msg_undo_point_new(context_id));
    }
    DP_stroke_engine_stroke_begin(&be->se);

    be->pixel.have_last = false;
    be->pixel.have_tentative = false;

    // If we're supposed to synchronize smudging, grab a fresh canvas state here
    // if we haven't been given one to use yet, cf. DP_StrokeWorker not passing
    // canvas states when it's running with a worker thread.
    DP_CanvasState *sync_cs;
    if (!cs_or_null && be->stroke.sync_samples) {
        DP_EVENT_LOG("stroke_begin sync");
        sync_cs = be->sync(be->user);
        DP_EVENT_LOG("stroke_begin synced");
        cs_or_null = sync_cs;
    }
    else {
        sync_cs = NULL;
    }

    int next_selection_id = be->mask.next_selection_id;
    DP_LayerContent *mask_lc =
        search_sel_lc(cs_or_null, context_id, next_selection_id);
    if (!compatibility_mode && mask_lc) {
        be->mask.active = true;
        be->mask.preview = !push_undo_point;
        bool sync_id_changed;
        if (push_undo_point
            && ((sync_id_changed = update_mask_sync_id(be))
                || mask_lc != be->mask.lc)) {
            // Not strictly necessary, but we'll clear the next selection if
            // another brush engine has changed it from under us.
            bool clear_next =
                sync_id_changed
                && be->mask.current_selection_id != next_selection_id;
            push_mask_selection_sync_clear(be);
            be->mask.current_selection_id = next_selection_id;
            if (clear_next) {
                push_mask_selection_sync_clear(be);
            }
            DP_layer_content_decref_nullable(be->mask.lc);
            be->mask.lc = DP_layer_content_incref(mask_lc);
            size_t required_capacity = DP_int_to_size(DP_tile_total_round(
                                           DP_layer_content_width(mask_lc),
                                           DP_layer_content_height(mask_lc)))
                                     * sizeof(*be->mask.map);
            if (required_capacity <= be->mask.capacity) {
                memset(be->mask.map, 0, required_capacity);
            }
            else {
                be->mask.capacity = required_capacity;
                DP_free(be->mask.map);
                be->mask.map = DP_malloc_zeroed(required_capacity);
            }
        }
    }
    else {
        be->mask.active = false;
        if (push_undo_point && (update_mask_sync_id(be) || be->mask.lc)) {
            push_mask_selection_sync_clear(be);
            be->mask.current_selection_id = 0;
            DP_layer_content_decref_nullable(be->mask.lc);
            be->mask.lc = NULL;
        }
    }

    DP_canvas_state_decref_nullable(sync_cs);
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
        diameter, !cb->smudge_alpha || be->stroke.compatibility_mode,
        is_pigment_mode(get_classic_brush_blend_mode(be, cb)),
        &be->last_diameter, NULL);
}

static void update_classic_smudge(DP_BrushEngine *be, DP_ClassicBrush *cb,
                                  float x, float y, float pressure,
                                  float velocity, float distance)
{
    float smudge = DP_classic_brush_smudge_at(cb, pressure, velocity, distance);
    int smudge_distance = ++be->classic.smudge_distance;
    if (smudge > 0.0f && smudge_distance > cb->resmudge) {
        DP_LayerContent *lc = update_sample_layer_content(be);
        if (lc) {
            DP_UPixelFloat sample = sample_classic_smudge(
                be, cb, lc, x, y, pressure, velocity, distance);
            DP_UPixelFloat *sp = &be->classic.smudge_color;
            if (cb->smudge_alpha && !be->stroke.compatibility_mode) {
                if (sample.a > 0.0f) {
                    *sp = sample;
                }
                else {
                    sp->a = sample.a;
                }
            }
            else {
                float a = sp->a > 0.0f ? sample.a * smudge : 1.0f;
                float a1 = (1.0f - a);
                sp->r = sp->r * a1 + sample.r * a;
                sp->g = sp->g * a1 + sample.g * a;
                sp->b = sp->b * a1 + sample.b * a;
            }
            be->classic.smudge_distance = 0;
        }
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

static void stroke_pixel(DP_BrushEngine *be, DP_ClassicBrush *cb, float x,
                         float y, float pressure, float delta_sec)
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
            update_classic_smudge(be, cb, DP_int_to_float(x0),
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

static void stroke_soft(DP_BrushEngine *be, DP_ClassicBrush *cb, float x,
                        float y, float pressure, float delta_sec)
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

            update_classic_smudge(be, cb, dab_x, dab_y, dab_p, dab_v, dab_d);
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
    void (*stroke)(DP_BrushEngine *, DP_ClassicBrush *, float, float, float,
                   float))
{
    if (be->pixel.snap) {
        x = floorf(x);
        y = floorf(y);
    }

    DP_ClassicBrush *cb = &be->classic.brush;
    if (be->stroke.in_progress) {
        float delta_sec = DP_max_float(
            DP_llong_to_float(time_msec - be->stroke.last_time_msec) / 1000.0f,
            0.0001f);
        stroke(be, cb, x, y, pressure, delta_sec);
    }
    else {
        be->classic.last_velocity = 0.0f;
        be->classic.last_distance = 0.0f;
        be->classic.last_left = false;
        be->classic.last_up = false;
        be->stroke.in_progress = true;
        DP_LayerContent *lc;
        bool smudge_alpha = cb->smudge_alpha && !be->stroke.compatibility_mode;
        bool colorpick =
            (cb->colorpick || (smudge_alpha && cb->smudge.max > 0.0f))
            && get_classic_blend_mode(be) != DP_BLEND_MODE_ERASE
            && (lc = update_sample_layer_content(be));
        if (colorpick) {
            be->classic.smudge_color =
                sample_classic_smudge(be, cb, lc, x, y, pressure, 0.0f, 0.0f);
            if (smudge_alpha) {
                if (be->classic.smudge_color.a > 0.0f) {
                    if (cb->colorpick) {
                        be->classic.brush_color.b = be->classic.smudge_color.b;
                        be->classic.brush_color.g = be->classic.smudge_color.g;
                        be->classic.brush_color.r = be->classic.smudge_color.r;
                    }
                }
                else {
                    be->classic.smudge_color.b = be->classic.brush_color.b;
                    be->classic.smudge_color.g = be->classic.brush_color.g;
                    be->classic.smudge_color.r = be->classic.brush_color.r;
                }
            }
            else {
                be->classic.smudge_color.a = 1.0f;
            }
            be->classic.smudge_distance = -1;
        }
        else {
            be->classic.smudge_color = be->classic.brush_color;
            be->classic.smudge_color.a = 1.0f;
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

static void stroke_to(DP_BrushEngine *be, DP_BrushPoint bp,
                      DP_CanvasState *cs_or_null)
{
    DP_BrushEngineActiveType active = be->active;
    DP_PERF_BEGIN_DETAIL(fn, "stroke_to", "active=%d", (int)active);

    if (cs_or_null != be->cs) {
        DP_canvas_state_decref_nullable(be->cs);
        be->cs = DP_canvas_state_incref_nullable(cs_or_null);
        be->lc_valid = false;
    }

    if (bp.time_msec < be->stroke.last_time_msec) {
        bp.time_msec = be->stroke.last_time_msec;
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


void DP_brush_engine_stroke_to(DP_BrushEngine *be, DP_BrushPoint bp,
                               DP_CanvasState *cs_or_null)
{
    DP_ASSERT(be);
    DP_stroke_engine_stroke_to(&be->se, bp, cs_or_null);
}

void DP_brush_engine_poll(DP_BrushEngine *be, long long time_msec,
                          DP_CanvasState *cs_or_null)
{
    DP_ASSERT(be);
    if (be->stroke.active) {
        DP_stroke_engine_poll(&be->se, time_msec, cs_or_null);
    }
}

void DP_brush_engine_stroke_end(DP_BrushEngine *be, long long time_msec,
                                DP_CanvasState *cs_or_null, bool push_pen_up)
{
    DP_ASSERT(be);
    DP_PERF_BEGIN_DETAIL(fn, "stroke_end", "active=%d", (int)be->active);
    DP_EVENT_LOG("stroke_end");

    DP_stroke_engine_stroke_end(&be->se, time_msec, cs_or_null);

    DP_canvas_state_decref_nullable(be->cs);
    be->cs = NULL;
    be->lc_valid = false;

    DP_brush_engine_dabs_flush(be);

    if (push_pen_up) {
        be->push_message(
            be->user, DP_msg_pen_up_new(be->stroke.context_id,
                                        be->stroke.compatibility_mode
                                            ? (uint32_t)0
                                            : DP_int_to_uint32(be->layer_id)));
    }
    be->stroke.in_progress = false;
    be->stroke.active = false;

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
