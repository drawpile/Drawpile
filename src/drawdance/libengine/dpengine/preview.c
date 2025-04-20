// SPDX-License-Identifier: GPL-3.0-or-later
#include "preview.h"
#include "canvas_state.h"
#include "draw_context.h"
#include "image.h"
#include "layer_content.h"
#include "layer_props.h"
#include "layer_routes.h"
#include "paint.h"
#include "pixels.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>


#define RENDER_NEEDED 0
#define RENDER_QUEUED 1
#define RENDER_ACTIVE 2
#define RENDER_DONE   3
#define RENDER_CANCEL 4

typedef const int *(*DP_PreviewGetLayerIdsFn)(DP_Preview *pv, int *out_count);
typedef void (*DP_PreviewRenderFn)(DP_Preview *pv, DP_DrawContext *dc,
                                   int offset_x, int offset_y,
                                   DP_TransientLayerContent *tlc);
typedef void (*DP_PreviewDisposeFn)(DP_Preview *pv);

struct DP_Preview {
    DP_Atomic refcount;
    DP_Atomic status;
    int type;
    int blend_mode;
    uint16_t opacity;
    int initial_offset_x, initial_offset_y;
    DP_AtomicPtr render_lc;
    DP_LayerContent *lc;
    DP_LayerProps *lp;
    DP_PreviewGetLayerIdsFn get_layer_ids;
    DP_PreviewRenderFn render;
    DP_PreviewDisposeFn dispose;
};

struct DP_PreviewRenderer {
    struct {
        DP_PreviewRenderedFn rendered;
        DP_PreviewRerenderedFn rerendered;
        DP_PreviewClearFn clear;
        void *user;
    } fn;
    // Temporary buffer to collect layer route entries to render a preview onto.
    struct {
        int capacity;
        DP_LayerRoutesSelEntry *entries;
    } lrse_buffer;
    DP_Queue queue;
    DP_DrawContext *dc;
    DP_Mutex *queue_mutex;
    DP_Semaphore *queue_sem;
    DP_Thread *thread;
};

typedef enum DP_PreviewRenderJobType {
    DP_PREVIEW_RENDER_JOB_RENDER,
    DP_PREVIEW_RENDER_JOB_RERENDER,
    DP_PREVIEW_RENDER_JOB_CLEAR,
    DP_PREVIEW_RENDER_JOB_CLEAR_ALL_TRANSFORMS,
    DP_PREVIEW_RENDER_JOB_QUIT,
} DP_PreviewRenderJobType;

typedef struct DP_PreviewRenderJob {
    DP_PreviewRenderJobType type;
    union {
        struct {
            DP_Preview *pv;
            int canvas_width, canvas_height;
            int offset_x, offset_y;
        } render;
        struct {
            int type;
        } clear;
    };
} DP_PreviewRenderJob;


DP_Preview DP_preview_null;

static void init_preview(DP_Preview *pv, int type, int blend_mode,
                         uint16_t opacity, int initial_offset_x,
                         int initial_offset_y,
                         DP_PreviewGetLayerIdsFn get_layer_ids,
                         DP_PreviewRenderFn render, DP_PreviewDisposeFn dispose)
{
    DP_ASSERT(pv);
    DP_ASSERT(DP_blend_mode_exists((int)blend_mode));
    DP_ASSERT(type >= 0);
    DP_ASSERT(type < DP_PREVIEW_COUNT);
    DP_ASSERT(get_layer_ids);
    DP_ASSERT(render);
    DP_atomic_set(&pv->refcount, 1);
    DP_atomic_set(&pv->status, RENDER_NEEDED);
    pv->type = type;
    pv->blend_mode = blend_mode;
    pv->opacity = opacity > DP_BIT15 ? DP_BIT15 : opacity;
    pv->initial_offset_x = initial_offset_x;
    pv->initial_offset_y = initial_offset_y;
    DP_atomic_ptr_set(&pv->render_lc, NULL);
    pv->lc = NULL;
    pv->lp = NULL;
    pv->get_layer_ids = get_layer_ids;
    pv->render = render;
    pv->dispose = dispose;
}

DP_Preview *DP_preview_incref(DP_Preview *pv)
{
    DP_ASSERT(pv);
    DP_ASSERT(DP_atomic_get(&pv->refcount) > 0);
    DP_atomic_inc(&pv->refcount);
    return pv;
}

DP_Preview *DP_preview_incref_nullable(DP_Preview *pv_or_null)
{
    return pv_or_null ? DP_preview_incref(pv_or_null) : NULL;
}

