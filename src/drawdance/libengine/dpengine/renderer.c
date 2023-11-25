#include "renderer.h"
#include "canvas_diff.h"
#include "canvas_state.h"
#include "layer_content.h"
#include "local_state.h"
#include "pixels.h"
#include "tile.h"
#include "view_mode.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/geom.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpmsg/blend_mode.h>

#define TILE_QUEUE_INITIAL_CAPACITY 1024

#define TILE_QUEUED_NONE 0
#define TILE_QUEUED_HIGH 1
#define TILE_QUEUED_LOW  2

#define CHANGE_NONE        0u
#define CHANGE_RESIZE      (1u << 0u)
#define CHANGE_LOCAL_STATE (1u << 1u)
#define CHANGE_UNLOCK      (1u << 2u)

typedef struct DP_RenderContext {
    DP_ALIGNAS_SIMD DP_Pixel8 pixels[DP_TILE_LENGTH];
    DP_TransientTile *tt;
    DP_ViewModeBuffer vmb;
} DP_RenderContext;

typedef struct DP_RendererLocalState {
    DP_ViewMode view_mode;
    int active;
    DP_OnionSkins *oss;
} DP_RendererLocalState;

typedef enum DP_RenderJobType {
    DP_RENDER_JOB_TILE,
    DP_RENDER_JOB_UNLOCK,
    DP_RENDER_JOB_BLOCKING,
    DP_RENDER_JOB_WAIT,
    DP_RENDER_JOB_INVALID,
    DP_RENDER_JOB_QUIT,
} DP_RenderJobType;

typedef struct DP_RendererTileCoords {
    int tile_x, tile_y;
} DP_RendererTileCoords;

typedef struct DP_RendererTileJob {
    int tile_x, tile_y;
    int tile_index;
    DP_CanvasState *cs;
    bool needs_checkers;
} DP_RendererTileJob;

typedef struct DP_RendererResize {
    int width, height;
    int prev_width, prev_height;
    int offset_x, offset_y;
} DP_RendererResize;

typedef struct DP_RendererBlocking {
    unsigned int changes;
    DP_RendererResize resize;
    DP_RendererLocalState local_state;
} DP_RendererBlocking;

typedef struct DP_RenderJob {
    DP_RenderJobType type;
    union {
        DP_RendererTileJob tile;
        DP_RendererBlocking blocking;
    };
} DP_RenderJob;

struct DP_Renderer {
    struct {
        DP_RendererTileFn tile;
        DP_RendererUnlockFn unlock;
        DP_RendererResizeFn resize;
        void *user;
    } fn;
    DP_RenderContext *contexts;
    DP_Queue blocking_queue;
    struct {
        DP_Queue queue_high;
        DP_Queue queue_low;
        size_t map_capacity;
        char *map;
    } tile;
    DP_Tile *checker;
    DP_CanvasState *cs;
    bool needs_checkers;
    int xtiles;
    DP_RendererLocalState local_state;
    DP_Mutex *queue_mutex;
    DP_Semaphore *queue_sem;
    DP_Semaphore *wait_ready_sem;
    DP_Semaphore *wait_done_sem;
    int thread_count;
    DP_Thread *threads[];
};


static void handle_tile_job(DP_Renderer *renderer, DP_RenderContext *rc,
                            DP_RendererTileJob *job)
{
    DP_TransientTile *tt = rc->tt;
    DP_CanvasState *cs = job->cs;
    DP_Tile *background_tile = DP_canvas_state_background_tile_noinc(cs);
    if (background_tile) {
        DP_transient_tile_copy(tt, background_tile);
    }
    else {
        DP_transient_tile_clear(tt);
    }

    DP_ViewModeFilter vmf = DP_view_mode_filter_make_from_active(
        &rc->vmb, renderer->local_state.view_mode, cs,
        renderer->local_state.active, renderer->local_state.oss);

    DP_canvas_state_flatten_tile_to(cs, job->tile_index, tt, true, &vmf);

    if (job->needs_checkers) {
        DP_transient_tile_merge(tt, renderer->checker, DP_BIT15,
                                DP_BLEND_MODE_BEHIND);
    }

    DP_Pixel8 *pixel_buffer = rc->pixels;
    DP_pixels15_to_8_tile(pixel_buffer, DP_transient_tile_pixels(tt));
    renderer->fn.tile(renderer->fn.user, job->tile_x, job->tile_y,
                      pixel_buffer);

    DP_canvas_state_decref(cs);
}


