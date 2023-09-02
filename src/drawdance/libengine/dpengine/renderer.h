// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_RENDERER_H
#define DPENGINE_RENDERER_H
#include <dpcommon/common.h>
#include <dpcommon/geom.h>

typedef struct DP_CanvasDiff DP_CanvasDiff;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_LocalState DP_LocalState;
typedef union DP_Pixel8 DP_Pixel8;


typedef struct DP_Renderer DP_Renderer;
typedef void (*DP_RendererTileFn)(void *user, int x, int y, DP_Pixel8 *pixels);
typedef void (*DP_RendererUnlockFn)(void *user);
typedef void (*DP_RendererResizeFn)(void *user, int width, int height,
                                    int prev_width, int prev_height,
                                    int offset_x, int offset_y);

typedef enum DP_RendererMode {
    // Normal, asynchronous rendering. Don't call the unlock function.
    DP_RENDERER_CONTINUOUS,
    // Render after a change of view bounds. Calls the unlock function on the
    // current thread if no new tiles have been added by the change, otherwise
    // calls it on a render thread after the render queue has drained.
    DP_RENDERER_VIEW_BOUNDS_CHANGED,
    // Render the whole canvas. Calls the unlock function on a render thread
    // after the render queue has drained, even if nothing changed on it.
    DP_RENDERER_EVERYTHING,
} DP_RendererMode;

DP_Renderer *DP_renderer_new(int thread_count, DP_RendererTileFn tile_fn,
                             DP_RendererUnlockFn unlock_fn,
                             DP_RendererResizeFn resize_fn, void *user);

void DP_renderer_free(DP_Renderer *renderer);

int DP_renderer_thread_count(DP_Renderer *renderer);

// Increments refcount on the given canvas state, resets the given diff.
void DP_renderer_apply(DP_Renderer *renderer, DP_CanvasState *cs,
                       DP_LocalState *ls, DP_CanvasDiff *diff,
                       bool layers_can_decrease_opacity,
                       DP_Rect view_tile_bounds, bool render_outside_view,
                       DP_RendererMode mode);

#endif
