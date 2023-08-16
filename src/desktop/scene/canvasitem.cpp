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
	, m_pixelGrid{false}
{
	setFlag(ItemUsesExtendedStyleOption);
}

void CanvasItem::setPaintEngine(canvas::PaintEngine *pe)
{
	m_image = pe;
	if(m_image) {
		connect(m_image, &canvas::PaintEngine::areaChanged, this, &CanvasItem::refreshImage, Qt::QueuedConnection);
		connect(m_image, &canvas::PaintEngine::resized, this, &CanvasItem::canvasResize, Qt::QueuedConnection);
		m_image->setCanvasViewArea(m_visibleArea);
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

void CanvasItem::setPixelGrid(bool pixelGrid)
{
	if(pixelGrid != m_pixelGrid) {
		m_pixelGrid = pixelGrid;
		update(m_visibleArea);
	}
}

void CanvasItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *)
{
	if(m_image) {
		QRect exposed = option->exposedRect.toAlignedRect();
		m_image->withPixmap([&](const QPixmap &pixmap) {
			painter->drawPixmap(exposed, pixmap, exposed);
		});
		if(m_pixelGrid) {
			QPen pen(QColor(160, 160, 160));
			pen.setCosmetic(true);
			painter->setPen(pen);
			for(int x = exposed.left(); x <= exposed.right(); ++x) {
				painter->drawLine(x, exposed.top(), x, exposed.bottom() + 1);
			}
			for(int y = exposed.top(); y <= exposed.bottom(); ++y) {
				painter->drawLine(exposed.left(), y, exposed.right() + 1, y);
			}
		}
	}
}

void CanvasItem::updateVisibleArea()
{
	m_visibleArea = m_viewportBounds.intersected(m_boundingRect).toAlignedRect();
	if(m_image) {
		m_image->setCanvasViewArea(m_visibleArea);
	}
}

}