void DP_preview_decref(DP_Preview *pv)
{
    DP_ASSERT(pv);
    DP_ASSERT(DP_atomic_get(&pv->refcount) > 0);
    if (DP_atomic_dec(&pv->refcount)) {
        DP_PreviewDisposeFn dispose = pv->dispose;
        if (dispose) {
            dispose(pv);
        }
        DP_layer_props_decref_nullable(pv->lp);
        DP_layer_content_decref_nullable(pv->lc);
        DP_layer_content_decref_nullable(
            DP_atomic_ptr_xch(&pv->render_lc, NULL));
        DP_free(pv);
    }
}

void DP_preview_decref_nullable(DP_Preview *pv_or_null)
{
    if (pv_or_null) {
        DP_preview_decref(pv_or_null);
    }
}

int DP_preview_refcount(DP_Preview *pv)
{
    DP_ASSERT(pv);
    DP_ASSERT(DP_atomic_get(&pv->refcount) > 0);
    return DP_atomic_get(&pv->refcount);
}

int DP_preview_type(DP_Preview *pv)
{
    DP_ASSERT(pv);
    DP_ASSERT(DP_atomic_get(&pv->refcount) > 0);
    return pv->type;
}

static DP_TransientCanvasState *
get_or_make_transient_canvas_state(DP_CanvasState *cs)
{
    if (DP_canvas_state_transient(cs)) {
        return (DP_TransientCanvasState *)cs;
    }
    else {
        DP_TransientCanvasState *tcs = DP_transient_canvas_state_new(cs);
        DP_canvas_state_decref(cs);
        return tcs;
    }
}

static DP_LayerRoutesSelEntry *
preview_renderer_lrse_buffer_require(DP_PreviewRenderer *pvr, int count)
{
    DP_ASSERT(pvr);
    if (pvr->lrse_buffer.capacity < count) {
        DP_free(pvr->lrse_buffer.entries);
        pvr->lrse_buffer.entries = DP_malloc(sizeof(*pvr->lrse_buffer.entries)
                                             * DP_int_to_size(count));
        pvr->lrse_buffer.capacity = count;
    }
    return pvr->lrse_buffer.entries;
}

static DP_LayerRoutesSelEntry *
get_layer_route_sel_entries(DP_Preview *pv, DP_CanvasState *cs,
                            DP_PreviewRenderer *pvr, int *out_count)
{
    int layer_id_count;
    const int *layer_ids = pv->get_layer_ids(pv, &layer_id_count);
    DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
    int lrse_count = 0;
    DP_LayerRoutesSelEntry *lrses =
        preview_renderer_lrse_buffer_require(pvr, layer_id_count);
    for (int i = 0; i < layer_id_count; ++i) {
        DP_LayerRoutesSelEntry lrse =
            DP_layer_routes_search_sel(lr, cs, layer_ids[i]);
        if (DP_layer_routes_sel_entry_is_valid_target(&lrse)) {
            lrses[lrse_count++] = lrse;
        }
    }
    *out_count = lrse_count;
    return lrses;
}

DP_CanvasState *DP_preview_apply(DP_Preview *pv, DP_CanvasState *cs,
                                 DP_PreviewRenderer *pvr)
{
    DP_ASSERT(pv);
    DP_ASSERT(DP_atomic_get(&pv->refcount) > 0);

    // Figure out where to put the preview or bail out if there's nowhere.
    int lrse_count;
    DP_LayerRoutesSelEntry *lrses =
        get_layer_route_sel_entries(pv, cs, pvr, &lrse_count);
    if (lrse_count == 0) {
        return cs;
    }

    // If there's a new render available, grab it, otherwise use the cache.
    DP_LayerContent *lc = DP_atomic_ptr_xch(&pv->render_lc, NULL);
    if (lc) {
        DP_layer_content_decref_nullable(pv->lc);
        pv->lc = lc;
    }
    else {
        lc = pv->lc;
        if (!lc) {
            return cs;
        }
    }

    // If the canvas size changed, the existing render is unusable.
    int canvas_width = DP_canvas_state_width(cs);
    int canvas_height = DP_canvas_state_height(cs);
    bool canvas_size_changed = DP_layer_content_width(lc) != canvas_width
                            || DP_layer_content_height(lc) != canvas_height;
    if (canvas_size_changed) {
        if (DP_atomic_compare_exchange(&pv->status, RENDER_DONE,
                                       RENDER_NEEDED)) {
            DP_preview_renderer_push_rerender_inc(
                pvr, pv, canvas_width, canvas_height,
                DP_canvas_state_offset_x(cs), DP_canvas_state_offset_y(cs));
        }
        return cs;
    }

    // We have a usable render. Create props if we don't have them yet.
    DP_LayerProps *lp = pv->lp;
    if (!lp) {
        DP_TransientLayerProps *tlp = DP_transient_layer_props_new_init(
            DP_PREVIEW_SUBLAYER_ID(pv->type), false);
        DP_transient_layer_props_blend_mode_set(tlp, pv->blend_mode);
        DP_transient_layer_props_opacity_set(tlp, pv->opacity);
        lp = DP_transient_layer_props_persist(tlp);
        pv->lp = lp;
    }

    // Jam the preview into the canvas state.
    DP_TransientCanvasState *tcs = get_or_make_transient_canvas_state(cs);
    for (int i = 0; i < lrse_count; ++i) {
        DP_LayerRoutesSelEntry *lrse = &lrses[i];
        DP_TransientLayerContent *tlc =
            DP_layer_routes_sel_entry_transient_content(lrse, tcs);
        DP_transient_layer_content_sublayer_insert_inc(tlc, lc, lp);
    }
    return (DP_CanvasState *)tcs;
}

