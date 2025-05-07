// SPDX-License-Identifier: GPL-3.0-or-later
#include "snapshots.h"
#include "annotation.h"
#include "annotation_list.h"
#include "canvas_history.h"
#include "canvas_state.h"
#include "compress.h"
#include "document_metadata.h"
#include "image.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "pixels.h"
#include "selection.h"
#include "selection_set.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/output.h>
#include <dpcommon/perf.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
#include <dpmsg/ids.h>
#include <dpmsg/message.h>

#define DP_PERF_CONTEXT "snapshots"


#define ELEMENT_SIZE (sizeof(DP_Snapshot))
#define FILL_BUCKETS 8

struct DP_Snapshot {
    long long timestamp_ms;
    DP_CanvasState *cs;
};

struct DP_SnapshotQueue {
    size_t max_count;
    long long min_delay_ms;
    struct {
        DP_SnapshotQueueTimestampMsFn fn;
        void *user;
    } timestamp;
    DP_Mutex *mutex;
    DP_Queue queue;
};


static void dispose_snapshot(DP_Snapshot *s)
{
    DP_ASSERT(s);
    DP_canvas_state_decref(s->cs);
}

long long DP_snapshot_timestamp_ms(DP_Snapshot *s)
{
    DP_ASSERT(s);
    return s->timestamp_ms;
}

DP_CanvasState *DP_snapshot_canvas_state_noinc(DP_Snapshot *s)
{
    DP_ASSERT(s);
    return s->cs;
}


DP_SnapshotQueue *
DP_snapshot_queue_new(size_t max_count, long long min_delay_ms,
                      DP_SnapshotQueueTimestampMsFn timestamp_fn,
                      void *timestamp_user)
{
    DP_ASSERT(timestamp_fn);
    DP_Mutex *mutex = DP_mutex_new();
    if (mutex) {
        DP_SnapshotQueue *sq = DP_malloc(sizeof(*sq));
        *sq = (DP_SnapshotQueue){max_count,
                                 min_delay_ms,
                                 {timestamp_fn, timestamp_user},
                                 mutex,
                                 DP_QUEUE_NULL};
        DP_queue_init(&sq->queue, DP_max_size(1, max_count), ELEMENT_SIZE);
        return sq;
    }
    else {
        return NULL;
    }
}

static void dispose_queued_snapshot(void *s)
{
    dispose_snapshot(s);
}

void DP_snapshot_queue_free(DP_SnapshotQueue *sq)
{
    if (sq) {
        DP_queue_clear(&sq->queue, ELEMENT_SIZE, dispose_queued_snapshot);
        DP_queue_dispose(&sq->queue);
        DP_mutex_free(sq->mutex);
        DP_free(sq);
    }
}


void DP_snapshot_queue_max_count_set(DP_SnapshotQueue *sq, size_t max_count)
{
    DP_ASSERT(sq);
    DP_Mutex *mutex = sq->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    sq->max_count = max_count;
    if (max_count == 0) {
        DP_queue_clear(&sq->queue, ELEMENT_SIZE, dispose_queued_snapshot);
    }
    else {
        while (sq->queue.used >= max_count) {
            dispose_snapshot(DP_queue_peek(&sq->queue, ELEMENT_SIZE));
            DP_queue_shift(&sq->queue);
        }
    }
    DP_MUTEX_MUST_UNLOCK(mutex);
}

void DP_snapshot_queue_min_delay_ms_set(DP_SnapshotQueue *sq,
                                        long long min_delay_ms)
{
    DP_ASSERT(sq);
    sq->min_delay_ms = min_delay_ms;
}


static bool should_make_snapshot(DP_SnapshotQueue *sq, DP_CanvasState *cs,
                                 bool snapshot_requested,
                                 long long timestamp_ms)
{
    if (sq->max_count == 0) {
        return false; // We're not supposed to make snapshots.
    }
    else {
        DP_Snapshot *s = DP_queue_peek_last(&sq->queue, ELEMENT_SIZE);
        if (s) {
            // Make a new snapshot if its canvas state would be different from
            // the last and if it was explicitly requested or enough time has
            // elapsed.
            return s->cs != cs
                && (snapshot_requested
                    || timestamp_ms - s->timestamp_ms >= sq->min_delay_ms);
        }
        else {
            return true; // There's no prior snapshot, make one now.
        }
    }
}

static void make_snapshot(DP_SnapshotQueue *sq, long long timestamp_ms,
                          DP_CanvasState *cs)
{
    size_t max_count = sq->max_count;
    if (max_count != 0) {
        while (sq->queue.used >= max_count) {
            dispose_snapshot(DP_queue_peek(&sq->queue, ELEMENT_SIZE));
            DP_queue_shift(&sq->queue);
        }
        DP_Snapshot *s = DP_queue_push(&sq->queue, ELEMENT_SIZE);
        *s = (DP_Snapshot){timestamp_ms, DP_canvas_state_incref(cs)};
    }
}

void DP_snapshot_queue_on_save_point(void *user, DP_CanvasState *cs,
                                     bool snapshot_requested)
{
    DP_SnapshotQueue *sq = user;
    // Don't snapshot if we're supposed to keep zero snapshots. Also don't
    // snapshot zero-size canvases, nobody wants to reset to those.
    bool want_snapshot = sq->max_count != 0 && DP_canvas_state_width(cs) > 0
                      && DP_canvas_state_height(cs) > 0;
    if (want_snapshot) {
        long long timestamp_ms = sq->timestamp.fn(sq->timestamp.user);
        if (should_make_snapshot(sq, cs, snapshot_requested, timestamp_ms)) {
            DP_Mutex *mutex = sq->mutex;
            DP_MUTEX_MUST_LOCK(mutex);
            make_snapshot(sq, timestamp_ms, cs);
            DP_MUTEX_MUST_UNLOCK(mutex);
        }
    }
}


