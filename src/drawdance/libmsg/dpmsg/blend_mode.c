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


#define LAYER                        (1 << 0)
#define BRUSH                        (1 << 1)
#define DECREASE_OPACITY             (1 << 2)
#define INCREASE_OPACITY             (1 << 3)
#define BLEND_BLANK                  (1 << 4)
#define PRESERVES_ALPHA              (1 << 5)
#define PRESENTS_AS_ERASER           (1 << 6)
#define PRESENTS_AS_ALPHA_PRESERVING (1 << 7)
#define SECONDARY_ALIAS              (1 << 8)
#define COMPARES_ALPHA               (1 << 9)

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
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_MULTIPLY",
            "svg:multiply",
            "svg:multiply",
            "Multiply",
        },
    [DP_BLEND_MODE_DIVIDE] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_DIVIDE",
            "krita:divide",
            "krita:divide",
            "Divide",
        },
    [DP_BLEND_MODE_BURN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_BURN",
            "svg:color-burn",
            "svg:color-burn",
            "Burn",
        },
    [DP_BLEND_MODE_DODGE] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_DODGE",
            "svg:color-dodge",
            "svg:color-dodge",
            "Dodge",
        },
    [DP_BLEND_MODE_DARKEN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_DARKEN",
            "svg:darken",
            "svg:darken",
            "Darken",
        },
    [DP_BLEND_MODE_LIGHTEN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_LIGHTEN",
            "svg:lighten",
            "svg:lighten",
            "Lighten",
        },
    [DP_BLEND_MODE_SUBTRACT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_SUBTRACT",
            "krita:subtract",
            "krita:subtract",
            "Subtract",
        },
    [DP_BLEND_MODE_ADD] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_ADD",
            "svg:plus",
            "svg:plus",
            "Add",
        },
    [DP_BLEND_MODE_RECOLOR] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
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
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
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
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_LUMINOSITY_SHINE_SAI",
            "krita:luminosity_sai",
            "krita:luminosity_sai",
            "Shine (SAI)",
        },
    [DP_BLEND_MODE_OVERLAY] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_OVERLAY",
            "svg:overlay",
            "svg:overlay",
            "Overlay",
        },
    [DP_BLEND_MODE_HARD_LIGHT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_HARD_LIGHT",
            "svg:hard-light",
            "svg:hard-light",
            "Hard Light",
        },
    [DP_BLEND_MODE_SOFT_LIGHT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_SOFT_LIGHT",
            "svg:soft-light",
            "svg:soft-light",
            "Soft Light",
        },
    [DP_BLEND_MODE_LINEAR_BURN] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_LINEAR_BURN",
            "krita:linear_burn",
            "krita:linear_burn",
            "Linear Burn",
        },
    [DP_BLEND_MODE_LINEAR_LIGHT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_LINEAR_LIGHT",
            "krita:linear light",
            "krita:linear light",
            "Linear Light",
        },
    [DP_BLEND_MODE_HUE] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_HUE",
            "svg:hue",
            "svg:hue",
            "Hue",
        },
    [DP_BLEND_MODE_SATURATION] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_SATURATION",
            "svg:saturation",
            "svg:saturation",
            "Saturation",
        },
    [DP_BLEND_MODE_LUMINOSITY] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_LUMINOSITY",
            "svg:luminosity",
            "svg:luminosity",
            "Luminosity",
        },
    [DP_BLEND_MODE_COLOR] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_COLOR",
            "svg:color",
            "svg:color",
            "Color",
        },
    [DP_BLEND_MODE_COMPARE_DENSITY_SOFT] =
        {
            BRUSH | INCREASE_OPACITY | BLEND_BLANK | COMPARES_ALPHA,
            "DP_BLEND_MODE_COMPARE_DENSITY_SOFT",
            "-dp-compare-density-soft",
            "-dp-compare-density-soft",
            "Compare Density Soft",
        },
    [DP_BLEND_MODE_COMPARE_DENSITY] =
        {
            BRUSH | INCREASE_OPACITY | BLEND_BLANK | COMPARES_ALPHA,
            "DP_BLEND_MODE_COMPARE_DENSITY",
            "-dp-compare-density",
            "-dp-compare-density",
            "Compare Density",
        },
    [DP_BLEND_MODE_VIVID_LIGHT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_VIVID_LIGHT",
            "krita:vivid_light",
            "krita:vivid_light",
            "Vivid Light",
        },
    [DP_BLEND_MODE_PIN_LIGHT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_PIN_LIGHT",
            "krita:pin_light",
            "krita:pin_light",
            "Pin Light",
        },
    [DP_BLEND_MODE_DIFFERENCE] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_DIFFERENCE",
            "svg:difference",
            "svg:difference",
            "Difference",
        },
    [DP_BLEND_MODE_DARKER_COLOR] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_DARKER_COLOR",
            "krita:darker color",
            "krita:darker color",
            "Darker Color",
        },
    [DP_BLEND_MODE_LIGHTER_COLOR] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_LIGHTER_COLOR",
            "krita:lighter color",
            "krita:lighter color",
            "Lighter Color",
        },
    [DP_BLEND_MODE_SHADE_SAI] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_SHADE_SAI",
            "krita:shade_sai",
            "krita:shade_sai",
            "Shade (SAI)",
        },
    [DP_BLEND_MODE_SHADE_SHINE_SAI] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_SHADE_SHINE_SAI",
            "krita:shade_shine_sai",
            "krita:shade_shine_sai",
            "Shade/Shine (SAI)",
        },
    [DP_BLEND_MODE_BURN_SAI] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_BURN_SAI",
            "krita:burn_sai",
            "krita:burn_sai",
            "Burn (SAI)",
        },
    [DP_BLEND_MODE_DODGE_SAI] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_DODGE_SAI",
            "krita:dodge_sai",
            "krita:dodge_sai",
            "Dodge (SAI)",
        },
    [DP_BLEND_MODE_BURN_DODGE_SAI] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_BURN_DODGE_SAI",
            "krita:burn_dodge_sai",
            "krita:burn_dodge_sai",
            "Burn/Dodge (SAI)",
        },
    [DP_BLEND_MODE_HARD_MIX_SAI] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_HARD_MIX_SAI",
            "krita:hard_mix_sai",
            "krita:hard_mix_sai",
            "Hard Mix (SAI)",
        },
    [DP_BLEND_MODE_DIFFERENCE_SAI] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_DIFFERENCE_SAI",
            "krita:difference_sai",
            "krita:difference_sai",
            "Difference (SAI)",
        },
    [DP_BLEND_MODE_MARKER] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING
                | COMPARES_ALPHA,
            "DP_BLEND_MODE_MARKER",
            "-dp-marker",
            "-dp-marker",
            "Marker",
        },
    [DP_BLEND_MODE_MARKER_WASH] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING
                | COMPARES_ALPHA,
            "DP_BLEND_MODE_MARKER_WASH",
            "-dp-marker",
            "-dp-marker-wash",
            "Marker Wash",
        },
    [DP_BLEND_MODE_GREATER] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING
                | COMPARES_ALPHA,
            "DP_BLEND_MODE_GREATER",
            "krita:greater",
            "-dp-greater",
            "Greater",
        },
    [DP_BLEND_MODE_GREATER_WASH] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING
                | COMPARES_ALPHA,
            "DP_BLEND_MODE_GREATER_WASH",
            "krita:greater",
            "-dp-greater-wash",
            "Greater Wash",
        },
    [DP_BLEND_MODE_PIGMENT] =
        {
            LAYER | BRUSH | PRESERVES_ALPHA | PRESENTS_AS_ALPHA_PRESERVING,
            "DP_BLEND_MODE_PIGMENT",
            "mypaint:spectral-wgm",
            "-dp-pigment",
            "Pigment",
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
    [DP_BLEND_MODE_VIVID_LIGHT_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_VIVID_LIGHT_ALPHA",
            "krita:vivid_light",
            "-dp-vivid-light-alpha",
            "Vivid Light Alpha",
        },
    [DP_BLEND_MODE_PIN_LIGHT_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_PIN_LIGHT_ALPHA",
            "krita:pin_light",
            "-dp-pin-light-alpha",
            "Pin Light Alpha",
        },
    [DP_BLEND_MODE_DIFFERENCE_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_DIFFERENCE_ALPHA",
            "svg:difference",
            "-dp-difference-alpha",
            "Difference Alpha",
        },
    [DP_BLEND_MODE_DARKER_COLOR_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_DARKER_COLOR_ALPHA",
            "krita:darker color",
            "-dp-darker-color-alpha",
            "Darker Color Alpha",
        },
    [DP_BLEND_MODE_LIGHTER_COLOR_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_LIGHTER_COLOR_ALPHA",
            "krita:lighter color",
            "-dp-lighter-color-alpha",
            "Lighter Color Alpha",
        },
    [DP_BLEND_MODE_SHADE_SAI_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_SHADE_SAI_ALPHA",
            "krita:shade_sai",
            "-dp-shade-sai-alpha",
            "Shade (SAI) Alpha",
        },
    [DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA",
            "krita:shade_shine_sai",
            "-dp-shade-shine-sai-alpha",
            "Shade/Shine (SAI) ALpha",
        },
    [DP_BLEND_MODE_BURN_SAI_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_BURN_SAI_ALPHA",
            "krita:burn_sai",
            "-dp-burn-sai-alpha",
            "Burn (SAI) Alpha",
        },
    [DP_BLEND_MODE_DODGE_SAI_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_DODGE_SAI_ALPHA",
            "krita:dodge_sai",
            "-dp-dodge-sai-alpha",
            "Dodge (SAI) Alpha",
        },
    [DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA",
            "krita:burn_dodge_sai",
            "-dp-burn-dodge-sai-alpha",
            "Burn/Dodge (SAI) Alpha",
        },
    [DP_BLEND_MODE_HARD_MIX_SAI_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_HARD_MIX_SAI_ALPHA",
            "krita:hard_mix_sai",
            "-dp-hard-mix-sai-alpha",
            "Hard Mix (SAI) Alpha",
        },
    [DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA",
            "krita:difference_sai",
            "-dp-difference-sai-alpha",
            "Difference (SAI) Alpha",
        },
    [DP_BLEND_MODE_MARKER_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK | COMPARES_ALPHA,
            "DP_BLEND_MODE_MARKER_ALPHA",
            "-dp-marker",
            "-dp-marker-alpha",
            "Marker Alpha",
        },
    [DP_BLEND_MODE_MARKER_ALPHA_WASH] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK | COMPARES_ALPHA,
            "DP_BLEND_MODE_MARKER_ALPHA_WASH",
            "-dp-marker",
            "-dp-marker-alpha-wash",
            "Marker Alpha Wash",
        },
    [DP_BLEND_MODE_GREATER_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK | COMPARES_ALPHA,
            "DP_BLEND_MODE_GREATER_ALPHA",
            "krita:greater",
            "krita:greater",
            "Greater Alpha",
        },
    [DP_BLEND_MODE_GREATER_ALPHA_WASH] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK | COMPARES_ALPHA,
            "DP_BLEND_MODE_GREATER_ALPHA",
            "krita:greater",
            "-dp-greater-alpha-wash",
            "Greater Alpha Wash",
        },
    [DP_BLEND_MODE_PIGMENT_ALPHA] =
        {
            LAYER | BRUSH | INCREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_PIGMENT_ALPHA",
            "mypaint:spectral-wgm",
            "mypaint:spectral-wgm",
            "Pigment Alpha",
        },
    [DP_BLEND_MODE_PIGMENT_AND_ERASER] =
        {
            BRUSH | INCREASE_OPACITY | DECREASE_OPACITY | BLEND_BLANK,
            "DP_BLEND_MODE_PIGMENT_AND_ERASER",
            "-dp-pigment-and-eraser",
            "-dp-pigment-and-eraser",
            "Pigment and Eraser",
        },
    [DP_BLEND_MODE_ERASE_PRESERVE] =
        {
            LAYER | BRUSH | DECREASE_OPACITY | PRESENTS_AS_ERASER
                | PRESENTS_AS_ALPHA_PRESERVING | SECONDARY_ALIAS,
            "DP_BLEND_MODE_ERASE_PRESERVE",
            "svg:dst-out",
            "-dp-erase-preserve",
            "Erase Preserve",
        },
    [DP_BLEND_MODE_BEHIND_PRESERVE] =
        {
            BRUSH | INCREASE_OPACITY | BLEND_BLANK
                | PRESENTS_AS_ALPHA_PRESERVING | SECONDARY_ALIAS,
            "DP_BLEND_MODE_BEHIND_PRESERVE",
            "svg:dst-over",
            "-dp-behind-preserve",
            "Behind Preserve",
        },
    [DP_BLEND_MODE_COLOR_ERASE_PRESERVE] =
        {
            BRUSH | DECREASE_OPACITY | PRESENTS_AS_ERASER
                | PRESENTS_AS_ALPHA_PRESERVING | SECONDARY_ALIAS,
            "DP_BLEND_MODE_COLOR_ERASE_PRESERVE",
            "-dp-cerase",
            "-dp-cerase-preserve",
            "Color Erase Preserve",
        },
    [DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE] =
        {
            LAYER | BRUSH | DECREASE_OPACITY | SECONDARY_ALIAS,
            "DP_BLEND_MODE_LIGHT_TO_ALPHA",
            "-dp-light-to-alpha",
            "-dp-light-to-alpha-preserve",
            "Lightness to Alpha Preserve",
        },
    [DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE] =
        {
            LAYER | BRUSH | DECREASE_OPACITY | SECONDARY_ALIAS,
            "DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE",
            "-dp-dark-to-alpha",
            "-dp-dark-to-alpha-preserve",
            "Darkness to Alpha Preserve",
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

bool DP_blend_mode_secondary_alias(int blend_mode)
{
    return get_attributes(blend_mode)->flags & SECONDARY_ALIAS;
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

bool DP_blend_mode_presents_as_alpha_preserving(int blend_mode)
{
    return get_attributes(blend_mode)->flags & PRESENTS_AS_ALPHA_PRESERVING;
}

bool DP_blend_mode_compares_alpha(int blend_mode)
{
    return get_attributes(blend_mode)->flags & COMPARES_ALPHA;
}

bool DP_blend_mode_direct_only(int blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_MARKER:
    case DP_BLEND_MODE_MARKER_WASH:
    case DP_BLEND_MODE_MARKER_ALPHA:
    case DP_BLEND_MODE_MARKER_ALPHA_WASH:
        return true;
    default:
        return false;
    }
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
    case DP_BLEND_MODE_ERASE:
    case DP_BLEND_MODE_ERASE_PRESERVE:
        alpha_affecting = DP_BLEND_MODE_ERASE;
        alpha_preserving = DP_BLEND_MODE_ERASE_PRESERVE;
        break;
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
    case DP_BLEND_MODE_BEHIND:
    case DP_BLEND_MODE_BEHIND_PRESERVE:
        alpha_affecting = DP_BLEND_MODE_BEHIND;
        alpha_preserving = DP_BLEND_MODE_BEHIND_PRESERVE;
        break;
    case DP_BLEND_MODE_COLOR_ERASE:
    case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
        alpha_affecting = DP_BLEND_MODE_COLOR_ERASE;
        alpha_preserving = DP_BLEND_MODE_COLOR_ERASE_PRESERVE;
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
    case DP_BLEND_MODE_VIVID_LIGHT:
    case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
        alpha_affecting = DP_BLEND_MODE_VIVID_LIGHT_ALPHA;
        alpha_preserving = DP_BLEND_MODE_VIVID_LIGHT;
        break;
    case DP_BLEND_MODE_PIN_LIGHT:
    case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
        alpha_affecting = DP_BLEND_MODE_PIN_LIGHT_ALPHA;
        alpha_preserving = DP_BLEND_MODE_PIN_LIGHT;
        break;
    case DP_BLEND_MODE_DIFFERENCE:
    case DP_BLEND_MODE_DIFFERENCE_ALPHA:
        alpha_affecting = DP_BLEND_MODE_DIFFERENCE_ALPHA;
        alpha_preserving = DP_BLEND_MODE_DIFFERENCE;
        break;
    case DP_BLEND_MODE_DARKER_COLOR:
    case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
        alpha_affecting = DP_BLEND_MODE_DARKER_COLOR_ALPHA;
        alpha_preserving = DP_BLEND_MODE_DARKER_COLOR;
        break;
    case DP_BLEND_MODE_LIGHTER_COLOR:
    case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
        alpha_affecting = DP_BLEND_MODE_LIGHTER_COLOR_ALPHA;
        alpha_preserving = DP_BLEND_MODE_LIGHTER_COLOR;
        break;
    case DP_BLEND_MODE_SHADE_SAI:
    case DP_BLEND_MODE_SHADE_SAI_ALPHA:
        alpha_affecting = DP_BLEND_MODE_SHADE_SAI_ALPHA;
        alpha_preserving = DP_BLEND_MODE_SHADE_SAI;
        break;
    case DP_BLEND_MODE_SHADE_SHINE_SAI:
    case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
        alpha_affecting = DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA;
        alpha_preserving = DP_BLEND_MODE_SHADE_SHINE_SAI;
        break;
    case DP_BLEND_MODE_BURN_SAI:
    case DP_BLEND_MODE_BURN_SAI_ALPHA:
        alpha_affecting = DP_BLEND_MODE_BURN_SAI_ALPHA;
        alpha_preserving = DP_BLEND_MODE_BURN_SAI;
        break;
    case DP_BLEND_MODE_DODGE_SAI:
    case DP_BLEND_MODE_DODGE_SAI_ALPHA:
        alpha_affecting = DP_BLEND_MODE_DODGE_SAI_ALPHA;
        alpha_preserving = DP_BLEND_MODE_DODGE_SAI;
        break;
    case DP_BLEND_MODE_BURN_DODGE_SAI:
    case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
        alpha_affecting = DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA;
        alpha_preserving = DP_BLEND_MODE_BURN_DODGE_SAI;
        break;
    case DP_BLEND_MODE_HARD_MIX_SAI:
    case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
        alpha_affecting = DP_BLEND_MODE_HARD_MIX_SAI_ALPHA;
        alpha_preserving = DP_BLEND_MODE_HARD_MIX_SAI;
        break;
    case DP_BLEND_MODE_DIFFERENCE_SAI:
    case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
        alpha_affecting = DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA;
        alpha_preserving = DP_BLEND_MODE_DIFFERENCE_SAI;
        break;
    case DP_BLEND_MODE_MARKER:
    case DP_BLEND_MODE_MARKER_ALPHA:
        alpha_affecting = DP_BLEND_MODE_MARKER_ALPHA;
        alpha_preserving = DP_BLEND_MODE_MARKER;
        break;
    case DP_BLEND_MODE_MARKER_WASH:
    case DP_BLEND_MODE_MARKER_ALPHA_WASH:
        alpha_affecting = DP_BLEND_MODE_MARKER_ALPHA_WASH;
        alpha_preserving = DP_BLEND_MODE_MARKER_WASH;
        break;
    case DP_BLEND_MODE_GREATER:
    case DP_BLEND_MODE_GREATER_ALPHA:
        alpha_affecting = DP_BLEND_MODE_GREATER_ALPHA;
        alpha_preserving = DP_BLEND_MODE_GREATER;
        break;
    case DP_BLEND_MODE_GREATER_WASH:
    case DP_BLEND_MODE_GREATER_ALPHA_WASH:
        alpha_affecting = DP_BLEND_MODE_GREATER_ALPHA_WASH;
        alpha_preserving = DP_BLEND_MODE_GREATER_WASH;
        break;
    case DP_BLEND_MODE_PIGMENT:
    case DP_BLEND_MODE_PIGMENT_ALPHA:
        alpha_affecting = DP_BLEND_MODE_PIGMENT_ALPHA;
        alpha_preserving = DP_BLEND_MODE_PIGMENT;
        break;
    case DP_BLEND_MODE_LIGHT_TO_ALPHA:
    case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
        alpha_affecting = DP_BLEND_MODE_LIGHT_TO_ALPHA;
        alpha_preserving = DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE;
        break;
    case DP_BLEND_MODE_DARK_TO_ALPHA:
    case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
        alpha_affecting = DP_BLEND_MODE_DARK_TO_ALPHA;
        alpha_preserving = DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE;
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
    case DP_BLEND_MODE_VIVID_LIGHT:
        return DP_BLEND_MODE_VIVID_LIGHT_ALPHA;
    case DP_BLEND_MODE_PIN_LIGHT:
        return DP_BLEND_MODE_PIN_LIGHT_ALPHA;
    case DP_BLEND_MODE_DIFFERENCE:
        return DP_BLEND_MODE_DIFFERENCE_ALPHA;
    case DP_BLEND_MODE_DARKER_COLOR:
        return DP_BLEND_MODE_DARKER_COLOR_ALPHA;
    case DP_BLEND_MODE_LIGHTER_COLOR:
        return DP_BLEND_MODE_LIGHTER_COLOR_ALPHA;
    case DP_BLEND_MODE_SHADE_SAI:
        return DP_BLEND_MODE_SHADE_SAI_ALPHA;
    case DP_BLEND_MODE_SHADE_SHINE_SAI:
        return DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA;
    case DP_BLEND_MODE_BURN_SAI:
        return DP_BLEND_MODE_BURN_SAI_ALPHA;
    case DP_BLEND_MODE_DODGE_SAI:
        return DP_BLEND_MODE_DODGE_SAI_ALPHA;
    case DP_BLEND_MODE_BURN_DODGE_SAI:
        return DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA;
    case DP_BLEND_MODE_HARD_MIX_SAI:
        return DP_BLEND_MODE_HARD_MIX_SAI_ALPHA;
    case DP_BLEND_MODE_DIFFERENCE_SAI:
        return DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA;
    case DP_BLEND_MODE_MARKER:
        return DP_BLEND_MODE_MARKER_ALPHA;
    case DP_BLEND_MODE_MARKER_WASH:
        return DP_BLEND_MODE_MARKER_ALPHA_WASH;
    case DP_BLEND_MODE_GREATER:
        return DP_BLEND_MODE_GREATER_ALPHA;
    case DP_BLEND_MODE_GREATER_WASH:
        return DP_BLEND_MODE_GREATER_ALPHA_WASH;
    case DP_BLEND_MODE_PIGMENT:
        return DP_BLEND_MODE_PIGMENT_ALPHA;
    case DP_BLEND_MODE_ERASE_PRESERVE:
        return DP_BLEND_MODE_ERASE;
    case DP_BLEND_MODE_BEHIND_PRESERVE:
        return DP_BLEND_MODE_BEHIND;
    case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
        return DP_BLEND_MODE_COLOR_ERASE;
    case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
        return DP_BLEND_MODE_LIGHT_TO_ALPHA;
    case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
        return DP_BLEND_MODE_DARK_TO_ALPHA;
    default:
        return blend_mode;
    }
}

