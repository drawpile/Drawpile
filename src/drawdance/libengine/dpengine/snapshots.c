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
 */
#include "snapshots.h"
#include "annotation.h"
#include "annotation_list.h"
#include "canvas_history.h"
#include "canvas_state.h"
#include "document_metadata.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "pixels.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/perf.h>
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
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

struct DP_ResetImagePixelBuffer {
    DP_Pixel8 data[DP_TILE_LENGTH];
};

struct DP_ResetImageContext {
    DP_Worker *worker;
    void (*handle_entry)(void *, const DP_ResetEntry *);
    void *handle_entry_user;
    struct DP_ResetImageOutputBuffer *output_buffers;
    struct DP_ResetImagePixelBuffer *pixel_buffers;
};

struct DP_ResetImageJob {
    struct DP_ResetImageContext *c;
    int layer_index;
    int layer_id;
    int sublayer_id;
    int tile_index;
    int tile_run;
    DP_Tile *tile;
};

static void reset_image_handle(struct DP_ResetImageContext *c,
                               DP_ResetEntry entry)
{
    DP_Worker *worker = c->worker;
    if (worker) {
        DP_worker_lock(worker);
        c->handle_entry(c->handle_entry_user, &entry);
        DP_worker_unlock(worker);
    }
    else {
        c->handle_entry(c->handle_entry_user, &entry);
    }
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

static size_t reset_image_compress_tile(struct DP_ResetImageContext *c,
                                        int buffer_index, DP_Tile *t)
{
    struct DP_ResetImageOutputBuffer *output_buffer =
        &c->output_buffers[buffer_index];
    size_t size =
        t ? DP_tile_compress(t, c->pixel_buffers[buffer_index].data,
                             reset_image_get_output_buffer, output_buffer)
          : DP_tile_compress_pixel(DP_pixel15_zero(),
                                   reset_image_get_output_buffer,
                                   output_buffer);
    if (size == 0) {
        DP_warn("Reset image: error tile: %s", DP_error());
    }
    return size;
}

static size_t reset_image_maybe_compress_tile(struct DP_ResetImageContext *c,
                                              int buffer_index,
                                              DP_Tile *tile_or_null)
{
    return tile_or_null
             ? reset_image_compress_tile(c, buffer_index, tile_or_null)
             : 0;
}

#define SET_FLAG_IF(VALUE, CONDITION, FLAG) \
    do {                                    \
        if (CONDITION) {                    \
            (VALUE) |= (FLAG);              \
        }                                   \
    } while (0)

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

static void layer_to_reset_image(struct DP_ResetImageContext *c, int layer_id,
                                 int layer_index, int sublayer_id,
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
                                        tile_index, tile_run, size,
                                        c->output_buffers[buffer_index].data}});
    }
}

static void tile_to_reset_image_job(void *user, int thread_index)
{
    struct DP_ResetImageJob *job = user;
    tile_to_reset_image(job->c, thread_index, job->layer_index, job->layer_id,
                        job->sublayer_id, job->tile_index, job->tile_run,
                        job->tile);
}

static void flush_tile(struct DP_ResetImageContext *c, int layer_index,
                       int layer_id, int sublayer_id, int tile_index,
                       int tile_run, DP_Tile *t)
{
    DP_Worker *worker = c->worker;
    if (worker) {
        struct DP_ResetImageJob job = {
            c, layer_index, layer_id, sublayer_id, tile_index, tile_run, t};
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
                                 DP_Pixel15 fill_pixel)
{
    int count = DP_tile_total_round(DP_layer_content_width(lc),
                                    DP_layer_content_height(lc));
    int tile_run = 0;
    DP_Tile *start_tile;
    int start_index;

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
                               start_index, tile_run, start_tile);
                }
            }

            tile_run = 1;
            start_tile = t;
            start_index = i;
        }
        else if (tile_run != 0) {
            flush_tile(c, layer_index, layer_id, sublayer_id, start_index,
                       tile_run, start_tile);
            tile_run = 0;
        }
    }

    if (tile_run != 0) {
        flush_tile(c, layer_index, layer_id, sublayer_id, start_index, tile_run,
                   start_tile);
    }
}

static void layer_content_to_reset_image(struct DP_ResetImageContext *c,
                                         int layer_index, int parent_index,
                                         int parent_id, DP_LayerContent *lc,
                                         DP_LayerProps *lp)
{
    DP_Pixel15 fill_pixel;
    bool have_fill = layer_fill(lc, &fill_pixel);
    uint32_t fill =
        have_fill ? DP_upixel15_to_8(DP_pixel15_unpremultiply(fill_pixel)).color
                  : 0;
    int layer_id = DP_layer_props_id(lp);
    layer_to_reset_image(c, layer_index, layer_id, 0, parent_index, parent_id,
                         lp, fill);
    if (fill == 0) {
        fill_pixel = DP_pixel15_zero();
    }

    tiles_to_reset_image(c, lc, layer_index, layer_id, 0, fill_pixel);

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
                                 DP_pixel15_zero());
        }
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

