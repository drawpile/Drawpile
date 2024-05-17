// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/pathpreviewitem.h"
#include <QMarginsF>
#include <QPaintEngine>
#include <QPainter>

namespace drawingboard {

PathPreviewItem::PathPreviewItem(
	const QPainterPath &path, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_path(path)
{
}

QRectF PathPreviewItem::boundingRect() const
{
	return m_path.boundingRect().marginsAdded(QMarginsF(1.0, 1.0, 1.0, 1.0));
}

void PathPreviewItem::setPath(const QPainterPath &path)
{
	refreshGeometry();
	m_path = path;
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