static void handle_blocking_job(DP_Renderer *renderer, DP_Mutex *queue_mutex,
                                DP_RendererBlocking *job)
{
    // Wait for all threads to be in a safe state before proceeding.
    int wait_thread_count = renderer->thread_count - 1;
    DP_SEMAPHORE_MUST_WAIT_N(renderer->wait_ready_sem, wait_thread_count);
    DP_MUTEX_MUST_LOCK(queue_mutex);

    unsigned int changes = job->changes;
    if (changes & CHANGE_RESIZE) {
        renderer->fn.resize(renderer->fn.user, job->resize.width,
                            job->resize.height, job->resize.prev_width,
                            job->resize.prev_height, job->resize.offset_x,
                            job->resize.offset_y);
    }

    if (changes & CHANGE_LOCAL_STATE) {
        DP_onion_skins_free(renderer->local_state.oss);
        renderer->local_state = job->local_state;
    }

    if (changes & CHANGE_UNLOCK) {
        renderer->fn.unlock(renderer->fn.user);
    }

    // Unlock the other threads.
    DP_SEMAPHORE_MUST_POST_N(renderer->wait_done_sem, wait_thread_count);
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
}


static void handle_wait_job(DP_Renderer *renderer)
{
    DP_SEMAPHORE_MUST_POST(renderer->wait_ready_sem);
    DP_SEMAPHORE_MUST_WAIT(renderer->wait_done_sem);
}


static bool dequeue_job_resize(DP_Renderer *renderer, DP_RenderJob *out_job)
{
    DP_Queue *blocking_queue = &renderer->blocking_queue;
    DP_RenderJob *blocking_job =
        DP_queue_peek(blocking_queue, sizeof(*blocking_job));
    if (blocking_job) {
        *out_job = *blocking_job;
        DP_queue_shift(blocking_queue);
        return true;
    }
    else {
        return false;
    }
}

static int enqueue_blocking_job(DP_Renderer *renderer,
                                DP_RendererBlocking *blocking)
{
    DP_Queue *blocking_queue = &renderer->blocking_queue;

    // We have to synchronize all render threads to perform these blocking
    // operations, so we enqueue the actual blocking job for one thread and
    // waiting jobs for the rest, providing a barrier of no activity.
    int thread_count = renderer->thread_count;
    int wait_thread_count = thread_count - 1;
    for (int i = 0; i < wait_thread_count; ++i) {
        DP_RenderJob *wait_job =
            DP_queue_push(blocking_queue, sizeof(*wait_job));
        wait_job->type = DP_RENDER_JOB_WAIT;
    }

    DP_RenderJob *blocking_job =
        DP_queue_push(blocking_queue, sizeof(*blocking_job));
    blocking_job->type = DP_RENDER_JOB_BLOCKING;
    blocking_job->blocking = *blocking;

    return thread_count;
}