static void set_document_metadata_int_field_if_not_default(
    struct DP_ResetImageContext *c, int field, int value, int default_value)
{
    if (value != default_value) {
        reset_image_handle(c, (DP_ResetEntry){DP_RESET_ENTRY_METADATA,
                                              .metadata = {field, value}});
    }
}

static void document_metadata_to_reset_image(struct DP_ResetImageContext *c,
                                             DP_DocumentMetadata *dm)
{
    set_document_metadata_int_field_if_not_default(
        c, DP_MSG_SET_METADATA_INT_FIELD_DPIX, DP_document_metadata_dpix(dm),
        DP_DOCUMENT_METADATA_DPIX_DEFAULT);
    set_document_metadata_int_field_if_not_default(
        c, DP_MSG_SET_METADATA_INT_FIELD_DPIY, DP_document_metadata_dpiy(dm),
        DP_DOCUMENT_METADATA_DPIY_DEFAULT);
    set_document_metadata_int_field_if_not_default(
        c, DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE,
        DP_document_metadata_framerate(dm),
        DP_DOCUMENT_METADATA_FRAMERATE_DEFAULT);
    set_document_metadata_int_field_if_not_default(
        c, DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT,
        DP_document_metadata_frame_count(dm),
        DP_DOCUMENT_METADATA_FRAME_COUNT_DEFAULT);
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
    DP_PERF_BEGIN(canvas, "canvas");
    reset_image_handle(c,
                       (DP_ResetEntry){DP_RESET_ENTRY_CANVAS,
                                       .canvas = {DP_canvas_state_width(cs),
                                                  DP_canvas_state_height(cs)}});

    size_t size = reset_image_maybe_compress_tile(
        c, 0, DP_canvas_state_background_tile_noinc(cs));
    if (size != 0) {
        reset_image_handle(
            c,
            (DP_ResetEntry){DP_RESET_ENTRY_BACKGROUND,
                            .background = {size, c->output_buffers[0].data}});
    }
    DP_PERF_END(canvas);

    DP_PERF_BEGIN(layers, "layers");
    layers_to_reset_image(c, 0, -1, 0, DP_canvas_state_layers_noinc(cs),
                          DP_canvas_state_layer_props_noinc(cs));
    DP_PERF_END(layers);

    DP_PERF_BEGIN(annotations, "annotations");
    annotations_to_reset_image(c, DP_canvas_state_annotations_noinc(cs));
    DP_PERF_END(annotations);

    DP_PERF_BEGIN(metadata, "metadata");
    document_metadata_to_reset_image(c, DP_canvas_state_metadata_noinc(cs));
    DP_PERF_END(metadata);

    DP_PERF_BEGIN(timeline, "timeline");
    timeline_to_reset_image(c, DP_canvas_state_timeline_noinc(cs));
    DP_PERF_END(timeline);
}

void DP_reset_image_build_with(DP_CanvasState *cs,
                               void (*handle_entry)(void *,
                                                    const DP_ResetEntry *),
                               void *user)
{
    DP_PERF_BEGIN(fn, "reset_image");
    int buffers_count = DP_worker_cpu_count(128);
    DP_Worker *worker = DP_worker_new(1024, sizeof(struct DP_ResetImageJob),
                                      buffers_count, tile_to_reset_image_job);
    if (!worker) {
        DP_warn("Reset image failed to create worker: %s", DP_error());
        buffers_count = 1;
    }

    struct DP_ResetImageContext c = {
        worker,
        handle_entry,
        user,
        DP_malloc(sizeof(struct DP_ResetImageOutputBuffer)
                  * DP_int_to_size(buffers_count)),
        DP_malloc(sizeof(struct DP_ResetImagePixelBuffer)
                  * DP_int_to_size(buffers_count)),
    };
    for (int i = 0; i < buffers_count; ++i) {
        c.output_buffers[i] = (struct DP_ResetImageOutputBuffer){0, NULL};
    }

    canvas_state_to_reset_image(&c, cs);

    DP_worker_free_join(worker);
    for (int i = 0; i < buffers_count; ++i) {
        DP_free(c.output_buffers[i].data);
    }
    DP_free(c.output_buffers);
    DP_free(c.pixel_buffers);
    DP_PERF_END(fn);
}


struct DP_ResetImageMessageContext {
    unsigned int context_id;
    int xtiles;
    void (*push_message)(void *, DP_Message *);
    void *push_message_user;
};

static void reset_message_push(struct DP_ResetImageMessageContext *c,
                               DP_Message *msg)
{
    c->push_message(c->push_message_user, msg);
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
}