static DP_Snapshot *snapshot_at(DP_SnapshotQueue *sq, size_t index)
{
    DP_ASSERT(sq);
    DP_ASSERT(index < sq->queue.used);
    return DP_queue_at(&sq->queue, ELEMENT_SIZE, index);
}

void DP_snapshot_queue_get_with(DP_SnapshotQueue *sq, DP_SnapshotsGetFn get_fn,
                                void *user)
{
    DP_ASSERT(sq);
    DP_ASSERT(get_fn);
    DP_Mutex *mutex = sq->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    get_fn(user, sq, sq->queue.used, snapshot_at);
    DP_MUTEX_MUST_UNLOCK(mutex);
}


struct DP_ResetImageOutputBuffer {
    size_t capacity;
    void *data;
};

union DP_ResetImagePixelBuffer {
    DP_Pixel8 tile[DP_TILE_LENGTH];
    DP_SplitTile8 split;
    uint8_t channel[DP_TILE_LENGTH];
};

struct DP_ResetImageBuffer {
    struct DP_ResetImageOutputBuffer output;
    union DP_ResetImagePixelBuffer *pixels;
    ZSTD_CCtx *zstd_context;
};

struct DP_ResetImageContext {
    DP_Worker *worker;
    DP_ResetImageOptions options;
    void (*handle_entry)(void *, const DP_ResetEntry *);
    void *handle_entry_user;
    struct DP_ResetImageBuffer *buffers;
};

enum DP_ResetImageJobType {
    DP_RESET_IMAGE_JOB_TILE,
    DP_RESET_IMAGE_JOB_SELECTION_TILE,
    DP_RESET_IMAGE_JOB_BACKGROUND,
    DP_RESET_IMAGE_JOB_THUMB,
};

struct DP_ResetImageTileJob {
    DP_Tile *t;
    int layer_index;
    int layer_id;
    int sublayer_id;
    int tile_index;
    int tile_run;
    bool holds_ref;
};

struct DP_ResetImageSelectionTileJob {
    DP_Tile *t;
    int selection_index;
    unsigned int context_id;
    int selection_id;
    int tile_index;
};

struct DP_ResetImageJob {
    struct DP_ResetImageContext *c;
    enum DP_ResetImageJobType type;
    union {
        struct DP_ResetImageTileJob tile;
        struct DP_ResetImageSelectionTileJob selection_tile;
        DP_Tile *background;
        DP_CanvasState *thumb;
    };
};

static void reset_image_handle(struct DP_ResetImageContext *c,
                               DP_ResetEntry entry)
{
    DP_PERF_BEGIN_DETAIL(fn, "image:handle", "type=%d", (int)entry.type);
    c->handle_entry(c->handle_entry_user, &entry);
    DP_PERF_END(fn);
}

static unsigned char *reset_image_get_output_buffer(size_t size, void *user)
{
    struct DP_ResetImageOutputBuffer *output_buffer = user;
    if (output_buffer->capacity < size) {
        DP_free(output_buffer->data);
        output_buffer->data = DP_malloc(size);
        output_buffer->capacity = size;
    }
    return output_buffer->data;
}

static size_t reset_image_compress_tile_gzip8be(struct DP_ResetImageContext *c,
                                                int buffer_index, DP_Tile *t)
{
    struct DP_ResetImageBuffer *buffer = &c->buffers[buffer_index];
    return t ? DP_tile_compress_deflate(t, buffer->pixels->tile,
                                        reset_image_get_output_buffer,
                                        &buffer->output)
             : DP_tile_compress_pixel8be(DP_pixel15_zero(),
                                         reset_image_get_output_buffer,
                                         &buffer->output);
}

static size_t reset_image_compress_tile_zstd8le(struct DP_ResetImageContext *c,
                                                int buffer_index, DP_Tile *t)
{
    struct DP_ResetImageBuffer *buffer = &c->buffers[buffer_index];
    return t ? DP_tile_compress_split_delta_zstd8le(
                   t, &buffer->zstd_context, &buffer->pixels->split,
                   reset_image_get_output_buffer, &buffer->output)
             : DP_tile_compress_pixel8le(DP_pixel15_zero(),
                                         reset_image_get_output_buffer,
                                         &buffer->output);
}

static size_t reset_image_compress_tile(struct DP_ResetImageContext *c,
                                        int buffer_index, DP_Tile *t)
{
    size_t size;
    switch (c->options.compression) {
    case DP_RESET_IMAGE_COMPRESSION_ZSTD8LE:
        size = reset_image_compress_tile_zstd8le(c, buffer_index, t);
        break;
    default:
        size = reset_image_compress_tile_gzip8be(c, buffer_index, t);
        break;
    }
    if (size == 0) {
        DP_warn("Reset image: error tile: %s", DP_error());
    }
    return size;
}

static size_t
reset_image_compress_selection_tile(struct DP_ResetImageContext *c,
                                    int buffer_index, DP_Tile *t)
{
    struct DP_ResetImageBuffer *buffer = &c->buffers[buffer_index];
    size_t size = DP_tile_compress_mask_delta_zstd8le(
        t, &buffer->zstd_context, buffer->pixels->channel,
        reset_image_get_output_buffer, &buffer->output);
    if (size == 0) {
        DP_warn("Reset image: error selection tile: %s", DP_error());
    }
    return size;
}

