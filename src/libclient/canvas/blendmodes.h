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
#ifndef PAINTCORE_BLENDMODES_H
#define PAINTCORE_BLENDMODES_H

#include <QString>
#include <QVector>
#include <QPair>

namespace rustpile {
	enum class Blendmode : uint8_t;
}

namespace canvas {
namespace blendmode {
	//! Get the SVG name for the given blend mode
	QString svgName(rustpile::Blendmode mode);

	//! Find a blend mode by its SVG name
	rustpile::Blendmode fromSvgName(const QString &name);

	//! Get a list of (brush) blend modes and their translated names
	QVector<QPair<rustpile::Blendmode, QString>> brushModeNames();

	//! Get a list of (layer) blend modes and their translated names
	QVector<QPair<rustpile::Blendmode, QString>> layerModeNames();
}
}

#endif

