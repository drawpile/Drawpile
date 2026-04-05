// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_CHECKERBACKGROUND_H
#define LIBCLIENT_UTILS_CHECKERBACKGROUND_H
#include <QColor>
#include <QPixmap>

class QPalette;

namespace config {
class Config;
}

class CheckerBackground final {
public:
	const QPixmap &
	getPixmap(const QColor &color1, const QColor &color2, qreal dpr);

	const QPixmap &getPixmapPlain(const QPalette &pal, qreal dpr);
	const QPixmap &getPixmapConfig(const config::Config *cfg, qreal dpr);

private:
	QPixmap m_pixmap;
	QColor m_color1;
	QColor m_color2;
	int m_dimension;
};

#endif