int DP_blend_mode_to_alpha_preserving(int blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_ERASE:
        return DP_BLEND_MODE_ERASE_PRESERVE;
    case DP_BLEND_MODE_NORMAL:
        return DP_BLEND_MODE_RECOLOR;
    case DP_BLEND_MODE_BEHIND:
        return DP_BLEND_MODE_BEHIND_PRESERVE;
    case DP_BLEND_MODE_COLOR_ERASE:
        return DP_BLEND_MODE_COLOR_ERASE_PRESERVE;
    case DP_BLEND_MODE_LIGHT_TO_ALPHA:
        return DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE;
    case DP_BLEND_MODE_DARK_TO_ALPHA:
        return DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE;
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
    case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
        return DP_BLEND_MODE_VIVID_LIGHT;
    case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
        return DP_BLEND_MODE_PIN_LIGHT;
    case DP_BLEND_MODE_DIFFERENCE_ALPHA:
        return DP_BLEND_MODE_DIFFERENCE;
    case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
        return DP_BLEND_MODE_DARKER_COLOR;
    case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
        return DP_BLEND_MODE_LIGHTER_COLOR;
    case DP_BLEND_MODE_SHADE_SAI_ALPHA:
        return DP_BLEND_MODE_SHADE_SAI;
    case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
        return DP_BLEND_MODE_SHADE_SHINE_SAI;
    case DP_BLEND_MODE_BURN_SAI_ALPHA:
        return DP_BLEND_MODE_BURN_SAI;
    case DP_BLEND_MODE_DODGE_SAI_ALPHA:
        return DP_BLEND_MODE_DODGE_SAI;
    case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
        return DP_BLEND_MODE_BURN_DODGE_SAI;
    case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
        return DP_BLEND_MODE_HARD_MIX_SAI;
    case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
        return DP_BLEND_MODE_DIFFERENCE_SAI;
    case DP_BLEND_MODE_MARKER_ALPHA:
        return DP_BLEND_MODE_MARKER;
    case DP_BLEND_MODE_MARKER_ALPHA_WASH:
        return DP_BLEND_MODE_MARKER_WASH;
    case DP_BLEND_MODE_GREATER_ALPHA:
        return DP_BLEND_MODE_GREATER;
    case DP_BLEND_MODE_GREATER_ALPHA_WASH:
        return DP_BLEND_MODE_GREATER_WASH;
    case DP_BLEND_MODE_PIGMENT_ALPHA:
        return DP_BLEND_MODE_PIGMENT;
    default:
        return blend_mode;
    }
}

