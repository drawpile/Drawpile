// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_FILTER_HSX_ADJUST_H
#define DPENGINE_FILTER_HSX_ADJUST_H
#include <dpcommon/common.h>

DP_TYPEDEF_PERSISTENT(Tile);

typedef struct DP_FilterPropsHcyAdjust DP_FilterPropsHcyAdjust;
typedef struct DP_FilterPropsHslAdjust DP_FilterPropsHslAdjust;
typedef struct DP_FilterPropsHsvAdjust DP_FilterPropsHsvAdjust;
typedef struct DP_Pixel15 DP_Pixel15;


void DP_filter_pixels_hsv_adjust(DP_FilterPropsHsvAdjust *fpha, int count,
                                 DP_Pixel15 *DP_RESTRICT dst,
                                 const DP_Pixel15 *DP_RESTRICT src);

void DP_filter_pixels_hsl_adjust(DP_FilterPropsHslAdjust *fpha, int count,
                                 DP_Pixel15 *DP_RESTRICT dst,
                                 const DP_Pixel15 *DP_RESTRICT src);

void DP_filter_pixels_hcy_adjust(DP_FilterPropsHcyAdjust *fpha, int count,
                                 DP_Pixel15 *DP_RESTRICT dst,
                                 const DP_Pixel15 *DP_RESTRICT src);


void DP_filter_tile_hsv_adjust(DP_FilterPropsHsvAdjust *fpha,
                               DP_TransientTile *DP_RESTRICT tt,
                               DP_Tile *DP_RESTRICT t);

void DP_filter_tile_hsl_adjust(DP_FilterPropsHslAdjust *fpha,
                               DP_TransientTile *DP_RESTRICT tt,
                               DP_Tile *DP_RESTRICT t);

void DP_filter_tile_hcy_adjust(DP_FilterPropsHcyAdjust *fpha,
                               DP_TransientTile *DP_RESTRICT tt,
                               DP_Tile *DP_RESTRICT t);


#endif
