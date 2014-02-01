/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <Qt>
#if (QT_VERSION < QT_VERSION_CHECK(5, 1, 0))
#include <cmath>
#define qSin sin
#define qCos cos
#else
#include <QtMath>
#endif

#include "shapes.h"

namespace paintcore{
namespace shapes {

PointVector rectangle(const QRectF &rect)
{
	const QPointF p1 = rect.topLeft();
	const QPointF p2 = rect.bottomRight();

	PointVector pv;
	pv.reserve(5);
	pv << Point(p1, 1);
	pv << Point(p1.x(), p2.y(), 1);
	pv << Point(p2, 1);
	pv << Point(p2.x(), p1.y(), 1);
	pv << Point(p1.x() + 1, p1.y(), 1);
	return pv;
}


PointVector ellipse(const QRectF &rect)
{
	const qreal a = rect.width() / 2.0;
	const qreal b = rect.height() / 2.0;
	const qreal cx = rect.x() + a;
	const qreal cy = rect.y() + b;

	PointVector pv;

	// TODO smart step size selection
	for(qreal t=0;t<2*M_PI;t+=M_PI/20) {
		pv << Point(cx + a*qCos(t), cy + b*qSin(t), 1.0);
	}
	pv << Point(cx+a, cy, 1);
	return pv;
}

}
}
