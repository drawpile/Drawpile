// SPDX-License-Identifier: GPL-3.0-or-later

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

