/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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

#include "canvasitem.h"
#include "core/layerstack.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTimer>

namespace drawingboard {

/**
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
CanvasItem::CanvasItem(paintcore::LayerStack *layerstack, QGraphicsItem *parent)
	: QGraphicsObject(parent), m_image(layerstack)
{
	connect(m_image, &paintcore::LayerStack::areaChanged, this, &CanvasItem::refreshImage);
	connect(m_image, &paintcore::LayerStack::resized, this, &CanvasItem::canvasResize);

	m_refreshTimer = new QTimer(this);
	m_refreshTimer->setSingleShot(true);
	connect(m_refreshTimer, &QTimer::timeout, [this]() { refreshImage(QRect()); });
}

void CanvasItem::refreshImage(const QRect &area)
{
	m_refresh |= area;
	if(m_image->lock(5)) {
		if((m_cache.isNull() || m_cache.size() != m_image->size()) && m_image->size().isValid()) {
			m_cache = QPixmap(m_image->size());
			m_cache.fill();
		}

		m_image->paintChangedTiles(m_refresh, &m_cache, true);
		m_image->unlock();

		update(m_refresh.adjusted(-2, -2, 2, 2));
		m_refresh = QRect();

	} else if(!m_refreshTimer->isActive()) {
		// Couldn't get a lock: re-enter the eventloop and try again
		m_refreshTimer->start(10);
	}
}

QRectF CanvasItem::boundingRect() const
{
	return QRectF(0,0, m_image->width(), m_image->height());
}

void CanvasItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *)
{
	QRect exposed = option->exposedRect.adjusted(-1, -1, 1, 1).toAlignedRect();
	exposed &= m_cache.rect();

	painter->drawPixmap(exposed, m_cache, exposed);
}

void CanvasItem::canvasResize()
{
	prepareGeometryChange();
}

}

