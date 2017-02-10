/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

namespace drawingboard {

LaserTrailItem::LaserTrailItem(QGraphicsItem *parent)
	: QGraphicsItem(parent), _blink(false)
{
	_pen1.setWidth(qApp->devicePixelRatio() * 3);
	_pen2.setWidth(_pen1.width());
}

void LaserTrailItem::setColor(const QColor &color)
{
	_pen1.setCosmetic(true);
	_pen1.setColor(color.lighter(110));

	_pen2 = _pen1;
	_pen2.setColor(color.lighter(90));
}

QRectF LaserTrailItem::boundingRect() const
{
	return m_bounds;
}

void LaserTrailItem::setPoints(const QVector<QPointF> &points)
{
	prepareGeometryChange();
	m_points = points;

	QRectF bounds;
	if(m_points.size()>0) {
		bounds = QRectF(m_points.at(0), QSize(1,1));
		for(int i=1;i<m_points.size();++i) {
			qreal x = m_points.at(i).x();
			qreal y = m_points.at(i).y();

			if(x<bounds.left())
				bounds.setLeft(x);
			if(x>bounds.right())
				bounds.setRight(x);
			if(y<bounds.top())
				bounds.setTop(y);
			if(y>bounds.bottom())
				bounds.setBottom(y);
		}
		m_bounds = bounds;
	}
}

void LaserTrailItem::setFadeVisible(bool visible)
{
	// Visibility change is animated
	m_visible = visible;
}

void LaserTrailItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setPen(_blink ? _pen1 : _pen2);
	painter->drawPolyline(m_points.constData(), m_points.size());
	painter->restore();
}

void LaserTrailItem::animationStep(float dt)
{
	_blink = !_blink;

	if(m_visible) {
		if(opacity() < 1.0)
			setOpacity(qMin(1.0, opacity() + dt));
	} else {
		if(opacity() > 0)
			setOpacity(qMax(0.0, opacity() - dt));
	}

	update(boundingRect());
}

}
