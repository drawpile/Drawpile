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
	reset();
}

void StrokeSmoother::setSmoothing(int strength)
{
	Q_ASSERT(strength>0);
	_points.resize(strength);
	reset();
}

void StrokeSmoother::addOffset(const QPointF &offset)
{
	for(int i=0;i<_points.size();++i)
		_points[i] += offset;
}

void StrokeSmoother::addPoint(const canvas::Point &point)
{
	Q_ASSERT(_points.size()>0);

	if(_count == 0) {
		/* Pad the buffer with this point, so we blend away from it
		 * gradually as we gain more points. We still only count this
		 * as one point so we know how much real data we have to
		 * drain if it was a very short stroke. */
		_points.fill(point);
	} else {
		if(--_pos < 0)
			_pos = _points.size()-1;
		_points[_pos] = point;
	}

	if(_count < _points.size())
		++_count;
}

canvas::Point StrokeSmoother::at(int i) const
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

canvas::Point StrokeSmoother::smoothPoint() const
{
	Q_ASSERT(hasSmoothPoint());

	// A simple unweighted sliding-average smoother
	auto p = at(0);

	qreal pressure = p.pressure();
	qreal xtilt = p.xtilt();
	qreal ytilt = p.ytilt();
	for(int i=1;i<_points.size();++i) {
		const auto pi = at(i);
		p.rx() += pi.x();
		p.ry() += pi.y();
		pressure += pi.pressure();
		xtilt += pi.xtilt();
		ytilt += pi.ytilt();
	}

	const qreal c = _points.size();
	p.rx() /= c;
	p.ry() /= c;
	p.setPressure(pressure / c);
	p.setXtilt(xtilt / c);
	p.setYtilt(ytilt / c);

	return p;
}

void StrokeSmoother::removePoint()
{
	Q_ASSERT(!_points.isEmpty());
	/* Pad the buffer with the final point, overwriting the oldest first,
	 * for symmetry with starting. For very short strokes this should
	 * really set all points between --_count and _points.size()-1. */
	_points[(_pos + --_count) % _points.size()] = latestPoint();
}
