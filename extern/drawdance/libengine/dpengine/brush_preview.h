/*
 * Copyright (C) 2022 - 2023 askmeaboutloom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#ifndef DPENGINE_BRUSHPREVIEW_H
#define DPENGINE_BRUSHPREVIEW_H
#include "pixels.h"
#include <dpcommon/common.h>

typedef struct DP_ClassicBrush DP_ClassicBrush;
typedef struct DP_BrushEngine DP_BrushEngine;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;
typedef struct DP_MyPaintBrush DP_MyPaintBrush;
typedef struct DP_MyPaintSettings DP_MyPaintSettings;


typedef enum DP_BrushPreviewShape {
    DP_BRUSH_PREVIEW_STROKE,
    DP_BRUSH_PREVIEW_LINE,
    DP_BRUSH_PREVIEW_RECTANGLE,
    DP_BRUSH_PREVIEW_ELLIPSE,
    DP_BRUSH_PREVIEW_FLOOD_FILL,
    DP_BRUSH_PREVIEW_FLOOD_ERASE,
} DP_BrushPreviewShape;

typedef struct DP_BrushPreview DP_BrushPreview;

DP_BrushPreview *DP_brush_preview_new(void);

void DP_brush_preview_free(DP_BrushPreview *bp);

void DP_brush_preview_render_classic(DP_BrushPreview *bp, DP_DrawContext *dc,
                                     int width, int height,
                                     const DP_ClassicBrush *brush,
                                     DP_BrushPreviewShape shape);

void DP_brush_preview_render_mypaint(DP_BrushPreview *bp, DP_DrawContext *dc,
                                     int width, int height,
                                     const DP_MyPaintBrush *brush,
                                     const DP_MyPaintSettings *settings,
                                     DP_BrushPreviewShape shape);

void DP_brush_preview_render_flood_fill(DP_BrushPreview *bp,
                                        DP_UPixelFloat fill_color,
                                        double tolerance, int expand,
                                        int feather_radius, int blend_mode);

DP_Image *DP_brush_preview_to_image(DP_BrushPreview *bp);

DP_Image *DP_classic_brush_preview_dab(const DP_ClassicBrush *cb,
                                       DP_DrawContext *dc, int width,
                                       int height, uint32_t color);

#endif
