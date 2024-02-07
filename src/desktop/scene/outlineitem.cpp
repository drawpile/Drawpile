// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/outlineitem.h"
#include <QPainter>

namespace drawingboard {

OutlineItem::OutlineItem(QGraphicsItem *parent)
	: QGraphicsItem(parent)
{
	setFlag(ItemIgnoresTransformations);
}

QRectF OutlineItem::boundingRect() const
{
	return m_outerBounds;
}

void OutlineItem::setOutline(qreal outlineSize, qreal outlineWidth)
{
	if(outlineSize != m_outlineSize || outlineWidth != m_outlineWidth) {
		m_outlineSize = outlineSize;
		m_outlineWidth = outlineWidth;
		qreal offset = m_outlineSize * -0.5;
		m_bounds = QRectF(offset, offset, outlineSize, outlineSize);
		updateVisibility();
	}
}

void OutlineItem::setSquare(bool square)
{
	if(square != m_square) {
		m_square = square;
		update();
	}
}

void OutlineItem::setVisibleInMode(bool visibleInMode)
{
	m_visibleInMode = visibleInMode;
	updateVisibility();
}

void OutlineItem::setOnCanvas(bool onCanvas)
{
	m_onCanvas = onCanvas;
	updateVisibility();
}

void OutlineItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	if(m_actuallyVisible) {
		painter->save();
		QPen pen(QColor(96, 191, 96));
		pen.setCosmetic(true);
		pen.setWidthF(m_outlineWidth * painter->device()->devicePixelRatioF());
		painter->setPen(pen);
		painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
		if(m_square) {
			painter->drawRect(m_bounds);
		} else {
			painter->drawEllipse(m_bounds);
		}
		painter->restore();
	}
}

void OutlineItem::updateVisibility()
{
	m_actuallyVisible = m_onCanvas && m_visibleInMode && m_outlineSize > 0.0 &&
						m_outlineWidth > 0.0 && m_bounds.isValid();
#ifdef Q_OS_WIN
	// On some Windows systems and with Windows Ink enabled, having the outline
	// not visible causes a rectangular region around the cursor to flicker.
	// Having the outline item visible and updating the scene gets rid of it.
	bool visible = true;
#else
	bool visible = m_actuallyVisible;
#endif
	setVisible(visible);
	QRectF outerBounds = visible ? m_bounds.marginsAdded(QMarginsF(
									   m_outlineWidth, m_outlineWidth,
									   m_outlineWidth, m_outlineWidth))
								 : QRectF();
	if(outerBounds != m_outerBounds) {
		prepareGeometryChange();
		m_outerBounds = outerBounds;
	}
}

}
