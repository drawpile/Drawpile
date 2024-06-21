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
#include "paint_engine.h"
#include "brush.h"
#include "canvas_diff.h"
#include "canvas_history.h"
#include "canvas_state.h"
#include "dab_cost.h"
#include "draw_context.h"
#include "image.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "layer_routes.h"
#include "local_state.h"
#include "paint.h"
#include "player.h"
#include "preview.h"
#include "recorder.h"
#include "renderer.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include "view_mode.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/cpu.h>
#include <dpcommon/file.h>
#include <dpcommon/geom.h>
#include <dpcommon/output.h>
#include <dpcommon/perf.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpcommon/vector.h>
#include <dpcommon/worker.h>
#include <dpmsg/acl.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>
#include <dpmsg/message_queue.h>
#include <dpmsg/msg_internal.h>
#include <ctype.h>
#include <limits.h>

#define DP_PERF_CONTEXT "paint_engine"


#define INITIAL_QUEUE_CAPACITY 64

#define INSPECT_SUBLAYER_ID -200

#define RECORDER_UNCHANGED 0
#define RECORDER_STARTED   1
#define RECORDER_STOPPED   2

#define PLAYBACK_STEP_MESSAGES    0
#define PLAYBACK_STEP_UNDO_POINTS 1
#define PLAYBACK_STEP_MSECS       2

#define NO_PUSH               0
#define PUSH_MESSAGE          1
#define PUSH_CLEAR_LOCAL_FORK 2

typedef struct DP_PaintEngineCursorChange {
    DP_MessageType type;
    unsigned int context_id;
    union {
        struct {
            int persistence;
            uint32_t color;
        } laser;
        struct {
            int x;
            int y;
        } move;
    };
} DP_PaintEngineCursorChange;

struct DP_PaintEngine {
    DP_AclState *acls;
    DP_CanvasHistory *ch;
    struct {
        DP_CanvasHistorySoftResetFn fn;
        void *user;
    } soft_reset;
    DP_CanvasDiff *diff;
    DP_CanvasState *history_cs;
    DP_CanvasState *view_cs;
    DP_LocalState *local_state;
    struct {
        bool reveal_censored;
        bool inspect_show_tiles;
        bool layers_can_decrease_opacity;
        unsigned int inspect_context_id;
        DP_Pixel8 checker_color1;
        DP_Pixel8 checker_color2;
        struct {
            DP_LayerPropsList *prev_lpl;
            DP_Timeline *prev_tl;
            DP_LayerPropsList *lpl;
        } layers;
        struct {
            DP_Timeline *prev_tl;
            DP_Timeline *tl;
        } tracks;
    } local_view;
    DP_DrawContext *paint_dc;
    DP_DrawContext *main_dc;
    DP_Preview *previews[DP_PREVIEW_COUNT];
    DP_AtomicPtr next_previews[DP_PREVIEW_COUNT];
    DP_Atomic preview_rerendered;
    DP_PreviewRenderer *preview_renderer;
    DP_Queue local_queue;
    DP_Queue remote_queue;
    DP_Semaphore *queue_sem;
    DP_Mutex *queue_mutex;
    DP_Atomic running;
    DP_Atomic catchup;
    DP_Atomic default_layer_id;
    DP_Atomic undo_depth_limit;
    DP_Atomic just_reset;
    bool catching_up;
    bool reset_locked;
    DP_Thread *paint_thread;
    DP_Renderer *renderer;
    struct {
        uint8_t acl_change_flags;
        DP_Vector cursor_changes;
        DP_UserCursorBuffer ucb;
    } meta;
    struct {
        char *path;
        DP_Recorder *recorder;
        DP_Semaphore *start_sem;
        int state_change;
        DP_RecorderGetTimeMsFn get_time_ms_fn;
        void *get_time_ms_user;
    } record;
    struct {
        DP_Player *player;
        long long msecs;
        bool next_has_time;
        DP_PaintEnginePlaybackFn fn;
        DP_PaintEngineDumpPlaybackFn dump_fn;
        void *user;
    } playback;
};


static void push_cleanup_message(void *user, DP_Message *msg)
{
    DP_PaintEngine *pe = user;
    DP_message_queue_push_noinc(&pe->remote_queue, msg);
    DP_SEMAPHORE_MUST_POST(pe->queue_sem);
}

static void free_preview(DP_Preview *pv)
{
    if (pv && pv != &DP_preview_null) {
        DP_preview_decref(pv);
    }
}

static void decref_messages(int count, DP_Message **msgs)
{
    for (int i = 0; i < count; ++i) {
        DP_message_decref(msgs[i]);
    }
}

static void drop_message(DP_UNUSED void *user, DP_Message *msg)
{
    DP_message_decref(msg);
}

static void update_undo_depth_limit(DP_PaintEngine *pe)
{
    DP_atomic_xch(&pe->undo_depth_limit,
                  DP_canvas_history_undo_depth_limit(pe->ch));
}

static void handle_dump_command(DP_PaintEngine *pe, DP_MsgInternal *mi)
{
    DP_CanvasHistory *ch = pe->ch;
    DP_DrawContext *dc = pe->paint_dc;
    int count;
    DP_Message **msgs = DP_msg_internal_dump_command_messages(mi, &count);
    int type = DP_msg_internal_dump_command_type(mi);
    switch (type) {
    case DP_DUMP_REMOTE_MESSAGE:
        DP_canvas_history_local_drawing_in_progress_set(ch, false);
        if (!DP_canvas_history_handle(ch, dc, msgs[0])) {
            DP_warn("Error handling remote dump message: %s", DP_error());
        }
        decref_messages(count, msgs);
        break;
    case DP_DUMP_REMOTE_MESSAGE_LOCAL_DRAWING_IN_PROGRESS:
        DP_canvas_history_local_drawing_in_progress_set(ch, true);
        if (!DP_canvas_history_handle(ch, dc, msgs[0])) {
            DP_warn("Error handling remote dump message with local drawing in "
                    "progress: %s",
                    DP_error());
        }
        decref_messages(count, msgs);
        break;
    case DP_DUMP_LOCAL_MESSAGE:
        if (!DP_canvas_history_handle_local(ch, dc, msgs[0])) {
            DP_warn("Error handling local dump message: %s", DP_error());
        }
        decref_messages(count, msgs);
        break;
    case DP_DUMP_REMOTE_MULTIDAB:
        DP_canvas_history_local_drawing_in_progress_set(ch, false);
        DP_canvas_history_handle_multidab_dec(ch, dc, count, msgs);
        break;
    case DP_DUMP_REMOTE_MULTIDAB_LOCAL_DRAWING_IN_PROGRESS:
        DP_canvas_history_local_drawing_in_progress_set(ch, true);
        DP_canvas_history_handle_multidab_dec(ch, dc, count, msgs);
        break;
    case DP_DUMP_LOCAL_MULTIDAB:
        DP_canvas_history_handle_local_multidab_dec(ch, dc, count, msgs);
        break;
    case DP_DUMP_RESET:
        decref_messages(count, msgs);
        DP_canvas_history_reset(ch);
        update_undo_depth_limit(pe);
        break;
    case DP_DUMP_SOFT_RESET:
        decref_messages(count, msgs);
        DP_canvas_history_soft_reset(ch, dc, 0, pe->soft_reset.fn,
                                     pe->soft_reset.user);
        break;
    case DP_DUMP_CLEANUP:
        decref_messages(count, msgs);
        DP_canvas_history_cleanup(ch, dc, drop_message, NULL);
        break;
    case DP_DUMP_UNDO_DEPTH_LIMIT:
        DP_ASSERT(count == 1);
        DP_canvas_history_undo_depth_limit_set(
            ch, dc, DP_msg_undo_depth_depth(DP_message_internal(msgs[0])));
        update_undo_depth_limit(pe);
        decref_messages(count, msgs);
        break;
    default:
        decref_messages(count, msgs);
        DP_warn("Unknown dump entry type %d", type);
        break;
    }
}

static void handle_internal(DP_PaintEngine *pe, DP_DrawContext *dc,
                            DP_MsgInternal *mi)
{
    DP_MsgInternalType type = DP_msg_internal_type(mi);
    switch (type) {
    case DP_MSG_INTERNAL_TYPE_RESET:
        DP_atomic_set(&pe->just_reset, true);
        DP_canvas_history_reset(pe->ch);
        update_undo_depth_limit(pe);
        break;
    case DP_MSG_INTERNAL_TYPE_RESET_TO_STATE:
        DP_canvas_history_reset_to_state_noinc(
            pe->ch, DP_msg_internal_reset_to_state_data(mi));
        update_undo_depth_limit(pe);
        break;
    case DP_MSG_INTERNAL_TYPE_SNAPSHOT:
        if (!DP_canvas_history_save_point_make(pe->ch)) {
            DP_warn("Error requesting snapshot: %s", DP_error());
        }
        break;
    case DP_MSG_INTERNAL_TYPE_CATCHUP: {
        int progress = DP_msg_internal_catchup_progress(mi);
        DP_atomic_set(&pe->catchup, progress);
        // We might get disconnected right after the reset notice rolls in but
        // before the initial canvas resize is received, which would leave us
        // with the just_reset flag being stuck. Since a cleanup will enqueue
        // a 100% progress message, we can use that to clear the flag.
        if (progress >= 100) {
            DP_atomic_set(&pe->just_reset, false);
        }
        break;
    }
    case DP_MSG_INTERNAL_TYPE_CLEANUP:
        DP_MUTEX_MUST_LOCK(pe->queue_mutex);
        DP_canvas_history_cleanup(pe->ch, dc, push_cleanup_message, pe);
        while (pe->local_queue.used != 0) {
            DP_message_queue_push_noinc(
                &pe->remote_queue, DP_message_queue_shift(&pe->local_queue));
        }
        // We might have gotten disconnected while catching up after joining the
        // session or during a reset, so say we're 100% caught up after cleanup.
        push_cleanup_message(pe, DP_msg_internal_catchup_new(0, 100));
        DP_MUTEX_MUST_UNLOCK(pe->queue_mutex);
        break;
    case DP_MSG_INTERNAL_TYPE_PREVIEW: {
        int preview_type = DP_msg_internal_preview_type(mi);
        DP_ASSERT(preview_type >= 0);
        DP_ASSERT(preview_type < DP_PREVIEW_COUNT);
        free_preview(DP_atomic_ptr_xch(&pe->next_previews[preview_type],
                                       DP_msg_internal_preview_data(mi)));
        break;
    }
    case DP_MSG_INTERNAL_TYPE_RECORDER_START:
        DP_SEMAPHORE_MUST_POST(pe->record.start_sem);
        break;
    case DP_MSG_INTERNAL_TYPE_PLAYBACK: {
        DP_PaintEnginePlaybackFn playback_fn = pe->playback.fn;
        if (playback_fn) {
            playback_fn(pe->playback.user,
                        DP_msg_internal_playback_position(mi));
        }
        break;
    }
    case DP_MSG_INTERNAL_TYPE_DUMP_PLAYBACK: {
        DP_PaintEngineDumpPlaybackFn dump_playback_fn = pe->playback.dump_fn;
        if (dump_playback_fn) {
            DP_CanvasHistorySnapshot *chs =
                DP_canvas_history_snapshot_new(pe->ch);
            dump_playback_fn(pe->playback.user,
                             DP_msg_internal_dump_playback_position(mi), chs);
            DP_canvas_history_snapshot_decref(chs);
        }
        break;
    }
    case DP_MSG_INTERNAL_TYPE_DUMP_COMMAND:
        handle_dump_command(pe, mi);
        break;
    case DP_MSG_INTERNAL_TYPE_LOCAL_FORK_CLEAR:
        if (!DP_canvas_history_local_fork_clear(pe->ch, dc)) {
            DP_warn("Error clearing local fork: %s", DP_error());
        }
        break;
    default:
        DP_warn("Unhandled internal message type %d", (int)type);
        break;
    }
}


