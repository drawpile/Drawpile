/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

#include "lasertrailitem.h"

#include <QApplication>
#include <QPainter>
#include <QDateTime>

namespace drawingboard {

LaserTrailItem::LaserTrailItem(uint8_t owner, const QColor &color, QGraphicsItem *parent)
	: QGraphicsItem(parent), m_blink(false), m_fadeout(false), m_owner(owner), m_lastModified(0)
{
	m_pen.setWidth(qApp->devicePixelRatio() * 3);
	m_pen.setCapStyle(Qt::RoundCap);
	m_pen.setCosmetic(true);
	m_pen.setColor(color);
}

QRectF LaserTrailItem::boundingRect() const
{
	return m_bounds;
}

void LaserTrailItem::addPoint(const QPointF &point)
{
	prepareGeometryChange();
	m_points.append(point);
	if(m_points.length()==1) {
		m_bounds = QRectF{point, QSizeF{1, 1}};
	} else {
		m_bounds = m_bounds.united(QRectF{point, QSizeF{1, 1}});
	}
	m_lastModified = QDateTime::currentMSecsSinceEpoch();
}

void LaserTrailItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);

	if(m_blink) {
		QPen pen = m_pen;
		pen.setWidth(pen.width() + 1);
		painter->setPen(pen);
	} else {
		painter->setPen(m_pen);
	}

	painter->drawPolyline(m_points.constData(), m_points.size());

	painter->restore();
}

bool LaserTrailItem::animationStep(float dt)
{
	m_blink = !m_blink;

	bool retain = true;
	if(m_fadeout) {
		setOpacity(qMax(0.0, opacity() - dt));
		retain = opacity() == 0.0;
	} else if(m_lastModified < QDateTime::currentMSecsSinceEpoch() - 3000) {
		m_fadeout = true;
	}

	update(boundingRect());

	return retain;
}

}
