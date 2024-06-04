// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_LOAD_ANIMATION_H
#define DPENGINE_LOAD_ANIMATION_H
#include "load.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientLayerProps DP_TransientLayerProps;
typedef struct DP_TransientTrack DP_TransientTrack;
#else
typedef struct DP_LayerProps DP_TransientLayerProps;
typedef struct DP_Track DP_TransientTrack;
#endif


typedef const char *(*DP_LoadAnimationFramesPathAtFn)(void *user, int i);

typedef void (*DP_LoadAnimationSetLayerTitleFn)(DP_TransientLayerProps *tlp,
                                                int i);

typedef void (*DP_LoadAnimationSetGroupTitleFn)(DP_TransientLayerProps *tlp,
                                                int i);

typedef void (*DP_LoadAnimationSetTrackTitleFn)(DP_TransientTrack *tt, int i);

DP_CanvasState *DP_load_animation_frames(
    DP_DrawContext *dc, int path_count, DP_LoadAnimationFramesPathAtFn path_at,
    void *user, int hold_time, int framerate,
    DP_LoadAnimationSetLayerTitleFn set_layer_title,
    DP_LoadAnimationSetGroupTitleFn set_group_title,
    DP_LoadAnimationSetTrackTitleFn set_track_title, DP_LoadResult *out_result);

DP_CanvasState *DP_load_animation_layers(
    DP_DrawContext *dc, const char *path, int hold_time, int framerate,
    unsigned int flags, DP_LoadAnimationSetGroupTitleFn set_group_title,
    DP_LoadAnimationSetTrackTitleFn set_track_title, DP_LoadResult *out_result);

#endif