// Since draw dabs messages are so common and come in bunches, we have special
// handling to deal with them in batches. That makes the code more complicated,
// but it gives significantly better performance, so it's worth it in the end.
// These limits are measured using the bench_multidab program in dpengine/bench.

// Maximum number of multidab messages in a single go.
#define MAX_MULTIDAB_MESSAGES 8192

// 0.2 milliseconds, according to benchmark numbers. Hopefully enough for slow
// machines to not drop below 60 fps, fast machines don't care anyway.
#define MAX_MULTIDAB_COST 200000.0

static bool shift_first_message(DP_PaintEngine *pe, DP_Message **msgs)
{
    // Local queue takes priority, we want our own strokes to be responsive.
    DP_Message *msg = DP_message_queue_shift(&pe->local_queue);
    if (msg) {
        msgs[0] = msg;
        return true;
    }
    else {
        msgs[0] = DP_message_queue_shift(&pe->remote_queue);
        return false;
    }
}

static double get_classic_dabs_cost(DP_MsgDrawDabsClassic *mddc,
                                    double dabs_cost)
{
    int count;
    const DP_ClassicDab *cds = DP_msg_draw_dabs_classic_dabs(mddc, &count);
    double base_cost =
        DP_dab_cost_classic(DP_msg_draw_dabs_classic_indirect(mddc),
                            DP_msg_draw_dabs_classic_mode(mddc));
    for (int i = 0; i < count && dabs_cost < MAX_MULTIDAB_COST; ++i) {
        double size =
            DP_int_to_double(DP_classic_dab_size(DP_classic_dab_at(cds, i)));
        double cost = base_cost * size * size;
        dabs_cost += cost;
    }
    return dabs_cost;
}

static double get_pixel_dabs_cost(DP_MsgDrawDabsPixel *mddp, double dabs_cost)
{
    int count;
    const DP_PixelDab *pds = DP_msg_draw_dabs_pixel_dabs(mddp, &count);
    double base_cost = DP_dab_cost_pixel(DP_msg_draw_dabs_pixel_indirect(mddp),
                                         DP_msg_draw_dabs_pixel_mode(mddp));
    for (int i = 0; i < count && dabs_cost < MAX_MULTIDAB_COST; ++i) {
        double size = DP_pixel_dab_size(DP_pixel_dab_at(pds, i));
        double cost = base_cost * size * size;
        dabs_cost += cost;
    }
    return dabs_cost;
}

static double get_pixel_square_dabs_cost(DP_MsgDrawDabsPixel *mddp,
                                         double dabs_cost)
{
    int count;
    const DP_PixelDab *pds = DP_msg_draw_dabs_pixel_dabs(mddp, &count);
    double base_cost =
        DP_dab_cost_pixel_square(DP_msg_draw_dabs_pixel_indirect(mddp),
                                 DP_msg_draw_dabs_pixel_mode(mddp));
    for (int i = 0; i < count && dabs_cost < MAX_MULTIDAB_COST; ++i) {
        double size = DP_pixel_dab_size(DP_pixel_dab_at(pds, i));
        double cost = base_cost * size * size;
        dabs_cost += cost;
    }
    return dabs_cost;
}

static double get_mypaint_dabs_cost(DP_MsgDrawDabsMyPaint *mddmp,
                                    double dabs_cost)
{
    int count;
    const DP_MyPaintDab *mpds = DP_msg_draw_dabs_mypaint_dabs(mddmp, &count);
    double base_cost = DP_dab_cost_mypaint(
        DP_mypaint_brush_mode_indirect(DP_msg_draw_dabs_mypaint_mode(mddmp)),
        DP_msg_draw_dabs_mypaint_lock_alpha(mddmp),
        DP_msg_draw_dabs_mypaint_colorize(mddmp),
        DP_msg_draw_dabs_mypaint_posterize(mddmp));
    for (int i = 0; i < count && dabs_cost < MAX_MULTIDAB_COST; ++i) {
        double size = DP_mypaint_dab_size(DP_mypaint_dab_at(mpds, i));
        double cost = base_cost * size * size;
        dabs_cost += cost;
    }
    return dabs_cost;
}

static double get_dabs_cost(DP_Message *msg, DP_MessageType type,
                            double dabs_cost)
{
    switch (type) {
    case DP_MSG_DRAW_DABS_CLASSIC:
        return get_classic_dabs_cost(DP_message_internal(msg), dabs_cost);
    case DP_MSG_DRAW_DABS_PIXEL:
        return get_pixel_dabs_cost(DP_message_internal(msg), dabs_cost);
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
        return get_pixel_square_dabs_cost(DP_message_internal(msg), dabs_cost);
    case DP_MSG_DRAW_DABS_MYPAINT:
        return get_mypaint_dabs_cost(DP_message_internal(msg), dabs_cost);
    default:
        return MAX_MULTIDAB_COST + 1.0;
    }
}

static int shift_more_draw_dabs_messages(DP_PaintEngine *pe, bool local,
                                         DP_Message **msgs,
                                         double initial_dabs_cost)
{
    int count = 1;
    double total_dabs_cost = initial_dabs_cost;
    DP_Queue *queue = local ? &pe->local_queue : &pe->remote_queue;

    DP_Message *msg;
    while (count < MAX_MULTIDAB_MESSAGES
           && (msg = DP_message_queue_peek(queue)) != NULL) {
        total_dabs_cost =
            get_dabs_cost(msg, DP_message_type(msg), total_dabs_cost);
        if (total_dabs_cost <= MAX_MULTIDAB_COST) {
            DP_queue_shift(queue);
            msgs[count++] = msg;
        }
        else {
            break;
        }
    }

    if (count > 1) {
        int n = count - 1;
        DP_ASSERT(DP_semaphore_value(pe->queue_sem) >= n);
        DP_SEMAPHORE_MUST_WAIT_N(pe->queue_sem, n);
    }

    return count;
}

static int maybe_shift_more_messages(DP_PaintEngine *pe, bool local,
                                     DP_MessageType type, DP_Message **msgs)
{
    double dabs_cost = get_dabs_cost(msgs[0], type, 0);
    if (dabs_cost <= MAX_MULTIDAB_COST) {
        return shift_more_draw_dabs_messages(pe, local, msgs, dabs_cost);
    }
    else {
        return 1;
    }
}

static void handle_single_message(DP_PaintEngine *pe, DP_DrawContext *dc,
                                  bool local, DP_MessageType type,
                                  DP_Message *msg)
{
    if (type == DP_MSG_INTERNAL) {
        handle_internal(pe, dc, DP_msg_internal_cast(msg));
    }
    else if (type == DP_MSG_SOFT_RESET) {
        DP_canvas_history_soft_reset(pe->ch, dc, DP_message_context_id(msg),
                                     pe->soft_reset.fn, pe->soft_reset.user);
    }
    else if (type == DP_MSG_DEFAULT_LAYER) {
        DP_MsgDefaultLayer *mdl = DP_message_internal(msg);
        int default_layer_id = DP_msg_default_layer_id(mdl);
        DP_atomic_xch(&pe->default_layer_id, default_layer_id);
    }
    else if (type == DP_MSG_UNDO_DEPTH) {
        DP_MsgUndoDepth *mud = DP_message_internal(msg);
        int undo_depth_limit = DP_msg_undo_depth_depth(mud);
        DP_canvas_history_undo_depth_limit_set(pe->ch, dc, undo_depth_limit);
        DP_atomic_xch(&pe->undo_depth_limit,
                      DP_canvas_history_undo_depth_limit(pe->ch));
    }
    else if (local) {
        if (!DP_canvas_history_handle_local(pe->ch, dc, msg)) {
            DP_warn("Handle local command: %s", DP_error());
        }
    }
    else {
        if (!DP_canvas_history_handle(pe->ch, dc, msg)) {
            DP_warn("Handle remote command: %s", DP_error());
        }
        // Resets are always followed by a canvas resize, so we can use that as
        // a reasonable point to clear this flag and get the UI to update again.
        if (type == DP_MSG_CANVAS_RESIZE) {
            DP_atomic_set(&pe->just_reset, false);
        }
    }
    DP_message_decref(msg);
}

static void handle_multidab(DP_PaintEngine *pe, DP_DrawContext *dc, bool local,
                            int count, DP_Message **msgs)
{
    if (local) {
        DP_canvas_history_handle_local_multidab_dec(pe->ch, dc, count, msgs);
    }
    else {
        DP_canvas_history_handle_multidab_dec(pe->ch, dc, count, msgs);
    }
}

static void handle_message(DP_PaintEngine *pe, DP_DrawContext *dc,
                           DP_Message **msgs)
{
    DP_MUTEX_MUST_LOCK(pe->queue_mutex);
    bool local = shift_first_message(pe, msgs);
    DP_Message *first = msgs[0];
    DP_MessageType type = DP_message_type(first);
    int count = maybe_shift_more_messages(pe, local, type, msgs);
    DP_MUTEX_MUST_UNLOCK(pe->queue_mutex);

    DP_ASSERT(count > 0);
    DP_ASSERT(count <= MAX_MULTIDAB_MESSAGES);
    if (count == 1) {
        handle_single_message(pe, dc, local, type, first);
    }
    else {
        handle_multidab(pe, dc, local, count, msgs);
    }
}

static void run_paint_engine(void *user)
{
    DP_PaintEngine *pe = user;
    DP_DrawContext *dc = pe->paint_dc;
    DP_Semaphore *sem = pe->queue_sem;
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    DP_Message **msgs = DP_malloc(sizeof(*msgs) * MAX_MULTIDAB_MESSAGES);
    while (true) {
        DP_SEMAPHORE_MUST_WAIT(sem);
        if (DP_atomic_get(&pe->running)) {
            handle_message(pe, dc, msgs);
        }
        else {
            break;
        }
    }
    DP_free(msgs);
}


static void invalidate_local_view(DP_PaintEngine *pe, bool check_all)
{
    DP_layer_props_list_decref_nullable(pe->local_view.layers.prev_lpl);
    pe->local_view.layers.prev_lpl = NULL;
    DP_timeline_decref_nullable(pe->local_view.tracks.prev_tl);
    pe->local_view.tracks.prev_tl = NULL;
    if (check_all) {
        DP_canvas_diff_check_all(pe->diff);
    }
}

static void local_view_invalidated(void *user, bool check_all, int layer_id)
{
    DP_PaintEngine *pe = user;
    if (layer_id != 0) {
        DP_CanvasState *cs = pe->view_cs;
        DP_LayerRoutes *lr = DP_canvas_state_layer_routes_noinc(cs);
        DP_LayerRoutesEntry *lre = DP_layer_routes_search(lr, layer_id);
        if (lre) {
            if (DP_layer_routes_entry_is_group(lre)) {
                DP_LayerGroup *lg = DP_layer_routes_entry_group(lre, cs);
                DP_layer_group_diff_mark(lg, pe->diff);
            }
            else {
                DP_LayerContent *lc = DP_layer_routes_entry_content(lre, cs);
                DP_layer_content_diff_mark(lc, pe->diff);
            }
        }
    }
    invalidate_local_view(pe, check_all);
}

static void sync_preview(DP_PaintEngine *pe, int type, DP_Preview *pv)
{
    // Make the preview go through the paint engine so that there's less jerking
    // as previews are created and cleared. There may still be flickering, but
    // it won't look like transforms undo themselves for a moment.
    DP_Message *msg = DP_msg_internal_preview_new(0, type, pv);
    DP_MUTEX_MUST_LOCK(pe->queue_mutex);
    DP_message_queue_push_noinc(&pe->local_queue, msg);
    DP_SEMAPHORE_MUST_POST(pe->queue_sem);
    DP_MUTEX_MUST_UNLOCK(pe->queue_mutex);
}

static void preview_rendered(void *user, DP_Preview *pv)
{
    DP_PaintEngine *pe = user;
    int type = DP_preview_type(pv);
    sync_preview(pe, type, DP_preview_incref(pv));
}

