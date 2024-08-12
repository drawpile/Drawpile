// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_SAVE_VIDEO_H
#define DPIMPEX_SAVE_VIDEO_H
#include "save.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Rect DP_Rect;

#define DP_SAVE_VIDEO_FLAGS_NONE 0x0u
#define DP_SAVE_VIDEO_FLAGS_SCALE_SMOOTH 0x1u

typedef enum DP_SaveVideoFormat {
    DP_SAVE_VIDEO_FORMAT_MP4,
    DP_SAVE_VIDEO_FORMAT_WEBM,
    DP_SAVE_VIDEO_FORMAT_WEBP,
} DP_SaveVideoFormat;

typedef struct DP_SaveVideoParams {
    DP_CanvasState *cs;
    const DP_Rect *area;
    const char *path;
    unsigned int flags;
    int format;
    int width;
    int height;
    int start;
    int end_inclusive;
    int framerate;
    int loops;
    DP_SaveAnimationProgressFn progress_fn;
    void *user;
} DP_SaveVideoParams;

bool DP_save_video_format_supported(int format);

DP_SaveResult DP_save_animation_video(DP_SaveVideoParams params);

#endif
