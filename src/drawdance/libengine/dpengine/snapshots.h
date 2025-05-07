// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_SNAPSHOTS_H
#define DPENGINE_SNAPSHOTS_H
#include <dpcommon/common.h>

typedef struct DP_Annotation DP_Annotation;
typedef struct DP_CanvasHistory DP_CanvasHistory;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DocumentMetadata DP_DocumentMetadata;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_KeyFrame DP_KeyFrame;
typedef struct DP_LayerProps DP_LayerProps;
typedef struct DP_Message DP_Message;
typedef struct DP_Output DP_Output;
typedef struct DP_Track DP_Track;


typedef struct DP_Snapshot DP_Snapshot;
typedef struct DP_SnapshotQueue DP_SnapshotQueue;

typedef long long (*DP_SnapshotQueueTimestampMsFn)(void *user);

typedef DP_Snapshot *(*DP_SnapshotAtFn)(DP_SnapshotQueue *sq, size_t index);

typedef void (*DP_SnapshotsGetFn)(void *user, DP_SnapshotQueue *sq,
                                  size_t count, DP_SnapshotAtFn at);

typedef enum DP_ResetImageCompression {
    DP_RESET_IMAGE_COMPRESSION_GZIP8BE,
    DP_RESET_IMAGE_COMPRESSION_ZSTD8LE,
} DP_ResetImageCompression;

typedef struct DP_ResetImageOptions {
    bool use_worker;
    bool merge_sublayers;
    bool include_selections;
    DP_ResetImageCompression compression;
    int thumb_width;
    int thumb_height;
    bool (*thumb_write)(DP_Image *, DP_Output *);
} DP_ResetImageOptions;

typedef enum DP_ResetEntryType {
    DP_RESET_ENTRY_CANVAS,
    DP_RESET_ENTRY_BACKGROUND,
    DP_RESET_ENTRY_LAYER,
    DP_RESET_ENTRY_TILE,
    DP_RESET_ENTRY_SELECTION_TILE,
    DP_RESET_ENTRY_ANNOTATION,
    DP_RESET_ENTRY_TRACK,
    DP_RESET_ENTRY_FRAME,
    DP_RESET_ENTRY_THUMB,
} DP_ResetEntryType;

typedef struct DP_ResetEntryCanvas {
    int width;
    int height;
    DP_DocumentMetadata *dm;
} DP_ResetEntryCanvas;

typedef struct DP_ResetEntryBackground {
    size_t size;
    void *data;
} DP_ResetEntryBackground;

typedef struct DP_ResetEntryLayer {
    int layer_index;
    int layer_id;
    int sublayer_id;
    int parent_index;
    int parent_id;
    uint32_t fill;
    DP_LayerProps *lp;
} DP_ResetEntryLayer;

typedef struct DP_ResetEntryTile {
    int layer_index;
    int layer_id;
    int sublayer_id;
    int tile_index;
    int tile_run;
    unsigned int context_id;
    size_t size;
    void *data;
} DP_ResetEntryTile;

typedef struct DP_ResetEntrySelectionTile {
    int selection_index;
    int selection_id;
    int tile_index;
    unsigned int context_id;
    size_t size;
    void *data;
} DP_ResetEntrySelectionTile;

typedef struct DP_ResetEntryAnnotation {
    int annotation_index;
    DP_Annotation *a;
} DP_ResetEntryAnnotation;

typedef struct DP_ResetEntryTrack {
    int track_index;
    DP_Track *t;
} DP_ResetEntryTrack;

typedef struct DP_ResetEntryFrame {
    int track_index;
    int track_id;
    int frame_index;
    DP_KeyFrame *kf;
} DP_ResetEntryFrame;

typedef struct DP_ResetEntryThumb {
    size_t size;
    void *data;
} DP_ResetEntryThumb;

typedef struct DP_ResetEntry {
    DP_ResetEntryType type;
    union {
        DP_ResetEntryCanvas canvas;
        DP_ResetEntryBackground background;
        DP_ResetEntryLayer layer;
        DP_ResetEntryTile tile;
        DP_ResetEntrySelectionTile selection_tile;
        DP_ResetEntryAnnotation annotation;
        DP_ResetEntryTrack track;
        DP_ResetEntryFrame frame;
        DP_ResetEntryThumb thumb;
    } DP_ANONYMOUS(value);
} DP_ResetEntry;


long long DP_snapshot_timestamp_ms(DP_Snapshot *s);

DP_CanvasState *DP_snapshot_canvas_state_noinc(DP_Snapshot *s);


DP_SnapshotQueue *
DP_snapshot_queue_new(size_t max_count, long long min_delay_ms,
                      DP_SnapshotQueueTimestampMsFn timestamp_fn,
                      void *timestamp_user);

void DP_snapshot_queue_free(DP_SnapshotQueue *sq);

void DP_snapshot_queue_max_count_set(DP_SnapshotQueue *sq, size_t max_count);
void DP_snapshot_queue_min_delay_ms_set(DP_SnapshotQueue *sq,
                                        long long min_delay_ms);

// Pass this to canvas_history_new to wire up the snapshot queue.
void DP_snapshot_queue_on_save_point(void *user, DP_CanvasState *cs,
                                     bool snapshot_requested);

void DP_snapshot_queue_get_with(DP_SnapshotQueue *sq, DP_SnapshotsGetFn get_fn,
                                void *user);


void DP_reset_image_build_with(
    DP_CanvasState *cs, const DP_ResetImageOptions *options,
    void (*handle_entry)(void *, const DP_ResetEntry *), void *user);

void DP_reset_image_build(DP_CanvasState *cs, unsigned int context_id,
                          void (*push_message)(void *, DP_Message *),
                          void *user);


#endif