static void preview_rerendered(void *user)
{
    DP_PaintEngine *pe = user;
    DP_atomic_set(&pe->preview_rerendered, true);
}

static void preview_clear(void *user, int type)
{
    DP_PaintEngine *pe = user;
    sync_preview(pe, type, &DP_preview_null);
}

DP_PaintEngine *DP_paint_engine_new_inc(
    DP_DrawContext *paint_dc, DP_DrawContext *main_dc,
    DP_DrawContext *preview_dc, DP_AclState *acls, DP_CanvasState *cs_or_null,
    bool renderer_checker, uint32_t checker_color1, uint32_t checker_color2,
    DP_RendererTileFn renderer_tile_fn, DP_RendererUnlockFn renderer_unlock_fn,
    DP_RendererResizeFn renderer_resize_fn, void *renderer_user,
    DP_CanvasHistorySavePointFn save_point_fn, void *save_point_user,
    DP_CanvasHistorySoftResetFn soft_reset_fn, void *soft_reset_user,
    bool want_canvas_history_dump, const char *canvas_history_dump_dir,
    DP_RecorderGetTimeMsFn get_time_ms_fn, void *get_time_ms_user,
    DP_Player *player_or_null, DP_PaintEnginePlaybackFn playback_fn,
    DP_PaintEngineDumpPlaybackFn dump_playback_fn, void *playback_user)
{
    DP_PaintEngine *pe = DP_malloc(sizeof(*pe));

    pe->acls = acls;
    pe->ch = DP_canvas_history_new_inc(
        cs_or_null, save_point_fn, save_point_user, want_canvas_history_dump,
        canvas_history_dump_dir);
    pe->soft_reset.fn = soft_reset_fn;
    pe->soft_reset.user = soft_reset_user;
    pe->diff = DP_canvas_diff_new();
    pe->history_cs = DP_canvas_state_new();
    pe->view_cs = DP_canvas_state_incref(pe->history_cs);
    pe->local_state =
        DP_local_state_new(cs_or_null, local_view_invalidated, pe);
    pe->local_view.reveal_censored = false;
    pe->local_view.inspect_context_id = 0;
    pe->local_view.inspect_show_tiles = false;
    pe->local_view.layers_can_decrease_opacity = true;
    pe->local_view.checker_color1 = (DP_Pixel8){checker_color1};
    pe->local_view.checker_color2 = (DP_Pixel8){checker_color2};
    pe->local_view.layers.prev_lpl = NULL;
    pe->local_view.layers.prev_tl = NULL;
    pe->local_view.layers.lpl = NULL;
    pe->local_view.tracks.prev_tl = NULL;
    pe->local_view.tracks.tl = NULL;
    pe->paint_dc = paint_dc;
    pe->main_dc = main_dc;
    for (int i = 0; i < DP_PREVIEW_COUNT; ++i) {
        pe->previews[i] = NULL;
        DP_atomic_ptr_set(&pe->next_previews[i], NULL);
    }
    DP_atomic_set(&pe->preview_rerendered, false);
    pe->preview_renderer = DP_preview_renderer_new(
        preview_dc, preview_rendered, preview_rerendered, preview_clear, pe);
    DP_message_queue_init(&pe->local_queue, INITIAL_QUEUE_CAPACITY);
    DP_message_queue_init(&pe->remote_queue, INITIAL_QUEUE_CAPACITY);
    pe->queue_sem = DP_semaphore_new(0);
    pe->queue_mutex = DP_mutex_new();
    DP_atomic_set(&pe->running, true);
    DP_atomic_set(&pe->catchup, -1);
    DP_atomic_set(&pe->default_layer_id, -1);
    DP_atomic_set(&pe->undo_depth_limit,
                  DP_canvas_history_undo_depth_limit(pe->ch));
    DP_atomic_set(&pe->just_reset, false);
    pe->catching_up = false;
    pe->reset_locked = false;
    pe->paint_thread = DP_thread_new(run_paint_engine, pe);
    pe->renderer =
        DP_renderer_new(DP_worker_cpu_count(128), renderer_checker,
                        pe->local_view.checker_color1,
                        pe->local_view.checker_color2, renderer_tile_fn,
                        renderer_unlock_fn, renderer_resize_fn, renderer_user);
    pe->meta.acl_change_flags = 0;
    DP_VECTOR_INIT_TYPE(&pe->meta.cursor_changes, DP_PaintEngineCursorChange,
                        8);
    pe->record.path = NULL;
    pe->record.recorder = NULL;
    pe->record.start_sem = DP_semaphore_new(0);
    pe->record.state_change = RECORDER_STOPPED;
    pe->record.get_time_ms_fn = get_time_ms_fn;
    pe->record.get_time_ms_user = get_time_ms_user;
    pe->playback.player = player_or_null;
    pe->playback.msecs = 0;
    pe->playback.next_has_time = true;
    pe->playback.fn = playback_fn;
    pe->playback.dump_fn = dump_playback_fn;
    pe->playback.user = playback_user;
    return pe;
}

void DP_paint_engine_free_join(DP_PaintEngine *pe)
{
    if (pe) {
        DP_paint_engine_recorder_stop(pe);
        DP_atomic_set(&pe->running, false);
        DP_SEMAPHORE_MUST_POST(pe->queue_sem);
        DP_thread_free_join(pe->paint_thread);
        DP_player_free(pe->playback.player);
        DP_semaphore_free(pe->record.start_sem);
        DP_vector_dispose(&pe->meta.cursor_changes);
        DP_renderer_free(pe->renderer);
        DP_mutex_free(pe->queue_mutex);
        DP_semaphore_free(pe->queue_sem);
        DP_message_queue_dispose(&pe->remote_queue);
        DP_Message *msg;
        while ((msg = DP_message_queue_shift(&pe->local_queue)) != NULL) {
            if (DP_message_type(msg) == DP_MSG_INTERNAL) {
                DP_MsgInternal *mi = DP_msg_internal_cast(msg);
                switch (DP_msg_internal_type(mi)) {
                case DP_MSG_INTERNAL_TYPE_RESET_TO_STATE:
                    DP_canvas_state_decref(
                        DP_msg_internal_reset_to_state_data(mi));
                    break;
                case DP_MSG_INTERNAL_TYPE_PREVIEW:
                    free_preview(DP_msg_internal_preview_data(mi));
                    break;
                case DP_MSG_INTERNAL_TYPE_DUMP_COMMAND: {
                    int count;
                    DP_Message **msgs =
                        DP_msg_internal_dump_command_messages(mi, &count);
                    decref_messages(count, msgs);
                    break;
                }
                default:
                    break;
                }
            }
            DP_message_decref(msg);
        }
        DP_message_queue_dispose(&pe->local_queue);
        DP_preview_renderer_free(pe->preview_renderer);
        for (int i = 0; i < DP_PREVIEW_COUNT; ++i) {
            free_preview(DP_atomic_ptr_xch(&pe->next_previews[i], NULL));
            DP_preview_decref_nullable(pe->previews[i]);
        }
        DP_timeline_decref_nullable(pe->local_view.tracks.tl);
        DP_timeline_decref_nullable(pe->local_view.tracks.prev_tl);
        DP_layer_props_list_decref_nullable(pe->local_view.layers.lpl);
        DP_timeline_decref_nullable(pe->local_view.layers.prev_tl);
        DP_layer_props_list_decref_nullable(pe->local_view.layers.prev_lpl);
        DP_local_state_free(pe->local_state);
        DP_canvas_state_decref_nullable(pe->history_cs);
        DP_canvas_state_decref_nullable(pe->view_cs);
        DP_canvas_diff_free(pe->diff);
        DP_canvas_history_free(pe->ch);
        DP_free(pe);
    }
}

int DP_paint_engine_render_thread_count(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_renderer_thread_count(pe->renderer);
}

void DP_paint_engine_local_drawing_in_progress_set(
    DP_PaintEngine *pe, bool local_drawing_in_progress)
{
    DP_ASSERT(pe);
    DP_canvas_history_local_drawing_in_progress_set(pe->ch,
                                                    local_drawing_in_progress);
}

bool DP_paint_engine_want_canvas_history_dump(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_canvas_history_want_dump(pe->ch);
}

void DP_paint_engine_want_canvas_history_dump_set(DP_PaintEngine *pe,
                                                  bool want_canvas_history_dump)
{
    DP_ASSERT(pe);
    DP_canvas_history_want_dump_set(pe->ch, want_canvas_history_dump);
}


bool DP_paint_engine_local_state_reset_image_build(
    DP_PaintEngine *pe, DP_LocalStateAcceptResetMessageFn fn, void *user)
{
    DP_ASSERT(pe);
    return DP_local_state_reset_image_build(pe->local_state, pe->main_dc, fn,
                                            user);
}

void DP_paint_engine_get_layers_visible_in_frame(DP_PaintEngine *pe,
                                                 DP_AddVisibleLayerFn fn,
                                                 void *user)
{
    DP_ASSERT(pe);
    DP_view_mode_get_layers_visible_in_frame(pe->view_cs, pe->local_state, fn,
                                             user);
}

int DP_paint_engine_active_layer_id(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_local_state_active_layer_id(pe->local_state);
}

int DP_paint_engine_active_frame_index(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_local_state_active_frame_index(pe->local_state);
}

DP_ViewMode DP_paint_engine_view_mode(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_local_state_view_mode(pe->local_state);
}

DP_ViewModeFilter DP_paint_engine_view_mode_filter(DP_PaintEngine *pe,
                                                   DP_ViewModeBuffer *vmb,
                                                   DP_CanvasState *cs)
{
    DP_ASSERT(pe);
    DP_ASSERT(vmb);
    DP_ASSERT(cs);
    return DP_view_mode_filter_make(
        vmb, DP_paint_engine_view_mode(pe), cs,
        DP_paint_engine_active_layer_id(pe),
        DP_paint_engine_active_frame_index(pe),
        DP_local_state_onion_skins(pe->local_state));
}

bool DP_paint_engine_reveal_censored(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return pe->local_view.reveal_censored;
}

void DP_paint_engine_reveal_censored_set(DP_PaintEngine *pe,
                                         bool reveal_censored)
{
    DP_ASSERT(pe);
    if (pe->local_view.reveal_censored != reveal_censored) {
        pe->local_view.reveal_censored = reveal_censored;
        invalidate_local_view(pe, false);
    }
}


DP_ViewModePick DP_paint_engine_pick(DP_PaintEngine *pe, int x, int y)
{
    DP_ASSERT(pe);
    return DP_view_mode_pick(pe->view_cs, pe->local_state, x, y);
}

void DP_paint_engine_inspect_set(DP_PaintEngine *pe, unsigned int context_id,
                                 bool show_tiles)
{
    DP_ASSERT(pe);
    if (pe->local_view.inspect_context_id != context_id
        || pe->local_view.inspect_show_tiles != show_tiles) {
        pe->local_view.inspect_context_id = context_id;
        pe->local_view.inspect_show_tiles = show_tiles;
        invalidate_local_view(pe, false);
    }
}

bool DP_paint_engine_checkers_visible(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_renderer_checkers_visible(pe->renderer);
}

uint32_t DP_paint_engine_checker_color1(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return pe->local_view.checker_color1.color;
}

uint32_t DP_paint_engine_checker_color2(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return pe->local_view.checker_color2.color;
}

void DP_paint_engine_checker_color1_set(DP_PaintEngine *pe, uint32_t color1)
{
    DP_ASSERT(pe);
    if (pe->local_view.checker_color1.color != color1) {
        pe->local_view.checker_color1.color = color1;
        if (DP_renderer_checkers(pe->renderer)) {
            invalidate_local_view(pe, true);
        }
    }
}