static bool dequeue_job_tile(DP_Renderer *renderer, DP_Queue *queue,
                             DP_RenderJob *out_job)
{
    DP_RendererTileCoords *coords = DP_queue_peek(queue, sizeof(*coords));
    if (coords) {
        int tile_x = coords->tile_x;
        int tile_y = coords->tile_y;
        if (tile_x >= 0) {
            int tile_index = tile_y * renderer->xtiles + tile_x;
            renderer->tile.map[tile_index] = TILE_QUEUED_NONE;
            out_job->type = DP_RENDER_JOB_TILE;
            out_job->tile = (DP_RendererTileJob){
                tile_x, tile_y, tile_index,
                DP_canvas_state_incref(renderer->cs), renderer->needs_checkers};
        }
        else {
            if (tile_y == DP_RENDER_JOB_UNLOCK) {
                DP_RendererBlocking blocking;
                blocking.changes = CHANGE_UNLOCK;
                int pushed = enqueue_blocking_job(renderer, &blocking);
                DP_SEMAPHORE_MUST_POST_N(renderer->queue_sem, pushed);
            }
            out_job->type = DP_RENDER_JOB_INVALID;
        }
        DP_queue_shift(queue);
        return true;
    }
    else {
        return false;
    }
}

static DP_RenderJob dequeue_job(DP_Renderer *renderer)
{
    DP_RenderJob job;
    bool job_found =
        dequeue_job_resize(renderer, &job)
        || dequeue_job_tile(renderer, &renderer->tile.queue_high, &job)
        || dequeue_job_tile(renderer, &renderer->tile.queue_low, &job);
    if (!job_found) {
        job.type = DP_RENDER_JOB_QUIT;
    }
    return job;
}

static void handle_jobs(DP_Renderer *renderer, int thread_index)
{
    DP_Mutex *queue_mutex = renderer->queue_mutex;
    DP_Semaphore *queue_sem = renderer->queue_sem;
    DP_RenderContext *rc = &renderer->contexts[thread_index];
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(queue_sem);
        DP_MUTEX_MUST_LOCK(queue_mutex);
        DP_RenderJob job = dequeue_job(renderer);
        DP_MUTEX_MUST_UNLOCK(queue_mutex);
        switch (job.type) {
        case DP_RENDER_JOB_TILE:
            handle_tile_job(renderer, rc, &job.tile);
            break;
        case DP_RENDER_JOB_BLOCKING:
            handle_blocking_job(renderer, queue_mutex, &job.blocking);
            break;
        case DP_RENDER_JOB_WAIT:
            handle_wait_job(renderer);
            break;
        case DP_RENDER_JOB_INVALID:
            break;
        case DP_RENDER_JOB_QUIT:
            return;
        default:
            DP_UNREACHABLE();
        }
    }
}

struct DP_RenderWorkerParams {
    DP_Renderer *renderer;
    int thread_index;
};

static void run_worker_thread(void *user)
{
    struct DP_RenderWorkerParams *params = user;
    DP_Renderer *renderer = params->renderer;
    int thread_index = params->thread_index;
    DP_free(params);
    handle_jobs(renderer, thread_index);
}