void DP_preview_render_reset(DP_Preview *pv, DP_PreviewRenderer *pvr,
                             int canvas_width, int canvas_height, int offset_x,
                             int offset_y)
{
    DP_ASSERT(pv);
    DP_ASSERT(DP_atomic_get(&pv->refcount) > 0);
    DP_ASSERT(pvr);
    DP_layer_content_decref(pv->lc);
    pv->lc = NULL;
    DP_preview_renderer_push_noinc(pvr, pv, canvas_width, canvas_height,
                                   offset_x, offset_y);
}


typedef struct DP_PreviewCut {
    DP_Preview parent;
    int x, y;
    int width, height;
    int layer_id_count;
    // Flexible-length data: first layer ids as ints, then a byte that says
    // whether there's a mask or not, then the mask bytes.
    alignas(int) unsigned char data[];
} DP_PreviewCut;

static const int *preview_cut_get_layer_ids(DP_Preview *pv, int *out_count)
{
    DP_PreviewCut *pvc = (DP_PreviewCut *)pv;
    *out_count = pvc->layer_id_count;
    // Have to cast to void * intead of int * because otherwise Clang emits a
    // bogus warning about alignment, it doesn't understand alignas I guess.
    return (void *)pvc->data;
}

static const uint8_t *preview_cut_mask(DP_PreviewCut *pvc)
{
    unsigned char *mask_data =
        pvc->data + sizeof(int) * DP_int_to_size(pvc->layer_id_count);
    if (*mask_data) {
        return (uint8_t *)mask_data + 1;
    }
    else {
        return NULL;
    }
}

static void preview_cut_render(DP_Preview *pv, DP_UNUSED DP_DrawContext *dc,
                               int offset_x, int offset_y,
                               DP_TransientLayerContent *tlc)
{
    DP_PreviewCut *pvc = (DP_PreviewCut *)pv;
    int left = pvc->x + offset_x;
    int top = pvc->y + offset_y;
    int width = pvc->width;
    int height = pvc->height;
    int right = DP_min_int(DP_transient_layer_content_width(tlc), left + width);
    int bottom =
        DP_min_int(DP_transient_layer_content_height(tlc), top + height);
    const uint8_t *mask = preview_cut_mask(pvc);
    if (mask) {
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                uint8_t a = mask[(y - top) * width + (x - left)];
                if (a != 0) {
                    DP_transient_layer_content_pixel_at_set(
                        tlc, 0, x, y,
                        (DP_Pixel15){0, 0, 0, DP_channel8_to_15(a)});
                }
            }
        }
    }
    else {
        DP_transient_layer_content_fill_rect(tlc, 0, DP_BLEND_MODE_REPLACE,
                                             left, top, right, bottom,
                                             (DP_UPixel15){0, 0, 0, DP_BIT15});
    }
}

DP_Preview *DP_preview_new_cut(int initial_offset_x, int initial_offset_y,
                               int x, int y, int width, int height,
                               const DP_Pixel8 *mask_or_null,
                               int layer_id_count, const int *layer_ids)
{
    size_t mask_size =
        mask_or_null ? DP_int_to_size(width) * DP_int_to_size(height) : 0;
    size_t layer_ids_size = sizeof(int) * DP_int_to_size(layer_id_count);
    DP_PreviewCut *pvc = DP_malloc(DP_FLEX_SIZEOF(
        DP_PreviewCut, data, layer_ids_size + (size_t)1 + mask_size));
    init_preview(&pvc->parent, DP_PREVIEW_CUT, DP_BLEND_MODE_ERASE, DP_BIT15,
                 initial_offset_x, initial_offset_y, preview_cut_get_layer_ids,
                 preview_cut_render, NULL);
    pvc->x = x;
    pvc->y = y;
    pvc->width = width;
    pvc->height = height;
    pvc->layer_id_count = layer_id_count;
    memcpy(pvc->data, layer_ids, layer_ids_size);
    pvc->data[layer_ids_size] = mask_or_null != NULL;
    for (size_t i = 0; i < mask_size; ++i) {
        pvc->data[layer_ids_size + (size_t)1 + i] = mask_or_null[i].a;
    }
    return &pvc->parent;
}


