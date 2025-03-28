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
    DP_BLEND_MODE_ALPHA_DARKEN_LERP,
    DP_BLEND_MODE_ERASE_LIGHT,
    DP_BLEND_MODE_ERASE_DARK,
    DP_BLEND_MODE_LIGHT_TO_ALPHA,
    DP_BLEND_MODE_DARK_TO_ALPHA,
    DP_BLEND_MODE_MULTIPLY_ALPHA,
    DP_BLEND_MODE_DIVIDE_ALPHA,
    DP_BLEND_MODE_BURN_ALPHA,
    DP_BLEND_MODE_DODGE_ALPHA,
    DP_BLEND_MODE_DARKEN_ALPHA,
    DP_BLEND_MODE_LIGHTEN_ALPHA,
    DP_BLEND_MODE_SUBTRACT_ALPHA,
    DP_BLEND_MODE_ADD_ALPHA,
    DP_BLEND_MODE_SCREEN_ALPHA,
    DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA,
    DP_BLEND_MODE_OVERLAY_ALPHA,
    DP_BLEND_MODE_HARD_LIGHT_ALPHA,
    DP_BLEND_MODE_SOFT_LIGHT_ALPHA,
    DP_BLEND_MODE_LINEAR_BURN_ALPHA,
    DP_BLEND_MODE_LINEAR_LIGHT_ALPHA,
    DP_BLEND_MODE_HUE_ALPHA,
    DP_BLEND_MODE_SATURATION_ALPHA,
    DP_BLEND_MODE_LUMINOSITY_ALPHA,
    DP_BLEND_MODE_COLOR_ALPHA,
    DP_BLEND_MODE_LAST_EXCEPT_REPLACE, // Put new blend modes before this value.
    // Compatibility hack, not actual blend modes. Selections need new commands,
    // which are not accepted on the thick/builtin server. So we disguise them
    // as PutImage commands there with these blend modes instead.
    DP_BLEND_MODE_COMPAT_LOCAL_MATCH = DP_BLEND_MODE_MAX - 3,
    // The following two compatibility blend modes were used temporarily during
    // development. Consider them reserved for now.
    // DP_BLEND_MODE_COMPAT_SELECTION_PUT = DP_BLEND_MODE_MAX - 2,
    // DP_BLEND_MODE_COMPAT_SELECTION_CLEAR = DP_BLEND_MODE_MAX - 1,
    DP_BLEND_MODE_REPLACE = DP_BLEND_MODE_MAX,
    DP_BLEND_MODE_COUNT,
} DP_BlendMode;


bool DP_blend_mode_exists(int blend_mode);

bool DP_blend_mode_valid_for_layer(int blend_mode);

bool DP_blend_mode_valid_for_brush(int blend_mode);

const char *DP_blend_mode_enum_name(int blend_mode);

const char *DP_blend_mode_enum_name_unprefixed(int blend_mode);

const char *DP_blend_mode_ora_name(int blend_mode);

const char *DP_blend_mode_dptxt_name(int blend_mode);

bool DP_blend_mode_can_increase_opacity(int blend_mode);

bool DP_blend_mode_can_decrease_opacity(int blend_mode);

bool DP_blend_mode_blend_blank(int blend_mode);

bool DP_blend_mode_preserves_alpha(int blend_mode);

bool DP_blend_mode_presents_as_eraser(int blend_mode);

DP_BlendMode DP_blend_mode_by_ora_name(const char *svg_name,
                                       DP_BlendMode not_found_value);

DP_BlendMode DP_blend_mode_by_dptxt_name(const char *svg_name,
                                         DP_BlendMode not_found_value);

// Returns if the given blend mode is part of an alpha-preserving and
// alpha-affecting pair of blend modes. For example, Normal and Recolor are the
// same blend mode, but the former affects alpha and the latter doesn't. If
// there's such a pair, they are returned in the out parameters, both of which
// are optional.
bool DP_blend_mode_alpha_preserve_pair(int blend_mode,
                                       DP_BlendMode *out_alpha_affecting,
                                       DP_BlendMode *out_alpha_preserving);


#endif