DP_Renderer *DP_renderer_new(int thread_count, DP_RendererTileFn tile_fn,
                             DP_RendererUnlockFn unlock_fn,
                             DP_RendererResizeFn resize_fn, void *user)
{
    DP_ASSERT(thread_count > 0);
    DP_ASSERT(tile_fn);
    DP_ASSERT(unlock_fn);
    DP_ASSERT(resize_fn);

    size_t size_thread_count = DP_int_to_size(thread_count);
    DP_Renderer *renderer =
        DP_malloc(DP_FLEX_SIZEOF(DP_Renderer, threads, size_thread_count));
    renderer->fn.tile = tile_fn;
    renderer->fn.unlock = unlock_fn;
    renderer->fn.resize = resize_fn;
    renderer->fn.user = user;
    renderer->thread_count = thread_count;
    renderer->queue_mutex = NULL;
    renderer->queue_sem = NULL;
    renderer->wait_ready_sem = NULL;
    renderer->wait_done_sem = NULL;
    DP_queue_init(&renderer->blocking_queue, size_thread_count * 2,
                  sizeof(DP_RenderJob));
    DP_queue_init(&renderer->tile.queue_high, TILE_QUEUE_INITIAL_CAPACITY,
                  sizeof(DP_RendererTileCoords));
    DP_queue_init(&renderer->tile.queue_low, TILE_QUEUE_INITIAL_CAPACITY,
                  sizeof(DP_RendererTileCoords));
    renderer->tile.map_capacity = 0;
    renderer->tile.map = NULL;
    renderer->checker = DP_tile_new_checker(
        0, (DP_Pixel15){DP_BIT15 / 2, DP_BIT15 / 2, DP_BIT15 / 2, DP_BIT15},
        (DP_Pixel15){DP_BIT15, DP_BIT15, DP_BIT15, DP_BIT15});
    renderer->cs = DP_canvas_state_new();
    renderer->needs_checkers = true;
    renderer->xtiles = 0;
    renderer->local_state =
        (DP_RendererLocalState){DP_VIEW_MODE_NORMAL, 0, NULL};
    renderer->contexts = DP_malloc_simd(sizeof(*renderer->contexts)
                                        * DP_int_to_size(thread_count));
    for (int i = 0; i < thread_count; ++i) {
        renderer->contexts[i].tt = DP_transient_tile_new_blank(0);
        DP_view_mode_buffer_init(&renderer->contexts[i].vmb);
        renderer->threads[i] = NULL;
    }

    bool ok = (renderer->queue_mutex = DP_mutex_new()) != NULL
           && (renderer->queue_sem = DP_semaphore_new(0)) != NULL
           && (renderer->wait_ready_sem = DP_semaphore_new(0)) != NULL
           && (renderer->wait_done_sem = DP_semaphore_new(0)) != NULL;
    if (!ok) {
        DP_renderer_free(renderer);
        return NULL;
    }

    for (int i = 0; i < thread_count; ++i) {
        struct DP_RenderWorkerParams *params = DP_malloc(sizeof(*params));
        *params = (struct DP_RenderWorkerParams){renderer, i};
        renderer->threads[i] = DP_thread_new(run_worker_thread, params);
        if (!renderer->threads[i]) {
            DP_free(params);
            DP_renderer_free(renderer);
            return NULL;
        }
    }

    return renderer;
}

static void dispose_blocking_job(void *element)
{
    DP_RenderJob *job = element;
    if (job->type == DP_RENDER_JOB_BLOCKING
        && (job->blocking.changes & CHANGE_LOCAL_STATE)) {
        DP_onion_skins_free(job->blocking.local_state.oss);
    }
}

void DP_renderer_free(DP_Renderer *renderer)
{
    if (renderer) {
        int thread_count = renderer->thread_count;
        if (renderer->queue_mutex && renderer->queue_sem) {
            DP_MUTEX_MUST_LOCK(renderer->queue_mutex);
            DP_queue_clear(&renderer->blocking_queue, sizeof(DP_RenderJob),
                           dispose_blocking_job);
            renderer->tile.queue_high.used = 0;
            renderer->tile.queue_low.used = 0;
            DP_SEMAPHORE_MUST_POST_N(renderer->queue_sem, thread_count);
            DP_MUTEX_MUST_UNLOCK(renderer->queue_mutex);
            for (int i = 0; i < thread_count; ++i) {
                DP_thread_free_join(renderer->threads[i]);
            }
        }
        DP_semaphore_free(renderer->wait_done_sem);
        DP_semaphore_free(renderer->wait_ready_sem);
        DP_semaphore_free(renderer->queue_sem);
        DP_mutex_free(renderer->queue_mutex);
        DP_onion_skins_free(renderer->local_state.oss);
        DP_canvas_state_decref(renderer->cs);
        DP_tile_decref(renderer->checker);
        DP_free(renderer->tile.map);
        DP_queue_dispose(&renderer->tile.queue_low);
        DP_queue_dispose(&renderer->tile.queue_high);
        DP_queue_dispose(&renderer->blocking_queue);
        for (int i = 0; i < thread_count; ++i) {
            DP_view_mode_buffer_dispose(&renderer->contexts[i].vmb);
            DP_transient_tile_decref(renderer->contexts[i].tt);
        }
        DP_free_simd(renderer->contexts);
        DP_free(renderer);
    }
}

