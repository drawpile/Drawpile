/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2018 Calle Laakkonen

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

#include "shapes.h"

#include <QtMath>
#include <algorithm>

namespace brushes {
namespace shapes {

using paintcore::Point;
using paintcore::PointVector;

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
		pv << Point(cx + a*cos(t), cy + b*sin(t), 1.0);
	}
	pv << Point(cx+a, cy, 1);
	return pv;
}

static Point _cubicBezierPoint(const QPointF p[4], float t)
{
	const float t1 = 1-t;
	const float Ax = t1*p[0].x() + t*p[1].x();
	const float Ay = t1*p[0].y() + t*p[1].y();
	const float Bx = t1*p[1].x() + t*p[2].x();
	const float By = t1*p[1].y() + t*p[2].y();
	const float Cx = t1*p[2].x() + t*p[3].x();
	const float Cy = t1*p[2].y() + t*p[3].y();

	const float Dx = t1*Ax + t*Bx;
	const float Dy = t1*Ay + t*By;
	const float Ex = t1*Bx + t*Cx;
	const float Ey = t1*By + t*Cy;

	return Point(t1*Dx + t*Ex, t1*Dy + t*Ey, 1);;
}

PointVector cubicBezierCurve(const QPointF p[4])
{
	PointVector pv;
	// TODO smart step size selection
	for(float t=0;t<1;t+=0.05) {
		pv << _cubicBezierPoint(p, t);
	}
	return pv;
}

PointVector sampleStroke(const QRectF &rect)
{
	const int strokew = rect.width();
	const qreal strokeh = rect.height() * 0.6;
	const qreal offy = rect.top() + rect.height()/2;
	const qreal dphase = (2*M_PI)/qreal(strokew);
	qreal phase = 0;

	PointVector pointvector;
	pointvector.reserve(strokew);
	pointvector << paintcore::Point(rect.left(), offy, 0.0);
	for(int x=0;x<strokew;++x, phase += dphase) {

		const qreal fx = x/qreal(strokew);
		const qreal pressure = qBound(0.0, ((fx*fx) - (fx*fx*fx))*6.756, 1.0);
		const qreal y = sin(phase) * strokeh;
		pointvector << Point(rect.left()+x, offy+y, pressure);
	}
	return pointvector;
}

// this is used to demonstrate the flood fill
PointVector sampleBlob(const QRectF &rect)
{
	PointVector pv;

	const float mid = rect.top() + rect.height()/2;
	const float h = rect.height() * 0.8;

	for(float a=0;a<M_PI;a+=0.1) {
		float x = rect.left() + (a / M_PI * rect.width());
		float y = pow(sin(a), 0.5)*0.7 + sin(a*3)*0.3;

		pv << Point(x, mid-y*h, 1);
	}
	pv << Point(rect.right(), mid, 1);

	for(float a=0.1;a<M_PI;a+=0.1) {
		float x = rect.right() - (a / M_PI * rect.width());
		float y = pow(sin(a), 0.5)*0.7 + sin(a*2.8)*0.2;

		pv << Point(x, mid+y*h, 1);
	}

	pv << pv[0];

	return pv;
}

}
}