typedef struct DP_PreviewTransform {
    DP_Preview parent;
    int layer_id;
    int x, y, width, height;
    DP_Quad dst_quad;
    int interpolation;
    DP_Image *img;
    struct {
        DP_PreviewTransformGetPixelsFn get;
        DP_PreviewTransformDisposePixelsFn dispose;
        void *user;
    } pixels;
} DP_PreviewTransform;

static const int *preview_transform_get_layer_ids(DP_Preview *pv,
                                                  int *out_count)
{
    DP_PreviewTransform *pvtf = (DP_PreviewTransform *)pv;
    *out_count = 1;
    return &pvtf->layer_id;
}

static bool preview_transform_prepare_image(DP_PreviewTransform *pvtf,
                                            DP_DrawContext *dc)
{
    if (pvtf->img) {
        return true; // Image alread transformed successfully.
    }

    DP_PreviewTransformGetPixelsFn get_pixels = pvtf->pixels.get;
    if (!get_pixels) {
        return false; // Transform already attempted, but failed.
    }

    void *user = pvtf->pixels.user;
    const DP_Pixel8 *pixels = get_pixels(user);
    pvtf->pixels.get = NULL;
    DP_Image *img = DP_image_transform_pixels(
        pvtf->width, pvtf->height, pixels, dc, &pvtf->dst_quad,
        pvtf->interpolation, false, NULL, NULL);
    pvtf->pixels.dispose(user);
    pvtf->pixels.dispose = NULL;

    if (img) {
        pvtf->img = img;
        return true;
    }
    else {
        DP_warn("Error transforming preview: %s", DP_error());
        return false;
    }
}

static void preview_transform_render(DP_Preview *pv, DP_DrawContext *dc,
                                     int offset_x, int offset_y,
                                     DP_TransientLayerContent *tlc)
{
    DP_PreviewTransform *pvtf = (DP_PreviewTransform *)pv;
    if (preview_transform_prepare_image(pvtf, dc)) {
        int left = pvtf->x + offset_x;
        int top = pvtf->y + offset_y;
        DP_transient_layer_content_put_image(tlc, 0, DP_BLEND_MODE_REPLACE,
                                             left, top, pvtf->img);
    }
}

static void preview_transform_dispose(DP_Preview *pv)
{
    DP_PreviewTransform *pvtf = (DP_PreviewTransform *)pv;
    DP_PreviewTransformDisposePixelsFn dispose = pvtf->pixels.dispose;
    if (dispose) {
        dispose(pvtf->pixels.user);
    }
    DP_image_free(pvtf->img);
}

DP_Preview *DP_preview_new_transform(
    int id, int initial_offset_x, int initial_offset_y, int layer_id,
    int blend_mode, uint16_t opacity, int x, int y, int width, int height,
    const DP_Quad *dst_quad, int interpolation,
    DP_PreviewTransformGetPixelsFn get_pixels,
    DP_PreviewTransformDisposePixelsFn dispose_pixels, void *user)
{
    DP_ASSERT(id >= 0);
    DP_ASSERT(id < DP_PREVIEW_TRANSFORM_COUNT);
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);
    DP_ASSERT(dst_quad);
    DP_ASSERT(get_pixels);
    DP_ASSERT(dispose_pixels);
    DP_PreviewTransform *pvtf = DP_malloc(sizeof(*pvtf));
    init_preview(&pvtf->parent, DP_PREVIEW_TRANSFORM_FIRST + id, blend_mode,
                 opacity, initial_offset_x, initial_offset_y,
                 preview_transform_get_layer_ids, preview_transform_render,
                 preview_transform_dispose);
    pvtf->layer_id = layer_id;
    pvtf->x = x;
    pvtf->y = y;
    pvtf->width = width;
    pvtf->height = height;
    pvtf->dst_quad = *dst_quad;
    pvtf->interpolation = interpolation;
    pvtf->pixels.get = get_pixels;
    pvtf->pixels.dispose = dispose_pixels;
    pvtf->pixels.user = user;
    pvtf->img = NULL;
    return &pvtf->parent;
}


typedef struct DP_PreviewDabs {
    DP_Preview parent;
    int layer_id;
    int count;
    DP_Message *messages[];
} DP_PreviewDabs;

static const int *preview_dabs_get_layer_ids(DP_Preview *pv, int *out_count)
{
    DP_PreviewDabs *pvd = (DP_PreviewDabs *)pv;
    *out_count = 1;
    return &pvd->layer_id;
}

