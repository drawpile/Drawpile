/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include <QVector>
#include <QCache>

#include "brush.h"

namespace paintcore {

class BrushMask
{
public:
	BrushMask() : _diameter(0) {}
	BrushMask(int dia, const QVector<uchar> data) : _diameter(dia), _data(data) {}

	/**
	 * @brief Get the brush mask diameter.
	 *
	 * @return
	 */
	int diameter() const { return _diameter; }

	/**
	 * @brief Get brush mask data
	 *
	 * The data is a square of area diameter²
	 *
	 * @return data
	 * @pre diameter() > 0
	 */
	const uchar *data() const { return _data.data(); }

private:
	int _diameter;
	QVector<uchar> _data;
};

class BrushMaskGenerator
{
public:
	BrushMaskGenerator();
	BrushMaskGenerator(const Brush &brush);

	static const BrushMaskGenerator &cached(const Brush &brush);

	BrushMask make(float pressure) const;
	BrushMask make(float xfrac, float yfrac, float pressure) const;

private:
	void buildLUT(const Brush &brush);

	QVector<uchar> _lut;
	QVector<uint> _index;
	QVector<float> _radius;
	bool _usepressure;
	mutable QCache<int, BrushMask> _cache;
};

}

#endif
