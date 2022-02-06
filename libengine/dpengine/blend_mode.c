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
            BRUSH | DECREASE_OPACITY,
            "DP_BLEND_MODE_ERASE",
            "-dp-erase",
            "Erase",
        },
    [DP_BLEND_MODE_NORMAL] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_NORMAL",
            "src-over",
            "Normal",
        },
    [DP_BLEND_MODE_MULTIPLY] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_MULTIPLY",
            "multiply",
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
            "color-burn",
            "Burn",
        },
    [DP_BLEND_MODE_DODGE] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_DODGE",
            "color-dodge",
            "Dodge",
        },
    [DP_BLEND_MODE_DARKEN] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_DARKEN",
            "darken",
            "Darken",
        },
    [DP_BLEND_MODE_LIGHTEN] =
        {
            LAYER | BRUSH,
            "DP_BLEND_MODE_LIGHTEN",
            "lighten",
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
            "plus",
            "Add",
        },
    [DP_BLEND_MODE_RECOLOR] =
        {
            BRUSH,
            "DP_BLEND_MODE_RECOLOR",
            "src-atop",
            "Recolor",
        },
    [DP_BLEND_MODE_BEHIND] =
        {
            BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_BEHIND",
            "dst-over",
            "Behind",
        },
    [DP_BLEND_MODE_COLOR_ERASE] =
        {
            BRUSH | DECREASE_OPACITY,
            "DP_BLEND_MODE_COLOR_ERASE",
            "-dp-cerase",
            "Color Erase",
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

DP_BlendModeBlankTileBehavior DP_blend_mode_blank_tile_behavior(int blend_mode)
{
    return get_attributes(blend_mode)->flags & BLEND_BLANK
             ? DP_BLEND_MODE_BLANK_TILE_BLEND
             : DP_BLEND_MODE_BLANK_TILE_SKIP;
}
