// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_PLAYER_INDEX_H
#define DPIMPEX_PLAYER_INDEX_H
#include <dpcommon/common.h>
#include <dpengine/player.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_Message DP_Message;
typedef struct DP_Player DP_Player;


typedef struct DP_PlayerIndexEntrySnapshot DP_PlayerIndexEntrySnapshot;

typedef bool (*DP_PlayerIndexShouldSnapshotFn)(void *user);
typedef void (*DP_PlayerIndexProgressFn)(void *user, int percent);


bool DP_player_index_build(DP_Player *player, DP_DrawContext *dc,
                           DP_PlayerIndexShouldSnapshotFn should_snapshot_fn,
                           DP_PlayerIndexProgressFn progress_fn, void *user);


bool DP_player_index_load(DP_Player *player);

unsigned int DP_player_index_message_count(DP_Player *player);

size_t DP_player_index_entry_count(DP_Player *player);

DP_PlayerIndexEntry
DP_player_index_entry_search(DP_Player *player, long long position, bool after);

DP_PlayerIndexEntrySnapshot *
DP_player_index_entry_load(DP_Player *player, DP_DrawContext *dc,
                           DP_PlayerIndexEntry entry);

DP_CanvasState *DP_player_index_entry_snapshot_canvas_state_inc(
    DP_PlayerIndexEntrySnapshot *snapshot);

int DP_player_index_entry_snapshot_message_count(
    DP_PlayerIndexEntrySnapshot *snapshot);

DP_Message *DP_player_index_entry_snapshot_message_at_inc(
    DP_PlayerIndexEntrySnapshot *snapshot, int i);

void DP_player_index_entry_snapshot_free(DP_PlayerIndexEntrySnapshot *snapshot);

DP_Image *DP_player_index_thumbnail_at(DP_Player *player, size_t index,
                                       bool *out_error);

#endif
