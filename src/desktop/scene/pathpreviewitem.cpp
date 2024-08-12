// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/pathpreviewitem.h"
#include <QMarginsF>
#include <QPaintEngine>
#include <QPainter>
#include <cmath>

namespace drawingboard {

PathPreviewItem::PathPreviewItem(
	const QPainterPath &path, qreal zoom, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_path(path)
	, m_zoom(zoom)
{
}

QRectF PathPreviewItem::boundingRect() const
{
	qreal m = m_zoom > 0.0 ? std::ceil(1.0 / m_zoom) : 1.0;
	return m_path.boundingRect().marginsAdded(QMarginsF(m, m, m, m));
}

void PathPreviewItem::setPath(const QPainterPath &path)
{
	refreshGeometry();
	m_path = path;
}

void PathPreviewItem::setZoom(qreal zoom)
{
	if(zoom != m_zoom) {
		refreshGeometry();
		m_zoom = zoom;
	}
}

void PathPreviewItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);

	QPen pen;
	pen.setWidth(painter->device()->devicePixelRatioF());
	pen.setCosmetic(true);
	pen.setColor(Qt::black);
	painter->setPen(pen);
	painter->drawPath(m_path);

	pen.setColor(Qt::white);
	pen.setDashPattern({4.0, 4.0});
	painter->setPen(pen);
	painter->drawPath(m_path);
}

}