uint8_t DP_blend_mode_to_compatible(uint8_t blend_mode)
{
    switch (blend_mode) {
    case DP_BLEND_MODE_COMPARE_DENSITY_SOFT:
    case DP_BLEND_MODE_COMPARE_DENSITY:
    case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
    case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
    case DP_BLEND_MODE_DIFFERENCE_ALPHA:
    case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
    case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
    case DP_BLEND_MODE_SHADE_SAI_ALPHA:
    case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
    case DP_BLEND_MODE_BURN_SAI_ALPHA:
    case DP_BLEND_MODE_DODGE_SAI_ALPHA:
    case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
    case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
    case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
    case DP_BLEND_MODE_MARKER_ALPHA:
    case DP_BLEND_MODE_MARKER_ALPHA_WASH:
    case DP_BLEND_MODE_GREATER_ALPHA:
    case DP_BLEND_MODE_GREATER_ALPHA_WASH:
    case DP_BLEND_MODE_PIGMENT_ALPHA:
    case DP_BLEND_MODE_PIGMENT_AND_ERASER:
        return DP_BLEND_MODE_NORMAL;
    case DP_BLEND_MODE_VIVID_LIGHT:
    case DP_BLEND_MODE_PIN_LIGHT:
    case DP_BLEND_MODE_DIFFERENCE:
    case DP_BLEND_MODE_DARKER_COLOR:
    case DP_BLEND_MODE_LIGHTER_COLOR:
    case DP_BLEND_MODE_SHADE_SAI:
    case DP_BLEND_MODE_SHADE_SHINE_SAI:
    case DP_BLEND_MODE_BURN_SAI:
    case DP_BLEND_MODE_DODGE_SAI:
    case DP_BLEND_MODE_BURN_DODGE_SAI:
    case DP_BLEND_MODE_HARD_MIX_SAI:
    case DP_BLEND_MODE_DIFFERENCE_SAI:
    case DP_BLEND_MODE_MARKER:
    case DP_BLEND_MODE_MARKER_WASH:
    case DP_BLEND_MODE_GREATER:
    case DP_BLEND_MODE_GREATER_WASH:
    case DP_BLEND_MODE_PIGMENT:
        return DP_BLEND_MODE_RECOLOR;
    case DP_BLEND_MODE_ERASE_PRESERVE:
    case DP_BLEND_MODE_LIGHT_TO_ALPHA:
    case DP_BLEND_MODE_DARK_TO_ALPHA:
    case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
    case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
        return DP_BLEND_MODE_ERASE;
    case DP_BLEND_MODE_BEHIND_PRESERVE:
        return DP_BLEND_MODE_BEHIND;
    case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
        return DP_BLEND_MODE_COLOR_ERASE;
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
    }
    return blend_mode;
}


