// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/checkerbackground.h"
#include "libclient/config/config.h"
#include <QPainter>
#include <QPalette>

const QPixmap &CheckerBackground::getPixmap(
	const QColor &color1, const QColor &color2, qreal dpr)
{
	int dimension = qRound(16.0 * dpr);
	bool needsUpdate = m_pixmap.isNull() || color1 != m_color1 ||
					   color2 != m_color2 || dimension != m_dimension;
	if(needsUpdate) {
		m_pixmap = QPixmap(dimension * 2, dimension * 2);
		m_pixmap.fill(color1);
		QPainter p(&m_pixmap);
		p.fillRect(0, 0, dimension, dimension, color2);
		p.fillRect(dimension, dimension, dimension, dimension, color2);
		m_color1 = color1;
		m_color2 = color2;
		m_dimension = dimension;
	}
	return m_pixmap;
}

const QPixmap &CheckerBackground::getPixmapPlain(const QPalette &pal, qreal dpr)
{
	QColor baseColor = pal.color(QPalette::Base);
	if(baseColor.lightnessF() < 0.5) {
		return getPixmap(baseColor.darker(150), baseColor.lighter(250), dpr);
	} else {
		return getPixmap(baseColor.darker(200), baseColor.lighter(150), dpr);
	}
}

const QPixmap &
CheckerBackground::getPixmapConfig(const config::Config *cfg, qreal dpr)
{
	return getPixmap(cfg->getCheckerColor1(), cfg->getCheckerColor2(), dpr);
}
