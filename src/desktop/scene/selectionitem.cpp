/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#include "selectionitem.h"
#include "arrows_data.h"

#include <QPainter>
#include <QApplication>
#include <QStyleOptionGraphicsItem>

namespace drawingboard {

SelectionItem::SelectionItem(canvas::Selection *selection, QGraphicsItem *parent)
	: QGraphicsObject(parent), m_selection(selection), m_marchingants(0)
{
	Q_ASSERT(selection);
	connect(selection, &canvas::Selection::shapeChanged, this, &SelectionItem::onShapeChanged);
	connect(selection, &canvas::Selection::pasteImageChanged, this, &SelectionItem::onShapeChanged);
	connect(selection, &canvas::Selection::closed, this, &SelectionItem::onShapeChanged);
	connect(selection, &canvas::Selection::adjustmentModeChanged, this, &SelectionItem::onAdjustmentModeChanged);
	m_shape = m_selection->shape();
}

void SelectionItem::onShapeChanged()
{
	prepareGeometryChange();
	m_shape = m_selection->shape();
}

void SelectionItem::onAdjustmentModeChanged()
{
	update();
}

QRectF SelectionItem::boundingRect() const
{
	const int h = m_selection ? m_selection->handleSize() : 0;
	return m_shape.boundingRect().adjusted(-h, -h, h, h);
}

static inline void drawPolygon(QPainter *painter, const QPolygonF &polygon, bool closed)
{
	if(closed)
		painter->drawPolygon(polygon);
	else
		painter->drawPolyline(polygon);
}

static void drawHandle(QPainter *painter, const QPointF &point, qreal size, canvas::Selection::Handle handle, bool scaleMode)
{
	const QPointF *arrow;
	unsigned int plen=0;

	if(scaleMode) {
		switch(handle) {
			case canvas::Selection::Handle::TopLeft:
			case canvas::Selection::Handle::BottomRight:
				arrow = arrows::diag1; plen = sizeof(arrows::diag1);
				break;
			case canvas::Selection::Handle::TopRight:
			case canvas::Selection::Handle::BottomLeft:
				arrow = arrows::diag2; plen = sizeof(arrows::diag2);
				break;
			case canvas::Selection::Handle::Top:
			case canvas::Selection::Handle::Bottom:
				arrow = arrows::vertical; plen = sizeof(arrows::vertical);
				break;
			case canvas::Selection::Handle::Left:
			case canvas::Selection::Handle::Right:
				arrow = arrows::horizontal; plen = sizeof(arrows::horizontal);
				break;
			default: return;
		}
	} else {
		switch(handle) {
			case canvas::Selection::Handle::TopLeft:
				arrow = arrows::rotate1; plen = sizeof(arrows::rotate1);
				break;
			case canvas::Selection::Handle::TopRight:
				arrow = arrows::rotate2; plen = sizeof(arrows::rotate2);
				break;
			case canvas::Selection::Handle::BottomRight:
				arrow = arrows::rotate3; plen = sizeof(arrows::rotate3);
				break;
			case canvas::Selection::Handle::BottomLeft:
				arrow = arrows::rotate4; plen = sizeof(arrows::rotate4);
				break;
			case canvas::Selection::Handle::Left:
			case canvas::Selection::Handle::Right:
				arrow = arrows::verticalSkew; plen = sizeof(arrows::verticalSkew);
				break;
			case canvas::Selection::Handle::Top:
			case canvas::Selection::Handle::Bottom:
				arrow = arrows::horizontalSkew; plen = sizeof(arrows::horizontalSkew);
				break;
			default: return;
		}
	}

	const QPointF offset = point - QPointF(size/2, size/2);

	QPointF polygon[12];
	Q_ASSERT(plen <= sizeof(polygon)/sizeof(*polygon));
	for(unsigned int i=0;i<plen/sizeof(*polygon);++i)
		polygon[i] = offset + arrow[i] / 10 * size;

	painter->drawPolygon(polygon, plen/sizeof(*polygon));
}

void SelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *opt, QWidget *)
{
	if(!m_selection->pasteImage().isNull()) {
		if(m_shape.size() == 4) {
			QPolygonF src({
				QPointF(0, 0),
				QPointF(m_selection->pasteImage().width(), 0),
				QPointF(m_selection->pasteImage().width(), m_selection->pasteImage().height()),
				QPointF(0, m_selection->pasteImage().height())
			});

			QTransform t;
			if(QTransform::quadToQuad(src, m_shape, t)) {
				painter->save();
				painter->setTransform(t, true);
				painter->drawImage(0, 0, m_selection->pasteImage());
				painter->restore();
			} else
				qWarning("Couldn't transform pasted image!");

		} else {
			qWarning("Pasted selection item with non-rectangular polygon!");
		}
	}

	painter->setClipRect(boundingRect().adjusted(-1, -1, 1, 1));

	QPen pen;
	pen.setWidth(qApp->devicePixelRatio());
	pen.setCosmetic(true);

	// Rectangular selections should always be *drawn* as closed
	const bool drawClosed = m_selection->isClosed() || m_shape.size() == 4;

	// Draw base color
	pen.setColor(Qt::black);
	painter->setPen(pen);
	drawPolygon(painter, m_shape, drawClosed);

	// Draw dashes
	pen.setColor(Qt::white);
	pen.setStyle(Qt::DashLine);
	pen.setDashOffset(m_marchingants);
	painter->setPen(pen);
	drawPolygon(painter, m_shape, drawClosed);

	// Draw resizing handles when initial drawing is finished
	if(m_selection->isClosed()) {
		const QRect rect = m_selection->boundingRect();

		pen.setStyle(Qt::SolidLine);
		painter->setPen(pen);
		painter->setBrush(Qt::black);

		const qreal  s = m_selection->handleSize() / opt->levelOfDetailFromTransform(painter->transform());
		const bool am = m_selection->adjustmentMode() == canvas::Selection::AdjustmentMode::Scale;

		drawHandle(painter, rect.topLeft(), s, canvas::Selection::Handle::TopLeft, am);
		drawHandle(painter, rect.topLeft() + QPointF(rect.width()/2, 0), s, canvas::Selection::Handle::Top, am);
		drawHandle(painter, rect.topRight(), s, canvas::Selection::Handle::TopRight, am);

		drawHandle(painter, rect.topLeft() + QPointF(0, rect.height()/2), s, canvas::Selection::Handle::Left, am);
		drawHandle(painter, rect.topRight() + QPointF(0, rect.height()/2), s, canvas::Selection::Handle::Right, am);

		drawHandle(painter, rect.bottomLeft(), s, canvas::Selection::Handle::BottomLeft, am);
		drawHandle(painter, rect.bottomLeft() + QPointF(rect.width()/2, 0), s, canvas::Selection::Handle::Bottom, am);
		drawHandle(painter, rect.bottomRight(), s, canvas::Selection::Handle::BottomRight, am);
	}
}

void SelectionItem::marchingAnts()
{
	m_marchingants += 1;
	update();
}

}