static int layers_to_reset_image(struct DP_ResetImageContext *c,
                                 int next_layer_index, int parent_index,
                                 int parent_id, DP_LayerList *ll,
                                 DP_LayerPropsList *lpl);

static bool layer_fill(DP_LayerContent *lc, DP_Pixel15 *out_pixel)
{
    int width = DP_layer_content_width(lc);
    int height = DP_layer_content_height(lc);
    int total = DP_tile_total_round(width, height);
    int threshold = (total + 1) / 2;
    int blanks = 0;
    int used = 0;
    int counts[FILL_BUCKETS];
    DP_Pixel15 pixels[FILL_BUCKETS];

    for (int i = 0; i < total; ++i) {
        DP_Tile *t = DP_layer_content_tile_at_index_noinc(lc, i);
        DP_Pixel15 pixel;
        if (DP_tile_same_pixel(t, &pixel) && pixel.a != 0) {
            int j;
            for (j = 0; j < used; ++j) {
                if (DP_pixel15_equal(pixel, pixels[j])) {
                    break;
                }
            }

            if (j < used) {
                if (++counts[j] > threshold) {
                    *out_pixel = pixel;
                    return true;
                }
            }
            else if (used < FILL_BUCKETS) {
                counts[used] = 1;
                pixels[used] = pixel;
                ++used;
            }
            else {
                break;
            }
        }
        else if (++blanks > threshold) {
            break;
        }
    }

    return false;
}

static void layer_to_reset_image(struct DP_ResetImageContext *c,
                                 int layer_index, int layer_id, int sublayer_id,
                                 int parent_index, int parent_id,
                                 DP_LayerProps *lp, uint32_t fill)
{
    reset_image_handle(
        c, (DP_ResetEntry){DP_RESET_ENTRY_LAYER,
                           .layer = {layer_index, layer_id, sublayer_id,
                                     parent_index, parent_id, fill, lp}});
}

static bool tile_is_effectively_blank(DP_Tile *t, DP_Pixel15 fill_pixel)
{
    if (fill_pixel.a == 0) {
        return !t;
    }
    else {
        return t && DP_tile_pixels_equal_pixel(t, fill_pixel);
    }
}

static void background_to_reset_image(struct DP_ResetImageContext *c,
                                      int buffer_index, DP_Tile *t)
{
    size_t size = reset_image_compress_tile(c, buffer_index, t);
    if (size != 0) {
        reset_image_handle(
            c, (DP_ResetEntry){
                   DP_RESET_ENTRY_BACKGROUND,
                   .background = {size, c->buffers[buffer_index].output.data}});
    }
}

static void tile_to_reset_image(struct DP_ResetImageContext *c,
                                int buffer_index, int layer_index, int layer_id,
                                int sublayer_id, int tile_index, int tile_run,
                                DP_Tile *t)
{
    size_t size = reset_image_compress_tile(c, buffer_index, t);
    if (size != 0) {
        reset_image_handle(
            c, (DP_ResetEntry){DP_RESET_ENTRY_TILE,
                               .tile = {layer_index, layer_id, sublayer_id,
                                        tile_index, tile_run,
                                        t ? DP_tile_context_id(t) : 0u, size,
                                        c->buffers[buffer_index].output.data}});
    }
}

static void selection_tile_to_reset_image(struct DP_ResetImageContext *c,
                                          int buffer_index, int selection_index,
                                          unsigned int context_id,
                                          int selection_id, int tile_index,
                                          DP_Tile *t)
{
    size_t size = reset_image_compress_selection_tile(c, buffer_index, t);
    if (size != 0) {
        reset_image_handle(
            c, (DP_ResetEntry){
                   DP_RESET_ENTRY_SELECTION_TILE,
                   .selection_tile = {selection_index, selection_id, tile_index,
                                      context_id, size,
                                      c->buffers[buffer_index].output.data}});
    }
}

static void thumb_to_reset_image(struct DP_ResetImageContext *c,
                                 DP_CanvasState *cs)
{
    DP_PERF_BEGIN(fn, "image:thumb");
    int canvas_width = DP_canvas_state_width(cs);
    int canvas_height = DP_canvas_state_height(cs);
    int thumb_width, thumb_height;
    DP_image_thumbnail_dimensions(
        canvas_width, canvas_height, c->options.thumb_width,
        c->options.thumb_height, &thumb_width, &thumb_height);
    double scale_x =
        DP_int_to_double(canvas_width) / DP_int_to_double(thumb_width);
    double scale_y =
        DP_int_to_double(canvas_height) / DP_int_to_double(thumb_height);

    DP_Image *thumb = DP_image_new(thumb_width, thumb_height);
    for (int y = 0; y < thumb_height; ++y) {
        for (int x = 0; x < thumb_width; ++x) {
            DP_image_pixel_at_set(
                thumb, x, y,
                DP_pixel15_to_8(DP_canvas_state_to_flat_pixel(
                    cs, DP_double_to_int(DP_int_to_double(x) * scale_x),
                    DP_double_to_int(DP_int_to_double(y) * scale_y))));
        }
    }

    void **buffer_ptr;
    size_t *size_ptr;
    DP_Output *output = DP_mem_output_new(64, false, &buffer_ptr, &size_ptr);
    bool write_ok = c->options.thumb_write(thumb, output);
    void *buffer = *buffer_ptr;
    size_t size = *size_ptr;
    DP_output_free(output);
    DP_image_free(thumb);

    if (!write_ok) {
        DP_warn("Failed to write reset thumbnail: %s", DP_error());
    }
    else if (!buffer || size == 0) {
        DP_warn("Writing reset thumbnail resulted in no data");
    }
    else {
        reset_image_handle(
            c, (DP_ResetEntry){DP_RESET_ENTRY_THUMB, .thumb = {size, buffer}});
    }
    DP_free(buffer);
    DP_PERF_END(fn);
}

