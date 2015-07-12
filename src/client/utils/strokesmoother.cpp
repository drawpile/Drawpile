/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "strokesmoother.h"

StrokeSmoother::StrokeSmoother()
{
}

void StrokeSmoother::setSmoothing(int strength)
{
	Q_ASSERT(strength>0);
	_points.resize(strength);
	reset();
}

void StrokeSmoother::addPoint(const paintcore::Point &point)
{
	Q_ASSERT(_points.size()>0);

	if(_count == 0) {
		// Pad the buffer with this point, so we blend away from it
		// gradually as we gain more points.
		_points.fill(point);
		_count = _points.size();
	} else {
		if(--_pos < 0)
			_pos = _points.size()-1;
		_points[_pos] = point;
	}
}

paintcore::Point StrokeSmoother::at(int i) const
{
	return _points.at((_pos+i) % _points.size());
}

void StrokeSmoother::reset()
{
	_count=0;
	_pos = 0;
}

bool StrokeSmoother::hasSmoothPoint() const
{
	return _count > 0;
}

paintcore::Point StrokeSmoother::smoothPoint() const
{
	Q_ASSERT(hasSmoothPoint());

	// A simple unweighted sliding-average smoother
	paintcore::Point p = at(0);

	float pressure = p.pressure();
	for(int i=1;i<_count;++i) {
		paintcore::Point pi = at(i);
		p.rx() += pi.x();
		p.ry() += pi.y();
		pressure += pi.pressure();
	}

	const float c = _count;
	p.rx() /= c;
	p.ry() /= c;
	p.setPressure(pressure / c);

	return p;
}
