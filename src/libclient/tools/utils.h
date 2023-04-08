// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_UTILS_H
#define TOOLS_UTILS_H

#include <QPointF>

namespace tools {

namespace constraints {
	QPointF angle(const QPointF &p1, const QPointF &p2);
	QPointF square(const QPointF &p1, const QPointF &p2);
}

}

#endif

