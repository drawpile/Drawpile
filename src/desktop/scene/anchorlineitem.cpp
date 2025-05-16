// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/anchorlineitem.h"
#include "libclient/tools/gradient.h"
#include <QLineF>
#include <QMarginsF>
#include <QPaintEngine>
#include <QPainter>

namespace drawingboard {

AnchorLineItem::AnchorLineItem(
	const QVector<QPointF> &points, int activeIndex, qreal zoom,
	QGraphicsItem *parent)
	: BaseItem(parent)
	, m_points(points)
	, m_zoom(zoom)
	, m_activeIndex(activeIndex)
{
	updatePath();
}

QRectF AnchorLineItem::boundingRect() const
{
	qreal m = m_zoom > 0.0 ? 2.0 / m_zoom : 2.0;
	return m_path.boundingRect().marginsAdded(QMarginsF(m, m, m, m));
}

void AnchorLineItem::setPoints(const QVector<QPointF> &points, int activeIndex)
{
	m_points = points;
	m_activeIndex = activeIndex;
	updatePath();
}

void AnchorLineItem::setActiveIndex(int activeIndex)
{
	if(activeIndex != m_activeIndex) {
		m_activeIndex = activeIndex;
		refresh();
	}
}

void AnchorLineItem::setZoom(qreal zoom)
{
	if(zoom != m_zoom) {
		m_zoom = zoom;
		updatePath();
	}
}

void AnchorLineItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);
	painter->setRenderHint(QPainter::Antialiasing, true);

	if(m_activeIndex >= 0 && m_activeIndex < m_points.size()) {
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor(255, 255, 255, 200));
		qreal radius = calculateHandleRadius();
		painter->drawEllipse(m_points[m_activeIndex], radius, radius);
		painter->setBrush(Qt::NoBrush);
	}

	QPen pen(Qt::darkGray);
	pen.setCosmetic(true);
	qreal dpr = painter->device()->devicePixelRatioF();
	pen.setWidth(dpr * 3.0);
	painter->setPen(pen);
	painter->setOpacity(0.5);
	painter->drawPath(m_path);

	pen.setColor(Qt::white);
	pen.setWidth(dpr);
	painter->setPen(pen);
	painter->setOpacity(1.0);
	painter->drawPath(m_path);
}

void AnchorLineItem::updatePath()
{
	refreshGeometry();
	m_path.clear();
	qreal radius = calculateHandleRadius();

	int pointCount = m_points.size();
	for(int i = 0; i < pointCount - 1; ++i) {
		QLineF line(m_points[i], m_points[i + 1]);
		if(line.length() > radius * 2.0) {
			qreal angle = line.angle();
			m_path.moveTo(line.p1() + QLineF::fromPolar(radius, angle).p2());
			m_path.lineTo(line.p2() + QLineF::fromPolar(-radius, angle).p2());
		}
	}

	for(const QPointF &point : m_points) {
		m_path.addEllipse(point, radius, radius);
	}
}

qreal AnchorLineItem::calculateHandleRadius() const
{
	return m_zoom > 0.0 ? tools::GradientTool::HANDLE_RADIUS / m_zoom : 0.0;
}

}
