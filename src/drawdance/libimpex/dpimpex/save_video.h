// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_SAVE_VIDEO_H
#define DPIMPEX_SAVE_VIDEO_H
#include "save.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Output DP_Output;
typedef struct DP_Rect DP_Rect;

#define DP_SAVE_VIDEO_FLAGS_NONE         0x0u
#define DP_SAVE_VIDEO_FLAGS_SCALE_SMOOTH 0x1u

// GIF palette sizes, always 16x16 images in BGRA format.
#define DP_SAVE_VIDEO_GIF_PALETTE_DIMENSION 16
#define DP_SAVE_VIDEO_GIF_PALETTE_BYTES     ((size_t)1024)

typedef enum DP_SaveVideoFormat {
    DP_SAVE_VIDEO_FORMAT_MP4,
    DP_SAVE_VIDEO_FORMAT_WEBM,
    DP_SAVE_VIDEO_FORMAT_WEBP,
    DP_SAVE_VIDEO_FORMAT_PALETTE,
    DP_SAVE_VIDEO_FORMAT_GIF,
} DP_SaveVideoFormat;

typedef enum DP_SaveVideoDestination {
    DP_SAVE_VIDEO_DESTINATION_PATH,
    DP_SAVE_VIDEO_DESTINATION_OUTPUT,
} DP_SaveVideoDestination;

typedef struct DP_SaveVideoParams {
    DP_CanvasState *cs;
    const DP_Rect *area;
    DP_SaveVideoDestination destination;
    void *path_or_output;
    const unsigned char *palette_data;
    size_t palette_size;
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

typedef struct DP_SaveGifParams {
    DP_CanvasState *cs;
    const DP_Rect *area;
    DP_SaveVideoDestination destination;
    void *path_or_output;
    unsigned int flags;
    int width;
    int height;
    int start;
    int end_inclusive;
    int framerate;
    DP_SaveAnimationProgressFn progress_fn;
    void *user;
} DP_SaveGifParams;

bool DP_save_video_format_supported(int format);

DP_SaveResult DP_save_animation_video(DP_SaveVideoParams params);

DP_SaveResult DP_save_animation_video_gif(DP_SaveGifParams params);

#endif
