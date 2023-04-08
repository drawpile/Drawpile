// SPDX-License-Identifier: GPL-3.0-or-later

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

