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
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#ifndef DPMSG_BLEND_MODE_H
#define DPMSG_BLEND_MODE_H
#include <dpcommon/common.h>


#define DP_BLEND_MODE_MAX 255

typedef enum DP_BlendMode {
    DP_BLEND_MODE_ERASE = 0,
    DP_BLEND_MODE_NORMAL,
    DP_BLEND_MODE_MULTIPLY,
    DP_BLEND_MODE_DIVIDE,
    DP_BLEND_MODE_BURN,
    DP_BLEND_MODE_DODGE,
    DP_BLEND_MODE_DARKEN,
    DP_BLEND_MODE_LIGHTEN,
    DP_BLEND_MODE_SUBTRACT,
    DP_BLEND_MODE_ADD,
    DP_BLEND_MODE_RECOLOR,
    DP_BLEND_MODE_BEHIND,
    DP_BLEND_MODE_COLOR_ERASE,
    DP_BLEND_MODE_SCREEN,
    DP_BLEND_MODE_NORMAL_AND_ERASER,
    DP_BLEND_MODE_LUMINOSITY_SHINE_SAI,
    DP_BLEND_MODE_OVERLAY,
    DP_BLEND_MODE_HARD_LIGHT,
    DP_BLEND_MODE_SOFT_LIGHT,
    DP_BLEND_MODE_LINEAR_BURN,
    DP_BLEND_MODE_LINEAR_LIGHT,
    DP_BLEND_MODE_HUE,
    DP_BLEND_MODE_SATURATION,
    DP_BLEND_MODE_LUMINOSITY,
    DP_BLEND_MODE_COLOR,
    DP_BLEND_MODE_ALPHA_DARKEN,
    DP_BLEND_MODE_LAST_EXCEPT_REPLACE, // Put new blend modes before this value.
    DP_BLEND_MODE_REPLACE = DP_BLEND_MODE_MAX,
    DP_BLEND_MODE_COUNT,
} DP_BlendMode;


bool DP_blend_mode_exists(int blend_mode);

bool DP_blend_mode_valid_for_layer(int blend_mode);

bool DP_blend_mode_valid_for_brush(int blend_mode);

const char *DP_blend_mode_enum_name(int blend_mode);

const char *DP_blend_mode_enum_name_unprefixed(int blend_mode);

const char *DP_blend_mode_svg_name(int blend_mode);

const char *DP_blend_mode_text_name(int blend_mode);

bool DP_blend_mode_can_increase_opacity(int blend_mode);

bool DP_blend_mode_can_decrease_opacity(int blend_mode);

bool DP_blend_mode_blend_blank(int blend_mode);

DP_BlendMode DP_blend_mode_by_svg_name(const char *svg_name,
                                       DP_BlendMode not_found_value);


#endif