static int preview_dabs_blend_mode(DP_Message *msg)
{
    switch (DP_message_type(msg)) {
    case DP_MSG_DRAW_DABS_CLASSIC:
        return DP_msg_draw_dabs_classic_mode(DP_message_internal(msg));
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
        return DP_msg_draw_dabs_pixel_mode(DP_message_internal(msg));
    case DP_MSG_DRAW_DABS_MYPAINT: {
        DP_MsgDrawDabsMyPaint *mddmp = DP_message_internal(msg);
        if (DP_msg_draw_dabs_mypaint_lock_alpha(mddmp) == UINT8_MAX) {
            return DP_BLEND_MODE_RECOLOR;
        }
        else if (DP_msg_draw_dabs_mypaint_colorize(mddmp) == UINT8_MAX) {
            return DP_BLEND_MODE_COLOR;
        }
        else if (DP_msg_draw_dabs_mypaint_color(mddmp) & 0xff000000u) {
            return DP_BLEND_MODE_NORMAL;
        }
        else {
            return DP_BLEND_MODE_ERASE;
        }
    }
    case DP_MSG_DRAW_DABS_MYPAINT_BLEND:
        return DP_msg_draw_dabs_mypaint_blend_mode(DP_message_internal(msg));
    default:
        DP_UNREACHABLE();
    }
}

static void preview_dabs_render(DP_Preview *pv, DP_DrawContext *dc,
                                int offset_x, int offset_y,
                                DP_TransientLayerContent *tlc)
{
    DP_PreviewDabs *pvd = (DP_PreviewDabs *)pv;
    int count = pvd->count;
    DP_PaintDrawDabsParams params = {0};
    DP_TransientLayerContent *sub_tlc = NULL;
    uint16_t sub_opacity;

    for (int i = 0; i < count; ++i) {
        DP_Message *msg = pvd->messages[i];
        DP_MessageType type = DP_message_type(msg);
        switch (type) {
        case DP_MSG_DRAW_DABS_CLASSIC: {
            DP_MsgDrawDabsClassic *mddc = DP_message_internal(msg);
            params.origin_x = DP_msg_draw_dabs_classic_x(mddc);
            params.origin_y = DP_msg_draw_dabs_classic_y(mddc);
            params.color = DP_msg_draw_dabs_classic_color(mddc);
            params.paint_mode = DP_msg_draw_dabs_classic_paint_mode(mddc);
            params.classic.dabs =
                DP_msg_draw_dabs_classic_dabs(mddc, &params.dab_count);
            break;
        }
        case DP_MSG_DRAW_DABS_PIXEL:
        case DP_MSG_DRAW_DABS_PIXEL_SQUARE: {
            DP_MsgDrawDabsPixel *mddp = DP_message_internal(msg);
            params.origin_x = DP_msg_draw_dabs_pixel_x(mddp);
            params.origin_y = DP_msg_draw_dabs_pixel_y(mddp);
            params.color = DP_msg_draw_dabs_pixel_color(mddp);
            params.paint_mode = DP_msg_draw_dabs_pixel_paint_mode(mddp);
            params.pixel.dabs =
                DP_msg_draw_dabs_pixel_dabs(mddp, &params.dab_count);
            break;
        }
        case DP_MSG_DRAW_DABS_MYPAINT: {
            DP_MsgDrawDabsMyPaint *mddmp = DP_message_internal(msg);
            params.origin_x = DP_msg_draw_dabs_mypaint_x(mddmp);
            params.origin_y = DP_msg_draw_dabs_mypaint_y(mddmp);
            params.color = DP_msg_draw_dabs_mypaint_color(mddmp);
            params.paint_mode = DP_PAINT_MODE_DIRECT;
            params.mypaint.dabs =
                DP_msg_draw_dabs_mypaint_dabs(mddmp, &params.dab_count);
            params.mypaint.lock_alpha = 0;
            params.mypaint.colorize = 0;
            params.mypaint.posterize = 0;
            params.mypaint.posterize_num = 0;
            break;
        }
        case DP_MSG_DRAW_DABS_MYPAINT_BLEND:
            DP_warn("TODO: mypaint blend");
            DP_FALLTHROUGH();
        default:
            continue;
        }

        if (DP_paint_mode_indirect(params.paint_mode, &params.blend_mode)) {
            if (!sub_tlc) {
                sub_tlc = DP_transient_layer_content_new_init(
                    DP_transient_layer_content_width(tlc),
                    DP_transient_layer_content_height(tlc), NULL);
                sub_opacity = DP_channel8_to_15(
                    DP_uint32_to_uint8((params.color & 0xff000000u) >> 24u));
            }
        }
        else {
            params.blend_mode = DP_BLEND_MODE_NORMAL;
            params.color |= 0xff000000u;
        }

        params.type = (int)type;
        params.origin_x += offset_x;
        params.origin_y += offset_y;
        DP_paint_draw_dabs(dc, NULL, &params,
                           params.paint_mode == DP_PAINT_MODE_DIRECT ? tlc
                                                                     : sub_tlc);
    }

    if (sub_tlc) {
        DP_transient_layer_content_merge(tlc, 0, (DP_LayerContent *)sub_tlc,
                                         sub_opacity, DP_BLEND_MODE_NORMAL,
                                         false);
        DP_transient_layer_content_decref(sub_tlc);
    }
}