static void reset_image_job(void *user, int thread_index)
{
    struct DP_ResetImageJob *job = user;
    switch (job->type) {
    case DP_RESET_IMAGE_JOB_TILE: {
        DP_Tile *t = job->tile.t;
        tile_to_reset_image(job->c, thread_index, job->tile.layer_index,
                            job->tile.layer_id, job->tile.sublayer_id,
                            job->tile.tile_index, job->tile.tile_run, t);
        if (job->tile.holds_ref) {
            DP_tile_decref(t);
        }
        return;
    }
    case DP_RESET_IMAGE_JOB_SELECTION_TILE:
        selection_tile_to_reset_image(
            job->c, thread_index, job->selection_tile.selection_index,
            job->selection_tile.context_id, job->selection_tile.selection_id,
            job->selection_tile.tile_index, job->selection_tile.t);
        return;
    case DP_RESET_IMAGE_JOB_BACKGROUND:
        background_to_reset_image(job->c, thread_index, job->background);
        return;
    case DP_RESET_IMAGE_JOB_THUMB:
        thumb_to_reset_image(job->c, job->thumb);
        return;
    }
    DP_UNREACHABLE();
}

static void flush_tile(struct DP_ResetImageContext *c, int layer_index,
                       int layer_id, int sublayer_id, int tile_index,
                       int tile_run, DP_Tile *t, bool ephemeral)
{
    DP_Worker *worker = c->worker;
    if (worker) {
        bool hold_ref = ephemeral && t;
        struct DP_ResetImageJob job = {
            c, DP_RESET_IMAGE_JOB_TILE,
            .tile = {hold_ref ? DP_tile_incref(t) : t, layer_index, layer_id,
                     sublayer_id, tile_index, tile_run, hold_ref}};
        DP_worker_push(worker, &job);
    }
    else {
        tile_to_reset_image(c, 0, layer_index, layer_id, sublayer_id,
                            tile_index, tile_run, t);
    }
}

static void tiles_to_reset_image(struct DP_ResetImageContext *c,
                                 DP_LayerContent *lc, int layer_index,
                                 int layer_id, int sublayer_id,
                                 DP_Pixel15 fill_pixel, bool ephemeral)
{
    int count = DP_tile_total_round(DP_layer_content_width(lc),
                                    DP_layer_content_height(lc));
    int tile_run = 0;
    int start_index = 0;
    DP_Tile *start_tile = NULL;

    for (int i = 0; i < count; ++i) {
        DP_Tile *t = DP_layer_content_tile_at_index_noinc(lc, i);
        if (!tile_is_effectively_blank(t, fill_pixel)) {
            if (tile_run != 0) {
                if (DP_tile_pixels_equal(start_tile, t)) {
                    ++tile_run;
                    continue;
                }
                else {
                    flush_tile(c, layer_index, layer_id, sublayer_id,
                               start_index, tile_run, start_tile, ephemeral);
                }
            }

            tile_run = 1;
            start_tile = t;
            start_index = i;
        }
        else if (tile_run != 0) {
            flush_tile(c, layer_index, layer_id, sublayer_id, start_index,
                       tile_run, start_tile, ephemeral);
            tile_run = 0;
        }
    }

    if (tile_run != 0) {
        flush_tile(c, layer_index, layer_id, sublayer_id, start_index, tile_run,
                   start_tile, ephemeral);
    }
}

static int main_content_to_reset_image(struct DP_ResetImageContext *c,
                                       int layer_index, int parent_index,
                                       int parent_id, DP_LayerContent *lc,
                                       DP_LayerProps *lp, bool ephemeral)
{
    int layer_id = DP_layer_props_id(lp);
    DP_Pixel15 fill_pixel;
    bool have_fill = layer_fill(lc, &fill_pixel);
    uint32_t fill =
        have_fill ? DP_upixel15_to_8(DP_pixel15_unpremultiply(fill_pixel)).color
                  : 0;
    layer_to_reset_image(c, layer_index, layer_id, 0, parent_index, parent_id,
                         lp, fill);
    if (fill == 0) {
        fill_pixel = DP_pixel15_zero();
    }

    tiles_to_reset_image(c, lc, layer_index, layer_id, 0, fill_pixel,
                         ephemeral);
    return layer_id;
}

