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
	bool imageChanged = image.cacheKey() != m_image.cacheKey();
	bool offsetChanged = offset != m_offset;
	bool boundsChanged = offsetChanged || image.size() != m_image.size();

	if(imageChanged) {
		m_image = image;
	}

	if(offsetChanged) {
		m_offset = offset;
	}

	if(boundsChanged) {
		m_boundingRect = QRectF(QRect(offset, image.size()));
		refreshGeometry();
	} else if(imageChanged) {
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
