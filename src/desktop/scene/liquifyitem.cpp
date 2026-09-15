// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/liquifyitem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace drawingboard {

LiquifyItem::LiquifyItem(QGraphicsItem *parent)
	: BaseItem(parent)
{
}

void LiquifyItem::setImage(const QImage &image, QPoint offset)
{
	if(image.cacheKey() != m_image.cacheKey() || offset != m_offset) {
		m_image = image;
		m_offset = offset;
		m_boundingRect = QRectF(QRect(offset, image.size()));
		refresh();
	}
}

void LiquifyItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);
	if(!m_image.isNull() && !m_boundingRect.isEmpty()) {
		painter->setRenderHint(QPainter::Antialiasing, false);
		painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
		painter->drawImage(m_boundingRect, m_image);
	}
}

}
