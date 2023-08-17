// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_PREVIEW_H
#define DPENGINE_PREVIEW_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Message DP_Message;
typedef struct DP_Quad DP_Quad;
typedef union DP_Pixel8 DP_Pixel8;


#define DP_PREVIEW_BASE_SUBLAYER_ID  (-100)
#define DP_PREVIEW_SUBLAYER_ID(TYPE) (DP_PREVIEW_BASE_SUBLAYER_ID - (int)(TYPE))

typedef enum DP_PreviewType {
    DP_PREVIEW_CUT,
    DP_PREVIEW_TRANSFORM,
    DP_PREVIEW_DABS,
    DP_PREVIEW_COUNT,
} DP_PreviewType;

typedef struct DP_Preview DP_Preview;

typedef const DP_Pixel8 *(*DP_PreviewTransformGetPixelsFn)(void *user);
typedef void (*DP_PreviewTransformDisposePixelsFn)(void *user);

typedef struct DP_PreviewRenderer DP_PreviewRenderer;
typedef void (*DP_PreviewRenderedFn)(void *user, DP_Preview *pv);
typedef void (*DP_PreviewRerenderedFn)(void *user);
typedef void (*DP_PreviewClearFn)(void *user, DP_PreviewType type);


extern DP_Preview DP_preview_null;

DP_Preview *DP_preview_incref(DP_Preview *pv);

DP_Preview *DP_preview_incref_nullable(DP_Preview *pv_or_null);

void DP_preview_decref(DP_Preview *pv);

void DP_preview_decref_nullable(DP_Preview *pv_or_null);

int DP_preview_refcount(DP_Preview *pv);

DP_PreviewType DP_preview_type(DP_Preview *pv);

DP_CanvasState *DP_preview_apply(DP_Preview *pv, DP_CanvasState *cs,
                                 DP_PreviewRenderer *pvr);

void DP_preview_render_reset(DP_Preview *pv, DP_PreviewRenderer *pvr,
                             int canvas_width, int canvas_height, int offset_x,
                             int offset_y);


DP_Preview *DP_preview_new_cut(int initial_offset_x, int initial_offset_y,
                               int layer_id, int x, int y, int width,
                               int height, const DP_Pixel8 *mask_or_null);

DP_Preview *DP_preview_new_transform(
    int initial_offset_x, int initial_offset_y, int layer_id, int x, int y,
    int width, int height, const DP_Quad *dst_quad, int interpolation,
    DP_PreviewTransformGetPixelsFn get_pixels,
    DP_PreviewTransformDisposePixelsFn dispose_pixels, void *user);

DP_Preview *DP_preview_new_dabs_inc(int initial_offset_x, int initial_offset_y,
                                    int layer_id, int count,
                                    DP_Message **messages);


DP_PreviewRenderer *DP_preview_renderer_new(DP_DrawContext *dc,
                                            DP_PreviewRenderedFn rendered,
                                            DP_PreviewRerenderedFn rerendered,
                                            DP_PreviewClearFn clear,
                                            void *user);

void DP_preview_renderer_free(DP_PreviewRenderer *pvr);

void DP_preview_renderer_push_noinc(DP_PreviewRenderer *pvr, DP_Preview *pv,
                                    int canvas_width, int canvas_height,
                                    int offset_x, int offset_y);

void DP_preview_renderer_push_rerender_inc(DP_PreviewRenderer *pvr,
                                           DP_Preview *pv, int canvas_width,
                                           int canvas_height, int offset_x,
                                           int offset_y);

void DP_preview_renderer_cancel(DP_PreviewRenderer *pvr, DP_PreviewType type);


#endif