static void set_tile_data(size_t size, unsigned char *out, void *bytes)
{
    memcpy(out, bytes, size);
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
        uint8_t create_flags = 0;
        SET_FLAG_IF(create_flags, rel->parent_id != 0,
                    DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO);
        SET_FLAG_IF(create_flags, group, DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP);
        size_t title_length;
        const char *title = DP_layer_props_title(lp, &title_length);
        reset_message_push(c,
                           DP_msg_layer_tree_create_new(
                               c->context_id, DP_int_to_uint16(rel->layer_id),
                               0, DP_int_to_uint16(rel->parent_id), rel->fill,
                               create_flags, title, title_length));
    }
    uint8_t attr_flags = 0;
    SET_FLAG_IF(attr_flags, DP_layer_props_censored(lp),
                DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR);
    SET_FLAG_IF(attr_flags, group && DP_layer_props_isolated(lp),
                DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED);
    reset_message_push(c, DP_msg_layer_attributes_new(
                              c->context_id, DP_int_to_uint16(rel->layer_id),
                              DP_int_to_uint8(rel->sublayer_id), attr_flags,
                              DP_channel15_to_8(DP_layer_props_opacity(lp)),
                              DP_int_to_uint8(DP_layer_props_blend_mode(lp))));
}

static void reset_entry_tile_to_message(struct DP_ResetImageMessageContext *c,
                                        const DP_ResetEntryTile *ret)
{
    int xtiles = c->xtiles;
    if (xtiles > 0) {
        uint16_t layer_id = DP_int_to_uint16(ret->layer_id);
        uint8_t sublayer_id = DP_int_to_uint8(ret->sublayer_id);
        int tile_index = ret->tile_index;
        uint16_t row = DP_int_to_uint16(tile_index / xtiles);
        uint16_t col = DP_int_to_uint16(tile_index % xtiles);
        int max = UINT16_MAX;
        for (int tile_run = ret->tile_run; tile_run > 0; tile_run -= max + 1) {
            reset_message_push(
                c, DP_msg_put_tile_new(
                       c->context_id, layer_id, sublayer_id, col, row,
                       DP_int_to_uint16(DP_min_int(tile_run - 1, max)),
                       set_tile_data, ret->size, ret->data));
        }
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

    uint8_t edit_flags;
    switch (DP_annotation_valign(a)) {
    case DP_ANNOTATION_VALIGN_CENTER:
        edit_flags = DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER;
        break;
    case DP_ANNOTATION_VALIGN_BOTTOM:
        edit_flags = DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER;
        break;
    default:
        edit_flags = 0;
        break;
    }
    SET_FLAG_IF(edit_flags, DP_annotation_protect(a),
                DP_MSG_ANNOTATION_EDIT_FLAGS_PROTECT);
    size_t text_length;
    const char *text = DP_annotation_text(a, &text_length);
    reset_message_push(
        c, DP_msg_annotation_edit_new(c->context_id, annotation_id,
                                      DP_annotation_background_color(a),
                                      edit_flags, 0, text, text_length));
}

static void
reset_entry_metadata_to_message(struct DP_ResetImageMessageContext *c,
                                const DP_ResetEntryMetadata *rem)
{
    reset_message_push(c, DP_msg_set_metadata_int_new(
                              c->context_id, DP_int_to_uint8(rem->field),
                              DP_int_to_int32(rem->value_int)));
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

static void set_key_frame_layers(int count, uint16_t *out, void *user)
{
    int layer_count = count / 2;
    const DP_KeyFrameLayer *kfls = user;
    for (int i = 0; i < layer_count; ++i) {
        out[i * 2] = DP_int_to_uint16(kfls[i].layer_id);
        out[i * 2 + 1] = DP_uint_to_uint16(kfls[i].flags);
    }
}

static void reset_entry_frame_to_message(struct DP_ResetImageMessageContext *c,
                                         const DP_ResetEntryFrame *ref)
{
    uint16_t track_id = DP_int_to_uint16(ref->track_id);
    uint16_t frame_index = DP_int_to_uint16(ref->frame_index);
    DP_KeyFrame *kf = ref->kf;
    reset_message_push(
        c, DP_msg_key_frame_set_new(c->context_id, track_id, frame_index,
                                    DP_int_to_uint16(DP_key_frame_layer_id(kf)),
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
        reset_message_push(c, DP_msg_key_frame_layer_attributes_new(
                                  c->context_id, track_id, frame_index,
                                  set_key_frame_layers, kfl_count * 2,
                                  (void *)kfls));
    }
}

static void reset_entry_to_message(void *user, const DP_ResetEntry *re)
{
    struct DP_ResetImageMessageContext *c = user;
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
    case DP_RESET_ENTRY_ANNOTATION:
        reset_entry_annotation_to_message(c, &re->annotation);
        break;
    case DP_RESET_ENTRY_METADATA:
        reset_entry_metadata_to_message(c, &re->metadata);
        break;
    case DP_RESET_ENTRY_TRACK:
        reset_entry_track_to_message(c, &re->track);
        break;
    case DP_RESET_ENTRY_FRAME:
        reset_entry_frame_to_message(c, &re->frame);
        break;
    }
}

void DP_reset_image_build(DP_CanvasState *cs, unsigned int context_id,
                          void (*push_message)(void *, DP_Message *),
                          void *user)
{
    struct DP_ResetImageMessageContext c = {context_id, 0, push_message, user};
    DP_reset_image_build_with(cs, reset_entry_to_message, &c);
}