bool DP_paint_mode_exists(int paint_mode)
{
    switch (paint_mode) {
    case DP_PAINT_MODE_DIRECT:
    case DP_PAINT_MODE_INDIRECT_WASH:
    case DP_PAINT_MODE_INDIRECT_SOFT:
    case DP_PAINT_MODE_INDIRECT_NORMAL:
        return true;
    default:
        return false;
    }
}

const char *DP_paint_mode_enum_name(int paint_mode)
{
    switch (paint_mode) {
    case DP_PAINT_MODE_DIRECT:
        return "DP_PAINT_MODE_DIRECT";
    case DP_PAINT_MODE_INDIRECT_WASH:
        return "DP_PAINT_MODE_INDIRECT_WASH";
    case DP_PAINT_MODE_INDIRECT_SOFT:
        return "DP_PAINT_MODE_INDIRECT_SOFT";
    case DP_PAINT_MODE_INDIRECT_NORMAL:
        return "DP_PAINT_MODE_INDIRECT_NORMAL";
    default:
        return "DP_PAINT_MODE_UNKLNOWN";
    }
}

const char *DP_paint_mode_enum_name_unprefixed(int paint_mode)
{
    return DP_paint_mode_enum_name(paint_mode) + 14;
}

const char *DP_paint_mode_setting_name(int paint_mode)
{
    switch (paint_mode) {
    case DP_PAINT_MODE_DIRECT:
        return "direct";
    case DP_PAINT_MODE_INDIRECT_WASH:
        return "indirect_wash";
    case DP_PAINT_MODE_INDIRECT_SOFT:
        return "indirect_soft";
    case DP_PAINT_MODE_INDIRECT_NORMAL:
        return "indirect_normal";
    default:
        return "unknown";
    }
}

