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
#ifndef DPENGINE_PAINT_ENGINE
#define DPENGINE_PAINT_ENGINE
#include "canvas_diff.h"
#include "canvas_history.h"
#include "player.h"
#include "recorder.h"
#include "view_mode.h"
#include <dpcommon/common.h>

typedef struct DP_AclState DP_AclState;
typedef struct DP_AnnotationList DP_AnnotationList;
typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DocumentMetadata DP_DocumentMetadata;
typedef struct DP_Image DP_Image;
typedef struct DP_LayerPropsList DP_LayerPropsList;
typedef struct DP_Message DP_Message;
typedef union DP_Pixel8 DP_Pixel8;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientLayerContent DP_TransientLayerContent;
#else
typedef struct DP_LayerContent DP_TransientLayerContent;
#endif

typedef void (*DP_PaintEnginePlaybackFn)(void *user, long long position,
                                         int interval);
typedef void (*DP_PaintEngineAclsChangedFn)(void *user, int acl_change_flags);
typedef void (*DP_PaintEngineLaserTrailFn)(void *user, unsigned int context_id,
                                           int persistence, uint32_t color);
typedef void (*DP_PaintEngineMovePointerFn)(void *user, unsigned int context_id,
                                            int x, int y);
typedef void (*DP_PaintEngineDefaultLayerSetFn)(void *user, int layer_id);
typedef void (*DP_PaintEngineCatchupFn)(void *user, int progress);
typedef void (*DP_PaintEngineRecorderStateChanged)(void *user, bool started);
typedef void (*DP_PaintEngineResizedFn)(void *user, int offset_x, int offset_y,
                                        int prev_width, int prev_height);
typedef void (*DP_PaintEngineLayerPropsChangedFn)(void *user,
                                                  DP_LayerPropsList *lpl);
typedef void (*DP_PaintEngineAnnotationsChangedFn)(void *user,
                                                   DP_AnnotationList *al);
typedef void (*DP_PaintEngineDocumentMetadataChangedFn)(
    void *user, DP_DocumentMetadata *dm);
typedef void (*DP_PaintEngineTimelineChangedFn)(void *user, DP_Timeline *tl);
typedef void (*DP_PaintEngineCursorMovedFn)(void *user, unsigned int context_id,
                                            int layer_id, int x, int y);
typedef void (*DP_PaintEngineRenderSizeFn)(void *user, int width, int height);
typedef void (*DP_PaintEngineRenderTileFn)(void *user, int x, int y,
                                           DP_Pixel8 *pixels, int thread_index);
typedef void (*DP_PaintEnginePushMessageFn)(void *user, DP_Message *msg);


typedef struct DP_PaintEngine DP_PaintEngine;

DP_PaintEngine *DP_paint_engine_new_inc(
    DP_DrawContext *paint_dc, DP_DrawContext *preview_dc, DP_AclState *acls,
    DP_CanvasState *cs_or_null, DP_CanvasHistorySavePointFn save_point_fn,
    void *save_point_user, bool want_canvas_history_dump,
    const char *canvas_history_dump_dir, DP_RecorderGetTimeMsFn get_time_ms_fn,
    void *get_time_ms_user, DP_Player *player_or_null,
    DP_PaintEnginePlaybackFn playback_fn, void *playback_user);

void DP_paint_engine_free_join(DP_PaintEngine *pe);

int DP_paint_engine_render_thread_count(DP_PaintEngine *pe);

DP_TransientLayerContent *
DP_paint_engine_render_content_noinc(DP_PaintEngine *pe);

void DP_paint_engine_local_drawing_in_progress_set(
    DP_PaintEngine *pe, bool local_drawing_in_progress);

bool DP_paint_engine_want_canvas_history_dump(DP_PaintEngine *pe);

void DP_paint_engine_want_canvas_history_dump_set(
    DP_PaintEngine *pe, bool want_canvas_history_dump);

void DP_paint_engine_active_layer_id_set(DP_PaintEngine *pe, int layer_id);

void DP_paint_engine_active_frame_index_set(DP_PaintEngine *pe,
                                            int frame_index);

void DP_paint_engine_view_mode_set(DP_PaintEngine *pe, DP_ViewMode vm);

// Only keeps a reference to the given onion skins, pointer must remain valid.
void DP_paint_engine_onion_skins_set(DP_PaintEngine *pe,
                                     const DP_OnionSkins *oss_or_null);

bool DP_paint_engine_reveal_censored(DP_PaintEngine *pe);

void DP_paint_engine_reveal_censored_set(DP_PaintEngine *pe,
                                         bool reveal_censored);

void DP_paint_engine_inspect_context_id_set(DP_PaintEngine *pe,
                                            unsigned int context_id);

void DP_paint_engine_layer_visibility_set(DP_PaintEngine *pe, int layer_id,
                                          bool hidden);

