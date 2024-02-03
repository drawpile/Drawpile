// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_LOAD_OLD_ANIMATION_H
#define DPENGINE_LOAD_OLD_ANIMATION_H
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


typedef void (*DP_LoadOldAnimationSetGroupTitleFn)(DP_TransientLayerProps *tlp,
                                                   int i);

typedef void (*DP_LoadOldAnimationSetTrackTitleFn)(DP_TransientTrack *tt,
                                                   int i);

DP_CanvasState *
DP_load_old_animation(DP_DrawContext *dc, const char *path, int hold_time,
                      int framerate, unsigned int flags,
                      DP_LoadOldAnimationSetGroupTitleFn set_group_title,
                      DP_LoadOldAnimationSetTrackTitleFn set_track_title,
                      DP_LoadResult *out_result);

#endif
