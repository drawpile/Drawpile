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
#include <dpcommon/queue.h>
#include <dpcommon/threading.h>
#include <dpmsg/message.h>


#define ELEMENT_SIZE (sizeof(DP_Snapshot))

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


struct DP_ResetImageContext {
    unsigned int context_id;
    void (*push_message)(void *, DP_Message *);
    void *push_message_user;
    DP_Pixel8 *pixel_buffer;
    size_t capacity;
    void *output_buffer;
};

static void reset_image_push(struct DP_ResetImageContext *c, DP_Message *msg)
{
    c->push_message(c->push_message_user, msg);
}

static unsigned char *reset_image_get_output_buffer(size_t size, void *user)
{
    struct DP_ResetImageContext *c = user;
    if (c->capacity < size) {
        DP_free(c->output_buffer);
        c->output_buffer = DP_malloc(size);
        c->capacity = size;
    }
    return c->output_buffer;
}

static void set_tile_data(size_t size, unsigned char *out, void *bytes)
{
    memcpy(out, bytes, size);
}

static size_t reset_image_maybe_compress_tile(struct DP_ResetImageContext *c,
                                              DP_Tile *tile_or_null)
{
    if (tile_or_null) {
        size_t size = DP_tile_compress(tile_or_null, c->pixel_buffer,
                                       reset_image_get_output_buffer, c);
        if (size == 0) {
            DP_warn("Reset image: error tile: %s", DP_error());
        }
        return size;
    }
    else {
        return 0;
    }
}

#define SET_FLAG_IF(VALUE, CONDITION, FLAG) \
    do {                                    \
        if (CONDITION) {                    \
            (VALUE) |= (FLAG);              \
        }                                   \
    } while (0)

static void layers_to_reset_image(struct DP_ResetImageContext *c,
                                  uint16_t target_id, DP_LayerList *ll,
                                  DP_LayerPropsList *lpl);

static void layer_props_to_reset_image(struct DP_ResetImageContext *c,
                                       DP_LayerProps *lp, bool group,
                                       uint16_t layer_id, uint8_t sublayer_id)
{
    uint8_t attr_flags = 0;
    SET_FLAG_IF(attr_flags, DP_layer_props_censored(lp),
                DP_MSG_LAYER_ATTRIBUTES_FLAGS_CENSOR);
    SET_FLAG_IF(attr_flags, group && DP_layer_props_isolated(lp),
                DP_MSG_LAYER_ATTRIBUTES_FLAGS_ISOLATED);
    reset_image_push(c, DP_msg_layer_attributes_new(
                            c->context_id, layer_id, sublayer_id, attr_flags,
                            DP_channel15_to_8(DP_layer_props_opacity(lp)),
                            (uint8_t)DP_layer_props_blend_mode(lp)));
}

static uint16_t layer_to_reset_image(struct DP_ResetImageContext *c,
                                     uint16_t target_id, DP_LayerProps *lp,
                                     bool group, uint32_t fill)
{
    uint16_t layer_id = DP_int_to_uint16(DP_layer_props_id(lp));
    size_t name_len;
    const char *name = DP_layer_props_title(lp, &name_len);
    uint8_t create_flags = 0;
    SET_FLAG_IF(create_flags, target_id != 0,
                DP_MSG_LAYER_TREE_CREATE_FLAGS_INTO);
    SET_FLAG_IF(create_flags, group, DP_MSG_LAYER_TREE_CREATE_FLAGS_GROUP);
    reset_image_push(
        c, DP_msg_layer_tree_create_new(c->context_id, layer_id, 0, target_id,
                                        fill, create_flags, name, name_len));
    layer_props_to_reset_image(c, lp, group, layer_id, 0);
    return layer_id;
}

