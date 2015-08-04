/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#include <QPainter>
#include <QApplication>

namespace drawingboard {

SelectionItem::SelectionItem(canvas::Selection *selection, QGraphicsItem *parent)
	: QGraphicsObject(parent), m_selection(selection), m_marchingants(0)
{
	Q_ASSERT(selection);
	connect(selection, &canvas::Selection::shapeChanged, this, &SelectionItem::onShapeChanged);
	connect(selection, &canvas::Selection::pasteImageChanged, this, &SelectionItem::onShapeChanged);
	connect(selection, &canvas::Selection::closed, this, &SelectionItem::onShapeChanged);
	m_shape = m_selection->shape();
}

void SelectionItem::onShapeChanged()
{
	prepareGeometryChange();
	m_shape = m_selection->shape();
}

QRectF SelectionItem::boundingRect() const
{
	const int h = 5;
	return m_shape.boundingRect().adjusted(-h, -h, h, h);
}

void SelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
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
	pen.setColor(Qt::white);
	pen.setStyle(Qt::DashLine);
	pen.setDashOffset(m_marchingants);
	pen.setCosmetic(true);
	painter->setPen(pen);
	painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);

	if(m_selection->isClosed())
		painter->drawPolygon(m_shape);
	else
		painter->drawPolyline(m_shape);

	// Draw resizing handles
	if(m_selection->isClosed()) {
		QRect rect = m_selection->boundingRect();

		pen.setWidth(m_selection->handleSize());
		pen.setStyle(Qt::SolidLine);
		painter->setPen(pen);
		painter->drawPoint(rect.topLeft());
		painter->drawPoint(rect.topLeft() + QPointF(rect.width()/2, 0));
		painter->drawPoint(rect.topRight());

		painter->drawPoint(rect.topLeft() + QPointF(0, rect.height()/2));
		painter->drawPoint(rect.topRight() + QPointF(0, rect.height()/2));

		painter->drawPoint(rect.bottomLeft());
		painter->drawPoint(rect.bottomLeft() + QPointF(rect.width()/2, 0));
		painter->drawPoint(rect.bottomRight());
	}
}

void SelectionItem::marchingAnts()
{
	m_marchingants += 1;
	update();
}

}
