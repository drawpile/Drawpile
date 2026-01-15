// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/thumbnail.h"
#include <QPainter>

namespace widgets {

Thumbnail::Thumbnail(QWidget *parent)
	: Thumbnail(QPixmap(), parent)
{
}

Thumbnail::Thumbnail(const QPixmap &pixmap, QWidget *parent)
	: QFrame(parent)
	, m_pixmap(pixmap)
{
}

void Thumbnail::setPixmap(const QPixmap &pixmap)
{
	m_pixmap = pixmap;
	update();
}

void Thumbnail::paintEvent(QPaintEvent *event)
{
	paintPixmap();
	QFrame::paintEvent(event);
}

void Thumbnail::paintPixmap()
{
	if(m_pixmap.isNull()) {
		return;
	}

	QSize pixmapSize = m_pixmap.size();
	if(pixmapSize.isEmpty()) {
		return;
	}

	QSize widgetSize = size();
	if(widgetSize.isEmpty()) {
		return;
	}

	qreal widthRatio = qreal(widgetSize.width()) / qreal(pixmapSize.width());
	qreal heightRatio = qreal(widgetSize.height()) / qreal(pixmapSize.height());
	QRect targetRect;
	if(qFuzzyCompare(widthRatio, heightRatio)) {
		targetRect = QRect(0, 0, widgetSize.width(), widgetSize.height());
	} else if(widthRatio < heightRatio) {
		int h = qMin(
			pixmapSize.height(),
			qRound(qreal(pixmapSize.height()) * widthRatio));
		targetRect =
			QRect(0, (widgetSize.height() - h) / 2, widgetSize.width(), h);
	} else {
		int w = qMin(
			pixmapSize.width(),
			qRound(qreal(pixmapSize.width()) * heightRatio));
		targetRect =
			QRect((widgetSize.width() - w) / 2, 0, w, widgetSize.height());
	}

	QPainter painter(this);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.drawPixmap(targetRect, m_pixmap);
}

}
