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

#include "brushmask.h"

#include <cmath>

namespace paintcore {

template<typename T> static T square(T x) { return x*x; }

static const int LUT_RADIUS = 128;

static BrushMask makeColorSamplingStamp(int radius)
{
	static QVector<uchar> lut;
	if(lut.isEmpty()) {
		// Generate a lookup table for a Gimp style exponential brush shape
		const qreal hardness = 0.5;
		const qreal exponent = 0.4 / (1.0 - hardness);
		lut.resize(square(LUT_RADIUS));
		for(int i=0;i<lut.size();++i)
			lut[i] = 255 * (1-pow(pow(sqrt(i)/LUT_RADIUS, exponent), 2));
	}

	const int diameter = radius*2;
	const float lut_scale = square((LUT_RADIUS-1) / double(radius));

	QVector<uchar> data(square(diameter), 0);
	uchar *ptr = data.data();

	for(int y=0;y<diameter;++y) {
		const qreal yy = square(y-radius);
		for(int x=0;x<diameter;++x) {
			const int dist = int((square(x-radius) + yy) * lut_scale);
			*(ptr++) = dist<lut.size() ? lut.at(dist) : 0;
		}
	}

	return BrushMask(diameter, data);
}

BrushStamp makeColorSamplingStamp(int radius, const QPoint &point)
{
	Q_ASSERT(radius>0);

	// Sampling mask doesn't change size very often
	static BrushMask mask;
	if(mask.diameter() != radius*2)
		mask = makeColorSamplingStamp(radius);

	return BrushStamp { point.x() - radius, point.y() - radius, mask };
}

}
