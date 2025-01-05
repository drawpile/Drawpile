// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_IMAGE_SCALE_H
#define DPIMPEX_IMAGE_SCALE_H
#include <dpcommon/common.h>

typedef struct DP_Image DP_Image;
typedef union DP_Pixel8 DP_Pixel8;

typedef enum DP_ImageScaleInterpolation {
    DP_IMAGE_SCALE_INTERPOLATION_BILINEAR,
    DP_IMAGE_SCALE_INTERPOLATION_BICUBIC,
    DP_IMAGE_SCALE_INTERPOLATION_BICUBLIN,
    DP_IMAGE_SCALE_INTERPOLATION_GAUSS,
    DP_IMAGE_SCALE_INTERPOLATION_SINC,
    DP_IMAGE_SCALE_INTERPOLATION_LANCZOS,
    DP_IMAGE_SCALE_INTERPOLATION_SPLINE,
} DP_ImageScaleInterpolation;


DP_Image *DP_image_scale_sws_pixels(const DP_Pixel8 *pixels, int src_height,
                                    int src_width, int width, int height,
                                    DP_ImageScaleInterpolation interpolation);

DP_Image *DP_image_scale_sws(DP_Image *img, int width, int height,
                             DP_ImageScaleInterpolation interpolation);


#endif