static void sublayer_content_to_reset_image(struct DP_ResetImageContext *c,
                                            int layer_index, int parent_index,
                                            int parent_id, DP_LayerContent *lc,
                                            int layer_id)
{
    DP_LayerList *sub_ll = DP_layer_content_sub_contents_noinc(lc);
    DP_LayerPropsList *sub_lpl = DP_layer_content_sub_props_noinc(lc);
    int sub_count = DP_layer_list_count(sub_ll);
    DP_ASSERT(DP_layer_props_list_count(sub_lpl) == sub_count);
    for (int i = 0; i < sub_count; ++i) {
        DP_LayerProps *sub_lp = DP_layer_props_list_at_noinc(sub_lpl, i);
        int sub_id = DP_layer_props_id(sub_lp);
        if (sub_id > 0 && sub_id <= 255) {
            DP_LayerContent *sub_lc = DP_layer_list_entry_content_noinc(
                DP_layer_list_at_noinc(sub_ll, i));
            layer_to_reset_image(c, layer_id, layer_index, sub_id, parent_index,
                                 parent_id, sub_lp, 0);
            tiles_to_reset_image(c, sub_lc, layer_index, layer_id, sub_id,
                                 DP_pixel15_zero(), false);
        }
    }
}

static void layer_content_to_reset_image(struct DP_ResetImageContext *c,
                                         int layer_index, int parent_index,
                                         int parent_id, DP_LayerContent *lc,
                                         DP_LayerProps *lp)
{
    if (!DP_layer_content_has_sublayers(lc)) {
        main_content_to_reset_image(c, layer_index, parent_index, parent_id, lc,
                                    lp, false);
    }
    else if (c->options.merge_sublayers) {
        DP_LayerContent *merged_lc = DP_layer_content_merge_sublayers(lc);
        main_content_to_reset_image(c, layer_index, parent_index, parent_id, lc,
                                    lp, true);
        DP_layer_content_decref(merged_lc);
    }
    else {
        int layer_id = main_content_to_reset_image(c, layer_index, parent_index,
                                                   parent_id, lc, lp, false);
        sublayer_content_to_reset_image(c, layer_index, parent_index, parent_id,
                                        lc, layer_id);
    }
}

static int layer_group_to_reset_image(struct DP_ResetImageContext *c,
                                      int layer_index, int parent_index,
                                      int parent_id, DP_LayerGroup *lg,
                                      DP_LayerProps *lp)
{
    int layer_id = DP_layer_props_id(lp);
    layer_to_reset_image(c, layer_index, layer_id, 0, parent_index, parent_id,
                         lp, 0);
    return layers_to_reset_image(c, layer_index + 1, layer_index, layer_id,
                                 DP_layer_group_children_noinc(lg),
                                 DP_layer_props_children_noinc(lp));
}

static int layers_to_reset_image(struct DP_ResetImageContext *c,
                                 int next_layer_index, int parent_index,
                                 int parent_id, DP_LayerList *ll,
                                 DP_LayerPropsList *lpl)
{
    int count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (DP_layer_list_entry_is_group(lle)) {
            next_layer_index = layer_group_to_reset_image(
                c, next_layer_index, parent_index, parent_id,
                DP_layer_list_entry_group_noinc(lle), lp);
        }
        else {
            layer_content_to_reset_image(
                c, next_layer_index, parent_index, parent_id,
                DP_layer_list_entry_content_noinc(lle), lp);
            ++next_layer_index;
        }
    }
    return next_layer_index;
}

static void flush_selection_tile(struct DP_ResetImageContext *c,
                                 int selection_index, unsigned int context_id,
                                 int selection_id, int tile_index, DP_Tile *t)
{
    DP_Worker *worker = c->worker;
    if (worker) {
        struct DP_ResetImageJob job = {
            c, DP_RESET_IMAGE_JOB_SELECTION_TILE,
            .selection_tile = {t, selection_index, context_id, selection_id,
                               tile_index}};
        DP_worker_push(worker, &job);
    }
    else {
        selection_tile_to_reset_image(c, 0, selection_index, context_id,
                                      selection_id, tile_index, t);
    }
}

static void selections_to_reset_image(struct DP_ResetImageContext *c,
                                      DP_CanvasState *cs)
{
    DP_SelectionSet *ss = DP_canvas_state_selections_noinc_nullable(cs);
    if (ss) {
        int tile_count = DP_tile_total_round(DP_canvas_state_width(cs),
                                             DP_canvas_state_height(cs));
        int selection_count = DP_selection_set_count(ss);
        int selection_index = 0;
        for (int i = 0; i < selection_count; ++i) {
            DP_Selection *sel = DP_selection_set_at_noinc(ss, i);
            int selection_id = DP_selection_id(sel);
            if (selection_id >= DP_SELECTION_ID_FIRST_REMOTE) {
                unsigned int context_id = DP_selection_context_id(sel);
                DP_LayerContent *lc = DP_selection_content_noinc(sel);
                for (int tile_index = 0; tile_index < tile_count;
                     ++tile_index) {
                    DP_Tile *t =
                        DP_layer_content_tile_at_index_noinc(lc, tile_index);
                    if (t) {
                        flush_selection_tile(c, selection_index, context_id,
                                             selection_id, tile_index, t);
                    }
                }
                ++selection_index;
            }
        }
    }
}

static void annotations_to_reset_image(struct DP_ResetImageContext *c,
                                       DP_AnnotationList *al)
{
    int count = DP_annotation_list_count(al);
    for (int i = 0; i < count; ++i) {
        reset_image_handle(
            c, (DP_ResetEntry){
                   DP_RESET_ENTRY_ANNOTATION,
                   .annotation = {i, DP_annotation_list_at_noinc(al, i)}});
    }
}

static void key_frame_to_reset_image(struct DP_ResetImageContext *c,
                                     int track_index, int track_id,
                                     int frame_index, DP_KeyFrame *kf)
{

    reset_image_handle(
        c, (DP_ResetEntry){DP_RESET_ENTRY_FRAME,
                           .frame = {track_index, track_id, frame_index, kf}});
}

