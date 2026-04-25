// SPDX-License-Identifier: GPL-3.0-or-later
#include "filter_hsx_adjust.h"
#include "filter_props.h"
#include "pixels.h"
#include "tile.h"
#include <dpcommon/common.h>
#include <helpers.h>


static void filter_pixels_with(float h_adjust, float sc_adjust,
                               float vly_adjust, int count,
                               DP_Pixel15 *DP_RESTRICT dst,
                               const DP_Pixel15 *DP_RESTRICT src,
                               void (*from_rgb)(float *, float *, float *),
                               void (*to_rgb)(float *, float *, float *))
{
    for (int i = 0; i < count; ++i) {
        DP_Pixel15 dp = dst[i];
        if (dp.a != 0) {
            uint16_t sa = src[i].a;
            if (sa == 0) {
                dst[i] = DP_pixel15_zero();
            }
            else {
                DP_UPixelFloat dpuf =
                    DP_pixel_float_unpremultiply(DP_pixel15_to_float(dp));
                dpuf.a = DP_channel15_to_float(sa);

                from_rgb(&dpuf.r, &dpuf.g, &dpuf.b);

                // Smooth transition between chromatic and achromatic colors.
                float chroma_mask =
                    DP_clamp_float((dpuf.g - 0.02f) / 0.1f, 0.0f, 1.0f);
                dpuf.r += h_adjust * chroma_mask;
                dpuf.g *= 1.0f + ((sc_adjust - 1.0f) * chroma_mask);
                dpuf.b *= vly_adjust;

                to_rgb(&dpuf.r, &dpuf.g, &dpuf.b);

                dst[i] = DP_pixel_float_to_15(DP_pixel_float_premultiply(dpuf));
            }
        }
    }
}

void DP_filter_pixels_hsv_adjust(DP_FilterPropsHsvAdjust *fpha, int count,
                                 DP_Pixel15 *DP_RESTRICT dst,
                                 const DP_Pixel15 *DP_RESTRICT src)
{
    DP_ASSERT(fpha);
    DP_ASSERT(dst);
    DP_ASSERT(src);
    filter_pixels_with(DP_filter_props_hsv_adjust_h(fpha),
                       DP_filter_props_hsv_adjust_s(fpha),
                       DP_filter_props_hsv_adjust_v(fpha), count, dst, src,
                       rgb_to_hsv_float, hsv_to_rgb_float);
}

void DP_filter_pixels_hsl_adjust(DP_FilterPropsHslAdjust *fpha, int count,
                                 DP_Pixel15 *DP_RESTRICT dst,
                                 const DP_Pixel15 *DP_RESTRICT src)
{
    DP_ASSERT(fpha);
    DP_ASSERT(dst);
    DP_ASSERT(src);
    filter_pixels_with(DP_filter_props_hsl_adjust_h(fpha),
                       DP_filter_props_hsl_adjust_s(fpha),
                       DP_filter_props_hsl_adjust_l(fpha), count, dst, src,
                       rgb_to_hsl_float, hsl_to_rgb_float);
}

void DP_filter_pixels_hcy_adjust(DP_FilterPropsHcyAdjust *fpha, int count,
                                 DP_Pixel15 *DP_RESTRICT dst,
                                 const DP_Pixel15 *DP_RESTRICT src)
{
    DP_ASSERT(fpha);
    DP_ASSERT(dst);
    DP_ASSERT(src);
    filter_pixels_with(DP_filter_props_hcy_adjust_h(fpha),
                       DP_filter_props_hcy_adjust_c(fpha),
                       DP_filter_props_hcy_adjust_y(fpha), count, dst, src,
                       rgb_to_hcy_float, hcy_to_rgb_float);
}


void DP_filter_tile_hsv_adjust(DP_FilterPropsHsvAdjust *fpha,
                               DP_TransientTile *DP_RESTRICT tt,
                               DP_Tile *DP_RESTRICT t)
{
    DP_ASSERT(fpha);
    DP_ASSERT(tt);
    DP_ASSERT(t);
    DP_filter_pixels_hsv_adjust(
        fpha, DP_TILE_LENGTH, DP_transient_tile_pixels(tt), DP_tile_pixels(t));
}

void DP_filter_tile_hsl_adjust(DP_FilterPropsHslAdjust *fpha,
                               DP_TransientTile *DP_RESTRICT tt,
                               DP_Tile *DP_RESTRICT t)
{
    DP_ASSERT(fpha);
    DP_ASSERT(tt);
    DP_ASSERT(t);
    DP_filter_pixels_hsl_adjust(
        fpha, DP_TILE_LENGTH, DP_transient_tile_pixels(tt), DP_tile_pixels(t));
}

void DP_filter_tile_hcy_adjust(DP_FilterPropsHcyAdjust *fpha,
                               DP_TransientTile *DP_RESTRICT tt,
                               DP_Tile *DP_RESTRICT t)
{
    DP_ASSERT(fpha);
    DP_ASSERT(tt);
    DP_ASSERT(t);
    DP_filter_pixels_hcy_adjust(
        fpha, DP_TILE_LENGTH, DP_transient_tile_pixels(tt), DP_tile_pixels(t));
}
