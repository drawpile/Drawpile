// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_SAVE_VIDEO_H
#define DPIMPEX_SAVE_VIDEO_H
#include "save.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Output DP_Output;
typedef struct DP_Image DP_Image;
typedef struct DP_Rect DP_Rect;

#define DP_SAVE_VIDEO_FLAGS_NONE         0x0u
#define DP_SAVE_VIDEO_FLAGS_SCALE_SMOOTH 0x1u

// GIF palette sizes, always 16x16 images in BGRA format.
#define DP_SAVE_VIDEO_GIF_PALETTE_DIMENSION 16
#define DP_SAVE_VIDEO_GIF_PALETTE_BYTES     ((size_t)1024)

typedef enum DP_SaveVideoFormat {
    DP_SAVE_VIDEO_FORMAT_MP4_VP9,
    DP_SAVE_VIDEO_FORMAT_WEBM_VP8,
    DP_SAVE_VIDEO_FORMAT_WEBP,
    DP_SAVE_VIDEO_FORMAT_PALETTE,
    DP_SAVE_VIDEO_FORMAT_GIF,
    DP_SAVE_VIDEO_FORMAT_MP4_H264,
    DP_SAVE_VIDEO_FORMAT_MP4_AV1,
} DP_SaveVideoFormat;

typedef enum DP_SaveVideoDestination {
    DP_SAVE_VIDEO_DESTINATION_PATH,
    DP_SAVE_VIDEO_DESTINATION_OUTPUT,
} DP_SaveVideoDestination;

typedef struct DP_SaveVideoNextFrame {
    DP_SaveResult result;
    int width;
    int height;
    const void *pixels;
    double progress;
    int instances;
} DP_SaveVideoNextFrame;

typedef bool (*DP_SaveVideoNextFrameFn)(void *user, DP_SaveVideoNextFrame *f);

typedef struct DP_SaveVideoParams {
    DP_SaveVideoDestination destination;
    void *path_or_output;
    const unsigned char *palette_data;
    size_t palette_size;
    unsigned int flags;
    int format;
    int width;
    int height;
    double framerate;
    DP_SaveVideoNextFrameFn next_frame_fn;
    DP_SaveProgressFn progress_fn;
    void *user;
} DP_SaveVideoParams;

typedef struct DP_SaveAnimationVideoParams {
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
    double framerate;
    int loops;
    DP_SaveProgressFn progress_fn;
    void *user;
} DP_SaveAnimationVideoParams;

typedef struct DP_SaveAnimationGifParams {
    DP_CanvasState *cs;
    const DP_Rect *area;
    DP_SaveVideoDestination destination;
    void *path_or_output;
    unsigned int flags;
    int width;
    int height;
    int start;
    int end_inclusive;
    double framerate;
    DP_SaveProgressFn progress_fn;
    void *user;
} DP_SaveAnimationGifParams;

bool DP_save_video_format_supported(int format);

DP_SaveResult DP_save_video(DP_SaveVideoParams params);

DP_SaveResult DP_save_animation_video(DP_SaveAnimationVideoParams params);

DP_SaveResult DP_save_animation_gif(DP_SaveAnimationGifParams params);

#endif