static void track_to_reset_image(struct DP_ResetImageContext *c,
                                 int track_index, DP_Track *t)
{
    reset_image_handle(
        c, (DP_ResetEntry){DP_RESET_ENTRY_TRACK, .track = {track_index, t}});
    int track_id = DP_track_id(t);
    int key_frame_count = DP_track_key_frame_count(t);
    for (int i = 0; i < key_frame_count; ++i) {
        key_frame_to_reset_image(c, track_index, track_id,
                                 DP_track_frame_index_at_noinc(t, i),
                                 DP_track_key_frame_at_noinc(t, i));
    }
}

static void timeline_to_reset_image(struct DP_ResetImageContext *c,
                                    DP_Timeline *tl)
{
    int track_count = DP_timeline_count(tl);
    for (int i = track_count - 1; i >= 0; --i) {
        track_to_reset_image(c, i, DP_timeline_at_noinc(tl, i));
    }
}

static void canvas_state_to_reset_image(struct DP_ResetImageContext *c,
                                        DP_CanvasState *cs)
{
    DP_PERF_BEGIN(canvas, "image:canvas");
    DP_Worker *worker = c->worker;
    int width = DP_canvas_state_width(cs);
    int height = DP_canvas_state_height(cs);
    if (width > 0 && height > 0 && c->options.thumb_write
        && c->options.thumb_width > 0 && c->options.thumb_height > 0) {
        if (worker) {
            struct DP_ResetImageJob job = {c, DP_RESET_IMAGE_JOB_THUMB,
                                           .thumb = cs};
            DP_worker_push(worker, &job);
        }
        else {
            thumb_to_reset_image(c, cs);
        }
    }

    reset_image_handle(
        c, (DP_ResetEntry){
               DP_RESET_ENTRY_CANVAS,
               .canvas = {width, height, DP_canvas_state_metadata_noinc(cs)}});

    DP_Tile *t = DP_canvas_state_background_tile_noinc(cs);
    if (t) {
        if (worker) {
            struct DP_ResetImageJob job = {c, DP_RESET_IMAGE_JOB_BACKGROUND,
                                           .background = t};
            DP_worker_push(worker, &job);
        }
        else {
            background_to_reset_image(c, 0, t);
        }
    }
    DP_PERF_END(canvas);

    DP_PERF_BEGIN(layers, "image:layers");
    layers_to_reset_image(c, 0, -1, 0, DP_canvas_state_layers_noinc(cs),
                          DP_canvas_state_layer_props_noinc(cs));
    DP_PERF_END(layers);

    DP_PERF_BEGIN(selections, "image:selections");
    selections_to_reset_image(c, cs);
    DP_PERF_END(selections);

    DP_PERF_BEGIN(annotations, "image:annotations");
    annotations_to_reset_image(c, DP_canvas_state_annotations_noinc(cs));
    DP_PERF_END(annotations);

    DP_PERF_BEGIN(timeline, "image:timeline");
    timeline_to_reset_image(c, DP_canvas_state_timeline_noinc(cs));
    DP_PERF_END(timeline);
}

void DP_reset_image_build_with(
    DP_CanvasState *cs, const DP_ResetImageOptions *options,
    void (*handle_entry)(void *, const DP_ResetEntry *), void *user)
{
    DP_ASSERT(cs);
    DP_ASSERT(options);
    DP_ASSERT(handle_entry);
    DP_PERF_BEGIN(fn, "image");

    int buffers_count;
    DP_Worker *worker;
    if (options->use_worker) {
        int thread_count = DP_worker_cpu_count(128);
        worker = DP_worker_new(1024, sizeof(struct DP_ResetImageJob),
                               thread_count, reset_image_job);
        if (worker) {
            buffers_count = thread_count;
        }
        else {
            DP_warn("Reset image failed to create worker: %s", DP_error());
            buffers_count = 1;
        }
    }
    else {
        worker = NULL;
        buffers_count = 1;
    }

    struct DP_ResetImageContext c = {
        worker, *options, handle_entry, user,
        DP_malloc(sizeof(*c.buffers) * DP_int_to_size(buffers_count))};
    for (int i = 0; i < buffers_count; ++i) {
        c.buffers[i] = (struct DP_ResetImageBuffer){
            {0, NULL}, DP_malloc(sizeof(*c.buffers[i].pixels)), NULL};
    }

    canvas_state_to_reset_image(&c, cs);

    DP_PERF_BEGIN(join, "image:join");
    DP_worker_free_join(worker);
    DP_PERF_END(join);
    for (int i = 0; i < buffers_count; ++i) {
        struct DP_ResetImageBuffer *buffer = &c.buffers[i];
        DP_compress_zstd_free(&buffer->zstd_context);
        DP_free(buffer->pixels);
        DP_free(buffer->output.data);
    }
    DP_free(c.buffers);
    DP_PERF_END(fn);
}


struct DP_ResetImageMessageContext {
    unsigned int context_id;
    int xtiles;
    DP_Mutex *mutex;
    void (*push_message)(void *, DP_Message *);
    void *push_message_user;
};

static void reset_message_push(struct DP_ResetImageMessageContext *c,
                               DP_Message *msg)
{
    c->push_message(c->push_message_user, msg);
}

static void set_tile_data(size_t size, unsigned char *out, void *bytes)
{
    memcpy(out, bytes, size);
}

