/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "blendmodes.h"

#include <QCoreApplication>

namespace paintcore {

// Note. The modes are listed in the order they appear in the user interface
// In the protocol, blend modes are identified by the ID number.
const BlendMode BLEND_MODE[] = {
	{
		QT_TRANSLATE_NOOP("paintcore", "Erase"), // This is a special mode
		QString("-dp-erase"), /* this is used internally only */
		BlendMode::MODE_ERASE,
		BlendMode::PrivateMode | BlendMode::DecrOpacity
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Normal"),
		QString("src-over"),
		BlendMode::MODE_NORMAL,
		BlendMode::UniversalMode | BlendMode::IncrOpacity
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Recolor"),
		QString("src-atop"),
		BlendMode::MODE_RECOLOR,
		BlendMode::BrushMode
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Behind"),
		QString("dst-over"),
		BlendMode::MODE_BEHIND,
		BlendMode::BrushMode | BlendMode::IncrOpacity
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Multiply"),
		QString("multiply"),
		BlendMode::MODE_MULTIPLY,
		BlendMode::UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Divide"),
		QString("screen"),
		BlendMode::MODE_DIVIDE,
		BlendMode::UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Burn"),
		QString("color-burn"),
		BlendMode::MODE_BURN,
		BlendMode::UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Dodge"),
		QString("color-dodge"),
		BlendMode::MODE_DODGE,
		BlendMode::UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Darken"),
		QString("darken"),
		BlendMode::MODE_DARKEN,
		BlendMode::UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Lighten"),
		QString("lighten"),
		BlendMode::MODE_LIGHTEN,
		BlendMode::UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Subtract"),
		QString("-dp-minus"), /* not part of SVG or OpenRaster spec */
		BlendMode::MODE_SUBTRACT,
		BlendMode::UniversalMode
	},
	{
		QT_TRANSLATE_NOOP("paintcore", "Add"),
		QString("plus"),
		BlendMode::MODE_ADD,
		BlendMode::UniversalMode
	},
	{
		"Color erase", // This is a special mode
		QString("-dp-cerase"), /* this is used internally only */
		BlendMode::MODE_COLORERASE,
		BlendMode::PrivateMode | BlendMode::DecrOpacity
	},
	{
		"Replace", // Not selectable
		QString("-dp-replace"),
		BlendMode::MODE_REPLACE,
		BlendMode::PrivateMode | BlendMode::IncrOpacity | BlendMode::DecrOpacity
	}
};

static const int BLEND_MODES = sizeof(BLEND_MODE)/sizeof(BlendMode);

const BlendMode &findBlendMode(int id)
{
	for(int i=0;i<BLEND_MODES;++i) {
		if(BLEND_MODE[i].id == id)
			return BLEND_MODE[i];
	}
	qWarning("findBlendMode(%d): no such mode!", id);
	return BLEND_MODE[1];
}

const BlendMode &findBlendModeByName(const QString &name, bool *found)
{
	QStringRef n;
	if(name.startsWith("svg:"))
		n = name.midRef(4);
	else
		n = name.midRef(0);

	for(int i=0;i<BLEND_MODES;++i)
		if(BLEND_MODE[i].svgname == n) {
			if(found)
				*found = true;
			return BLEND_MODE[i];
		}

	if(found)
		*found = false;
	return BLEND_MODE[1];
}

QList<QPair<int, QString>> getBlendModeNames(BlendMode::Flags flags)
{
	QList<QPair<int, QString>> list;
	for(int i=0;i<BLEND_MODES;++i) {
		if((BLEND_MODE[i].flags & flags) == flags)
			list << QPair<int,QString> {BLEND_MODE[i].id, QCoreApplication::translate("paintcore", paintcore::BLEND_MODE[i].name)};
	}
	return list;
}

}
