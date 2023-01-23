/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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

#include "desktop/scene/canvasitem.h"

#include "libclient/canvas/paintengine.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace drawingboard {

/**
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
CanvasItem::CanvasItem(QGraphicsItem *parent)
	: QGraphicsObject{parent}
	, m_image{nullptr}
	, m_boundingRect{}
	, m_viewportBounds{}
{
	setFlag(ItemUsesExtendedStyleOption);
}

void CanvasItem::setPaintEngine(canvas::PaintEngine *pe)
{
	m_image = pe;
	if(m_image) {
		connect(m_image, &canvas::PaintEngine::areaChanged, this, &CanvasItem::refreshImage);
		connect(m_image, &canvas::PaintEngine::resized, this, &CanvasItem::canvasResize);
	}
	canvasResize();
}

void CanvasItem::refreshImage(const QRect &area)
{
	update(area);
}

void CanvasItem::canvasResize()
{
	QRectF bounds = m_image ? QRectF(QPointF(), QSizeF(m_image->viewCanvasState().size())) : QRectF{};
	if(bounds != m_boundingRect) {
		m_boundingRect = bounds;
		updateVisibleArea();
		prepareGeometryChange();
	}
}

QRectF CanvasItem::boundingRect() const
{
	return m_boundingRect;
}

void CanvasItem::setViewportBounds(const QRectF viewportBounds)
{
	if(viewportBounds != m_viewportBounds) {
		m_viewportBounds = viewportBounds;
		updateVisibleArea();
	}
}

void CanvasItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *)
{
	if(m_image) {
		QRect exposed = option->exposedRect.toAlignedRect();
		painter->drawPixmap(exposed, m_image->getPixmapView(m_visibleArea), exposed);
	}
}

void CanvasItem::updateVisibleArea()
{
	m_visibleArea = mapRectFromScene(m_viewportBounds).intersected(m_boundingRect).toAlignedRect();
}

}