static void push_document_metadata_int_field_if_not_default(
    struct DP_ResetImageMessageContext *c, int field, int value,
    int default_value)
{
    if (value != default_value) {
        reset_message_push(c, DP_msg_set_metadata_int_new(
                                  c->context_id, DP_int_to_uint8(field),
                                  DP_int_to_int32(value)));
    }
}

static void reset_entry_canvas_to_message(struct DP_ResetImageMessageContext *c,
                                          const DP_ResetEntryCanvas *rec)
{
    if (rec->width > 0 && rec->height > 0) {
        c->xtiles = DP_tile_count_round(rec->width);
        reset_message_push(c, DP_msg_canvas_resize_new(
                                  c->context_id, 0, DP_int_to_int32(rec->width),
                                  DP_int_to_int32(rec->height), 0));
    }

    DP_DocumentMetadata *dm = rec->dm;
    push_document_metadata_int_field_if_not_default(
        c, DP_MSG_SET_METADATA_INT_FIELD_DPIX, DP_document_metadata_dpix(dm),
        DP_DOCUMENT_METADATA_DPIX_DEFAULT);
    push_document_metadata_int_field_if_not_default(
        c, DP_MSG_SET_METADATA_INT_FIELD_DPIY, DP_document_metadata_dpiy(dm),
        DP_DOCUMENT_METADATA_DPIY_DEFAULT);
    push_document_metadata_int_field_if_not_default(
        c, DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE,
        DP_document_metadata_framerate(dm),
        DP_DOCUMENT_METADATA_FRAMERATE_DEFAULT);
    push_document_metadata_int_field_if_not_default(
        c, DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT,
        DP_document_metadata_frame_count(dm),
        DP_DOCUMENT_METADATA_FRAME_COUNT_DEFAULT);
}

static void
reset_entry_background_to_message(struct DP_ResetImageMessageContext *c,
                                  const DP_ResetEntryBackground *reb)
{
    reset_message_push(c, DP_msg_canvas_background_new(c->context_id,
                                                       set_tile_data, reb->size,
                                                       reb->data));
}

static void reset_entry_layer_to_message(struct DP_ResetImageMessageContext *c,
                                         const DP_ResetEntryLayer *rel)
{
    DP_LayerProps *lp = rel->lp;
    bool group = DP_layer_props_children_noinc(lp);
    if (rel->sublayer_id == 0) {
        uint8_t create_flags =
            DP_flag_uint8(rel->parent_id != 0,
                          DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO)
            | DP_flag_uint8(group, DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP);
        size_t title_length;
        const char *title = DP_layer_props_title(lp, &title_length);
        reset_message_push(
            c, DP_msg_layer_tree_create_new(
                   c->context_id, DP_layer_id_to_protocol(rel->layer_id), 0,
                   DP_layer_id_to_protocol(rel->parent_id), rel->fill,
                   create_flags, title, title_length));
    }
    uint8_t attr_flags = DP_flag_uint8(DP_layer_props_censored(lp),
                                       DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR)
                       | DP_flag_uint8(group && DP_layer_props_isolated(lp),
                                       DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED)
                       | DP_flag_uint8(DP_layer_props_clip(lp),
                                       DP_MSG_LAYER_ATTRIBUTES_FLAGS_CLIP);
    reset_message_push(c, DP_msg_layer_attributes_new(
                              c->context_id,
                              DP_layer_id_to_protocol(rel->layer_id),
                              DP_int_to_uint8(rel->sublayer_id), attr_flags,
                              DP_channel15_to_8(DP_layer_props_opacity(lp)),
                              DP_int_to_uint8(DP_layer_props_blend_mode(lp))));
}

static void reset_entry_tile_to_message(struct DP_ResetImageMessageContext *c,
                                        const DP_ResetEntryTile *ret)
{
    int xtiles = c->xtiles;
    if (xtiles > 0) {
        uint8_t user_id = DP_uint_to_uint8(ret->context_id);
        uint32_t layer_id = DP_layer_id_to_protocol(ret->layer_id);
        uint8_t sublayer_id = DP_int_to_uint8(ret->sublayer_id);
        int tile_index = ret->tile_index;
        uint16_t row = DP_int_to_uint16(tile_index / xtiles);
        uint16_t col = DP_int_to_uint16(tile_index % xtiles);
        int max = UINT16_MAX;
        for (int tile_run = ret->tile_run; tile_run > 0; tile_run -= max + 1) {
            reset_message_push(
                c, DP_msg_put_tile_zstd_new(
                       c->context_id, user_id, layer_id, sublayer_id, col, row,
                       DP_int_to_uint16(DP_min_int(tile_run - 1, max)),
                       set_tile_data, ret->size, ret->data));
        }
    }
}

static void
reset_entry_selection_tile_to_message(struct DP_ResetImageMessageContext *c,
                                      const DP_ResetEntrySelectionTile *rest)
{
    int xtiles = c->xtiles;
    if (xtiles > 0) {
        uint8_t user_id = DP_uint_to_uint8(rest->context_id);
        uint8_t selection_id = DP_int_to_uint8(rest->selection_id);
        int tile_index = rest->tile_index;
        uint16_t row = DP_int_to_uint16(tile_index / xtiles);
        uint16_t col = DP_int_to_uint16(tile_index % xtiles);
        reset_message_push(c, DP_msg_sync_selection_tile_new(
                                  c->context_id, user_id, selection_id, col,
                                  row, set_tile_data, rest->size, rest->data));
    }
}

