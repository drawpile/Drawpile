// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/cursoritem.h"
#include <QPainter>

namespace drawingboard {

CursorItem::CursorItem(QGraphicsItem *parent)
	: QGraphicsItem(parent)
{
	setFlag(ItemIgnoresTransformations);
	setZValue(999999);
	updateVisibility();
}

QRectF CursorItem::boundingRect() const
{
	return m_bounds;
}

void CursorItem::setCursor(const QCursor &cursor)
{
	if(cursor.shape() == Qt::BitmapCursor) {
		prepareGeometryChange();
		m_cursor = cursor;
		m_bounds = QRectF(-m_cursor.hotSpot(), QSizeF(cursor.pixmap().size()));
	} else {
		m_cursor = QCursor();
	}
	updateVisibility();
}

void CursorItem::setOnCanvas(bool onCanvas)
{
	m_onCanvas = onCanvas;
	updateVisibility();
}

void CursorItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->drawPixmap(m_bounds.topLeft(), m_cursor.pixmap());
}

void CursorItem::updateVisibility()
{
	setVisible(m_onCanvas && m_cursor.shape() == Qt::BitmapCursor);
}

}
