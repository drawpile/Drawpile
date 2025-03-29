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
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Recolor"),
		DP_BLEND_MODE_RECOLOR,
		DP_BLEND_MODE_NORMAL,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Behind"),
		DP_BLEND_MODE_BEHIND,
		DP_BLEND_MODE_BEHIND_PRESERVE,
		BrushMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Multiply"),
		DP_BLEND_MODE_MULTIPLY,
		DP_BLEND_MODE_MULTIPLY_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Screen"),
		DP_BLEND_MODE_SCREEN,
		DP_BLEND_MODE_SCREEN_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Overlay"),
		DP_BLEND_MODE_OVERLAY,
		DP_BLEND_MODE_OVERLAY_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Divide"),
		DP_BLEND_MODE_DIVIDE,
		DP_BLEND_MODE_DIVIDE_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Burn"),
		DP_BLEND_MODE_BURN,
		DP_BLEND_MODE_BURN_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Dodge"),
		DP_BLEND_MODE_DODGE,
		DP_BLEND_MODE_DODGE_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Darken"),
		DP_BLEND_MODE_DARKEN,
		DP_BLEND_MODE_DARKEN_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Lighten"),
		DP_BLEND_MODE_LIGHTEN,
		DP_BLEND_MODE_LIGHTEN_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Subtract"),
		DP_BLEND_MODE_SUBTRACT,
		DP_BLEND_MODE_SUBTRACT_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Add"),
		DP_BLEND_MODE_ADD,
		DP_BLEND_MODE_ADD_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Erase"),
		DP_BLEND_MODE_ERASE,
		DP_BLEND_MODE_ERASE_PRESERVE,
		LayerMode | EraserMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color Erase"),
		DP_BLEND_MODE_COLOR_ERASE,
		DP_BLEND_MODE_COLOR_ERASE_PRESERVE,
		EraserMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Erase Lightness"),
		DP_BLEND_MODE_ERASE_LIGHT,
		DP_BLEND_MODE_ERASE_LIGHT_PRESERVE,
		LayerMode | EraserMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Erase Darkness"),
		DP_BLEND_MODE_ERASE_DARK,
		DP_BLEND_MODE_ERASE_DARK_PRESERVE,
		LayerMode | EraserMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hard Light"),
		DP_BLEND_MODE_HARD_LIGHT,
		DP_BLEND_MODE_HARD_LIGHT_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Soft Light"),
		DP_BLEND_MODE_SOFT_LIGHT,
		DP_BLEND_MODE_SOFT_LIGHT_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Burn"),
		DP_BLEND_MODE_LINEAR_BURN,
		DP_BLEND_MODE_LINEAR_BURN_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Light"),
		DP_BLEND_MODE_LINEAR_LIGHT,
		DP_BLEND_MODE_LINEAR_LIGHT_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Luminosity/Shine (SAI)"),
		DP_BLEND_MODE_LUMINOSITY_SHINE_SAI,
		DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hue"),
		DP_BLEND_MODE_HUE,
		DP_BLEND_MODE_HUE_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Saturation"),
		DP_BLEND_MODE_SATURATION,
		DP_BLEND_MODE_SATURATION_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Luminosity"),
		DP_BLEND_MODE_LUMINOSITY,
		DP_BLEND_MODE_LUMINOSITY_ALPHA,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color"),
		DP_BLEND_MODE_COLOR,
		DP_BLEND_MODE_COLOR_ALPHA,
		UniversalMode,
	},
};

static const int BLEND_MODES = sizeof(BLEND_MODE) / sizeof(BlendModeInfo);

QString translatedName(int mode)
{
	const char *key = QT_TRANSLATE_NOOP("blendmode", "Unknown");
	for(const BlendModeInfo &info : BLEND_MODE) {
		if(int(info.id) == mode) {
			key = info.name;
			break;
		}
	}
	return QCoreApplication::translate("blendmode", key);
}

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
		if((BLEND_MODE[i].flags & flag)) {
			list.append(Named{
				DP_BlendMode(BLEND_MODE[i].id),
				QCoreApplication::translate("blendmode", BLEND_MODE[i].name),
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

void adjustAlphaBehavior(int &mode, bool preserveAlpha)
{
	if(preserveAlpha) {
		mode = toAlphaPreserving(mode);
	} else {
		mode = toAlphaAffecting(mode);
	}
}

}
}
