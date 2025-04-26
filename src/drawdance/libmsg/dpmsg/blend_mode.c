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


#define LAYER              (1 << 0)
#define BRUSH              (1 << 1)
#define DECREASE_OPACITY   (1 << 2)
#define INCREASE_OPACITY   (1 << 3)
#define BLEND_BLANK        (1 << 4)
#define PRESERVES_ALPHA    (1 << 5)
#define PRESENTS_AS_ERASER (1 << 6)

#define REPLACE_NAME "-dp-replace"

typedef struct DP_BlendModeAttributes {
    int flags;
    const char *enum_name;
    const char *ora_name;
    const char *dptxt_name;
    const char *name;
} DP_BlendModeAttributes;

static const DP_BlendModeAttributes invalid_attributes = {
    0, "DP_BLEND_MODE_UNKNOWN", "-dp-unknown", "-dp-unknown", "Unknown",
};

static const DP_BlendModeAttributes mode_attributes[DP_BLEND_MODE_COUNT] = {
    [DP_BLEND_MODE_ERASE] =
        {
            LAYER | BRUSH | DECREASE_OPACITY | PRESENTS_AS_ERASER,
            "DP_BLEND_MODE_ERASE",
            "svg:dst-out",
            "svg:dst-out",
            "Erase",
        },
    [DP_BLEND_MODE_NORMAL] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_NORMAL",
            "svg:src-over",
            "svg:src-over",
            "Normal",
        },
    [DP_BLEND_MODE_MULTIPLY] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_MULTIPLY",
            "svg:multiply",
            "svg:multiply",
            "Multiply",
        },
    [DP_BLEND_MODE_DIVIDE] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_DIVIDE",
            "krita:divide",
            "krita:divide",
            "Divide",
        },
    [DP_BLEND_MODE_BURN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_BURN",
            "svg:color-burn",
            "svg:color-burn",
            "Burn",
        },
    [DP_BLEND_MODE_DODGE] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_DODGE",
            "svg:color-dodge",
            "svg:color-dodge",
            "Dodge",
        },
    [DP_BLEND_MODE_DARKEN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_DARKEN",
            "svg:darken",
            "svg:darken",
            "Darken",
        },
    [DP_BLEND_MODE_LIGHTEN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_LIGHTEN",
            "svg:lighten",
            "svg:lighten",
            "Lighten",
        },
    [DP_BLEND_MODE_SUBTRACT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_SUBTRACT",
            "krita:subtract",
            "krita:subtract",
            "Subtract",
        },
    [DP_BLEND_MODE_ADD] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_ADD",
            "svg:plus",
            "svg:plus",
            "Add",
        },
    [DP_BLEND_MODE_RECOLOR] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_RECOLOR",
            "svg:src-atop",
            "svg:src-atop",
            "Recolor",
        },
    [DP_BLEND_MODE_BEHIND] =
        {
            BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_BEHIND",
            "svg:dst-over",
            "svg:dst-over",
            "Behind",
        },
    [DP_BLEND_MODE_COLOR_ERASE] =
        {
            BRUSH | DECREASE_OPACITY | PRESENTS_AS_ERASER,
            "DP_BLEND_MODE_COLOR_ERASE",
            "-dp-cerase",
            "-dp-cerase",
            "Color Erase",
        },
    [DP_BLEND_MODE_SCREEN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_SCREEN",
            "svg:screen",
            "svg:screen",
            "Screen",
        },
    [DP_BLEND_MODE_NORMAL_AND_ERASER] =
        {
            BRUSH | INCREASE_OPACITY | DECREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_NORMAL_AND_ERASER",
            "-dp-normal-and-eraser",
            "-dp-normal-and-eraser",
            "Normal and Eraser",
        },
    [DP_BLEND_MODE_LUMINOSITY_SHINE_SAI] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_LUMINOSITY_SHINE_SAI",
            "krita:luminosity_sai",
            "krita:luminosity_sai",
            "Luminosity/Shine (SAI)",
        },
    [DP_BLEND_MODE_OVERLAY] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_OVERLAY",
            "svg:overlay",
            "svg:overlay",
            "Overlay",
        },
    [DP_BLEND_MODE_HARD_LIGHT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_HARD_LIGHT",
            "svg:hard-light",
            "svg:hard-light",
            "Hard Light",
        },
    [DP_BLEND_MODE_SOFT_LIGHT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_SOFT_LIGHT",
            "svg:soft-light",
            "svg:soft-light",
            "Soft Light",
        },
    [DP_BLEND_MODE_LINEAR_BURN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_LINEAR_BURN",
            "krita:linear_burn",
            "krita:linear_burn",
            "Linear Burn",
        },
    [DP_BLEND_MODE_LINEAR_LIGHT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_LINEAR_LIGHT",
            "krita:linear light",
            "krita:linear light",
            "Linear Light",
        },
    [DP_BLEND_MODE_HUE] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_HUE",
            "svg:hue",
            "svg:hue",
            "Hue",
        },
    [DP_BLEND_MODE_SATURATION] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_SATURATION",
            "svg:saturation",
            "svg:saturation",
            "Saturation",
        },
    [DP_BLEND_MODE_LUMINOSITY] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_LUMINOSITY",
            "svg:luminosity",
            "svg:luminosity",
            "Luminosity",
        },
    [DP_BLEND_MODE_COLOR] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA,
            "DP_BLEND_MODE_COLOR",
            "svg:color",
            "svg:color",
            "Color",
        },
    [DP_BLEND_MODE_ALPHA_DARKEN] =
        {
            INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_ALPHA_DARKEN",
            // Actually, the lerp variant of this blend mode is closer to what
            // Krita uses. However, this blend mode is only ever used for
            // brushes, so it doesn't end up in anything you'd open in Krita.
            "krita:alphadarken",
            "krita:alphadarken",
            "Alpha Darken",
        },
    [DP_BLEND_MODE_ALPHA_DARKEN_LERP] =
        {
            INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_ALPHA_DARKEN_LERP",
            "-dp-alpha-darken-lerp",
            "-dp-alpha-darken-lerp",
            "Alpha Darken",
        },
    [DP_BLEND_MODE_ERASE_LIGHT] =
        {
            LAYER | BRUSH | DECREASE_OPACITY | PRESENTS_AS_ERASER,
            "DP_BLEND_MODE_ERASE_LIGHT",
            "-dp-erase-light",
            "-dp-erase-light",
            "Erase Lightness",
        },
    [DP_BLEND_MODE_ERASE_DARK] =
        {
            LAYER | BRUSH | DECREASE_OPACITY | PRESENTS_AS_ERASER,
            "DP_BLEND_MODE_ERASE_DARK",
            "-dp-erase-dark",
            "-dp-erase-dark",
            "Erase Darkness",
        },
    [DP_BLEND_MODE_LIGHT_TO_ALPHA] =
        {
            LAYER | BRUSH | DECREASE_OPACITY,
            "DP_BLEND_MODE_LIGHT_TO_ALPHA",
            "-dp-light-to-alpha",
            "-dp-light-to-alpha",
            "Lightness to Alpha",
        },
    [DP_BLEND_MODE_DARK_TO_ALPHA] =
        {
            LAYER | BRUSH | DECREASE_OPACITY,
            "DP_BLEND_MODE_DARK_TO_ALPHA",
            "-dp-dark-to-alpha",
            "-dp-dark-to-alpha",
            "Darkness to Alpha",
        },
    [DP_BLEND_MODE_MULTIPLY_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_MULTIPLY_ALPHA",
            "svg:multiply",
            "-dp-multiply-alpha",
            "Multiply Alpha",
        },
    [DP_BLEND_MODE_DIVIDE_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_DIVIDE_ALPHA",
            "krita:divide",
            "-dp-divide-alpha",
            "Divide Alpha",
        },
    [DP_BLEND_MODE_BURN_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_BURN_ALPHA",
            "svg:color-burn",
            "-dp-burn-alpha",
            "Burn Alpha",
        },
    [DP_BLEND_MODE_DODGE_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_DODGE_ALPHA",
            "svg:color-dodge",
            "-dp-dodge-alpha",
            "Dodge Alpha",
        },
    [DP_BLEND_MODE_DARKEN_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_DARKEN_ALPHA",
            "svg:darken",
            "-dp-darken-alpha",
            "Darken Alpha",
        },
    [DP_BLEND_MODE_LIGHTEN_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_LIGHTEN_ALPHA",
            "svg:lighten",
            "-dp-lighten-alpha",
            "Lighten Alpha",
        },
    [DP_BLEND_MODE_SUBTRACT_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_SUBTRACT_ALPHA",
            "krita:subtract",
            "-dp-subtract-alpha",
            "Subtract Alpha",
        },
    [DP_BLEND_MODE_ADD_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_ADD_ALPHA",
            "svg:plus",
            "-dp-add-alpha",
            "Add Alpha",
        },
    [DP_BLEND_MODE_SCREEN_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_SCREEN_ALPHA",
            "svg:screen",
            "-dp-screen-alpha",
            "Screen Alpha",
        },
    [DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA",
            "krita:luminosity_sai",
            "-dp-luminosity-shine-sai-alpha",
            "Luminosity/Shine (SAI) Alpha",
        },
    [DP_BLEND_MODE_OVERLAY_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_OVERLAY_ALPHA",
            "svg:overlay",
            "-dp-overlay-alpha",
            "Overlay Alpha",
        },
    [DP_BLEND_MODE_HARD_LIGHT_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_HARD_LIGHT_ALPHA",
            "svg:hard-light",
            "-dp-hard-light-alpha",
            "Hard Light Alpha",
        },
    [DP_BLEND_MODE_SOFT_LIGHT_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_SOFT_LIGHT_ALPHA",
            "svg:soft-light",
            "-dp-soft-light-alpha",
            "Soft Light Alpha",
        },
    [DP_BLEND_MODE_LINEAR_BURN_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_LINEAR_BURN_ALPHA",
            "krita:linear_burn",
            "-dp-linear-burn-alpha",
            "Linear Burn Alpha",
        },
    [DP_BLEND_MODE_LINEAR_LIGHT_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_LINEAR_LIGHT_ALPHA",
            "krita:linear light",
            "-dp-linear-light-alpha",
            "Linear Light Alpha",
        },
    [DP_BLEND_MODE_HUE_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_HUE_ALPHA",
            "svg:hue",
            "-dp-hue-alpha",
            "Hue Alpha",
        },
    [DP_BLEND_MODE_SATURATION_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_SATURATION_ALPHA",
            "svg:saturation",
            "-dp-saturation-alpha",
            "Saturation Alpha",
        },
    [DP_BLEND_MODE_LUMINOSITY_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_LUMINOSITY_ALPHA",
            "svg:luminosity",
            "-dp-luminosity-alpha",
            "Luminosity Alpha",
        },
    [DP_BLEND_MODE_COLOR_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_COLOR_ALPHA",
            "svg:color",
            "-dp-color-alpha",
            "Color Alpha",
        },

    [DP_BLEND_MODE_REPLACE] =
        {
            BRUSH | INCREASE_OPACITY | DECREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_REPLACE",
            REPLACE_NAME,
            REPLACE_NAME,
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

const char *DP_blend_mode_ora_name(int blend_mode)
{
    return get_attributes(blend_mode)->ora_name;
}

const char *DP_blend_mode_dptxt_name(int blend_mode)
{
    return get_attributes(blend_mode)->dptxt_name;
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

bool DP_blend_mode_preserves_alpha(int blend_mode)
{
    return get_attributes(blend_mode)->flags & PRESERVES_ALPHA;
}

bool DP_blend_mode_presents_as_eraser(int blend_mode)
{
    return get_attributes(blend_mode)->flags & PRESENTS_AS_ERASER;
}

static DP_BlendMode blend_mode_by_fallback_name(const char *name,
                                                DP_BlendMode not_found_value)
{
    static const struct {
        const char *name;
        DP_BlendMode mode;
    } additional_modes[] = {
        // Old names for blend modes that aren't compatible with Krita. We don't
        // write these anymore, but we still want to read them correctly.
        {"-dp-erase", DP_BLEND_MODE_ERASE},
        {"-dp-divide", DP_BLEND_MODE_DIVIDE},
        {"-dp-minus", DP_BLEND_MODE_SUBTRACT},
        // Makes the code a bit easier to have this here.
        {REPLACE_NAME, DP_BLEND_MODE_REPLACE},
    };
    for (size_t i = 0; i < DP_ARRAY_LENGTH(additional_modes); ++i) {
        if (DP_str_equal(name, additional_modes[i].name)) {
            return additional_modes[i].mode;
        }
    }

    return not_found_value;
}

DP_BlendMode DP_blend_mode_by_ora_name(const char *ora_name,
                                       DP_BlendMode not_found_value)
{
    for (int i = 0; i < DP_BLEND_MODE_LAST_EXCEPT_REPLACE; ++i) {
        if (DP_str_equal(ora_name, mode_attributes[i].ora_name)) {
            return (DP_BlendMode)i;
        }
    }
    return blend_mode_by_fallback_name(ora_name, not_found_value);
}

DP_BlendMode DP_blend_mode_by_dptxt_name(const char *ora_name,
                                         DP_BlendMode not_found_value)
{
    for (int i = 0; i < DP_BLEND_MODE_LAST_EXCEPT_REPLACE; ++i) {
        if (DP_str_equal(ora_name, mode_attributes[i].dptxt_name)) {
            return (DP_BlendMode)i;
        }
    }
    return blend_mode_by_fallback_name(ora_name, not_found_value);
}

bool DP_blend_mode_alpha_preserve_pair(int blend_mode,
                                       DP_BlendMode *out_alpha_affecting,
                                       DP_BlendMode *out_alpha_preserving)
{
    DP_BlendMode alpha_affecting;
    DP_BlendMode alpha_preserving;
    switch (blend_mode) {
    case DP_BLEND_MODE_NORMAL:
    case DP_BLEND_MODE_RECOLOR:
        alpha_affecting = DP_BLEND_MODE_NORMAL;
        alpha_preserving = DP_BLEND_MODE_RECOLOR;
        break;
    case DP_BLEND_MODE_MULTIPLY:
    case DP_BLEND_MODE_MULTIPLY_ALPHA:
        alpha_affecting = DP_BLEND_MODE_MULTIPLY_ALPHA;
        alpha_preserving = DP_BLEND_MODE_MULTIPLY;
        break;
    case DP_BLEND_MODE_DIVIDE:
    case DP_BLEND_MODE_DIVIDE_ALPHA:
        alpha_affecting = DP_BLEND_MODE_DIVIDE_ALPHA;
        alpha_preserving = DP_BLEND_MODE_DIVIDE;
        break;
    case DP_BLEND_MODE_BURN:
    case DP_BLEND_MODE_BURN_ALPHA:
        alpha_affecting = DP_BLEND_MODE_BURN_ALPHA;
        alpha_preserving = DP_BLEND_MODE_BURN;
        break;
    case DP_BLEND_MODE_DODGE:
    case DP_BLEND_MODE_DODGE_ALPHA:
        alpha_affecting = DP_BLEND_MODE_DODGE_ALPHA;
        alpha_preserving = DP_BLEND_MODE_DODGE;
        break;
    case DP_BLEND_MODE_DARKEN:
    case DP_BLEND_MODE_DARKEN_ALPHA:
        alpha_affecting = DP_BLEND_MODE_DARKEN_ALPHA;
        alpha_preserving = DP_BLEND_MODE_DARKEN;
        break;
    case DP_BLEND_MODE_LIGHTEN:
    case DP_BLEND_MODE_LIGHTEN_ALPHA:
        alpha_affecting = DP_BLEND_MODE_LIGHTEN_ALPHA;
        alpha_preserving = DP_BLEND_MODE_LIGHTEN;
        break;
    case DP_BLEND_MODE_SUBTRACT:
    case DP_BLEND_MODE_SUBTRACT_ALPHA:
        alpha_affecting = DP_BLEND_MODE_SUBTRACT_ALPHA;
        alpha_preserving = DP_BLEND_MODE_SUBTRACT;
        break;
    case DP_BLEND_MODE_ADD:
    case DP_BLEND_MODE_ADD_ALPHA:
        alpha_affecting = DP_BLEND_MODE_ADD_ALPHA;
        alpha_preserving = DP_BLEND_MODE_ADD;
        break;
    case DP_BLEND_MODE_SCREEN:
    case DP_BLEND_MODE_SCREEN_ALPHA:
        alpha_affecting = DP_BLEND_MODE_SCREEN_ALPHA;
        alpha_preserving = DP_BLEND_MODE_SCREEN;
        break;
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA:
        alpha_affecting = DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA;
        alpha_preserving = DP_BLEND_MODE_LUMINOSITY_SHINE_SAI;
        break;
    case DP_BLEND_MODE_OVERLAY:
    case DP_BLEND_MODE_OVERLAY_ALPHA:
        alpha_affecting = DP_BLEND_MODE_OVERLAY_ALPHA;
        alpha_preserving = DP_BLEND_MODE_OVERLAY;
        break;
    case DP_BLEND_MODE_HARD_LIGHT:
    case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
        alpha_affecting = DP_BLEND_MODE_HARD_LIGHT_ALPHA;
        alpha_preserving = DP_BLEND_MODE_HARD_LIGHT;
        break;
    case DP_BLEND_MODE_SOFT_LIGHT:
    case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
        alpha_affecting = DP_BLEND_MODE_SOFT_LIGHT_ALPHA;
        alpha_preserving = DP_BLEND_MODE_SOFT_LIGHT;
        break;
    case DP_BLEND_MODE_LINEAR_BURN:
    case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
        alpha_affecting = DP_BLEND_MODE_LINEAR_BURN_ALPHA;
        alpha_preserving = DP_BLEND_MODE_LINEAR_BURN;
        break;
    case DP_BLEND_MODE_LINEAR_LIGHT:
    case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
        alpha_affecting = DP_BLEND_MODE_LINEAR_LIGHT_ALPHA;
        alpha_preserving = DP_BLEND_MODE_LINEAR_LIGHT;
        break;
    case DP_BLEND_MODE_HUE:
    case DP_BLEND_MODE_HUE_ALPHA:
        alpha_affecting = DP_BLEND_MODE_HUE_ALPHA;
        alpha_preserving = DP_BLEND_MODE_HUE;
        break;
    case DP_BLEND_MODE_SATURATION:
    case DP_BLEND_MODE_SATURATION_ALPHA:
        alpha_affecting = DP_BLEND_MODE_SATURATION_ALPHA;
        alpha_preserving = DP_BLEND_MODE_SATURATION;
        break;
    case DP_BLEND_MODE_LUMINOSITY:
    case DP_BLEND_MODE_LUMINOSITY_ALPHA:
        alpha_affecting = DP_BLEND_MODE_LUMINOSITY_ALPHA;
        alpha_preserving = DP_BLEND_MODE_LUMINOSITY;
        break;
    case DP_BLEND_MODE_COLOR:
    case DP_BLEND_MODE_COLOR_ALPHA:
        alpha_affecting = DP_BLEND_MODE_COLOR_ALPHA;
        alpha_preserving = DP_BLEND_MODE_COLOR;
        break;
    default:
        return false;
    }

    if (out_alpha_affecting) {
        *out_alpha_affecting = alpha_affecting;
    }
    if (out_alpha_preserving) {
        *out_alpha_preserving = alpha_preserving;
    }
    return true;
}

int DP_blend_mode_to_alpha_affecting(int blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_RECOLOR:
        return DP_BLEND_MODE_NORMAL;
    case DP_BLEND_MODE_MULTIPLY:
        return DP_BLEND_MODE_MULTIPLY_ALPHA;
    case DP_BLEND_MODE_DIVIDE:
        return DP_BLEND_MODE_DIVIDE_ALPHA;
    case DP_BLEND_MODE_BURN:
        return DP_BLEND_MODE_BURN_ALPHA;
    case DP_BLEND_MODE_DODGE:
        return DP_BLEND_MODE_DODGE_ALPHA;
    case DP_BLEND_MODE_DARKEN:
        return DP_BLEND_MODE_DARKEN_ALPHA;
    case DP_BLEND_MODE_LIGHTEN:
        return DP_BLEND_MODE_LIGHTEN_ALPHA;
    case DP_BLEND_MODE_SUBTRACT:
        return DP_BLEND_MODE_SUBTRACT_ALPHA;
    case DP_BLEND_MODE_ADD:
        return DP_BLEND_MODE_ADD_ALPHA;
    case DP_BLEND_MODE_SCREEN:
        return DP_BLEND_MODE_SCREEN_ALPHA;
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
        return DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA;
    case DP_BLEND_MODE_OVERLAY:
        return DP_BLEND_MODE_OVERLAY_ALPHA;
    case DP_BLEND_MODE_HARD_LIGHT:
        return DP_BLEND_MODE_HARD_LIGHT_ALPHA;
    case DP_BLEND_MODE_SOFT_LIGHT:
        return DP_BLEND_MODE_SOFT_LIGHT_ALPHA;
    case DP_BLEND_MODE_LINEAR_BURN:
        return DP_BLEND_MODE_LINEAR_BURN_ALPHA;
    case DP_BLEND_MODE_LINEAR_LIGHT:
        return DP_BLEND_MODE_LINEAR_LIGHT_ALPHA;
    case DP_BLEND_MODE_HUE:
        return DP_BLEND_MODE_HUE_ALPHA;
    case DP_BLEND_MODE_SATURATION:
        return DP_BLEND_MODE_SATURATION_ALPHA;
    case DP_BLEND_MODE_LUMINOSITY:
        return DP_BLEND_MODE_LUMINOSITY_ALPHA;
    case DP_BLEND_MODE_COLOR:
        return DP_BLEND_MODE_COLOR_ALPHA;
    default:
        return blend_mode;
    }
}

int DP_blend_mode_to_alpha_preserving(int blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_NORMAL:
        return DP_BLEND_MODE_RECOLOR;
    case DP_BLEND_MODE_MULTIPLY_ALPHA:
        return DP_BLEND_MODE_MULTIPLY;
    case DP_BLEND_MODE_DIVIDE_ALPHA:
        return DP_BLEND_MODE_DIVIDE;
    case DP_BLEND_MODE_BURN_ALPHA:
        return DP_BLEND_MODE_BURN;
    case DP_BLEND_MODE_DODGE_ALPHA:
        return DP_BLEND_MODE_DODGE;
    case DP_BLEND_MODE_DARKEN_ALPHA:
        return DP_BLEND_MODE_DARKEN;
    case DP_BLEND_MODE_LIGHTEN_ALPHA:
        return DP_BLEND_MODE_LIGHTEN;
    case DP_BLEND_MODE_SUBTRACT_ALPHA:
        return DP_BLEND_MODE_SUBTRACT;
    case DP_BLEND_MODE_ADD_ALPHA:
        return DP_BLEND_MODE_ADD;
    case DP_BLEND_MODE_SCREEN_ALPHA:
        return DP_BLEND_MODE_SCREEN;
    case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA:
        return DP_BLEND_MODE_LUMINOSITY_SHINE_SAI;
    case DP_BLEND_MODE_OVERLAY_ALPHA:
        return DP_BLEND_MODE_OVERLAY;
    case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
        return DP_BLEND_MODE_HARD_LIGHT;
    case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
        return DP_BLEND_MODE_SOFT_LIGHT;
    case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
        return DP_BLEND_MODE_LINEAR_BURN;
    case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
        return DP_BLEND_MODE_LINEAR_LIGHT;
    case DP_BLEND_MODE_HUE_ALPHA:
        return DP_BLEND_MODE_HUE;
    case DP_BLEND_MODE_SATURATION_ALPHA:
        return DP_BLEND_MODE_SATURATION;
    case DP_BLEND_MODE_LUMINOSITY_ALPHA:
        return DP_BLEND_MODE_LUMINOSITY;
    case DP_BLEND_MODE_COLOR_ALPHA:
        return DP_BLEND_MODE_COLOR;
    default:
        return blend_mode;
    }
}
