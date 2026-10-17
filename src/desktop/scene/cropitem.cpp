// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/cropitem.h"
#include "libclient/view/enums.h"
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

namespace drawingboard {

CropItem::CropItem(QRect rect, int guides, qreal zoom, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_rect(rect)
	, m_guides(guides)
	, m_handleSize(calculateHandleSize(zoom))
	, m_boundingRect(calculateBoundingRect())
{
}

void CropItem::setRect(QRect rect)
{
	if(rect != m_rect) {
		refreshGeometry();
		m_rect = rect;
		m_boundingRect = calculateBoundingRect();
		m_path.clear();
	}
}

void CropItem::setGuides(int guides)
{
	if(guides != m_guides) {
		refresh();
		m_guides = guides;
		m_path.clear();
	}
}

void CropItem::setZoom(qreal zoom)
{
	int handleSize = calculateHandleSize(zoom);
	if(handleSize != m_handleSize) {
		refreshGeometry();
		m_handleSize = handleSize;
		m_boundingRect = calculateBoundingRect();
	}
}

int CropItem::defaultGuides()
{
	return int(view::CropGuides::None);
}

void CropItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);
	if(!m_rect.isNull()) {
		painter->setRenderHint(QPainter::Antialiasing, true);

		if(m_path.isEmpty()) {
			m_path.addRect(m_rect);
			switch(m_guides) {
			case int(view::CropGuides::Thirds):
				addGuidesNths(m_path, 3);
				break;
			case int(view::CropGuides::Fifths):
				addGuidesNths(m_path, 5);
				break;
			case int(view::CropGuides::Plus):
				addGuidesNths(m_path, 2);
				break;
			default:
				break;
			}
		}

		QPen pen(Qt::darkRed);
		pen.setCosmetic(true);
		qreal dpr = painter->device()->devicePixelRatioF();
		pen.setWidth(dpr * 3.0);
		painter->setPen(pen);
		painter->setOpacity(0.5);
		painter->drawPath(m_path);

		pen.setColor(Qt::white);
		pen.setWidth(dpr);
		painter->setPen(pen);
		painter->setOpacity(1.0);
		painter->drawPath(m_path);
	}
}

int CropItem::calculateHandleSize(qreal zoom) const
{
	return zoom > 0.0 ? qRound(4.0 / zoom) : 0;
}

QRectF CropItem::calculateBoundingRect() const
{
	return QRectF(m_rect).marginsAdded(
		QMarginsF(m_handleSize, m_handleSize, m_handleSize, m_handleSize));
}

void CropItem::addGuidesNths(QPainterPath &path, int n)
{
	// Right and bottom add one because the line they attach to is at the
	// bottom-right of the pixel, which is the top-left of the next one.
	qreal left = m_rect.left();
	qreal top = m_rect.top();
	qreal right = m_rect.right() + 1;
	qreal bottom = m_rect.bottom() + 1;

	QLineF topLine(QPointF(left, top), QPointF(right, top));
	QLineF leftLine(QPointF(left, top), QPointF(left, bottom));
	QLineF bottomLine(QPointF(left, bottom), QPointF(right, bottom));
	QLineF rightLine(QPointF(right, top), QPointF(right, bottom));

	for(int i = 1; i < n; ++i) {
		qreal t = qreal(i) / qreal(n);
		path.moveTo(topLine.pointAt(t));
		path.lineTo(bottomLine.pointAt(t));
		path.moveTo(leftLine.pointAt(t));
		path.lineTo(rightLine.pointAt(t));
	}
}

}