static void preview_dabs_dispose(DP_Preview *pv)
{
    DP_PreviewDabs *pvd = (DP_PreviewDabs *)pv;
    int count = pvd->count;
    for (int i = 0; i < count; ++i) {
        DP_message_decref(pvd->messages[i]);
    }
}

DP_Preview *DP_preview_new_dabs_inc(int initial_offset_x, int initial_offset_y,
                                    int layer_id, int count,
                                    DP_Message **messages)
{
    DP_ASSERT(count > 0);
    DP_ASSERT(messages);
    DP_PreviewDabs *pvd = DP_malloc(
        DP_FLEX_SIZEOF(DP_PreviewDabs, messages, DP_int_to_size(count)));
    init_preview(&pvd->parent, DP_PREVIEW_DABS,
                 preview_dabs_blend_mode(messages[0]), DP_BIT15,
                 initial_offset_x, initial_offset_y, preview_dabs_get_layer_ids,
                 preview_dabs_render, preview_dabs_dispose);
    pvd->layer_id = layer_id;
    pvd->count = count;
    for (int i = 0; i < count; ++i) {
        pvd->messages[i] = DP_message_incref(messages[i]);
    }
    return &pvd->parent;
}


typedef struct DP_PreviewFill {
    DP_Preview parent;
    int layer_id;
    int x, y, width, height;
    DP_Pixel8 pixels[];
} DP_PreviewFill;

static const int *preview_fill_get_layer_ids(DP_Preview *pv, int *out_count)
{
    DP_PreviewTransform *pvtf = (DP_PreviewTransform *)pv;
    *out_count = 1;
    return &pvtf->layer_id;
}

static void preview_fill_render(DP_Preview *pv, DP_UNUSED DP_DrawContext *dc,
                                int offset_x, int offset_y,
                                DP_TransientLayerContent *tlc)
{
    DP_PreviewFill *pvf = (DP_PreviewFill *)pv;
    int left = pvf->x + offset_x;
    int top = pvf->y + offset_y;
    DP_transient_layer_content_put_pixels(tlc, 0, DP_BLEND_MODE_REPLACE, left,
                                          top, pvf->width, pvf->height,
                                          pvf->pixels);
}

DP_Preview *DP_preview_new_fill(int initial_offset_x, int initial_offset_y,
                                int layer_id, int blend_mode, uint16_t opacity,
                                int x, int y, int width, int height,
                                const DP_Pixel8 *pixels)
{
    DP_ASSERT(width > 0);
    DP_ASSERT(height > 0);
    DP_ASSERT(pixels);
    size_t pixel_count = DP_int_to_size(width) * DP_int_to_size(height);
    DP_PreviewFill *pvf =
        DP_malloc(DP_FLEX_SIZEOF(DP_PreviewFill, pixels, pixel_count));
    init_preview(&pvf->parent, DP_PREVIEW_FILL, blend_mode, opacity,
                 initial_offset_x, initial_offset_y, preview_fill_get_layer_ids,
                 preview_fill_render, NULL);
    pvf->layer_id = layer_id;
    pvf->x = x;
    pvf->y = y;
    pvf->width = width;
    pvf->height = height;
    memcpy(pvf->pixels, pixels, sizeof(*pvf->pixels) * pixel_count);
    return &pvf->parent;
}


static DP_PreviewRenderJob shift_render_job(DP_Queue *queue)
{
    DP_PreviewRenderJob *peeked = DP_queue_peek(queue, sizeof(*peeked));
    if (peeked) {
        DP_PreviewRenderJob job = *peeked;
        DP_queue_shift(queue);
        return job;
    }
    else {
        DP_PreviewRenderJob job;
        job.type = DP_PREVIEW_RENDER_JOB_QUIT;
        return job;
    }
}
static bool handle_render_job(DP_Preview *pv, DP_DrawContext *dc,
                              int canvas_width, int canvas_height, int offset_x,
                              int offset_y)
{
    if (DP_atomic_compare_exchange(&pv->status, RENDER_QUEUED, RENDER_ACTIVE)) {
        DP_TransientLayerContent *tlc = DP_transient_layer_content_new_init(
            canvas_width, canvas_height, NULL);
        pv->render(pv, dc, pv->initial_offset_x - offset_x,
                   pv->initial_offset_y - offset_y, tlc);

        DP_LayerContent *lc = DP_atomic_ptr_xch(
            &pv->render_lc, DP_transient_layer_content_persist(tlc));
        DP_layer_content_decref_nullable(lc);

        return DP_atomic_compare_exchange(&pv->status, RENDER_ACTIVE,
                                          RENDER_DONE);
    }
    else {
        return false;
    }
}

