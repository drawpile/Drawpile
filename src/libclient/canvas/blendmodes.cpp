// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/canvas/blendmodes.h"

#include <QCoreApplication>

namespace canvas {
namespace blendmode {

// Mode can be used as a layer blending mode
static const uint8_t LayerMode = 0x01;

// Mode can be used as a brush blending mode
static const uint8_t BrushMode = 0x02;

// Mode can be used for both brushes and layers
static const uint8_t UniversalMode = 0x03;

// Mode is available when the eraser option is checked
static const uint8_t EraserMode = 0x04;

struct BlendModeInfo {
	//! The blend mode's translatable name
	const char *name;

	//! The blend mode ID
	const DP_BlendMode id;

	//! Blend mode category
	const uint8_t flags;
};

// List of blend modes available in the user interface
static const BlendModeInfo BLEND_MODE[] = {
	{
		QT_TRANSLATE_NOOP("blendmode", "Normal"),
		DP_BLEND_MODE_NORMAL,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Recolor"),
		DP_BLEND_MODE_RECOLOR,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Behind"),
		DP_BLEND_MODE_BEHIND,
		BrushMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Multiply"),
		DP_BLEND_MODE_MULTIPLY,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Screen"),
		DP_BLEND_MODE_SCREEN,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Overlay"),
		DP_BLEND_MODE_OVERLAY,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Divide"),
		DP_BLEND_MODE_DIVIDE,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Burn"),
		DP_BLEND_MODE_BURN,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Dodge"),
		DP_BLEND_MODE_DODGE,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Darken"),
		DP_BLEND_MODE_DARKEN,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Lighten"),
		DP_BLEND_MODE_LIGHTEN,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Subtract"),
		DP_BLEND_MODE_SUBTRACT,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Add"),
		DP_BLEND_MODE_ADD,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Erase"),
		DP_BLEND_MODE_ERASE,
		LayerMode | EraserMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color Erase"),
		DP_BLEND_MODE_COLOR_ERASE,
		EraserMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hard Light"),
		DP_BLEND_MODE_HARD_LIGHT,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Soft Light"),
		DP_BLEND_MODE_SOFT_LIGHT,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Burn"),
		DP_BLEND_MODE_LINEAR_BURN,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Light"),
		DP_BLEND_MODE_LINEAR_LIGHT,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Luminosity/Shine (SAI)"),
		DP_BLEND_MODE_LUMINOSITY_SHINE_SAI,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hue"),
		DP_BLEND_MODE_HUE,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Saturation"),
		DP_BLEND_MODE_SATURATION,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Luminosity"),
		DP_BLEND_MODE_LUMINOSITY,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color"),
		DP_BLEND_MODE_COLOR,
		UniversalMode
	},
};

static const int BLEND_MODES = sizeof(BLEND_MODE)/sizeof(BlendModeInfo);

QString svgName(DP_BlendMode mode)
{
	return QString::fromUtf8(DP_blend_mode_svg_name(mode));
}

DP_BlendMode fromSvgName(const QString &name, DP_BlendMode defaultMode)
{
	return DP_blend_mode_by_svg_name(name.toUtf8().constData(), defaultMode);
}


static QVector<Named> modeNames(uint8_t flag)
{
	QVector<Named> list;
	for(int i=0;i<BLEND_MODES;++i) {
		if((BLEND_MODE[i].flags & flag)) {
			list.append(Named{
				BLEND_MODE[i].id,
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

static bool hasFlag(DP_BlendMode mode, uint8_t flag)
{
	for(const BlendModeInfo &info : BLEND_MODE) {
		if(info.id == mode) {
			return info.flags & flag;
		}
	}
	return false;
}

bool isValidBrushMode(DP_BlendMode mode)
{
	return hasFlag(mode, BrushMode);
}

bool isValidEraseMode(DP_BlendMode mode)
{
	return hasFlag(mode, EraserMode);
}

}
}
