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

#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include "canvasitem.h"

#include "core/layerstack.h"

namespace drawingboard {

/**
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
CanvasItem::CanvasItem(paintcore::LayerStack *layerstack, QGraphicsItem *parent)
	: QGraphicsObject(parent), m_image(layerstack)
{
	connect(m_image, SIGNAL(areaChanged(QRect)), this, SLOT(refreshImage(QRect)));
	connect(m_image, SIGNAL(resized(int, int, QSize)), this, SLOT(canvasResize()));
	setFlag(ItemUsesExtendedStyleOption);
}

void CanvasItem::refreshImage(const QRect &area)
{
	update(area.adjusted(-2, -2, 2, 2));
}

QRectF CanvasItem::boundingRect() const
{
	return QRectF(0,0, m_image->width(), m_image->height());
}

void CanvasItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *)
{
	if((_cache.isNull() || _cache.size() != m_image->size()) && m_image->size().isValid()) {
		_cache = QPixmap(m_image->size());
		_cache.fill();
	}

	QRect exposed = option->exposedRect.adjusted(-1, -1, 1, 1).toAlignedRect();
	exposed &= _cache.rect();

	m_image->paintChangedTiles(exposed, &_cache, true);

	painter->drawPixmap(exposed, _cache, exposed);
}

void CanvasItem::canvasResize()
{
	prepareGeometryChange();
}

}

