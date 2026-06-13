// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_ANDROID_VIDEO_ENCODER_H
#define DPIMPEX_ANDROID_VIDEO_ENCODER_H
#include <dpcommon/common.h>

#define DP_ANDROID_VIDEO_ENCODER_STATUS_OK            0
#define DP_ANDROID_VIDEO_ENCODER_STATUS_TIMEOUT       1
#define DP_ANDROID_VIDEO_ENCODER_STATUS_END_OF_STREAM 2
#define DP_ANDROID_VIDEO_ENCODER_STATUS_NEEDS_COPY    3
#define DP_ANDROID_VIDEO_ENCODER_STATUS_ERROR_COPY    999

typedef void (*DP_AndroidVideoEncoderFormatSupportAddFn)(void *user,
                                                         const char *name,
                                                         bool hardware);

typedef struct DP_AndroidVideoEncoder DP_AndroidVideoEncoder;

typedef struct DP_AndroidVideoEncoderParams {
    const char *output;
    const char *temp;
    const char *encoder;
    double framerate;
    int format;
    int width;
    int height;
} DP_AndroidVideoEncoderParams;

typedef struct DP_AndroidVideoEncoderImage {
    uint8_t *buffer_y;
    uint8_t *buffer_u;
    uint8_t *buffer_v;
    int row_stride_y;
    int row_stride_u;
    int row_stride_v;
    int pixel_stride_u;
    int pixel_stride_v;
} DP_AndroidVideoEncoderImage;

void DP_android_video_encoder_format_support(
    int format, DP_AndroidVideoEncoderFormatSupportAddFn fn, void *user);

DP_AndroidVideoEncoder *
DP_android_video_encoder_new(DP_AndroidVideoEncoderParams params);

void DP_android_video_encoder_free(DP_AndroidVideoEncoder *ave);

int DP_android_video_encoder_start(DP_AndroidVideoEncoder *ave);

int DP_android_video_encoder_prepare(DP_AndroidVideoEncoder *ave,
                                     long long timeout_usec);

bool DP_android_video_encoder_image(DP_AndroidVideoEncoder *ave,
                                    DP_AndroidVideoEncoderImage *out_image);

int DP_android_video_encoder_commit(DP_AndroidVideoEncoder *ave);

int DP_android_video_encoder_drain(DP_AndroidVideoEncoder *ave,
                                   long long timeout_usec);

int DP_android_video_encoder_finish(DP_AndroidVideoEncoder *ave);

int DP_android_video_encoder_close(DP_AndroidVideoEncoder *ave);

#endif
