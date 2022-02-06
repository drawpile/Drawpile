/*
 * Copyright (C) 2022 askmeaboutloom
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
 * This code is wholly based on the Qt framework's raster paint engine
 * implementation, using it under the GNU General Public License, version 3.
 * See 3rdparty/licenses/qt/license.GPL3 for details.
 */
#ifndef DPENGINE_IMAGE_TRANSFORM_H
#define DPENGINE_IMAGE_TRANSFORM_H
#include <dpcommon/common.h>
#include <dpcommon/geom.h>

typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Image DP_Image;


bool DP_image_transform_draw(DP_Image *img, DP_DrawContext *dc,
                             DP_Image *dst_img, DP_Transform tf) DP_MUST_CHECK;


#endif
