/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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
#ifndef DP_BRUSHES_BRUSH_H
#define DP_BRUSHES_BRUSH_H

#include "../../rustpile/rustpile.h"

#include <QColor>
#include <QMetaType>

class QJsonObject;

namespace brushes {

//! A convenience wrapper for classic brush settings
struct ClassicBrush : public rustpile::ClassicBrush
{
	ClassicBrush();

	bool isEraser() const {
		return mode == rustpile::Blendmode::Erase || mode == rustpile::Blendmode::ColorErase;
	}

	void setQColor(const QColor& c) {
		color = {float(c.redF()), float(c.greenF()), float(c.blueF()), float(c.alphaF())};
	}

	QColor qColor() const {
		return QColor::fromRgbF(color.r, color.g, color.b, color.a);
	}

	QJsonObject toJson() const;
	static ClassicBrush fromJson(const QJsonObject &json);
};

}

Q_DECLARE_METATYPE(brushes::ClassicBrush)
Q_DECLARE_TYPEINFO(brushes::ClassicBrush, Q_MOVABLE_TYPE);

#endif