static bool tiles_to_reset_image(struct DP_ResetImageContext *c,
                                 DP_LayerContent *lc, uint16_t layer_id,
                                 uint8_t sublayer_id)
{
    // TODO: use tile runs and layer fill to optimize this.
    DP_TileCounts counts = DP_tile_counts_round(DP_layer_content_width(lc),
                                                DP_layer_content_height(lc));
    bool pushed = false;
    for (int y = 0; y < counts.y; ++y) {
        for (int x = 0; x < counts.x; ++x) {
            DP_Tile *t = DP_layer_content_tile_at_noinc(lc, x, y);
            if (t && !DP_tile_blank(t)) {
                size_t size = reset_image_maybe_compress_tile(c, t);
                if (size != 0) {
                    pushed = true;
                    reset_image_push(
                        c, DP_msg_put_tile_new(
                               c->context_id, layer_id, sublayer_id,
                               DP_int_to_uint16(x), DP_int_to_uint16(y), 0,
                               set_tile_data, size, c->output_buffer));
                }
            }
        }
    }
    return pushed;
}

static void layer_content_to_reset_image(struct DP_ResetImageContext *c,
                                         uint16_t target_id,
                                         DP_LayerContent *lc, DP_LayerProps *lp)
{
    uint16_t layer_id = layer_to_reset_image(c, target_id, lp, false, 0);
    tiles_to_reset_image(c, lc, layer_id, 0);

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
            uint8_t sublayer_id = DP_int_to_uint8(sub_id);
            if (tiles_to_reset_image(c, sub_lc, layer_id, sublayer_id)) {
                layer_props_to_reset_image(c, sub_lp, false, layer_id,
                                           sublayer_id);
            }
        }
    }
}

static void layer_group_to_reset_image(struct DP_ResetImageContext *c,
                                       uint16_t target_id, DP_LayerGroup *lg,
                                       DP_LayerProps *lp)
{
    uint16_t layer_id = layer_to_reset_image(c, target_id, lp, true, 0);
    layers_to_reset_image(c, layer_id, DP_layer_group_children_noinc(lg),
                          DP_layer_props_children_noinc(lp));
}

static void layers_to_reset_image(struct DP_ResetImageContext *c,
                                  uint16_t target_id, DP_LayerList *ll,
                                  DP_LayerPropsList *lpl)
{
    int count = DP_layer_list_count(ll);
    DP_ASSERT(DP_layer_props_list_count(lpl) == count);
    for (int i = 0; i < count; ++i) {
        DP_LayerListEntry *lle = DP_layer_list_at_noinc(ll, i);
        DP_LayerProps *lp = DP_layer_props_list_at_noinc(lpl, i);
        if (DP_layer_list_entry_is_group(lle)) {
            layer_group_to_reset_image(
                c, target_id, DP_layer_list_entry_group_noinc(lle), lp);
        }
        else {
            layer_content_to_reset_image(
                c, target_id, DP_layer_list_entry_content_noinc(lle), lp);
        }
    }
}

static uint8_t get_annotation_valign_flags(DP_Annotation *a)
{
    switch (DP_annotation_valign(a)) {
    case DP_ANNOTATION_VALIGN_CENTER:
        return DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER;
    case DP_ANNOTATION_VALIGN_BOTTOM:
        return DP_MSG_ANNOTATION_EDIT_FLAGS_VALIGN_CENTER;
    default:
        return 0;
    }
}

static void annotations_to_reset_image(struct DP_ResetImageContext *c,
                                       DP_AnnotationList *al)
{
    int count = DP_annotation_list_count(al);
    for (int i = 0; i < count; ++i) {
        DP_Annotation *a = DP_annotation_list_at_noinc(al, i);
        uint16_t annotation_id = DP_int_to_uint16(DP_annotation_id(a));
        reset_image_push(c, DP_msg_annotation_create_new(
                                c->context_id, annotation_id,
                                DP_int_to_int32(DP_annotation_x(a)),
                                DP_int_to_int32(DP_annotation_y(a)),
                                DP_int_to_uint16(DP_annotation_width(a)),
                                DP_int_to_uint16(DP_annotation_height(a))));

        size_t text_len;
        const char *text = DP_annotation_text(a, &text_len);
        uint8_t edit_flags = get_annotation_valign_flags(a);
        SET_FLAG_IF(edit_flags, DP_annotation_protect(a),
                    DP_MSG_ANNOTATION_EDIT_FLAGS_PROTECT);
        reset_image_push(
            c, DP_msg_annotation_edit_new(c->context_id, annotation_id,
                                          DP_annotation_background_color(a),
                                          edit_flags, 0, text, text_len));
    }
}

