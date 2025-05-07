// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/canvas/blendmodes.h"
#include <QCoreApplication>
#include <QStandardItemModel>

namespace canvas {
namespace blendmode {

// Mode can be used as a layer blending mode
static constexpr uint8_t LayerMode = 0x01;

// Mode can be used as a brush blending mode
static constexpr uint8_t BrushMode = 0x02;

// Mode can be used for both brushes and layers
static constexpr uint8_t UniversalMode = 0x03;

// Mode is available when the eraser option is checked
static constexpr uint8_t EraserMode = 0x04;

static constexpr uint8_t SeparatorAbove = 0x08;
static constexpr uint8_t SeparatorBelow = 0x10;

static constexpr uint8_t CompatibleMode = 0x20;

struct BlendModeInfo {
	//! The blend mode's translatable name
	const char *name;

	//! The blend mode ID
	const int id;

	//! Alternate ID, for alpha-affecting/preserving variants
	const int alternateId;

	//! Blend mode category
	const uint8_t flags;
};

// List of blend modes available in the user interface
static const BlendModeInfo BLEND_MODE[] = {
	{
		QT_TRANSLATE_NOOP("blendmode", "Normal"),
		DP_BLEND_MODE_NORMAL,
		DP_BLEND_MODE_RECOLOR,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Recolor"),
		DP_BLEND_MODE_RECOLOR,
		DP_BLEND_MODE_NORMAL,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Behind"),
		DP_BLEND_MODE_BEHIND,
		DP_BLEND_MODE_BEHIND_PRESERVE,
		BrushMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Greater Density"),
		DP_BLEND_MODE_GREATER_ALPHA,
		DP_BLEND_MODE_GREATER,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Marker"),
		DP_BLEND_MODE_MARKER_ALPHA,
		DP_BLEND_MODE_MARKER,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Pigment"),
		DP_BLEND_MODE_PIGMENT_ALPHA,
		DP_BLEND_MODE_PIGMENT,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Erase"),
		DP_BLEND_MODE_ERASE,
		DP_BLEND_MODE_ERASE_PRESERVE,
		LayerMode | EraserMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color Erase"),
		DP_BLEND_MODE_COLOR_ERASE,
		DP_BLEND_MODE_COLOR_ERASE_PRESERVE,
		EraserMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Multiply"),
		DP_BLEND_MODE_MULTIPLY,
		DP_BLEND_MODE_MULTIPLY_ALPHA,
		UniversalMode | SeparatorAbove | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Screen"),
		DP_BLEND_MODE_SCREEN,
		DP_BLEND_MODE_SCREEN_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Overlay"),
		DP_BLEND_MODE_OVERLAY,
		DP_BLEND_MODE_OVERLAY_ALPHA,
		UniversalMode | SeparatorBelow | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Burn"),
		DP_BLEND_MODE_LINEAR_BURN,
		DP_BLEND_MODE_LINEAR_BURN_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Dodge/Add"),
		DP_BLEND_MODE_ADD,
		DP_BLEND_MODE_ADD_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Light"),
		DP_BLEND_MODE_LINEAR_LIGHT,
		DP_BLEND_MODE_LINEAR_LIGHT_ALPHA,
		UniversalMode | SeparatorBelow | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color Burn"),
		DP_BLEND_MODE_BURN,
		DP_BLEND_MODE_BURN_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color Dodge"),
		DP_BLEND_MODE_DODGE,
		DP_BLEND_MODE_DODGE_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Vivid Light"),
		DP_BLEND_MODE_VIVID_LIGHT,
		DP_BLEND_MODE_VIVID_LIGHT_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Soft Light"),
		DP_BLEND_MODE_SOFT_LIGHT,
		DP_BLEND_MODE_SOFT_LIGHT_ALPHA,
		UniversalMode | SeparatorAbove | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hard Light"),
		DP_BLEND_MODE_HARD_LIGHT,
		DP_BLEND_MODE_HARD_LIGHT_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Pin Light"),
		DP_BLEND_MODE_PIN_LIGHT,
		DP_BLEND_MODE_PIN_LIGHT_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Darken"),
		DP_BLEND_MODE_DARKEN,
		DP_BLEND_MODE_DARKEN_ALPHA,
		UniversalMode | SeparatorAbove | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Lighten"),
		DP_BLEND_MODE_LIGHTEN,
		DP_BLEND_MODE_LIGHTEN_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Darker Color"),
		DP_BLEND_MODE_DARKER_COLOR,
		DP_BLEND_MODE_DARKER_COLOR_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Lighter Color"),
		DP_BLEND_MODE_LIGHTER_COLOR,
		DP_BLEND_MODE_LIGHTER_COLOR_ALPHA,
		UniversalMode | SeparatorBelow,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Difference"),
		DP_BLEND_MODE_DIFFERENCE,
		DP_BLEND_MODE_DIFFERENCE_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Subtract"),
		DP_BLEND_MODE_SUBTRACT,
		DP_BLEND_MODE_SUBTRACT_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Divide"),
		DP_BLEND_MODE_DIVIDE,
		DP_BLEND_MODE_DIVIDE_ALPHA,
		UniversalMode | SeparatorBelow | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hue"),
		DP_BLEND_MODE_HUE,
		DP_BLEND_MODE_HUE_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Saturation"),
		DP_BLEND_MODE_SATURATION,
		DP_BLEND_MODE_SATURATION_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Luminosity"),
		DP_BLEND_MODE_LUMINOSITY,
		DP_BLEND_MODE_LUMINOSITY_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color"),
		DP_BLEND_MODE_COLOR,
		DP_BLEND_MODE_COLOR_ALPHA,
		UniversalMode | SeparatorBelow | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Shade (SAI)"),
		DP_BLEND_MODE_SHADE_SAI,
		DP_BLEND_MODE_SHADE_SAI_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Shine (SAI)"),
		DP_BLEND_MODE_LUMINOSITY_SHINE_SAI,
		DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA,
		UniversalMode | CompatibleMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Shade/Shine (SAI)"),
		DP_BLEND_MODE_SHADE_SHINE_SAI,
		DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Burn (SAI)"),
		DP_BLEND_MODE_BURN_SAI,
		DP_BLEND_MODE_BURN_SAI_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Dodge (SAI)"),
		DP_BLEND_MODE_DODGE_SAI,
		DP_BLEND_MODE_DODGE_SAI_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Burn/Dodge (SAI)"),
		DP_BLEND_MODE_BURN_DODGE_SAI,
		DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hard Mix (SAI)"),
		DP_BLEND_MODE_HARD_MIX_SAI,
		DP_BLEND_MODE_HARD_MIX_SAI_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Difference (SAI)"),
		DP_BLEND_MODE_DIFFERENCE_SAI,
		DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA,
		UniversalMode,
	},
};

static const int BLEND_MODES = sizeof(BLEND_MODE) / sizeof(BlendModeInfo);

QString oraName(DP_BlendMode mode)
{
	return QString::fromUtf8(DP_blend_mode_ora_name(mode));
}

DP_BlendMode fromOraName(const QString &name, DP_BlendMode defaultMode)
{
	return DP_blend_mode_by_ora_name(name.toUtf8().constData(), defaultMode);
}


static QVector<Named> modeNames(uint8_t flag)
{
	QVector<Named> list;
	for(int i = 0; i < BLEND_MODES; ++i) {
		uint8_t flags = BLEND_MODE[i].flags;
		if(flags & flag) {
			list.append(Named{
				DP_BlendMode(BLEND_MODE[i].id),
				QCoreApplication::translate("blendmode", BLEND_MODE[i].name),
				(flags & SeparatorAbove) != 0,
				(flags & SeparatorBelow) != 0,
				(flags & CompatibleMode) != 0,
			});
		}
	}
	return list;
}

QVector<Named> brushModeNames()
{
	return modeNames(BrushMode);
}

QVector<Named> eraserModeNames()
{
	return modeNames(EraserMode);
}

QVector<Named> layerModeNames()
{
	return modeNames(LayerMode);
}

QVector<Named> pasteModeNames()
{
	return modeNames(0xff); // Fill tool supports all modes.
}

static bool hasFlag(int mode, uint8_t flag)
{
	for(const BlendModeInfo &info : BLEND_MODE) {
		if(info.id == mode || info.alternateId == mode) {
			return info.flags & flag;
		}
	}
	return false;
}

bool isValidBrushMode(int mode)
{
	return hasFlag(mode, BrushMode);
}

bool isValidEraseMode(int mode)
{
	return hasFlag(mode, EraserMode);
}

bool preservesAlpha(int mode)
{
	return mode >= 0 && mode < DP_BLEND_MODE_COUNT &&
		   DP_blend_mode_preserves_alpha(mode);
}

bool presentsAsEraser(int mode)
{
	return mode >= 0 && mode < DP_BLEND_MODE_COUNT &&
		   DP_blend_mode_presents_as_eraser(mode);
}

bool presentsAsAlphaPreserving(int mode)
{
	return mode >= 0 && mode < DP_BLEND_MODE_COUNT &&
		   DP_blend_mode_presents_as_alpha_preserving(mode);
}

bool comparesAlpha(int mode)
{
	return mode >= 0 && mode < DP_BLEND_MODE_COUNT &&
		   DP_blend_mode_compares_alpha(mode);
}

bool directOnly(int mode)
{
	return mode >= 0 && mode < DP_BLEND_MODE_COUNT &&
		   DP_blend_mode_direct_only(mode);
}

bool alphaPreservePair(
	int mode, DP_BlendMode *outAlphaAffecting, DP_BlendMode *outAlphaPreserving)
{
	return mode >= 0 && mode < DP_BLEND_MODE_COUNT &&
		   DP_blend_mode_alpha_preserve_pair(
			   mode, outAlphaAffecting, outAlphaPreserving);
}

int toAlphaAffecting(int mode)
{
	return mode >= 0 && mode < DP_BLEND_MODE_COUNT
			   ? DP_blend_mode_to_alpha_affecting(mode)
			   : DP_BLEND_MODE_NORMAL;
}

int toAlphaPreserving(int mode)
{
	return mode >= 0 && mode < DP_BLEND_MODE_COUNT
			   ? DP_blend_mode_to_alpha_preserving(mode)
			   : DP_BLEND_MODE_RECOLOR;
}

int toCompatible(int mode)
{
	return DP_blend_mode_to_compatible(uint8_t(mode));
}

void adjustAlphaBehavior(int &mode, bool preserveAlpha)
{
	if(preserveAlpha) {
		mode = toAlphaPreserving(mode);
	} else {
		mode = toAlphaAffecting(mode);
	}
}

}

namespace paintmode {

bool isValidPaintMode(int mode)
{
	switch(mode) {
	case DP_PAINT_MODE_DIRECT:
	case DP_PAINT_MODE_INDIRECT_WASH:
	case DP_PAINT_MODE_INDIRECT_SOFT:
	case DP_PAINT_MODE_INDIRECT_NORMAL:
		return true;
	default:
		return false;
	}
}

QString settingName(DP_PaintMode mode)
{
	return QString::fromUtf8(DP_paint_mode_setting_name(mode));
}

DP_PaintMode fromSettingName(const QString &name, DP_PaintMode defaultMode)
{
	return DP_paint_mode_by_setting_name(
		name.toUtf8().constData(), defaultMode);
}

}
}
