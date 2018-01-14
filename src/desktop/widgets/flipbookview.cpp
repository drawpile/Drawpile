/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "flipbookview.h"

#include <QPaintEvent>
#include <QPainter>
#include <QRubberBand>

FlipbookView::FlipbookView(QWidget *parent)
	: QWidget(parent), m_rubberband(nullptr)
{
	setCursor(Qt::CrossCursor);
}

void FlipbookView::startCrop()
{
	setCursor(Qt::CrossCursor);
}

void FlipbookView::setPixmap(const QPixmap &pixmap)
{
	m_pixmap = pixmap;
	update();
}

void FlipbookView::paintEvent(QPaintEvent *event)
{
	const int w = width();
	const int h = height();

	if(!QRect(0, 0, w, h).intersects(event->rect()))
		return;

	QPainter painter(this);
	painter.setClipRect(event->rect());
	painter.fillRect(QRect(0, 0, w, h), QColor(35, 38, 41));

	if(!m_pixmap.isNull()) {
		painter.drawPixmap(w/2 - m_pixmap.width()/2, h/2 - m_pixmap.height()/2, m_pixmap);
	}
}

void FlipbookView::mousePressEvent(QMouseEvent *event)
{
	if(m_pixmap.isNull())
		return;

	if(!m_rubberband)
		m_rubberband = new QRubberBand(QRubberBand::Rectangle, this);
	m_cropStart = event->pos();
	m_rubberband->setGeometry(QRect(m_cropStart, QSize()));
	m_rubberband->show();
}

void FlipbookView::mouseMoveEvent(QMouseEvent *event)
{
	if(m_rubberband)
		m_rubberband->setGeometry(QRect(m_cropStart, event->pos()).normalized());
}

void FlipbookView::mouseReleaseEvent(QMouseEvent *event)
{
	Q_UNUSED(event);
	if(m_rubberband && m_rubberband->isVisible()) {
		m_rubberband->hide();
		const int x0 = width()/2 - m_pixmap.width() / 2;
		const int y0 = height()/2 - m_pixmap.height()/2;
		const qreal w = m_pixmap.width();
		const qreal h = m_pixmap.height();
		const QRect bounds(x0, y0, w, h);
		const QRect g = m_rubberband->geometry().intersected(bounds);

		emit cropped(QRectF(
			(g.x() - x0) / w,
			(g.y() - y0) / h,
			g.width() / w,
			g.height() / h
		));
	}
}