static void set_document_metadata_int_field_if_not_default(
    struct DP_ResetImageContext *c, uint8_t field, int value, int default_value)
{
    if (value != default_value) {
        reset_image_push(c, DP_msg_set_metadata_int_new(
                                c->context_id, field, DP_int_to_int32(value)));
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
    if (DP_document_metadata_use_timeline(dm)) {
        reset_image_push(c, DP_msg_set_metadata_int_new(
                                c->context_id,
                                DP_MSG_SET_METADATA_INT_FIELD_USE_TIMELINE, 1));
    }
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

static void key_frame_to_reset_image(struct DP_ResetImageContext *c,
                                     uint16_t track_id, uint16_t frame_index,
                                     DP_KeyFrame *kf)
{
    reset_image_push(
        c, DP_msg_key_frame_set_new(c->context_id, track_id, frame_index,
                                    DP_int_to_uint16(DP_key_frame_layer_id(kf)),
                                    0, DP_MSG_KEY_FRAME_SET_SOURCE_LAYER));

    size_t title_length;
    const char *title = DP_key_frame_title(kf, &title_length);
    if (title_length != 0) {
        reset_image_push(c, DP_msg_key_frame_retitle_new(c->context_id,
                                                         track_id, frame_index,
                                                         title, title_length));
    }

    int layer_count;
    const DP_KeyFrameLayer *kfls = DP_key_frame_layers(kf, &layer_count);
    if (layer_count != 0) {
        reset_image_push(c, DP_msg_key_frame_layer_attributes_new(
                                c->context_id, track_id, frame_index,
                                set_key_frame_layers, layer_count * 2,
                                (void *)kfls));
    }
}

static void track_to_reset_image(struct DP_ResetImageContext *c, DP_Track *t)
{
    uint16_t track_id = DP_int_to_uint16(DP_track_id(t));
    size_t title_length;
    const char *title = DP_track_title(t, &title_length);
    reset_image_push(c, DP_msg_track_create_new(c->context_id, track_id, 0, 0,
                                                title, title_length));
    int key_frame_count = DP_track_key_frame_count(t);
    for (int i = 0; i < key_frame_count; ++i) {
        key_frame_to_reset_image(
            c, track_id, DP_int_to_uint16(DP_track_frame_index_at_noinc(t, i)),
            DP_track_key_frame_at_noinc(t, i));
    }
}

static void timeline_to_reset_image(struct DP_ResetImageContext *c,
                                    DP_Timeline *tl)
{
    int track_count = DP_timeline_count(tl);
    for (int i = track_count - 1; i >= 0; --i) {
        track_to_reset_image(c, DP_timeline_at_noinc(tl, i));
    }
}

static void canvas_state_to_reset_image(struct DP_ResetImageContext *c,
                                        DP_CanvasState *cs)
{
    int width = DP_canvas_state_width(cs);
    int height = DP_canvas_state_height(cs);
    if (width > 0 && height > 0) {
        reset_image_push(c, DP_msg_canvas_resize_new(
                                c->context_id, 0, DP_int_to_int32(width),
                                DP_int_to_int32(height), 0));
    }

    size_t size = reset_image_maybe_compress_tile(
        c, DP_canvas_state_background_tile_noinc(cs));
    if (size != 0) {
        reset_image_push(c, DP_msg_canvas_background_new(c->context_id,
                                                         set_tile_data, size,
                                                         c->output_buffer));
    }

    layers_to_reset_image(c, 0, DP_canvas_state_layers_noinc(cs),
                          DP_canvas_state_layer_props_noinc(cs));
    annotations_to_reset_image(c, DP_canvas_state_annotations_noinc(cs));
    document_metadata_to_reset_image(c, DP_canvas_state_metadata_noinc(cs));
    timeline_to_reset_image(c, DP_canvas_state_timeline_noinc(cs));
}

void DP_reset_image_build(DP_CanvasState *cs, unsigned int context_id,
                          void (*push_message)(void *, DP_Message *),
                          void *user)
{
    struct DP_ResetImageContext c = {
        context_id, push_message,
        user,       DP_malloc(sizeof(*c.pixel_buffer) * DP_TILE_LENGTH),
        0,          NULL};
    canvas_state_to_reset_image(&c, cs);
    DP_free(c.output_buffer);
    DP_free(c.pixel_buffer);
}
