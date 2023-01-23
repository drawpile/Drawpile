/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "libclient/canvas/blendmodes.h"
#include "rustpile/rustpile.h"

#include <QCoreApplication>

namespace canvas {
namespace blendmode {

using rustpile::Blendmode;

// Mode can be used as a layer blending mode
static const uint8_t LayerMode = 0x01;

// Mode can be used as a brush blending mode
static const uint8_t BrushMode = 0x02;

// Mode can be used for both brushes and layers
static const uint8_t UniversalMode = 0x03;

struct BlendModeInfo {
	//! The blend mode's translatable name
	const char *name;

	//! The blend mode ID
	const rustpile::Blendmode id;

	//! Blend mode category
	const uint8_t flags;
};

// List of blend modes available in the user interface
static const BlendModeInfo BLEND_MODE[] = {
	{
		QT_TRANSLATE_NOOP("blendmode", "Normal"),
		Blendmode::Normal,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Recolor"),
		Blendmode::Recolor,
		UniversalMode,
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Behind"),
		Blendmode::Behind,
		BrushMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Multiply"),
		Blendmode::Multiply,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Screen"),
		Blendmode::Screen,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Overlay"),
		Blendmode::Overlay,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Divide"),
		Blendmode::Divide,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Burn"),
		Blendmode::Burn,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Dodge"),
		Blendmode::Dodge,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Darken"),
		Blendmode::Darken,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Lighten"),
		Blendmode::Lighten,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Subtract"),
		Blendmode::Subtract,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Add"),
		Blendmode::Add,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Erase"),
		Blendmode::Erase,
		LayerMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hard Light"),
		Blendmode::HardLight,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Soft Light"),
		Blendmode::SoftLight,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Burn"),
		Blendmode::LinearBurn,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Linear Light"),
		Blendmode::LinearLight,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Luminosity/Shine (SAI)"),
		Blendmode::LuminosityShineSai,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Hue"),
		Blendmode::Hue,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Saturation"),
		Blendmode::Saturation,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Luminosity"),
		Blendmode::Luminosity,
		UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("blendmode", "Color"),
		Blendmode::Color,
		UniversalMode
	},
};

static const int BLEND_MODES = sizeof(BLEND_MODE)/sizeof(BlendModeInfo);

QString svgName(Blendmode mode)
{
	uintptr_t len;
	const char *name = reinterpret_cast<const char*>(rustpile::blendmode_svgname(mode, &len));
	return QString::fromUtf8(name, len);
}

Blendmode fromSvgName(const QString &name)
{
	return rustpile::blendmode_from_svgname(reinterpret_cast<const uint16_t*>(name.constData()), name.length());
}


QVector<QPair<rustpile::Blendmode, QString>> brushModeNames()
{
	QVector<QPair<rustpile::Blendmode, QString>> list;
	for(int i=0;i<BLEND_MODES;++i) {
		if((BLEND_MODE[i].flags & BrushMode))
			list << QPair<rustpile::Blendmode, QString>{BLEND_MODE[i].id, QCoreApplication::translate("blendmode", BLEND_MODE[i].name)};
	}
	return list;
}

QVector<QPair<rustpile::Blendmode, QString>> layerModeNames()
{
	QVector<QPair<rustpile::Blendmode, QString>> list;
	for(int i=0;i<BLEND_MODES;++i) {
		if((BLEND_MODE[i].flags & LayerMode))
			list << QPair<rustpile::Blendmode, QString>{BLEND_MODE[i].id, QCoreApplication::translate("blendmode", BLEND_MODE[i].name)};
	}
	return list;
}

}
}