static void run_worker_thread(void *user)
{
    DP_PreviewRenderer *pvr = user;
    DP_Mutex *queue_mutex = pvr->queue_mutex;
    DP_Semaphore *queue_sem = pvr->queue_sem;
    DP_Queue *queue = &pvr->queue;
    DP_DrawContext *dc = pvr->dc;
    DP_PreviewRenderedFn rendered = pvr->fn.rendered;
    DP_PreviewRerenderedFn rerendered = pvr->fn.rerendered;
    DP_PreviewClearFn clear = pvr->fn.clear;
    void *fn_user = pvr->fn.user;
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(queue_sem);
        DP_MUTEX_MUST_LOCK(queue_mutex);
        DP_PreviewRenderJob job = shift_render_job(queue);
        DP_MUTEX_MUST_UNLOCK(queue_mutex);
        if (job.type == DP_PREVIEW_RENDER_JOB_RENDER) {
            DP_Preview *pv = job.render.pv;
            if (handle_render_job(pv, dc, job.render.canvas_width,
                                  job.render.canvas_height, job.render.offset_x,
                                  job.render.offset_y)) {
                rendered(fn_user, pv);
            }
            DP_preview_decref(pv);
        }
        else if (job.type == DP_PREVIEW_RENDER_JOB_RERENDER) {
            DP_Preview *pv = job.render.pv;
            if (handle_render_job(pv, dc, job.render.canvas_width,
                                  job.render.canvas_height, job.render.offset_x,
                                  job.render.offset_y)) {
                rerendered(fn_user);
            }
            DP_preview_decref(pv);
        }
        else if (job.type == DP_PREVIEW_RENDER_JOB_CLEAR) {
            clear(fn_user, job.clear.type);
        }
        else if (job.type == DP_PREVIEW_RENDER_JOB_CLEAR_ALL_TRANSFORMS) {
            for (int i = 0; i < DP_PREVIEW_TRANSFORM_COUNT; ++i) {
                clear(fn_user, DP_PREVIEW_TRANSFORM_FIRST + i);
            }
        }
        else {
            DP_ASSERT(job.type == DP_PREVIEW_RENDER_JOB_QUIT);
            break;
        }
    }
}

DP_PreviewRenderer *DP_preview_renderer_new(DP_DrawContext *dc,
                                            DP_PreviewRenderedFn rendered,
                                            DP_PreviewRerenderedFn rerendered,
                                            DP_PreviewClearFn clear, void *user)
{
    DP_ASSERT(rendered);
    DP_ASSERT(clear);
    DP_PreviewRenderer *pvr = DP_malloc(sizeof(*pvr));
    *pvr = (DP_PreviewRenderer){
        {rendered, rerendered, clear, user},
        {0, NULL},
        DP_QUEUE_NULL,
        dc,
        NULL,
        NULL,
        NULL,
    };
    DP_queue_init(&pvr->queue, 8, sizeof(DP_PreviewRenderJob));
    bool ok = (pvr->queue_mutex = DP_mutex_new()) != NULL
           && (pvr->queue_sem = DP_semaphore_new(0)) != NULL
           && (pvr->thread = DP_thread_new(run_worker_thread, pvr)) != NULL;
    if (ok) {
        return pvr;
    }
    else {
        DP_preview_renderer_free(pvr);
        return NULL;
    }
}

static void dispose_preview_render_job(void *element)
{
    DP_PreviewRenderJob *job = element;
    if (job->type == DP_PREVIEW_RENDER_JOB_RENDER) {
        DP_preview_decref(job->render.pv);
    }
}

void DP_preview_renderer_free(DP_PreviewRenderer *pvr)
{
    if (pvr) {
        if (pvr->queue_mutex && pvr->queue_sem) {
            DP_MUTEX_MUST_LOCK(pvr->queue_mutex);
            DP_queue_clear(&pvr->queue, sizeof(DP_PreviewRenderJob),
                           dispose_preview_render_job);
            DP_SEMAPHORE_MUST_POST(pvr->queue_sem);
            DP_MUTEX_MUST_UNLOCK(pvr->queue_mutex);
            DP_thread_free_join(pvr->thread);
        }
        DP_semaphore_free(pvr->queue_sem);
        DP_mutex_free(pvr->queue_mutex);
        DP_queue_dispose(&pvr->queue);
        DP_free(pvr->lrse_buffer.entries);
        DP_free(pvr);
    }
}

static bool is_render_job_for_type(void *element, void *user)
{
    DP_PreviewRenderJob *job = element;
    if (job->type == DP_PREVIEW_RENDER_JOB_RENDER) {
        DP_Preview *pv = job->render.pv;
        return pv->type == *(int *)user
            && DP_atomic_get(&pv->status) != RENDER_CANCEL;
    }
    else {
        return false;
    }
}

