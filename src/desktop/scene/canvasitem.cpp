/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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

#include "core/layerstackpixmapcacheobserver.h"
#include "core/layerstack.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace drawingboard {

/**
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
CanvasItem::CanvasItem(paintcore::LayerStackPixmapCacheObserver *layerstack, QGraphicsItem *parent)
	: QGraphicsObject(parent), m_image(layerstack)
{
	connect(m_image, &paintcore::LayerStackPixmapCacheObserver::areaChanged, this, &CanvasItem::refreshImage);
	connect(m_image, &paintcore::LayerStackPixmapCacheObserver::resized, this, &CanvasItem::canvasResize);
	setFlag(ItemUsesExtendedStyleOption);
}

void CanvasItem::refreshImage(const QRect &area)
{
	update(area.adjusted(-2, -2, 2, 2));
}

void CanvasItem::canvasResize()
{
	prepareGeometryChange();
}

QRectF CanvasItem::boundingRect() const
{
	if(m_image->layerStack())
		return QRectF(0,0, m_image->layerStack()->width(), m_image->layerStack()->height());
	return QRectF();
}

void CanvasItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *)
{
	const QRect exposed = option->exposedRect.adjusted(-1, -1, 1, 1).toAlignedRect();
	painter->drawPixmap(exposed, m_image->getPixmap(exposed), exposed);
}

}

