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
#include "blend_mode.h"
#include <dpcommon/common.h>


#define LAYER            (1 << 0)
#define BRUSH            (1 << 1)
#define DECREASE_OPACITY (1 << 2)
#define INCREASE_OPACITY (1 << 3)
#define BLEND_BLANK      (1 << 4)

typedef struct DP_BlendModeAttributes {
    int flags;
    const char *enum_name;
    const char *svg_name;
    const char *name;
} DP_BlendModeAttributes;

static const DP_BlendModeAttributes invalid_attributes = {
    0,
    "DP_BLEND_MODE_UNKNOWN",
    "-dp-unknown",
    "Unknown",
};

static const DP_BlendModeAttributes mode_attributes[DP_BLEND_MODE_COUNT] = {
    [DP_BLEND_MODE_ERASE] =
        {
            LAYER | BRUSH | DECREASE_OPACITY,
            "DP_BLEND_MODE_ERASE",
            "-dp-erase",
            "Erase",
        },
    [DP_BLEND_MODE_NORMAL] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_NORMAL",
            "svg:src-over",
            "Normal",
        },
    [DP_BLEND_MODE_MULTIPLY] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_MULTIPLY",
            "svg:multiply",
            "Multiply",
        },
    [DP_BLEND_MODE_DIVIDE] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_DIVIDE",
            "-dp-divide",
            "Divide",
        },
    [DP_BLEND_MODE_BURN] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_BURN",
            "svg:color-burn",
            "Burn",
        },
    [DP_BLEND_MODE_DODGE] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_DODGE",
            "svg:color-dodge",
            "Dodge",
        },
    [DP_BLEND_MODE_DARKEN] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_DARKEN",
            "svg:darken",
            "Darken",
        },
    [DP_BLEND_MODE_LIGHTEN] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_LIGHTEN",
            "svg:lighten",
            "Lighten",
        },
    [DP_BLEND_MODE_SUBTRACT] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_SUBTRACT",
            "-dp-minus",
            "Subtract",
        },
    [DP_BLEND_MODE_ADD] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_ADD",
            "svg:plus",
            "Add",
        },
    [DP_BLEND_MODE_RECOLOR] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_RECOLOR",
            "svg:src-atop",
            "Recolor",
        },
    [DP_BLEND_MODE_BEHIND] =
        {
            BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_BEHIND",
            "svg:dst-over",
            "Behind",
        },
    [DP_BLEND_MODE_COLOR_ERASE] =
        {
            BRUSH | DECREASE_OPACITY,
            "DP_BLEND_MODE_COLOR_ERASE",
            "-dp-cerase",
            "Color Erase",
        },
    [DP_BLEND_MODE_SCREEN] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_SCREEN",
            "svg:screen",
            "Screen",
        },
    [DP_BLEND_MODE_NORMAL_AND_ERASER] =
        {
            BRUSH | INCREASE_OPACITY | DECREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_NORMAL_AND_ERASER",
            "-dp-normal-and-eraser",
            "Normal and Eraser",
        },
    [DP_BLEND_MODE_LUMINOSITY_SHINE_SAI] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_LUMINOSITY_SHINE_SAI",
            "krita:luminosity_sai",
            "Luminosity/Shine (SAI)",
        },
    [DP_BLEND_MODE_OVERLAY] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_OVERLAY",
            "svg:overlay",
            "Overlay",
        },
    [DP_BLEND_MODE_HARD_LIGHT] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_HARD_LIGHT",
            "svg:hard-light",
            "Hard Light",
        },
    [DP_BLEND_MODE_SOFT_LIGHT] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_SOFT_LIGHT",
            "svg:soft-light",
            "Soft Light",
        },
    [DP_BLEND_MODE_LINEAR_BURN] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_LINEAR_BURN",
            "krita:linear_burn",
            "Linear Burn",
        },
    [DP_BLEND_MODE_LINEAR_LIGHT] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_LINEAR_LIGHT",
            "krita:linear light",
            "Linear Light",
        },
    [DP_BLEND_MODE_HUE] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_HUE",
            "svg:hue",
            "Hue",
        },
    [DP_BLEND_MODE_SATURATION] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_SATURATION",
            "svg:saturation",
            "Saturation",
        },
    [DP_BLEND_MODE_LUMINOSITY] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_LUMINOSITY",
            "svg:luminosity",
            "Luminosity",
        },
    [DP_BLEND_MODE_COLOR] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_COLOR",
            "svg:color",
            "Color",
        },
    [DP_BLEND_MODE_REPLACE] =
        {
            BRUSH | INCREASE_OPACITY | DECREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_REPLACE",
            "-dp-replace",
            "Replace",
        },
};


static const DP_BlendModeAttributes *get_attributes(int blend_mode)
{
    DP_ASSERT(blend_mode >= 0);
    DP_ASSERT(blend_mode < DP_BLEND_MODE_COUNT);
    const DP_BlendModeAttributes *attrs = &mode_attributes[blend_mode];
    if (attrs->flags) {
        return attrs;
    }
    else {
        return &invalid_attributes;
    }
}

bool DP_blend_mode_exists(int blend_mode)
{
    return blend_mode >= 0 && blend_mode < DP_BLEND_MODE_COUNT
        && mode_attributes[blend_mode].flags;
}

bool DP_blend_mode_valid_for_layer(int blend_mode)
{
    return get_attributes(blend_mode)->flags & LAYER;
}

bool DP_blend_mode_valid_for_brush(int blend_mode)
{
    return get_attributes(blend_mode)->flags & BRUSH;
}

const char *DP_blend_mode_enum_name(int blend_mode)
{
    return get_attributes(blend_mode)->enum_name;
}

const char *DP_blend_mode_enum_name_unprefixed(int blend_mode)
{
    return get_attributes(blend_mode)->enum_name + 14;
}

const char *DP_blend_mode_svg_name(int blend_mode)
{
    return get_attributes(blend_mode)->svg_name;
}

bool DP_blend_mode_can_increase_opacity(int blend_mode)
{
    return get_attributes(blend_mode)->flags & INCREASE_OPACITY;
}

bool DP_blend_mode_can_decrease_opacity(int blend_mode)
{
    return get_attributes(blend_mode)->flags & DECREASE_OPACITY;
}

bool DP_blend_mode_blend_blank(int blend_mode)
{
    return get_attributes(blend_mode)->flags & BLEND_BLANK;
}

DP_BlendMode DP_blend_mode_by_svg_name(const char *svg_name,
                                       DP_BlendMode not_found_value)
{
    for (int i = 0; i < DP_BLEND_MODE_LAST_EXCEPT_REPLACE; ++i) {
        if (DP_str_equal(svg_name, mode_attributes[i].svg_name)) {
            return (DP_BlendMode)i;
        }
    }
    if (DP_str_equal(svg_name,
                     mode_attributes[DP_BLEND_MODE_REPLACE].svg_name)) {
        return DP_BLEND_MODE_ERASE;
    }
    return not_found_value;
}