void DP_preview_renderer_push_noinc(DP_PreviewRenderer *pvr, DP_Preview *pv,
                                    int canvas_width, int canvas_height,
                                    int offset_x, int offset_y)
{
    DP_ASSERT(pvr);
    DP_ASSERT(pv);

    DP_Queue *queue = &pvr->queue;
    DP_Mutex *queue_mutex = pvr->queue_mutex;
    DP_MUTEX_MUST_LOCK(queue_mutex);

    // Clobber an existing render job for this type if possible.
    size_t index = DP_queue_search_last_index(
        queue, sizeof(DP_PreviewRenderJob), is_render_job_for_type, &pv->type);

    DP_PreviewRenderJob *job;
    if (index == queue->used) {
        job = DP_queue_push(queue, sizeof(*job));
        DP_SEMAPHORE_MUST_POST(pvr->queue_sem);
    }
    else {
        job = DP_queue_at(queue, sizeof(*job), index);
        DP_preview_decref(job->render.pv);
    }

    *job = (DP_PreviewRenderJob){
        DP_PREVIEW_RENDER_JOB_RENDER,
        {.render = {pv, canvas_width, canvas_height, offset_x, offset_y}}};
    DP_atomic_set(&pv->status, RENDER_QUEUED);

    DP_MUTEX_MUST_UNLOCK(queue_mutex);
}

void DP_preview_renderer_push_rerender_inc(DP_PreviewRenderer *pvr,
                                           DP_Preview *pv, int canvas_width,
                                           int canvas_height, int offset_x,
                                           int offset_y)
{
    DP_ASSERT(pvr);
    DP_ASSERT(pv);

    DP_Queue *queue = &pvr->queue;
    DP_Mutex *queue_mutex = pvr->queue_mutex;
    DP_MUTEX_MUST_LOCK(queue_mutex);

    DP_PreviewRenderJob *job = DP_queue_push(queue, sizeof(*job));
    *job =
        (DP_PreviewRenderJob){DP_PREVIEW_RENDER_JOB_RERENDER,
                              {.render = {DP_preview_incref(pv), canvas_width,
                                          canvas_height, offset_x, offset_y}}};
    DP_SEMAPHORE_MUST_POST(pvr->queue_sem);
    DP_atomic_set(&pv->status, RENDER_QUEUED);

    DP_MUTEX_MUST_UNLOCK(queue_mutex);
}

static void cancel_render_job(void *element, void *user)
{
    DP_PreviewRenderJob *job = element;
    if (job->type == DP_PREVIEW_RENDER_JOB_RENDER) {
        DP_Preview *pv = job->render.pv;
        if (pv->type == *(int *)user) {
            DP_atomic_set(&pv->status, RENDER_CANCEL);
        }
    }
}

void DP_preview_renderer_cancel(DP_PreviewRenderer *pvr, int type)
{
    DP_ASSERT(pvr);
    DP_ASSERT(type >= 0);
    DP_ASSERT(type < DP_PREVIEW_COUNT);
    DP_Queue *queue = &pvr->queue;
    DP_Mutex *queue_mutex = pvr->queue_mutex;
    DP_MUTEX_MUST_LOCK(queue_mutex);
    DP_queue_each(queue, sizeof(DP_PreviewRenderJob), cancel_render_job, &type);
    DP_PreviewRenderJob *job = DP_queue_push(queue, sizeof(*job));
    *job =
        (DP_PreviewRenderJob){DP_PREVIEW_RENDER_JOB_CLEAR, {.clear = {type}}};
    DP_SEMAPHORE_MUST_POST(pvr->queue_sem);
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
}

static void cancel_all_transform_render_jobs(void *element,
                                             DP_UNUSED void *user)
{
    DP_PreviewRenderJob *job = element;
    if (job->type == DP_PREVIEW_RENDER_JOB_RENDER) {
        DP_Preview *pv = job->render.pv;
        int type = pv->type;
        if (type >= DP_PREVIEW_TRANSFORM_FIRST
            && type <= DP_PREVIEW_TRANSFORM_LAST) {
            DP_atomic_set(&pv->status, RENDER_CANCEL);
        }
    }
}

void DP_preview_renderer_cancel_all_transforms(DP_PreviewRenderer *pvr)
{
    DP_ASSERT(pvr);
    DP_Queue *queue = &pvr->queue;
    DP_Mutex *queue_mutex = pvr->queue_mutex;
    DP_MUTEX_MUST_LOCK(queue_mutex);
    DP_queue_each(queue, sizeof(DP_PreviewRenderJob),
                  cancel_all_transform_render_jobs, NULL);
    DP_PreviewRenderJob *job = DP_queue_push(queue, sizeof(*job));
    job->type = DP_PREVIEW_RENDER_JOB_CLEAR_ALL_TRANSFORMS;
    DP_SEMAPHORE_MUST_POST(pvr->queue_sem);
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
}
