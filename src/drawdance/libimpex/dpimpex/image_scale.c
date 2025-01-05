// SPDX-License-Identifier: GPL-3.0-or-later
#include "image_scale.h"
#include <dpcommon/common.h>
#include <dpengine/image.h>
#include <libswscale/swscale.h>


static int
get_sws_flags_from_interpolation(DP_ImageScaleInterpolation interpolation)
{
    switch (interpolation) {
    case DP_IMAGE_SCALE_INTERPOLATION_BILINEAR:
        return SWS_BILINEAR;
    case DP_IMAGE_SCALE_INTERPOLATION_BICUBIC:
        return SWS_BICUBIC;
    case DP_IMAGE_SCALE_INTERPOLATION_BICUBLIN:
        return SWS_BICUBLIN;
    case DP_IMAGE_SCALE_INTERPOLATION_GAUSS:
        return SWS_GAUSS;
    case DP_IMAGE_SCALE_INTERPOLATION_SINC:
        return SWS_SINC;
    case DP_IMAGE_SCALE_INTERPOLATION_LANCZOS:
        return SWS_LANCZOS;
    case DP_IMAGE_SCALE_INTERPOLATION_SPLINE:
        return SWS_SPLINE;
    default:
        DP_warn("Unknown interpolation %d", (int)interpolation);
        return SWS_BILINEAR;
    }
}


DP_Image *DP_image_scale_sws_pixels(const DP_Pixel8 *pixels, int src_height,
                                    int src_width, int width, int height,
                                    DP_ImageScaleInterpolation interpolation)
{
    if (width > 0 && height > 0) {
        struct SwsContext *sws_context = sws_getContext(
            src_width, src_height, AV_PIX_FMT_BGRA, width, height,
            AV_PIX_FMT_BGRA, get_sws_flags_from_interpolation(interpolation),
            NULL, NULL, NULL);
        if (!sws_context) {
            DP_error_set("Failed to allocate scaling context");
            return NULL;
        }

        const uint8_t *src_data = (const uint8_t *)pixels;
        const int src_stride = src_width * 4;

        DP_Image *dst = DP_image_new(width, height);
        uint8_t *dst_data = (uint8_t *)DP_image_pixels(dst);
        const int dst_stride = width * 4;

        sws_scale(sws_context, &src_data, &src_stride, 0, src_height, &dst_data,
                  &dst_stride);

        sws_freeContext(sws_context);
        return dst;
    }
    else {
        DP_error_set("Can't scale to zero dimensions");
        return NULL;
    }
}

DP_Image *DP_image_scale_sws(DP_Image *img, int width, int height,
                             DP_ImageScaleInterpolation interpolation)
{
    DP_ASSERT(img);
    return DP_image_scale_sws_pixels(DP_image_pixels(img), DP_image_width(img),
                                     DP_image_height(img), width, height,
                                     interpolation);
}
