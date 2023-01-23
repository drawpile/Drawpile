/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

#include "libclient/tools/utils.h"

#include <QtMath>

namespace tools {

namespace constraints {

QPointF angle(const QPointF &p1, const QPointF &p2)
{
	QPointF dp = p2 - p1;
	double a = qAtan2(dp.y(), dp.x());
	double m = hypot(dp.x(), dp.y());

	// Round a to the nearest multiple of π/4
	const double STEPS = M_PI / 4.0;
	a = qRound(a / STEPS) * STEPS;

	return p1 + QPointF(cos(a)*m, sin(a)*m);
}

QPointF square(const QPointF &p1, const QPointF &p2)
{
	float dx = p2.x() - p1.x();
	float dy = p2.y() - p1.y();
	const float ax = qAbs(dx);
	const float ay = qAbs(dy);

	if(ax>ay)
		dy = ax * (dy<0?-1:1);
	else
		dx = ay * (dx<0?-1:1);

	return p1 + QPointF(dx, dy);
}

}

}