static void
reset_entry_annotation_to_message(struct DP_ResetImageMessageContext *c,
                                  const DP_ResetEntryAnnotation *rea)
{
    DP_Annotation *a = rea->a;
    uint16_t annotation_id = DP_int_to_uint16(DP_annotation_id(a));
    reset_message_push(c, DP_msg_annotation_create_new(
                              c->context_id, annotation_id,
                              DP_int_to_int32(DP_annotation_x(a)),
                              DP_int_to_int32(DP_annotation_y(a)),
                              DP_int_to_uint16(DP_annotation_width(a)),
                              DP_int_to_uint16(DP_annotation_height(a))));

    uint8_t edit_flags = DP_flag_uint8(DP_annotation_protect(a),
                                       DP_MSG_ANNOTATION_EDIT_FLAGS_PROTECT);
    switch (DP_annotation_valign(a)) {
    case DP_ANNOTATION_VALIGN_CENTER:
        edit_flags |= DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER;
        break;
    case DP_ANNOTATION_VALIGN_BOTTOM:
        edit_flags |= DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER;
        break;
    default:
        break;
    }
    size_t text_length;
    const char *text = DP_annotation_text(a, &text_length);
    reset_message_push(
        c, DP_msg_annotation_edit_new(c->context_id, annotation_id,
                                      DP_annotation_background_color(a),
                                      edit_flags, 0, text, text_length));
}

static void reset_entry_track_to_message(struct DP_ResetImageMessageContext *c,
                                         const DP_ResetEntryTrack *ret)
{
    DP_Track *t = ret->t;
    size_t title_length;
    const char *title = DP_track_title(t, &title_length);
    reset_message_push(c, DP_msg_track_create_new(
                              c->context_id, DP_int_to_uint16(DP_track_id(t)),
                              0, 0, title, title_length));
}

static void set_key_frame_layers(int count, uint32_t *out, void *user)
{
    const DP_KeyFrameLayer *kfls = user;
    for (int i = 0; i < count; ++i) {
        out[i] = DP_layer_id_to_protocol(kfls[i].layer_id)
               | (DP_uint_to_uint32(kfls[i].flags & 0xffu) << (uint32_t)24);
    }
}

static void reset_entry_frame_to_message(struct DP_ResetImageMessageContext *c,
                                         const DP_ResetEntryFrame *ref)
{
    uint16_t track_id = DP_int_to_uint16(ref->track_id);
    uint16_t frame_index = DP_int_to_uint16(ref->frame_index);
    DP_KeyFrame *kf = ref->kf;
    reset_message_push(c,
                       DP_msg_key_frame_set_new(
                           c->context_id, track_id, frame_index,
                           DP_layer_id_to_protocol(DP_key_frame_layer_id(kf)),
                           0, DP_MSG_KEY_FRAME_SET_SOURCE_LAYER));

    size_t title_length;
    const char *title = DP_key_frame_title(kf, &title_length);
    if (title_length != 0) {
        reset_message_push(
            c, DP_msg_key_frame_retitle_new(c->context_id, track_id,
                                            frame_index, title, title_length));
    }

    int kfl_count;
    const DP_KeyFrameLayer *kfls = DP_key_frame_layers(kf, &kfl_count);
    if (kfl_count != 0) {
        reset_message_push(c,
                           DP_msg_key_frame_layer_attributes_new(
                               c->context_id, track_id, frame_index,
                               set_key_frame_layers, kfl_count, (void *)kfls));
    }
}

static void reset_entry_to_message(void *user, const DP_ResetEntry *re)
{
    struct DP_ResetImageMessageContext *c = user;
    DP_Mutex *mutex = c->mutex;
    DP_MUTEX_MUST_LOCK(mutex);
    switch (re->type) {
    case DP_RESET_ENTRY_CANVAS:
        reset_entry_canvas_to_message(c, &re->canvas);
        break;
    case DP_RESET_ENTRY_BACKGROUND:
        reset_entry_background_to_message(c, &re->background);
        break;
    case DP_RESET_ENTRY_LAYER:
        reset_entry_layer_to_message(c, &re->layer);
        break;
    case DP_RESET_ENTRY_TILE:
        reset_entry_tile_to_message(c, &re->tile);
        break;
    case DP_RESET_ENTRY_SELECTION_TILE:
        reset_entry_selection_tile_to_message(c, &re->selection_tile);
        break;
    case DP_RESET_ENTRY_ANNOTATION:
        reset_entry_annotation_to_message(c, &re->annotation);
        break;
    case DP_RESET_ENTRY_TRACK:
        reset_entry_track_to_message(c, &re->track);
        break;
    case DP_RESET_ENTRY_FRAME:
        reset_entry_frame_to_message(c, &re->frame);
        break;
    case DP_RESET_ENTRY_THUMB:
        DP_warn("Got unexpected thumb reset entry");
        break;
    }
    DP_MUTEX_MUST_UNLOCK(mutex);
}

void DP_reset_image_build(DP_CanvasState *cs, unsigned int context_id,
                          void (*push_message)(void *, DP_Message *),
                          void *user)
{
    DP_ResetImageOptions options = {
        true, false, true, DP_RESET_IMAGE_COMPRESSION_ZSTD8LE, 0, 0, NULL};
    struct DP_ResetImageMessageContext c = {context_id, 0, DP_mutex_new(),
                                            push_message, user};
    if (!c.mutex) {
        options.use_worker = false;
        DP_warn("Failed to create reset image mutex: %s", DP_error());
    }
    DP_reset_image_build_with(cs, &options, reset_entry_to_message, &c);
    DP_mutex_free(c.mutex);
}
