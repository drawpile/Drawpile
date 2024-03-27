// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/scene/lasertrailitem.h"

#include <QApplication>
#include <QDateTime>
#include <QPainter>
#include <cmath>

namespace drawingboard {

LaserTrailItem::LaserTrailItem(
	int owner, int persistenceMs, const QColor &color, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_owner(owner)
	, m_lastModified(QDateTime::currentMSecsSinceEpoch())
	, m_persistence(persistenceMs)
{
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
	refreshGeometry();
	m_points.append(point);
	if(m_points.length() == 1) {
		m_bounds = QRectF{point, QSizeF{1, 1}};
	} else {
		m_bounds = m_bounds.united(QRectF{point, QSizeF{1, 1}});
	}
	m_lastModified = QDateTime::currentMSecsSinceEpoch();
}

void LaserTrailItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->setRenderHint(QPainter::Antialiasing, true);
	m_pen.setWidth(
		painter->device()->devicePixelRatioF() *
		(m_blink < ANIM_TIME / 2.0f ? 4.0 : 3.0));
	painter->setPen(m_pen);
	painter->drawPolyline(m_points.constData(), m_points.size());
}

bool LaserTrailItem::animationStep(qreal dt)
{
	m_blink = std::fmod(m_blink + dt, ANIM_TIME);

	bool retain = true;
	if(m_fadeout) {
		setOpacity(qMax(0.0, opacity() - dt));
		retain = opacity() > 0.0;
	} else if(
		m_lastModified < QDateTime::currentMSecsSinceEpoch() - m_persistence) {
		m_fadeout = true;
	}

	refresh();

	return retain;
}

}
