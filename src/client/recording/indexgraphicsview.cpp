/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QDebug>
#include <QMouseEvent>

#include "recording/indexgraphicsview.h"
#include "recording/indexgraphicsitem.h"

IndexGraphicsView::IndexGraphicsView(QWidget *parent) :
	QGraphicsView(parent), _highlight(-1), _highlightbrush(QColor(230, 230, 230))
{
	viewport()->setMouseTracking(true);
}

void IndexGraphicsView::mouseMoveEvent(QMouseEvent *e)
{
	QPointF sp = mapToScene(e->pos());

	int index = sp.x() / IndexGraphicsItem::STEP_WIDTH;
	if(index != _highlight) {
		QList<QRectF> up;
		up << _highlightrect;

		int x = index * IndexGraphicsItem::STEP_WIDTH;

		_highlightrect = QRectF(x, -999, IndexGraphicsItem::STEP_WIDTH, 2*999);
		up << _highlightrect;

		updateScene(up);
		_highlight = index;
	}

	QGraphicsView::mouseMoveEvent(e);
}

void IndexGraphicsView::drawBackground(QPainter *painter, const QRectF &rect)
{
	QRectF r = rect.intersected(_highlightrect);
	if(!r.isEmpty())
		painter->fillRect(r, _highlightbrush);
}

void IndexGraphicsView::mouseDoubleClickEvent(QMouseEvent *e)
{
	int sceneX = mapToScene(e->pos()).x();
	int recordPos = sceneX / IndexGraphicsItem::STEP_WIDTH;
	if(recordPos >= 0) {
		emit jumpRequest(recordPos);
	}
}
