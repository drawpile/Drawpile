// SPDX-License-Identifier: GPL-3.0-or-later
class QColor;
class QPainter;
class QSize;
class QString;

namespace utils {

void paintAnnotation(
	QPainter *painter, const QSize &size, const QColor &background,
	const QString &text, int valign);

}