DP_PaintMode DP_paint_mode_by_setting_name(const char *setting_name,
                                           DP_PaintMode not_found_value)
{
    if (DP_str_equal(setting_name, "direct")) {
        return DP_PAINT_MODE_DIRECT;
    }
    else if (DP_str_equal(setting_name, "indirect_wash")) {
        return DP_PAINT_MODE_INDIRECT_WASH;
    }
    else if (DP_str_equal(setting_name, "indirect_soft")) {
        return DP_PAINT_MODE_INDIRECT_SOFT;
    }
    else if (DP_str_equal(setting_name, "indirect_normal")) {
        return DP_PAINT_MODE_INDIRECT_NORMAL;
    }
    else {
        return not_found_value;
    }
}

bool DP_paint_mode_indirect(int paint_mode, int *out_blend_mode)
{
    DP_ASSERT(out_blend_mode);
    switch (paint_mode) {
    case DP_PAINT_MODE_INDIRECT_WASH:
        *out_blend_mode = DP_BLEND_MODE_COMPARE_DENSITY;
        return true;
    case DP_PAINT_MODE_INDIRECT_SOFT:
        *out_blend_mode = DP_BLEND_MODE_COMPARE_DENSITY_SOFT;
        return true;
    case DP_PAINT_MODE_INDIRECT_NORMAL:
        *out_blend_mode = DP_BLEND_MODE_NORMAL;
        return true;
    default:
        return false;
    }
}

uint8_t DP_paint_mode_to_compatible(uint8_t paint_mode)
{
    return paint_mode == DP_PAINT_MODE_DIRECT ? DP_PAINT_MODE_DIRECT
                                              : DP_PAINT_MODE_INDIRECT_SOFT;
}
