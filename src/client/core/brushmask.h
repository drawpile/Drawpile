/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2018 Calle Laakkonen

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
#ifndef PAINTCORE_BRUSHMASK_H
#define PAINTCORE_BRUSHMASK_H

#include "brush.h"
#include "point.h"

#include <QVector>

namespace paintcore {

class BrushMask
{
public:
	BrushMask() : m_diameter(0) {}
	BrushMask(int dia, const QVector<uchar> data) : m_diameter(dia), m_data(data) {}

	/**
	 * @brief Get the brush mask diameter.
	 *
	 * @return
	 */
	int diameter() const { return m_diameter; }

	/**
	 * @brief Get brush mask data
	 *
	 * The data is a square of area diameter²
	 *
	 * @return data
	 * @pre diameter() > 0
	 */
	const uchar *data() const { return m_data.data(); }

private:
	int m_diameter;
	QVector<uchar> m_data;
};

struct BrushStamp {
	int left;
	int top;
	BrushMask mask;
};

//! Make a brush stamp for area color picking
BrushStamp makeColorSamplingStamp(int radius, const QPoint &point);

BrushStamp makeGimpStyleBrushStamp(const Brush &brush, const Point &point);

}

#endif