int DP_renderer_thread_count(DP_Renderer *renderer)
{
    DP_ASSERT(renderer);
    return renderer->thread_count;
}


static bool local_state_params_differ(DP_RendererLocalState *rls,
                                      DP_LocalState *ls)
{
    switch (rls->view_mode) {
    case DP_VIEW_MODE_NORMAL:
        return false;
    case DP_VIEW_MODE_LAYER:
        return rls->active != DP_local_state_active_layer_id(ls);
    case DP_VIEW_MODE_FRAME:
        return rls->active != DP_local_state_active_frame_index(ls)
            || !DP_onion_skins_equal(rls->oss, DP_local_state_onion_skins(ls));
    }
    DP_UNREACHABLE();
}

static bool local_state_differs(DP_RendererLocalState *rls, DP_LocalState *ls)
{
    return rls->view_mode != DP_local_state_view_mode(ls)
        || local_state_params_differ(rls, ls);
}

static DP_RendererLocalState clone_local_state(DP_LocalState *ls)
{
    switch (DP_local_state_view_mode(ls)) {
    case DP_VIEW_MODE_NORMAL:
        return (DP_RendererLocalState){DP_VIEW_MODE_NORMAL, 0, NULL};
    case DP_VIEW_MODE_LAYER:
        return (DP_RendererLocalState){
            DP_VIEW_MODE_LAYER, DP_local_state_active_layer_id(ls), NULL};
    case DP_VIEW_MODE_FRAME:
        return (DP_RendererLocalState){
            DP_VIEW_MODE_FRAME, DP_local_state_active_frame_index(ls),
            DP_onion_skins_new_clone_nullable(DP_local_state_onion_skins(ls))};
    }
    DP_UNREACHABLE();
}


static void invalidate_tile_coords(void *element, DP_UNUSED void *user)
{
    DP_RendererTileCoords *coords = element;
    coords->tile_x = -1;
    coords->tile_y = DP_RENDER_JOB_INVALID;
}

static int push_blocking(DP_Renderer *renderer, DP_RendererBlocking *blocking)
{
    int pushed = enqueue_blocking_job(renderer, blocking);

    // All current tile jobs are invalidated by the blocking change, so we turn
    // those into tombstones to avoid any pointless processing thereof.
    DP_queue_each(&renderer->tile.queue_high, sizeof(DP_RendererTileCoords),
                  invalidate_tile_coords, NULL);
    DP_queue_each(&renderer->tile.queue_low, sizeof(DP_RendererTileCoords),
                  invalidate_tile_coords, NULL);

    // If there was a resize, update the tile priority lookup map and horizontal
    // tile stride for any upcoming tiles to be added to the queues.
    int width = blocking->resize.width;
    int height = blocking->resize.height;
    size_t required_capacity = DP_int_to_size(width) * DP_int_to_size(height);
    if (blocking->changes & CHANGE_RESIZE) {
        if (renderer->tile.map_capacity < required_capacity) {
            renderer->tile.map =
                DP_realloc(renderer->tile.map, required_capacity);
            renderer->tile.map_capacity = required_capacity;
        }

        renderer->xtiles = DP_tile_count_round(width);
    }
    memset(renderer->tile.map, TILE_QUEUED_NONE, required_capacity);

    return pushed;
}


static void enqueue_tile(DP_Queue *queue, char *map, int tile_x, int tile_y,
                         int tile_index, char priority)
{
    DP_RendererTileCoords *coords = DP_queue_push(queue, sizeof(*coords));
    *coords = (DP_RendererTileCoords){tile_x, tile_y};
    map[tile_index] = priority;
}

static bool should_remove_tile(void *element, void *user)
{
    DP_RendererTileCoords *a = element;
    DP_RendererTileCoords *b = user;
    return a->tile_x == b->tile_x && a->tile_y == b->tile_y;
}