void DP_paint_engine_checker_color2_set(DP_PaintEngine *pe, uint32_t color2)
{
    DP_ASSERT(pe);
    if (pe->local_view.checker_color2.color != color2) {
        pe->local_view.checker_color2.color = color2;
        if (DP_renderer_checkers(pe->renderer)) {
            invalidate_local_view(pe, true);
        }
    }
}


DP_Tile *DP_paint_engine_local_background_tile_noinc(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_local_state_background_tile_noinc(pe->local_state);
}


static bool record_initial_message(void *user, DP_Message *msg)
{
    return DP_recorder_message_push_noinc(user, msg);
}

static bool start_recording(DP_PaintEngine *pe, DP_RecorderType type,
                            JSON_Value *header, char *path, bool initial)
{
    DP_Output *output = DP_file_output_new_from_path(path);
    if (!output) {
        DP_free(path);
        return false;
    }

    DP_Recorder *r;
    if (initial) {
        // To get a clean initial recording, we have to first spin down all
        // queued messages. We send ourselves a RECORDER_START internal message
        // and then block this thread (which must be the only thread interacting
        // with the paint engine, maybe we should verify that somehow) until the
        // paint thread gets to it.
        DP_MUTEX_MUST_LOCK(pe->queue_mutex);
        DP_message_queue_push_noinc(&pe->remote_queue,
                                    DP_msg_internal_recorder_start_new(0));
        DP_SEMAPHORE_MUST_POST(pe->queue_sem);
        DP_MUTEX_MUST_UNLOCK(pe->queue_mutex);
        // The paint thread will post to this semaphore when it reaches our
        // recorder start message.
        DP_SEMAPHORE_MUST_WAIT(pe->record.start_sem);
        DP_ASSERT(pe->remote_queue.used == 0);

        // Now all queued messages have been handled. We can't just take the
        // current canvas state from the canvas history though, since that would
        // lose undo history and might contain state from local messages. So
        // instead, we grab the oldest reachable state and then record the
        // entire history since then.
        r = DP_canvas_history_recorder_new(pe->ch, type, header,
                                           pe->record.get_time_ms_fn,
                                           pe->record.get_time_ms_user, output);
    }
    else {
        r = DP_recorder_new_inc(type, header, NULL, pe->record.get_time_ms_fn,
                                pe->record.get_time_ms_user, output);
    }

    // After initializing the state, we can set up local state and permissions.
    if (r) {
        bool reset_image_ok =
            DP_local_state_reset_image_build(pe->local_state, pe->main_dc,
                                             record_initial_message, r)
            && DP_acl_state_reset_image_build(
                pe->acls, 0, DP_ACL_STATE_RESET_IMAGE_RECORDING_FLAGS,
                record_initial_message, r);
        if (!reset_image_ok) {
            DP_warn("Error build recorder reset image");
            DP_recorder_free_join(r, NULL);
            r = NULL;
        }
    }

    if (r) {
        if (pe->record.recorder) {
            DP_warn("Stopping already running recording");
            DP_paint_engine_recorder_stop(pe);
        }
        // When dealing with cleanup after a disconnect, the recorder might
        // currently be in the process of being fed messages from the local
        // fork, so we have to take a lock around manipulating it. We re-use
        // the queue mutex for that, since it's what the cleanup uses too.
        DP_MUTEX_MUST_LOCK(pe->queue_mutex);
        pe->record.path = path;
        pe->record.recorder = r;
        DP_MUTEX_MUST_UNLOCK(pe->queue_mutex);
        pe->record.state_change = RECORDER_STARTED;
        return true;
    }
    else {
        DP_free(path);
        return false;
    }
}

bool DP_paint_engine_recorder_start(DP_PaintEngine *pe, DP_RecorderType type,
                                    JSON_Value *header, const char *path)
{
    return start_recording(pe, type, header, DP_strdup(path), true);
}

bool DP_paint_engine_recorder_stop(DP_PaintEngine *pe)
{
    if (pe->record.recorder) {
        // Need to take a lock due to cleanup handling, see explanation above.
        DP_MUTEX_MUST_LOCK(pe->queue_mutex);
        DP_recorder_free_join(pe->record.recorder, NULL);
        DP_free(pe->record.path);
        pe->record.path = NULL;
        pe->record.recorder = NULL;
        DP_MUTEX_MUST_UNLOCK(pe->queue_mutex);
        pe->record.state_change = RECORDER_STOPPED;
        return true;
    }
    else {
        return false;
    }
}

bool DP_paint_engine_recorder_is_recording(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return pe->record.recorder != NULL;
}


static void push_playback(DP_PaintEnginePushMessageFn push_message, void *user,
                          long long position)
{
    push_message(user, DP_msg_internal_playback_new(0, position));
}

static long long guess_message_msecs(DP_Message *msg, DP_MessageType type,
                                     bool *out_next_has_time)
{
    // The recording format doesn't contain proper timing information, so we
    // just make some wild guesses as to how long each message takes to try to
    // get some vaguely okayly timed playback out of it.
    switch (type) {
    case DP_MSG_DRAW_DABS_CLASSIC:
    case DP_MSG_DRAW_DABS_PIXEL:
    case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
    case DP_MSG_DRAW_DABS_MYPAINT:
        return 5;
    case DP_MSG_PUT_IMAGE:
    case DP_MSG_MOVE_REGION:
    case DP_MSG_MOVE_RECT:
    case DP_MSG_MOVE_POINTER:
        return 10;
    case DP_MSG_LAYER_CREATE:
    case DP_MSG_LAYER_ATTRIBUTES:
    case DP_MSG_LAYER_RETITLE:
    case DP_MSG_LAYER_ORDER:
    case DP_MSG_LAYER_DELETE:
    case DP_MSG_LAYER_VISIBILITY:
    case DP_MSG_ANNOTATION_RESHAPE:
    case DP_MSG_ANNOTATION_EDIT:
        return 20;
    case DP_MSG_CANVAS_RESIZE:
        return 60;
    case DP_MSG_UNDO:
        // An undo for user 0 means that the next message is actually an undone
        // entry, which happens at the start of recordings. We treat those
        // messages as having a length of 0, since they just get appended to the
        // history and aren't visible.
        if (DP_message_context_id(msg) == 0
            && DP_msg_undo_override_user(DP_message_internal(msg)) == 0) {
            *out_next_has_time = false;
            return 0;
        }
        else {
            return 20;
        }
    default:
        return 0;
    }
}

