// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_WINDOWS_VIDEO_ENCODER_H
#define DPIMPEX_WINDOWS_VIDEO_ENCODER_H
#include <dpcommon/common.h>


typedef struct DP_WindowsMediaFoundation DP_WindowsMediaFoundation;
typedef struct DP_WindowsVideoEncoder DP_WindowsVideoEncoder;

typedef struct DP_WindowsVideoEncoderParams {
    const char *path;
    int framerate_num;
    int framerate_den;
    int width;
    int height;
    int frame_size;
} DP_WindowsVideoEncoderParams;

typedef struct DP_WindowsVideoEncoderFrame {
    uint8_t *buffers[4];
    int linesizes[4];
} DP_WindowsVideoEncoderFrame;


// See videoexporthandle.h.
DP_WindowsMediaFoundation *DP_windows_media_foundation_acquire(void);

void DP_windows_media_foundation_release(DP_WindowsMediaFoundation *wmf);


bool DP_windows_video_encoder_format_support(int format);

DP_WindowsVideoEncoder *
DP_windows_video_encoder_new(DP_WindowsVideoEncoderParams params);

void DP_windows_video_encoder_free(DP_WindowsVideoEncoder *wve);

bool DP_windows_video_encoder_start(DP_WindowsVideoEncoder *wve);

bool DP_windows_video_encoder_prepare(DP_WindowsVideoEncoder *wve,
                                      DP_WindowsVideoEncoderFrame *out_frame);

bool DP_windows_video_encoder_commit(DP_WindowsVideoEncoder *wve);

bool DP_windows_video_encoder_finish(DP_WindowsVideoEncoder *wve);

#endif