static void remove_tile(DP_Queue *queue, int tile_x, int tile_y)
{
    DP_RendererTileCoords coords = {tile_x, tile_y};
    size_t index = DP_queue_search_index(queue, sizeof(DP_RendererTileCoords),
                                         should_remove_tile, &coords);
    DP_ASSERT(index < queue->used);
    // We don't actually care too much about the order in our queue, so instead
    // of going through the effort of removing an element in the middle, we just
    // replace it with the last element. If it's at the beginning or end, we can
    // just shift or pop it respectively though, which is even easier.
    if (index == 0) {
        DP_queue_shift(queue);
    }
    else if (index == queue->used - 1) {
        DP_queue_pop(queue);
    }
    else {
        DP_RendererTileCoords *last =
            DP_queue_peek_last(queue, sizeof(DP_RendererTileCoords));
        DP_RendererTileCoords *target =
            DP_queue_at(queue, sizeof(DP_RendererTileCoords), index);
        *target = *last;
        DP_queue_pop(queue);
    }
}

static void upgrade_tile_priority(DP_Renderer *renderer, int tile_x, int tile_y,
                                  int tile_index)
{
    enqueue_tile(&renderer->tile.queue_high, renderer->tile.map, tile_x, tile_y,
                 tile_index, TILE_QUEUED_HIGH);
    remove_tile(&renderer->tile.queue_low, tile_x, tile_y);
}

static void push_tile_high_priority(DP_Renderer *renderer, int tile_x,
                                    int tile_y, int *out_pushed)
{
    int tile_index = tile_y * renderer->xtiles + tile_x;
    char status = renderer->tile.map[tile_index];
    if (status == TILE_QUEUED_NONE) {
        enqueue_tile(&renderer->tile.queue_high, renderer->tile.map, tile_x,
                     tile_y, tile_index, TILE_QUEUED_HIGH);
        ++*out_pushed;
    }
    else if (status == TILE_QUEUED_LOW) {
        upgrade_tile_priority(renderer, tile_x, tile_y, tile_index);
    }
}

struct DP_RendererPushTileParams {
    DP_Renderer *renderer;
    DP_Rect view_tile_bounds;
    int pushed;
};

static void push_tile(void *user, int tile_x, int tile_y)
{
    struct DP_RendererPushTileParams *params = user;
    DP_Renderer *renderer = params->renderer;
    if (DP_rect_contains(params->view_tile_bounds, tile_x, tile_y)) {
        push_tile_high_priority(renderer, tile_x, tile_y, &params->pushed);
    }
    else {
        int tile_index = tile_y * renderer->xtiles + tile_x;
        char status = renderer->tile.map[tile_index];
        if (status == TILE_QUEUED_NONE) {
            enqueue_tile(&renderer->tile.queue_low, renderer->tile.map, tile_x,
                         tile_y, tile_index, TILE_QUEUED_LOW);
            ++params->pushed;
        }
    }
}

struct DP_RendererPushTileInViewParams {
    DP_Renderer *renderer;
    int pushed;
};

static void push_tile_in_view(void *user, int tile_x, int tile_y)
{
    struct DP_RendererPushTileInViewParams *params = user;
    DP_Renderer *renderer = params->renderer;
    push_tile_high_priority(renderer, tile_x, tile_y, &params->pushed);
}

static void reprioritize_tiles(DP_Renderer *renderer, DP_CanvasDiff *diff,
                               DP_Rect tile_bounds)
{
    int left, top, right, bottom, xtiles;
    DP_canvas_diff_bounds_clamp(diff, tile_bounds.x1, tile_bounds.y1,
                                tile_bounds.x2, tile_bounds.y2, &left, &top,
                                &right, &bottom, &xtiles);
    char *tile_map = renderer->tile.map;
    for (int tile_y = top; tile_y <= bottom; ++tile_y) {
        for (int tile_x = left; tile_x <= right; ++tile_x) {
            int tile_index = tile_y * xtiles + tile_x;
            if (tile_map[tile_index] == TILE_QUEUED_LOW) {
                upgrade_tile_priority(renderer, tile_x, tile_y, tile_index);
            }
        }
    }
}