static DP_PlayerResult
skip_playback_forward(DP_PaintEngine *pe, long long steps, int what,
                      DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(steps >= 0);
    DP_ASSERT(push_message);
    DP_ASSERT(what == PLAYBACK_STEP_MESSAGES
              || what == PLAYBACK_STEP_UNDO_POINTS
              || what == PLAYBACK_STEP_MSECS);
    DP_debug("Skip playback forward by %lld %s", steps,
             what == PLAYBACK_STEP_MESSAGES      ? "step(s)"
             : what == PLAYBACK_STEP_UNDO_POINTS ? "undo point(s)"
                                                 : "millisecond(s)");

    DP_Player *player = pe->playback.player;
    if (!player) {
        DP_error_set("No player set");
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    long long done = 0;
    if (what == PLAYBACK_STEP_MSECS) {
        done += pe->playback.msecs;
    }

    while (done < steps) {
        DP_Message *msg;
        DP_PlayerResult result = DP_player_step(player, &msg);
        if (result == DP_PLAYER_SUCCESS) {
            DP_MessageType type = DP_message_type(msg);
            if (type == DP_MSG_INTERVAL) {
                if (what == PLAYBACK_STEP_MESSAGES) {
                    ++done;
                }
                else if (what == PLAYBACK_STEP_MSECS) {
                    DP_MsgInterval *mi = DP_message_internal(msg);
                    // Really no point in delaying playback for ages, it just
                    // makes the user wonder if there's something broken.
                    done += DP_min_int(1000, DP_msg_interval_msecs(mi));
                }
                DP_message_decref(msg);
            }
            else {
                if (what == PLAYBACK_STEP_MESSAGES
                    || (what == PLAYBACK_STEP_UNDO_POINTS
                        && type == DP_MSG_UNDO_POINT)) {
                    ++done;
                }
                else if (what == PLAYBACK_STEP_MSECS) {
                    if (pe->playback.next_has_time) {
                        done += guess_message_msecs(
                            msg, type, &pe->playback.next_has_time);
                    }
                    else {
                        pe->playback.next_has_time = true;
                    }
                }
                push_message(user, msg);
            }
        }
        else if (result == DP_PLAYER_ERROR_PARSE) {
            if (what == PLAYBACK_STEP_MESSAGES) {
                ++done;
            }
            DP_warn("Can't play back message: %s", DP_error());
        }
        else {
            // We're either at the end of the recording or encountered an input
            // error. In either case, we're done playing this recording, report
            // a negative value as the position to indicate that.
            push_playback(push_message, user, -1);
            return result;
        }
    }

    if (what == PLAYBACK_STEP_MSECS) {
        pe->playback.msecs = done - steps;
    }

    // If we don't have an index, report the position as a percentage
    // completion based on the amount of bytes read from the recording.
    long long position =
        DP_player_index_loaded(player)
            ? DP_player_position(player)
            : DP_double_to_llong(DP_player_progress(player) * 100.0 + 0.5);
    push_playback(push_message, user, position);
    return DP_PLAYER_SUCCESS;
}

DP_PlayerResult
DP_paint_engine_playback_step(DP_PaintEngine *pe, long long steps,
                              DP_PaintEnginePushMessageFn push_message,
                              void *user)
{
    DP_ASSERT(pe);
    DP_ASSERT(steps >= 0);
    DP_ASSERT(push_message);
    return skip_playback_forward(pe, steps, PLAYBACK_STEP_MESSAGES,
                                 push_message, user);
}

static DP_PlayerResult rewind_playback(DP_PaintEngine *pe,
                                       DP_PaintEnginePushMessageFn push_message,
                                       void *user)
{
    DP_Player *player = pe->playback.player;
    if (!player) {
        DP_error_set("No player set");
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    if (DP_player_rewind(player)) {
        push_message(user, DP_msg_internal_reset_new(0));
        push_playback(push_message, user, 0);
        return DP_PLAYER_SUCCESS;
    }
    else {
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_INPUT;
    }
}

static DP_PlayerResult
jump_playback_to(DP_PaintEngine *pe, DP_DrawContext *dc, long long to,
                 bool relative, bool exact,
                 DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_Player *player = pe->playback.player;
    if (!player) {
        DP_error_set("No player set");
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    if (!DP_player_index_loaded(player)) {
        DP_error_set("No index loaded");
        push_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    long long player_position = DP_player_position(player);
    long long target_position = relative ? player_position + to : to;
    DP_debug("Jump playback from %lld to %s %lld", player_position,
             exact ? "exactly" : "snapshot nearest", target_position);

    long long message_count =
        DP_uint_to_llong(DP_player_index_message_count(player));
    if (message_count == 0) {
        DP_error_set("Recording contains no messages");
        push_playback(push_message, user, player_position);
        return DP_PLAYER_ERROR_INPUT;
    }
    else if (target_position < 0) {
        target_position = 0;
    }
    else if (target_position >= message_count) {
        target_position = message_count - 1;
    }
    DP_debug("Clamped position to %lld (message count %lld)", target_position,
             message_count);

    DP_PlayerIndexEntry entry = DP_player_index_entry_search(
        player, target_position, relative && !exact && to > 0);
    DP_debug("Loaded entry with message index %lld, message offset %zu, "
             "snapshot offset %zu, thumbnail offset %zu",
             entry.message_index, entry.message_offset, entry.snapshot_offset,
             entry.thumbnail_offset);

    bool inside_snapshot = player_position >= entry.message_index
                        && player_position < target_position;
    if (inside_snapshot) {
        long long steps = target_position - player_position;
        DP_debug("Already inside snapshot, stepping forward by %lld", steps);
        DP_PlayerResult result = skip_playback_forward(
            pe, steps, PLAYBACK_STEP_MESSAGES, push_message, user);
        // If skipping playback forward doesn't work, e.g. because we're
        // in an input error state, punt to reloading the snapshot.
        if (result == DP_PLAYER_SUCCESS) {
            return result;
        }
        else {
            DP_warn("Skipping inside snapshot failed with result %d: %s",
                    (int)result, DP_error());
        }
    }

    DP_PlayerIndexEntrySnapshot *snapshot =
        DP_player_index_entry_load(player, dc, entry);
    if (!snapshot) {
        DP_debug("Reading snapshot failed: %s", DP_error());
        push_playback(push_message, user, player_position);
        return DP_PLAYER_ERROR_INPUT;
    }

    if (!DP_player_seek(player, entry.message_index, entry.message_offset)) {
        DP_player_index_entry_snapshot_free(snapshot);
        push_playback(push_message, user, player_position);
        return DP_PLAYER_ERROR_INPUT;
    }

    DP_CanvasState *cs =
        DP_player_index_entry_snapshot_canvas_state_inc(snapshot);
    push_message(user, DP_msg_internal_reset_to_state_new(0, cs));

    int count = DP_player_index_entry_snapshot_message_count(snapshot);
    for (int i = 0; i < count; ++i) {
        DP_Message *msg =
            DP_player_index_entry_snapshot_message_at_inc(snapshot, i);
        if (msg) {
            push_message(user, msg);
        }
    }

    DP_player_index_entry_snapshot_free(snapshot);
    return skip_playback_forward(
        pe, exact ? target_position - entry.message_index : 0,
        PLAYBACK_STEP_MESSAGES, push_message, user);
}

DP_PlayerResult DP_paint_engine_playback_skip_by(
    DP_PaintEngine *pe, DP_DrawContext *dc, long long steps, bool by_snapshots,
    DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    if (steps < 0 || by_snapshots) {
        return jump_playback_to(pe, dc, steps, true, !by_snapshots,
                                push_message, user);
    }
    else {
        return skip_playback_forward(pe, steps, PLAYBACK_STEP_UNDO_POINTS,
                                     push_message, user);
    }
}

DP_PlayerResult DP_paint_engine_playback_jump_to(
    DP_PaintEngine *pe, DP_DrawContext *dc, long long position,
    DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    if (position <= 0) {
        return rewind_playback(pe, push_message, user);
    }
    else {
        return jump_playback_to(pe, dc, position, false, true, push_message,
                                user);
    }
}

DP_PlayerResult DP_paint_engine_playback_begin(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (player) {
        pe->playback.msecs = 0;
        return DP_PLAYER_SUCCESS;
    }
    else {
        DP_error_set("No player set");
        return DP_PLAYER_ERROR_OPERATION;
    }
}

DP_PlayerResult
DP_paint_engine_playback_play(DP_PaintEngine *pe, long long msecs,
                              DP_PaintEnginePushMessageFn push_message,
                              void *user)
{
    DP_ASSERT(pe);
    return skip_playback_forward(pe, msecs, PLAYBACK_STEP_MSECS, push_message,
                                 user);
}

bool DP_paint_engine_playback_index_build(
    DP_PaintEngine *pe, DP_DrawContext *dc,
    DP_PlayerIndexShouldSnapshotFn should_snapshot_fn,
    DP_PlayerIndexProgressFn progress_fn, void *user)
{
    DP_ASSERT(pe);
    DP_ASSERT(dc);
    DP_Player *player = pe->playback.player;
    if (player) {
        return DP_player_index_build(player, dc, should_snapshot_fn,
                                     progress_fn, user);
    }
    else {
        DP_error_set("No player set");
        return false;
    }
}

bool DP_paint_engine_playback_index_load(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (player) {
        return DP_player_index_load(player);
    }
    else {
        DP_error_set("No player set");
        return false;
    }
}

unsigned int DP_paint_engine_playback_index_message_count(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (player) {
        return DP_player_index_message_count(player);
    }
    else {
        DP_error_set("No player set");
        return 0;
    }
}

size_t DP_paint_engine_playback_index_entry_count(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (player) {
        return DP_player_index_entry_count(player);
    }
    else {
        DP_error_set("No player set");
        return 0;
    }
}

DP_Image *DP_paint_engine_playback_index_thumbnail_at(DP_PaintEngine *pe,
                                                      size_t index,
                                                      bool *out_error)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (player) {
        return DP_player_index_thumbnail_at(player, index, out_error);
    }
    else {
        DP_error_set("No player set");
        if (out_error) {
            *out_error = true;
        }
        return NULL;
    }
}

static DP_PlayerResult step_dump(DP_Player *player,
                                 DP_PaintEnginePushMessageFn push_message,
                                 void *user)
{
    DP_DumpType type;
    int count;
    DP_Message **msgs;
    DP_PlayerResult result = DP_player_step_dump(player, &type, &count, &msgs);
    if (result == DP_PLAYER_SUCCESS) {
        push_message(user, DP_msg_internal_dump_command_new_inc(0, (int)type,
                                                                count, msgs));
    }
    return result;
}

static void push_dump_playback(DP_PaintEnginePushMessageFn push_message,
                               void *user, long long position)
{
    push_message(user, DP_msg_internal_dump_playback_new(0, position));
}

DP_PlayerResult DP_paint_engine_playback_dump_step(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (!player) {
        DP_error_set("No player set");
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    DP_PlayerResult result = step_dump(player, push_message, user);
    long long position_to_report =
        result == DP_PLAYER_SUCCESS ? DP_player_position(player) : -1;
    push_dump_playback(push_message, user, position_to_report);
    return result;
}

DP_PlayerResult DP_paint_engine_playback_dump_jump_previous_reset(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_Player *player = pe->playback.player;
    if (!player) {
        DP_error_set("No player set");
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    if (!DP_player_seek_dump(player, DP_player_position(player) - 1)) {
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_INPUT;
    }

    push_message(user, DP_msg_internal_reset_new(0));
    push_dump_playback(push_message, user, DP_player_position(player));
    return DP_PLAYER_SUCCESS;
}

static int step_toward_next_reset(DP_Player *player, bool stop_at_reset,
                                  DP_PaintEnginePushMessageFn push_message,
                                  void *user)
{
    long long position = DP_player_position(player);
    size_t offset = DP_player_tell(player);
    DP_DumpType type;
    int count;
    DP_Message **msgs;
    DP_PlayerResult result = DP_player_step_dump(player, &type, &count, &msgs);
    if (result == DP_PLAYER_SUCCESS) {
        if (type == DP_DUMP_RESET && stop_at_reset) {
            if (DP_player_seek(player, position, offset)) {
                return -1;
            }
            else {
                return DP_PLAYER_ERROR_INPUT;
            }
        }
        else {
            push_message(user, DP_msg_internal_dump_command_new_inc(
                                   0, (int)type, count, msgs));
        }
    }
    return (int)result;
}

DP_PlayerResult DP_paint_engine_playback_dump_jump_next_reset(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (!player) {
        DP_error_set("No player set");
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    // We want to jump up to, but excluding the next reset. So we keep walking
    // through the dump until we hit a reset and then seek to before it. If we
    // hit a reset as our very first message, we push through it, since
    // otherwise we'd be doing nothing at all until manually stepping forward.
    int result = step_toward_next_reset(player, false, push_message, user);
    while (result == DP_PLAYER_SUCCESS) {
        result = step_toward_next_reset(player, true, push_message, user);
    }

    push_dump_playback(push_message, user, DP_player_position(player));
    return result == -1 ? DP_PLAYER_SUCCESS : (DP_PlayerResult)result;
}

DP_PlayerResult
DP_paint_engine_playback_dump_jump(DP_PaintEngine *pe, long long position,
                                   DP_PaintEnginePushMessageFn push_message,
                                   void *user)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (!player) {
        DP_error_set("No player set");
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_OPERATION;
    }

    if (!DP_player_seek_dump(player, position)) {
        push_dump_playback(push_message, user, -1);
        return DP_PLAYER_ERROR_INPUT;
    }

    if (DP_player_position(player) == 0) {
        push_message(user, DP_msg_internal_reset_new(0));
    }

    DP_PlayerResult result = DP_PLAYER_SUCCESS;
    while (DP_player_position(player) < position) {
        result = step_dump(player, push_message, user);
        if (result != DP_PLAYER_SUCCESS) {
            break;
        }
    }
    push_dump_playback(push_message, user, DP_player_position(player));
    return result;
}

bool DP_paint_engine_playback_flush(DP_PaintEngine *pe,
                                    DP_PaintEnginePushMessageFn push_message,
                                    void *user)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (player) {
        while (true) {
            DP_Message *msg;
            DP_PlayerResult result = DP_player_step(player, &msg);
            if (result == DP_PLAYER_SUCCESS) {
                DP_MessageType type = DP_message_type(msg);
                if (type == DP_MSG_INTERVAL) {
                    DP_message_decref(msg);
                }
                else {
                    push_message(user, msg);
                }
            }
            else if (result == DP_PLAYER_ERROR_PARSE) {
                DP_warn("Can't play back message: %s", DP_error());
            }
            else if (result == DP_PLAYER_RECORDING_END) {
                return true;
            }
            else {
                return false; // Some kind of input error.
            }
        }
    }
    else {
        return false;
    }
}

bool DP_paint_engine_playback_close(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_Player *player = pe->playback.player;
    if (player) {
        DP_player_free(player);
        pe->playback.player = NULL;
        return true;
    }
    else {
        return false;
    }
}


static bool is_pushable_type(DP_MessageType type)
{
    return type >= 128 || type == DP_MSG_INTERNAL
        || type == DP_MSG_DEFAULT_LAYER;
}

static void restart_recording(DP_PaintEngine *pe)
{
    char *path = DP_file_path_unique_nonexistent(pe->record.path, 99);
    if (!path) {
        DP_warn("Can't restart recording: no available file name after %s",
                pe->record.path);
        DP_paint_engine_recorder_stop(pe);
        return;
    }

    JSON_Value *header =
        DP_recorder_header_clone(DP_recorder_header(pe->record.recorder));
    if (!header) {
        DP_warn("Can't restart recording:%s", DP_error());
        DP_free(path);
        DP_paint_engine_recorder_stop(pe);
        return;
    }

    DP_RecorderType type = DP_recorder_type(pe->record.recorder);
    DP_paint_engine_recorder_stop(pe);
    if (path && !start_recording(pe, type, header, path, false)) {
        DP_warn("Can't restart recording: %s", DP_error());
    }
}

static void record_message(DP_PaintEngine *pe, DP_Message *msg,
                           DP_MessageType type)
{
    DP_Recorder *r = pe->record.recorder;
    if (r) {
        if (type == DP_MSG_INTERNAL) {
            DP_MsgInternal *mi = DP_message_internal(msg);
            if (DP_msg_internal_type(mi) == DP_MSG_INTERNAL_TYPE_RESET) {
                restart_recording(pe);
            }
        }
        else if (!DP_message_type_control(type)) {
            if (!DP_recorder_message_push_inc(r, msg)) {
                DP_warn("Failed to push message to recorder: %s", DP_error());
                DP_paint_engine_recorder_stop(pe);
            }
        }
    }
}

static void handle_laser_trail(DP_PaintEngine *pe, DP_Message *msg)
{
    DP_MsgLaserTrail *mlt = DP_message_internal(msg);
    DP_PaintEngineCursorChange change = {
        DP_MSG_LASER_TRAIL, DP_message_context_id(msg),
        .laser = {DP_msg_laser_trail_persistence(mlt),
                  DP_msg_laser_trail_color(mlt)}};
    DP_VECTOR_PUSH_TYPE(&pe->meta.cursor_changes, DP_PaintEngineCursorChange,
                        change);
}

static void handle_move_pointer(DP_PaintEngine *pe, DP_Message *msg)
{
    DP_MsgMovePointer *mmp = DP_message_internal(msg);
    DP_PaintEngineCursorChange change = {
        DP_MSG_MOVE_POINTER, DP_message_context_id(msg),
        .move = {DP_msg_move_pointer_x(mmp), DP_msg_move_pointer_y(mmp)}};
    DP_VECTOR_PUSH_TYPE(&pe->meta.cursor_changes, DP_PaintEngineCursorChange,
                        change);
}

static int should_push_message_remote(DP_PaintEngine *pe, DP_Message *msg,
                                      bool override_acls)
{
    DP_MessageType type = DP_message_type(msg);
    record_message(pe, msg, type);
    DP_AclState *acls = pe->acls;
    uint8_t result = DP_acl_state_handle(acls, msg, override_acls);
    pe->meta.acl_change_flags |= result;
    if (result & DP_ACL_STATE_FILTERED_BIT) {
        DP_debug("ACL filtered %s message from user %u",
                 DP_message_type_enum_name_unprefixed(DP_message_type(msg)),
                 DP_message_context_id(msg));
        // If our own message was filtered, our local fork is desynchronized.
        // Tell the paint engine to go clear it to get back to a valid state.
        if (DP_message_context_id(msg) == DP_acl_state_local_user_id(acls)) {
            return PUSH_CLEAR_LOCAL_FORK;
        }
        else {
            return NO_PUSH;
        }
    }
    else {
        DP_local_state_handle(pe->local_state, pe->main_dc, msg);
        if (is_pushable_type(type) || type == DP_MSG_UNDO_DEPTH
            || type == DP_MSG_SOFT_RESET) {
            return PUSH_MESSAGE;
        }
        else if (type == DP_MSG_LASER_TRAIL) {
            if (DP_message_context_id(msg)
                != DP_acl_state_local_user_id(acls)) {
                handle_laser_trail(pe, msg);
            }
            return NO_PUSH;
        }
        else if (type == DP_MSG_MOVE_POINTER) {
            if (DP_message_context_id(msg)
                != DP_acl_state_local_user_id(acls)) {
                handle_move_pointer(pe, msg);
            }
            return NO_PUSH;
        }
        else {
            return NO_PUSH;
        }
    }
}

static int should_push_message_local(DP_UNUSED DP_PaintEngine *pe,
                                     DP_Message *msg,
                                     DP_UNUSED bool ignore_acls)
{
    DP_MessageType type = DP_message_type(msg);
    if (is_pushable_type(type)) {
        return PUSH_MESSAGE;
    }
    else {
        if (type == DP_MSG_LASER_TRAIL) {
            handle_laser_trail(pe, msg);
        }
        else if (type == DP_MSG_MOVE_POINTER) {
            handle_move_pointer(pe, msg);
        }
        return NO_PUSH;
    }
}

static int push_more_messages(DP_PaintEngine *pe, DP_Queue *queue,
                              bool override_acls, int count, DP_Message **msgs,
                              int (*should_push)(DP_PaintEngine *, DP_Message *,
                                                 bool))
{
    int pushed = 1;
    for (int i = 1; i < count; ++i) {
        DP_Message *msg = msgs[i];
        switch (should_push(pe, msg, override_acls)) {
        case NO_PUSH:
            break;
        case PUSH_MESSAGE:
            DP_message_queue_push_inc(queue, msg);
            ++pushed;
            break;
        case PUSH_CLEAR_LOCAL_FORK:
            DP_message_queue_push_noinc(
                queue, DP_msg_internal_local_fork_clear_new(0));
            ++pushed;
            break;
        default:
            DP_UNREACHABLE();
        }
    }
    DP_SEMAPHORE_MUST_POST_N(pe->queue_sem, pushed);
    return pushed;
}

static int push_messages(DP_PaintEngine *pe, DP_Queue *queue,
                         bool override_acls, int count, DP_Message **msgs,
                         int (*should_push)(DP_PaintEngine *, DP_Message *,
                                            bool))
{
    DP_MUTEX_MUST_LOCK(pe->queue_mutex);
    // First message is the one that triggered the call to this function,
    // push it unconditionally. Then keep checking the rest again.
    DP_message_queue_push_inc(queue, msgs[0]);
    int pushed =
        push_more_messages(pe, queue, override_acls, count, msgs, should_push);
    DP_MUTEX_MUST_UNLOCK(pe->queue_mutex);
    return pushed;
}

static int push_clear_local_fork_messages(
    DP_PaintEngine *pe, DP_Queue *queue, bool override_acls, int count,
    DP_Message **msgs, int (*should_push)(DP_PaintEngine *, DP_Message *, bool))
{
    DP_MUTEX_MUST_LOCK(pe->queue_mutex);
    // First message is to instruct the paint engine to clear the local fork.
    DP_message_queue_push_noinc(queue, DP_msg_internal_local_fork_clear_new(0));
    int pushed =
        push_more_messages(pe, queue, override_acls, count, msgs, should_push);
    DP_MUTEX_MUST_UNLOCK(pe->queue_mutex);
    return pushed;
}

int DP_paint_engine_handle_inc(DP_PaintEngine *pe, bool local,
                               bool override_acls, int count, DP_Message **msgs,
                               DP_PaintEngineAclsChangedFn acls_changed,
                               DP_PaintEngineLaserTrailFn laser_trail,
                               DP_PaintEngineMovePointerFn move_pointer,
                               void *user)
{
    DP_ASSERT(pe);
    DP_ASSERT(msgs);
    DP_PERF_BEGIN_DETAIL(fn, "handle", "count=%d,local=%d", count, local);

    int (*should_push)(DP_PaintEngine *, DP_Message *, bool) =
        local ? should_push_message_local : should_push_message_remote;

    pe->meta.acl_change_flags = 0;
    DP_Vector *cursor_changes = &pe->meta.cursor_changes;
    cursor_changes->used = 0;

    // Don't lock anything until we actually find a message to push.
    int pushed = 0;
    for (int i = 0; i < count; ++i) {
        int push = should_push(pe, msgs[i], override_acls);
        if (push) {
            DP_PERF_BEGIN(push, "handle:push");
            pushed = (push == PUSH_MESSAGE ? push_messages
                                           : push_clear_local_fork_messages)(
                pe, local ? &pe->local_queue : &pe->remote_queue, override_acls,
                count - i, msgs + i, should_push);
            DP_PERF_END(push);
            break;
        }
    }

    int acl_change_flags = pe->meta.acl_change_flags & DP_ACL_STATE_CHANGE_MASK;
    if (acl_change_flags != 0) {
        acls_changed(user, acl_change_flags);
    }

    size_t cursor_change_count = cursor_changes->used;
    for (size_t i = 0; i < cursor_change_count; ++i) {
        DP_PaintEngineCursorChange change =
            DP_VECTOR_AT_TYPE(cursor_changes, DP_PaintEngineCursorChange, i);
        switch (change.type) {
        case DP_MSG_LASER_TRAIL:
            laser_trail(user, change.context_id, change.laser.persistence,
                        change.laser.color);
            break;
        case DP_MSG_MOVE_POINTER:
            move_pointer(user, change.context_id, change.move.x, change.move.y);
            break;
        default:
            DP_UNREACHABLE();
        }
    }

    DP_PERF_END(fn);
    return pushed;
}


static DP_CanvasState *apply_previews(DP_PaintEngine *pe, DP_CanvasState *cs)
{
    DP_CanvasState *next_cs = cs;
    DP_PreviewRenderer *pvr = pe->preview_renderer;
    for (int i = 0; i < DP_PREVIEW_COUNT; ++i) {
        DP_Preview *pv = pe->previews[i];
        if (pv && pv != &DP_preview_null) {
            DP_PERF_BEGIN_DETAIL(preview, "tick:changes:preview", "type=%d", i);
            next_cs = DP_preview_apply(pv, next_cs, pvr);
            DP_PERF_END(preview);
        }
    }
    return next_cs;
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


static DP_TransientLayerContent *
make_inspect_sublayer(DP_TransientCanvasState *tcs, DP_DrawContext *dc,
                      int blend_mode)
{
    int index_count;
    int *indexes = DP_draw_context_layer_indexes(dc, &index_count);
    DP_TransientLayerContent *tlc =
        DP_layer_routes_entry_indexes_transient_content(index_count, indexes,
                                                        tcs);
    DP_TransientLayerContent *sub_tlc;
    DP_TransientLayerProps *sub_tlp;
    DP_transient_layer_content_transient_sublayer(tlc, INSPECT_SUBLAYER_ID,
                                                  &sub_tlc, &sub_tlp);
    DP_transient_layer_props_opacity_set(sub_tlp, DP_BIT15 - DP_BIT15 / 4);
    DP_transient_layer_props_blend_mode_set(sub_tlp, blend_mode);
    return sub_tlc;
}

static void maybe_add_inspect_sublayer(DP_TransientCanvasState *tcs,
                                       DP_DrawContext *dc,
                                       unsigned int context_id, int blend_mode,
                                       int xtiles, int ytiles,
                                       DP_LayerContent *lc)
{
    DP_TransientLayerContent *sub_tlc = NULL;
    for (int y = 0; y < ytiles; ++y) {
        for (int x = 0; x < xtiles; ++x) {
            DP_Tile *t = DP_layer_content_tile_at_noinc(lc, x, y);
            if (t && DP_tile_context_id(t) == context_id) {
                if (!sub_tlc) {
                    sub_tlc = make_inspect_sublayer(tcs, dc, blend_mode);
                }
                DP_transient_layer_content_tile_set_noinc(
                    sub_tlc, DP_tile_censored_inc(), y * xtiles + x);
            }
        }
    }
}

static void apply_inspect_recursive(DP_TransientCanvasState *tcs,
                                    DP_DrawContext *dc, unsigned int context_id,
                                    int blend_mode, int xtiles, int ytiles,
                                    DP_LayerList *ll)
{
    int count = DP_layer_list_count(ll);
    DP_draw_context_layer_indexes_push(dc);
    for (int i = 0; i < count; ++i) {
        DP_draw_context_layer_indexes_set(dc, i);
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        if (DP_layer_list_entry_is_group(lle)) {
            DP_LayerGroup *lg = DP_layer_list_entry_group_noinc(lle);
            DP_LayerList *child_lle = DP_layer_group_children_noinc(lg);
            apply_inspect_recursive(tcs, dc, context_id, blend_mode, xtiles,
                                    ytiles, child_lle);
        }
        else {
            DP_LayerContent *lc = DP_layer_list_entry_content_noinc(lle);
            maybe_add_inspect_sublayer(tcs, dc, context_id, blend_mode, xtiles,
                                       ytiles, lc);
        }
    }
    DP_draw_context_layer_indexes_pop(dc);
}

static DP_CanvasState *apply_inspect(DP_PaintEngine *pe, DP_CanvasState *cs)
{
    unsigned int context_id = pe->local_view.inspect_context_id;
    if (context_id == 0) {
        return cs;
    }
    else {
        DP_PERF_BEGIN(inspect, "tick:changes:inspect");
        DP_TransientCanvasState *tcs = get_or_make_transient_canvas_state(cs);
        DP_DrawContext *dc = pe->main_dc;
        DP_draw_context_layer_indexes_clear(dc);
        DP_TileCounts tile_counts = DP_tile_counts_round(
            DP_canvas_state_width(cs), DP_canvas_state_height(cs));
        apply_inspect_recursive(
            tcs, dc, context_id,
            pe->local_view.inspect_show_tiles ? DP_BLEND_MODE_NORMAL
                                              : DP_BLEND_MODE_RECOLOR,
            tile_counts.x, tile_counts.y, DP_canvas_state_layers_noinc(cs));
        DP_PERF_END(inspect);
        return (DP_CanvasState *)tcs;
    }
}


// Only grab the timeline if we're in a view mode where that's relevant.
static DP_Timeline *maybe_get_timeline(DP_CanvasState *cs, DP_ViewMode vm)
{
    bool want_timeline = vm == DP_VIEW_MODE_FRAME;
    return want_timeline ? DP_canvas_state_timeline_noinc(cs) : NULL;
}

static void set_local_layer_props_recursive(
    DP_TransientCanvasState *tcs, DP_DrawContext *dc, bool reveal_censored,
    DP_LayerPropsList *lpl,
    DP_PaintEngineCensoredLayerRevealedFn censored_layer_revealed, void *user)
{
    int count = DP_layer_props_list_count(lpl);
    DP_draw_context_layer_indexes_push(dc);
    for (int i = 0; i < count; ++i) {
        DP_draw_context_layer_indexes_set(dc, i);

        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        // Hidden layers are supposed to be purely local state. If the remote
        // state gives us hidden layers, we unhide them first. That may get
        // undone by the hidden layers processing that comes after this step.
        bool needs_show = DP_layer_props_hidden(lp);
        bool needs_reveal = reveal_censored && DP_layer_props_censored(lp);
        if (needs_show || needs_reveal) {
            int index_count;
            int *indexes = DP_draw_context_layer_indexes(dc, &index_count);
            DP_TransientLayerProps *tlp =
                DP_layer_routes_entry_indexes_transient_props(index_count,
                                                              indexes, tcs);
            if (needs_show) {
                DP_transient_layer_props_hidden_set(tlp, false);
            }
            if (needs_reveal) {
                DP_transient_layer_props_censored_set(tlp, false);
                censored_layer_revealed(user, DP_layer_props_id(lp));
            }
        }

        DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
        if (child_lpl) {
            set_local_layer_props_recursive(tcs, dc, reveal_censored, child_lpl,
                                            censored_layer_revealed, user);
        }
    }
    DP_draw_context_layer_indexes_pop(dc);
}

static void set_hidden_layer_props(DP_PaintEngine *pe,
                                   DP_TransientCanvasState *tcs)
{
    int count;
    const int *hidden_layer_ids =
        DP_local_state_hidden_layer_ids(pe->local_state, &count);
    DP_LayerRoutes *lr = DP_transient_canvas_state_layer_routes_noinc(tcs);
    for (int i = 0; i < count; ++i) {
        DP_LayerRoutesEntry *lre =
            DP_layer_routes_search(lr, hidden_layer_ids[i]);
        if (lre) {
            DP_TransientLayerProps *tlp =
                DP_layer_routes_entry_transient_props(lre, tcs);
            DP_transient_layer_props_hidden_set(tlp, true);
        }
    }
}

static DP_CanvasState *set_local_layer_props(
    DP_PaintEngine *pe, DP_CanvasState *cs,
    DP_PaintEngineCensoredLayerRevealedFn censored_layer_revealed, void *user)
{
    DP_TransientCanvasState *tcs = get_or_make_transient_canvas_state(cs);

    bool reveal_censored = pe->local_view.reveal_censored;
    DP_DrawContext *dc = pe->main_dc;
    DP_draw_context_layer_indexes_clear(dc);
    set_local_layer_props_recursive(
        tcs, dc, reveal_censored,
        DP_transient_canvas_state_layer_props_noinc(tcs),
        censored_layer_revealed, user);

    set_hidden_layer_props(pe, tcs);

    // We remember the root layer props list for later and then jam it into
    // subsequent canvas states. That'll make them diff correctly instead of
    // reporting a layer props change on every tick.
    DP_layer_props_list_decref_nullable(pe->local_view.layers.lpl);
    pe->local_view.layers.lpl =
        DP_layer_props_list_incref(DP_transient_layer_props_list_persist(
            DP_transient_canvas_state_transient_layer_props(tcs, 0)));
    return (DP_CanvasState *)tcs;
}

static DP_CanvasState *apply_local_layer_props(
    DP_PaintEngine *pe, DP_CanvasState *cs,
    DP_PaintEngineCensoredLayerRevealedFn censored_layer_revealed, void *user)
{
    DP_ViewMode vm = DP_local_state_view_mode(pe->local_state);
    DP_LayerPropsList *lpl = DP_canvas_state_layer_props_noinc(cs);
    DP_Timeline *tl = maybe_get_timeline(cs, vm);
    // This function here may replace the layer props entirely, so there can't
    // have been any meddling with it beforehand. Any layer props changes must
    // be part of this function so that they're cached properly.
    DP_ASSERT(!DP_layer_props_list_transient(lpl));
    DP_LayerPropsList *prev_lpl = pe->local_view.layers.prev_lpl;
    DP_Timeline *prev_tl = pe->local_view.layers.prev_tl;
    if (lpl == prev_lpl && tl == prev_tl) {
        // Layer state didn't change, jam the previous version in there.
        DP_TransientCanvasState *tcs = get_or_make_transient_canvas_state(cs);
        DP_transient_canvas_state_layer_props_set_inc(
            tcs, pe->local_view.layers.lpl);
        return (DP_CanvasState *)tcs;
    }
    else {
        DP_PERF_BEGIN(local, "tick:changes:local");
        if (vm == DP_VIEW_MODE_FRAME && tl != prev_tl) {
            // We're currently viewing the canvas in single-frame mode and the
            // timeline has been manipulated just now. The diff won't pick up
            // that it needs to potentially re-render some tiles, since it
            // doesn't have all that context. We'll just mark all tiles as
            // needing re-rendering instead of complicating the diff logic.
            DP_canvas_diff_check_all(pe->diff);
        }
        DP_layer_props_list_decref_nullable(prev_lpl);
        DP_timeline_decref_nullable(prev_tl);
        pe->local_view.layers.prev_lpl = DP_layer_props_list_incref(lpl);
        pe->local_view.layers.prev_tl = DP_timeline_incref_nullable(tl);
        DP_CanvasState *next_cs =
            set_local_layer_props(pe, cs, censored_layer_revealed, user);
        DP_PERF_END(local);
        return next_cs;
    }
}

static bool get_local_track_state(const DP_LocalTrackState *track_states,
                                  int count, int track_id, bool *out_hidden,
                                  bool *out_onion_skin)
{
    for (int i = 0; i < count; ++i) {
        const DP_LocalTrackState *lts = &track_states[i];
        if (lts->track_id == track_id) {
            *out_hidden = lts->hidden;
            *out_onion_skin = lts->onion_skin;
            return true;
        }
    }
    *out_hidden = false;
    *out_onion_skin = false;
    return false;
}

static DP_CanvasState *apply_local_track_state(DP_PaintEngine *pe,
                                               DP_CanvasState *cs)
{
    DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
    if (tl == pe->local_view.tracks.prev_tl) {
        // Timeline didn't change, jam the previous one in there or leave as-is.
        DP_Timeline *tracks_tl = pe->local_view.tracks.tl;
        if (tracks_tl) {
            DP_TransientCanvasState *tcs =
                get_or_make_transient_canvas_state(cs);
            DP_transient_canvas_state_timeline_set_inc(tcs, tracks_tl);
            return (DP_CanvasState *)tcs;
        }
        else {
            return cs;
        }
    }
    else {
        // Timeline changed, make a new local view.
        int lts_count;
        const DP_LocalTrackState *track_states =
            DP_local_state_track_states(pe->local_state, &lts_count);
        DP_TransientCanvasState *tcs = NULL;
        DP_TransientTimeline *ttl = NULL;
        int track_count = DP_timeline_count(tl);
        for (int i = 0; i < track_count; ++i) {
            DP_Track *t = DP_timeline_at_noinc(tl, i);
            bool hidden, onion_skin;
            get_local_track_state(track_states, lts_count, DP_track_id(t),
                                  &hidden, &onion_skin);
            bool local_state_changed = DP_track_hidden(t) != hidden
                                    || DP_track_onion_skin(t) != onion_skin;
            if (local_state_changed) {
                if (!tcs) {
                    tcs = get_or_make_transient_canvas_state(cs);
                    ttl = DP_transient_canvas_state_transient_timeline(tcs, 0);
                }
                DP_TransientTrack *tt =
                    DP_transient_timeline_transient_at_noinc(ttl, i, 0);
                DP_transient_track_hidden_set(tt, hidden);
                DP_transient_track_onion_skin_set(tt, onion_skin);
            }
        }
        // Remember the local view (or NULL if there's no change) for next time.
        DP_timeline_decref_nullable(pe->local_view.tracks.prev_tl);
        DP_timeline_decref_nullable(pe->local_view.tracks.tl);
        pe->local_view.tracks.prev_tl = DP_timeline_incref(tl);
        if (tcs) {
            pe->local_view.tracks.tl =
                DP_timeline_incref(DP_transient_timeline_persist(ttl));
            return (DP_CanvasState *)tcs;
        }
        else {
            pe->local_view.tracks.tl = NULL;
            return cs;
        }
    }
}

static DP_CanvasState *apply_local_background_tile(DP_PaintEngine *pe,
                                                   DP_CanvasState *cs)
{
    DP_LocalState *ls = pe->local_state;
    DP_Tile *t = DP_local_state_background_tile_noinc(ls);
    if (t) {
        DP_TransientCanvasState *tcs = get_or_make_transient_canvas_state(cs);
        DP_transient_canvas_state_background_tile_set_noinc(
            tcs, DP_tile_incref(t), DP_local_state_background_opaque(ls));
        return (DP_CanvasState *)tcs;
    }
    else {
        return cs;
    }
}

static void
emit_changes(DP_PaintEngine *pe, DP_CanvasState *prev, DP_CanvasState *cs,
             bool catching_up, bool catchup_done, DP_Rect tile_bounds,
             bool render_outside_tile_bounds,
             DP_PaintEngineLayerPropsChangedFn layer_props_changed,
             DP_PaintEngineAnnotationsChangedFn annotations_changed,
             DP_PaintEngineDocumentMetadataChangedFn document_metadata_changed,
             DP_PaintEngineTimelineChangedFn timeline_changed,
             DP_PaintEngineSelectionsChangedFn selections_changed,
             DP_PaintEngineCursorMovedFn cursor_moved, void *user)
{
    DP_CanvasDiff *diff = pe->diff;
    DP_canvas_state_diff(cs, prev, diff);
    DP_renderer_apply(pe->renderer, cs, pe->local_state, diff,
                      pe->local_view.layers_can_decrease_opacity,
                      pe->local_view.checker_color1,
                      pe->local_view.checker_color2, tile_bounds,
                      render_outside_tile_bounds, DP_RENDERER_CONTINUOUS);

    if (!catching_up) {
        if (DP_canvas_diff_layer_props_changed_reset(diff) || catchup_done) {
            layer_props_changed(user, DP_canvas_state_layer_props_noinc(cs));
        }

        DP_AnnotationList *al = DP_canvas_state_annotations_noinc(cs);
        if (al != DP_canvas_state_annotations_noinc(prev) || catchup_done) {
            annotations_changed(user, al);
        }

        DP_DocumentMetadata *dm = DP_canvas_state_metadata_noinc(cs);
        if (dm != DP_canvas_state_metadata_noinc(prev) || catchup_done) {
            document_metadata_changed(user, dm);
        }

        DP_Timeline *tl = DP_canvas_state_timeline_noinc(cs);
        if (tl != DP_canvas_state_timeline_noinc(prev) || catchup_done) {
            timeline_changed(user, tl);
        }

        DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
        if (ss != DP_canvas_state_selections_noinc_nullable(prev)
            || catchup_done) {
            selections_changed(user, ss);
        }
    }

    DP_UserCursorBuffer *ucb = &pe->meta.ucb;
    int cursors_count = ucb->count;
    for (int i = 0; i < cursors_count; ++i) {
        DP_UserCursor *uc = &ucb->cursors[i];
        cursor_moved(user, uc->flags, uc->context_id, uc->layer_id, uc->x,
                     uc->y);
    }
    ucb->count = 0;
}

void DP_paint_engine_tick(
    DP_PaintEngine *pe, DP_Rect tile_bounds, bool render_outside_tile_bounds,
    DP_PaintEngineCatchupFn catchup,
    DP_PaintEngineResetLockChangedFn reset_lock_changed,
    DP_PaintEngineRecorderStateChangedFn recorder_state_changed,
    DP_PaintEngineLayerPropsChangedFn layer_props_changed,
    DP_PaintEngineAnnotationsChangedFn annotations_changed,
    DP_PaintEngineDocumentMetadataChangedFn document_metadata_changed,
    DP_PaintEngineTimelineChangedFn timeline_changed,
    DP_PaintEngineSelectionsChangedFn selections_changed,
    DP_PaintEngineCursorMovedFn cursor_moved,
    DP_PaintEngineDefaultLayerSetFn default_layer_set,
    DP_PaintEngineUndoDepthLimitSetFn undo_depth_limit_set,
    DP_PaintEngineCensoredLayerRevealedFn censored_layer_revealed, void *user)
{
    DP_ASSERT(pe);
    DP_ASSERT(catchup);
    DP_ASSERT(layer_props_changed);
    DP_PERF_BEGIN(fn, "tick");

    int progress = DP_atomic_xch(&pe->catchup, -1);
    bool was_catching_up = pe->catching_up;
    if (progress != -1) {
        pe->catching_up = progress < 100;
    }

    switch (pe->record.state_change) {
    case RECORDER_STARTED:
        pe->record.state_change = RECORDER_UNCHANGED;
        recorder_state_changed(user, true);
        break;
    case RECORDER_STOPPED:
        pe->record.state_change = RECORDER_UNCHANGED;
        recorder_state_changed(user, false);
        break;
    default:
        break;
    }

    // There's two flags that block on reset. The just_reset one gets set by a
    // reset message and cleared when the first canvas resize comes in, the
    // catching_up flag gets set when a catchup message is received and cleared
    // when it reaches 100%. This may seem doubled up at first glance, but since
    // the catchup message comes in *after* the reset message, there is a small
    // chance that we grab the null canvas state right in between those two.
    bool just_reset = DP_atomic_get(&pe->just_reset);
    bool catching_up = pe->catching_up;
    DP_CanvasState *next_history_cs;
    bool preview_changed;
    bool local_view_changed;

    if (just_reset) {
        next_history_cs = NULL;
        preview_changed = false;
        local_view_changed = false;
    }
    else {
        DP_CanvasState *prev_history_cs = pe->history_cs;
        next_history_cs = DP_canvas_history_compare_and_get(
            pe->ch, prev_history_cs, &pe->meta.ucb);
        if (next_history_cs) {
            DP_canvas_state_decref(prev_history_cs);
            pe->history_cs = next_history_cs;
        }

        preview_changed = DP_atomic_xch(&pe->preview_rerendered, false);
        for (int i = 0; i < DP_PREVIEW_COUNT; ++i) {
            DP_Preview *next_preview =
                DP_atomic_ptr_xch(&pe->next_previews[i], NULL);
            if (next_preview) {
                DP_preview_decref_nullable(pe->previews[i]);
                pe->previews[i] =
                    next_preview == &DP_preview_null ? NULL : next_preview;
                preview_changed = true;
            }
        }

        local_view_changed =
            !pe->local_view.layers.prev_lpl || !pe->local_view.tracks.prev_tl;
    }

    bool catchup_done = was_catching_up && !catching_up;
    bool have_changes = next_history_cs || preview_changed || local_view_changed
                     || catchup_done;
    if (have_changes) {
        DP_PERF_BEGIN(changes, "tick:changes");
        // Previews, hidden layers etc. are local changes, so we have to apply
        // them on top of the canvas state we got out of the history.
        DP_CanvasState *prev_view_cs = pe->view_cs;
        DP_CanvasState *next_view_cs = DP_canvas_state_incref(pe->history_cs);
        next_view_cs = apply_previews(pe, next_view_cs);
        next_view_cs = apply_inspect(pe, next_view_cs);
        next_view_cs = apply_local_layer_props(pe, next_view_cs,
                                               censored_layer_revealed, user);
        next_view_cs = apply_local_track_state(pe, next_view_cs);
        next_view_cs = apply_local_background_tile(pe, next_view_cs);
        pe->view_cs = next_view_cs;

        DP_LayerPropsList *lpl =
            DP_canvas_state_layer_props_noinc(next_view_cs);
        if (lpl != DP_canvas_state_layer_props_noinc(prev_view_cs)) {
            pe->local_view.layers_can_decrease_opacity =
                DP_layer_props_list_can_decrease_opacity(lpl);
        }

        emit_changes(pe, prev_view_cs, next_view_cs, catching_up, catchup_done,
                     tile_bounds, render_outside_tile_bounds,
                     layer_props_changed, annotations_changed,
                     document_metadata_changed, timeline_changed,
                     selections_changed, cursor_moved, user);
        DP_canvas_state_decref(prev_view_cs);
        DP_PERF_END(changes);
    }

    bool reset_locked = just_reset || catching_up;
    if (reset_locked != pe->reset_locked) {
        DP_info("Reset lock changed to %d (reset %d, catchup %d)", reset_locked,
                just_reset, catching_up);
        pe->reset_locked = reset_locked;
        reset_lock_changed(user, reset_locked);
    }

    if (progress != -1) {
        catchup(user, progress);
    }

    int default_layer_id = DP_atomic_xch(&pe->default_layer_id, -1);
    if (default_layer_id != -1) {
        default_layer_set(user, default_layer_id);
    }

    int undo_depth_limit = DP_atomic_xch(&pe->undo_depth_limit, -1);
    if (undo_depth_limit != -1) {
        undo_depth_limit_set(user, undo_depth_limit);
    }

    DP_PERF_END(fn);
}

void DP_paint_engine_render_continuous(DP_PaintEngine *pe, DP_Rect tile_bounds,
                                       bool render_outside_tile_bounds)
{
    DP_renderer_apply(pe->renderer, pe->view_cs, pe->local_state, pe->diff,
                      pe->local_view.layers_can_decrease_opacity,
                      pe->local_view.checker_color1,
                      pe->local_view.checker_color2, tile_bounds,
                      render_outside_tile_bounds, DP_RENDERER_CONTINUOUS);
}

void DP_paint_engine_change_bounds(DP_PaintEngine *pe, DP_Rect tile_bounds,
                                   bool render_outside_tile_bounds)
{
    DP_renderer_apply(pe->renderer, pe->view_cs, pe->local_state, pe->diff,
                      pe->local_view.layers_can_decrease_opacity,
                      pe->local_view.checker_color1,
                      pe->local_view.checker_color2, tile_bounds,
                      render_outside_tile_bounds,
                      DP_RENDERER_VIEW_BOUNDS_CHANGED);
}

void DP_paint_engine_render_everything(DP_PaintEngine *pe)
{
    DP_renderer_apply(pe->renderer, pe->view_cs, pe->local_state, pe->diff,
                      pe->local_view.layers_can_decrease_opacity,
                      pe->local_view.checker_color1,
                      pe->local_view.checker_color2,
                      DP_rect_make(0, 0, UINT16_MAX, UINT16_MAX), false,
                      DP_RENDERER_EVERYTHING);
}


void DP_paint_engine_preview_cut(DP_PaintEngine *pe, int x, int y, int width,
                                 int height, const DP_Pixel8 *mask_or_null,
                                 int layer_id_count, const int *layer_ids)
{
    DP_ASSERT(pe);
    if (width > 0 && height > 0 && layer_id_count > 0) {
        DP_CanvasState *cs = pe->view_cs;
        int offset_x = DP_canvas_state_offset_x(cs);
        int offset_y = DP_canvas_state_offset_y(cs);
        DP_Preview *pv =
            DP_preview_new_cut(offset_x, offset_y, x, y, width, height,
                               mask_or_null, layer_id_count, layer_ids);
        DP_preview_renderer_push_noinc(
            pe->preview_renderer, pv, DP_canvas_state_width(cs),
            DP_canvas_state_height(cs), offset_x, offset_y);
    }
    else {
        DP_paint_engine_preview_clear(pe, DP_PREVIEW_CUT);
    }
}

void DP_paint_engine_preview_transform(
    DP_PaintEngine *pe, int id, int layer_id, int blend_mode, uint16_t opacity,
    int x, int y, int width, int height, const DP_Quad *dst_quad,
    int interpolation, DP_PreviewTransformGetPixelsFn get_pixels,
    DP_PreviewTransformDisposePixelsFn dispose_pixels, void *user)
{
    DP_ASSERT(dispose_pixels);
    if (width > 0 && height > 0) {
        DP_CanvasState *cs = pe->view_cs;
        int offset_x = DP_canvas_state_offset_x(cs);
        int offset_y = DP_canvas_state_offset_y(cs);
        DP_Preview *pv = DP_preview_new_transform(
            id, offset_x, offset_y, layer_id, blend_mode, opacity, x, y, width,
            height, dst_quad, interpolation, get_pixels, dispose_pixels, user);
        DP_preview_renderer_push_noinc(
            pe->preview_renderer, pv, DP_canvas_state_width(cs),
            DP_canvas_state_height(cs), offset_x, offset_y);
    }
    else {
        dispose_pixels(user);
        DP_paint_engine_preview_clear(pe, DP_PREVIEW_TRANSFORM_FIRST + id);
    }
}

void DP_paint_engine_preview_dabs_inc(DP_PaintEngine *pe, int layer_id,
                                      int count, DP_Message **messages)
{
    DP_ASSERT(pe);
    if (count > 0) {
        DP_CanvasState *cs = pe->view_cs;
        int offset_x = DP_canvas_state_offset_x(cs);
        int offset_y = DP_canvas_state_offset_y(cs);
        DP_Preview *pv = DP_preview_new_dabs_inc(offset_x, offset_y, layer_id,
                                                 count, messages);
        DP_preview_renderer_push_noinc(
            pe->preview_renderer, pv, DP_canvas_state_width(cs),
            DP_canvas_state_height(cs), offset_x, offset_y);
    }
    else {
        DP_paint_engine_preview_clear(pe, DP_PREVIEW_DABS);
    }
}

void DP_paint_engine_preview_fill(DP_PaintEngine *pe, int layer_id,
                                  int blend_mode, uint16_t opacity, int x,
                                  int y, int width, int height,
                                  const DP_Pixel8 *pixels)
{
    DP_ASSERT(pe);
    if (width > 0 && height > 0 && pixels) {
        DP_CanvasState *cs = pe->view_cs;
        int offset_x = DP_canvas_state_offset_x(cs);
        int offset_y = DP_canvas_state_offset_y(cs);
        DP_Preview *pv =
            DP_preview_new_fill(offset_x, offset_y, layer_id, blend_mode,
                                opacity, x, y, width, height, pixels);
        DP_preview_renderer_push_noinc(
            pe->preview_renderer, pv, DP_canvas_state_width(cs),
            DP_canvas_state_height(cs), offset_x, offset_y);
    }
    else {
        DP_paint_engine_preview_clear(pe, DP_PREVIEW_FILL);
    }
}

void DP_paint_engine_preview_clear(DP_PaintEngine *pe, int type)
{
    DP_ASSERT(pe);
    DP_ASSERT(type >= 0);
    DP_ASSERT(type < DP_PREVIEW_COUNT);
    DP_preview_renderer_cancel(pe->preview_renderer, type);
}

void DP_paint_engine_preview_clear_all_transforms(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    DP_preview_renderer_cancel_all_transforms(pe->preview_renderer);
}


DP_CanvasState *DP_paint_engine_view_canvas_state_inc(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_canvas_state_incref(pe->view_cs);
}

DP_CanvasState *DP_paint_engine_history_canvas_state_inc(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_canvas_state_incref(pe->history_cs);
}

DP_CanvasState *DP_paint_engine_sample_canvas_state_inc(DP_PaintEngine *pe)
{
    DP_ASSERT(pe);
    return DP_canvas_history_get(pe->ch);
}
