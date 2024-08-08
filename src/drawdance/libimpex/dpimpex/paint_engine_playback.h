// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_PAINT_ENGINE_PLAYBACK_H
#define DPIMPEX_PAINT_ENGINE_PLAYBACK_H
#include "player_index.h"
#include <dpcommon/common.h>
#include <dpengine/paint_engine.h>


DP_PlayerResult
DP_paint_engine_playback_step(DP_PaintEngine *pe, long long steps,
                              DP_PaintEnginePushMessageFn push_message,
                              void *user);

DP_PlayerResult DP_paint_engine_playback_skip_by(
    DP_PaintEngine *pe, DP_DrawContext *dc, long long steps, bool by_snapshots,
    DP_PaintEnginePushMessageFn push_message, void *user);

DP_PlayerResult DP_paint_engine_playback_jump_to(
    DP_PaintEngine *pe, DP_DrawContext *dc, long long position,
    DP_PaintEnginePushMessageFn push_message, void *user);

DP_PlayerResult DP_paint_engine_playback_begin(DP_PaintEngine *pe);

DP_PlayerResult DP_paint_engine_playback_play(
    DP_PaintEngine *pe, long long msecs,
    DP_PaintEngineFilterMessageFn filter_message_or_null,
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

bool DP_paint_engine_playback_flush(DP_PaintEngine *pe,
                                    DP_PaintEnginePushMessageFn push_message,
                                    void *user);

bool DP_paint_engine_playback_close(DP_PaintEngine *pe);


#endif