void DP_renderer_apply(DP_Renderer *renderer, DP_CanvasState *cs,
                       DP_LocalState *ls, DP_CanvasDiff *diff,
                       bool layers_can_decrease_opacity,
                       DP_Rect view_tile_bounds, bool render_outside_view,
                       DP_RendererMode mode)
{
    DP_ASSERT(renderer);
    DP_ASSERT(cs);
    DP_ASSERT(diff);

    DP_CanvasState *prev_cs = renderer->cs;
    int prev_width = DP_canvas_state_width(prev_cs);
    int prev_height = DP_canvas_state_height(prev_cs);
    int width = DP_canvas_state_width(cs);
    int height = DP_canvas_state_height(cs);

    DP_Mutex *queue_mutex = renderer->queue_mutex;
    DP_MUTEX_MUST_LOCK(queue_mutex);

    renderer->cs = DP_canvas_state_incref(cs);
    // It's very rare in practice for the checkerboard background to actually be
    // visible behind the canvas. Only if the canvas background is set to a
    // non-opaque value or there's weird blend modes like Erase at top-level.
    renderer->needs_checkers =
        !DP_canvas_state_background_opaque(cs) || layers_can_decrease_opacity;

    DP_RendererBlocking blocking;
    blocking.changes = CHANGE_NONE;

    if (width != prev_width || height != prev_height) {
        blocking.changes |= CHANGE_RESIZE;
    }
    blocking.resize = (DP_RendererResize){
        width,
        height,
        prev_width,
        prev_height,
        DP_canvas_state_offset_x(prev_cs) - DP_canvas_state_offset_x(cs),
        DP_canvas_state_offset_y(prev_cs) - DP_canvas_state_offset_y(cs),
    };

    if (local_state_differs(&renderer->local_state, ls)) {
        blocking.changes |= CHANGE_LOCAL_STATE;
        blocking.local_state = clone_local_state(ls);
    }

    int pushed = 0;
    if (blocking.changes) {
        pushed += push_blocking(renderer, &blocking);
    }

    DP_canvas_state_decref(prev_cs);

    DP_Queue *tile_queue_high = &renderer->tile.queue_high;
    size_t tile_queue_high_used_before = tile_queue_high->used;
    if (render_outside_view) {
        struct DP_RendererPushTileParams params = {renderer, view_tile_bounds,
                                                   0};
        DP_canvas_diff_each_pos_reset(diff, push_tile, &params);
        pushed += params.pushed;
    }
    else {
        struct DP_RendererPushTileInViewParams params = {renderer, 0};
        DP_canvas_diff_each_pos_tile_bounds_reset(
            diff, view_tile_bounds.x1, view_tile_bounds.y1, view_tile_bounds.x2,
            view_tile_bounds.y2, push_tile_in_view, &params);
        pushed += params.pushed;
    }

    if (mode != DP_RENDERER_CONTINUOUS) {
        reprioritize_tiles(renderer, diff, view_tile_bounds);
        // Block the main thread if there's new high-priority tiles to render.
        // This avoids tiles flickering in when the user moves the view
        // elsewhere at the expense of possibly chugging for a moment. But since
        // the user is currently manipulating the view, they're not doing
        // anything else important, so a bit of chug feels better than tiles
        // stumbling into view, which may be miscronstrued as them "glitching".
        if (mode != DP_RENDERER_EVERYTHING
            && tile_queue_high->used == tile_queue_high_used_before) {
            renderer->fn.unlock(renderer->fn.user);
        }
        else {
            DP_RendererTileCoords *coords =
                DP_queue_push(tile_queue_high, sizeof(*coords));
            *coords = (DP_RendererTileCoords){-1, DP_RENDER_JOB_UNLOCK};
            ++pushed;
        }
    }

    DP_SEMAPHORE_MUST_POST_N(renderer->queue_sem, pushed);
    DP_MUTEX_MUST_UNLOCK(queue_mutex);
}
