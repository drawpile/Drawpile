// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/maskpreviewitem.h"
#include <QMarginsF>
#include <QPaintEngine>
#include <QPainter>

namespace drawingboard {

MaskPreviewItem::MaskPreviewItem(const QImage &mask, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_mask(mask)
{
}

QRectF MaskPreviewItem::boundingRect() const
{
	return QRectF(m_mask.rect());
}

void MaskPreviewItem::setMask(const QImage &mask)
{
	refreshGeometry();
	m_mask = mask;
}

void MaskPreviewItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);
	painter->setOpacity(0.5);
	painter->drawImage(0, 0, m_mask);
}

}