DP_Tile *DP_paint_engine_local_background_tile_noinc(DP_PaintEngine *pe);

void DP_paint_engine_local_background_tile_set_noinc(DP_PaintEngine *pe,
                                                     DP_Tile *tile_or_null);

bool DP_paint_engine_recorder_start(DP_PaintEngine *pe, DP_RecorderType type,
                                    const char *path);

bool DP_paint_engine_recorder_stop(DP_PaintEngine *pe);

bool DP_paint_engine_recorder_is_recording(DP_PaintEngine *pe);

DP_PlayerResult
DP_paint_engine_playback_step(DP_PaintEngine *pe, long long steps,
                              DP_PaintEnginePushMessageFn push_message,
                              void *user);

DP_PlayerResult DP_paint_engine_playback_skip_by(
    DP_PaintEngine *pe, DP_DrawContext *dc, long long steps,
    DP_PaintEnginePushMessageFn push_message, void *user);

DP_PlayerResult DP_paint_engine_playback_jump_to(
    DP_PaintEngine *pe, DP_DrawContext *dc, long long position,
    DP_PaintEnginePushMessageFn push_message, void *user);

bool DP_paint_engine_playback_index_build(
    DP_PaintEngine *pe, DP_DrawContext *dc,
    DP_PlayerIndexShouldSnapshotFn should_snapshot_fn,
    DP_PlayerIndexProgressFn progress_fn, void *user);

bool DP_paint_engine_playback_index_load(DP_PaintEngine *pe);

unsigned int DP_paint_engine_playback_index_message_count(DP_PaintEngine *pe);

size_t DP_paint_engine_playback_index_entry_count(DP_PaintEngine *pe);

DP_Image *DP_paint_engine_playback_index_thumbnail_at(DP_PaintEngine *pe,
                                                      size_t index,
                                                      bool *out_error);

DP_PlayerResult DP_paint_engine_playback_dump_step(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user);

DP_PlayerResult DP_paint_engine_playback_dump_jump_previous_reset(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user);

DP_PlayerResult DP_paint_engine_playback_dump_jump_next_reset(
    DP_PaintEngine *pe, DP_PaintEnginePushMessageFn push_message, void *user);

DP_PlayerResult
DP_paint_engine_playback_dump_jump(DP_PaintEngine *pe, long long position,
                                   DP_PaintEnginePushMessageFn push_message,
                                   void *user);

bool DP_paint_engine_playback_close(DP_PaintEngine *pe);

// Returns the number of drawing commands actually pushed to the paint engine.
int DP_paint_engine_handle_inc(DP_PaintEngine *pe, bool local,
                               bool override_acls, int count, DP_Message **msgs,
                               DP_PaintEngineAclsChangedFn acls_changed,
                               DP_PaintEngineLaserTrailFn laser_trail,
                               DP_PaintEngineMovePointerFn move_pointer,
                               void *user);

void DP_paint_engine_tick(
    DP_PaintEngine *pe, DP_PaintEngineCatchupFn catchup,
    DP_PaintEngineRecorderStateChanged recorder_state_changed,
    DP_PaintEngineResizedFn resized, DP_CanvasDiffEachPosFn tile_changed,
    DP_PaintEngineLayerPropsChangedFn layer_props_changed,
    DP_PaintEngineAnnotationsChangedFn annotations_changed,
    DP_PaintEngineDocumentMetadataChangedFn document_metadata_changed,
    DP_PaintEngineTimelineChangedFn timeline_changed,
    DP_PaintEngineCursorMovedFn cursor_moved,
    DP_PaintEngineDefaultLayerSetFn default_layer_set, void *user);

void DP_paint_engine_prepare_render(DP_PaintEngine *pe,
                                    DP_PaintEngineRenderSizeFn render_size,
                                    void *user);

void DP_paint_engine_render_everything(DP_PaintEngine *pe,
                                       DP_PaintEngineRenderTileFn render_tile,
                                       void *user);

void DP_paint_engine_render_tile_bounds(DP_PaintEngine *pe, int tile_left,
                                        int tile_top, int tile_right,
                                        int tile_bottom,
                                        DP_PaintEngineRenderTileFn render_tile,
                                        void *user);

void DP_paint_engine_preview_cut(DP_PaintEngine *pe, int layer_id, int x, int y,
                                 int width, int height,
                                 const DP_Pixel8 *mask_or_null);

void DP_paint_engine_preview_dabs_inc(DP_PaintEngine *pe, int layer_id,
                                      int count, DP_Message **messages);

void DP_paint_engine_preview_clear(DP_PaintEngine *pe);

DP_CanvasState *DP_paint_engine_view_canvas_state_inc(DP_PaintEngine *pe);

DP_CanvasState *DP_paint_engine_history_canvas_state_inc(DP_PaintEngine *pe);


#endif
