/*
   Copyright (C) 2008-2014 Calle Laakkonen, 2007 M.K.A.

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

#include "navigator.h"
#include "docks/utils.h"

#include <QMouseEvent>
#include <QWindow>

namespace docks {

NavigatorView::NavigatorView(QWidget *parent)
	: QGraphicsView(parent), _zoomWheelDelta(0), _dragging(false)
{
	viewport()->setMouseTracking(true);
	setInteractive(false);

	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	setAlignment(Qt::AlignCenter);
	
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	
	setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);

	setBackgroundBrush(QColor(100, 100, 100));
}


void NavigatorView::resizeEvent(QResizeEvent *event)
{
	QGraphicsView::resizeEvent(event);
	rescale();
}

/**
 * Start dragging the view focus
 */
void NavigatorView::mousePressEvent(QMouseEvent *event)
{
	emit focusMoved(mapToScene(event->pos()).toPoint());
	_dragging = true;
}

/**
 * Drag the view focus
 */
void NavigatorView::mouseMoveEvent(QMouseEvent *event)
{
	if (_dragging)
		emit focusMoved(mapToScene(event->pos()).toPoint());
}

/**
 * Stop dragging the view focus
 */
void NavigatorView::mouseReleaseEvent(QMouseEvent *event)
{
	Q_UNUSED(event);
	_dragging = false;
}

void NavigatorView::wheelEvent(QWheelEvent *event)
{
	// Use scroll wheel for zooming
	_zoomWheelDelta += event->angleDelta().y();
	int steps=_zoomWheelDelta / 120;
	_zoomWheelDelta -= steps * 120;

	if(steps != 0) {
		emit wheelZoom(steps);
	}
}

/**
 * The focus rectangle represents the visible area in the
 * main viewport.
 */
void NavigatorView::setViewFocus(const QPolygonF& rect)
{
	QRegion up;

	up |= mapFromScene(rect).boundingRect().adjusted(-1,-1,1,1);
	up |= mapFromScene(_focusrect).boundingRect().adjusted(-1,-1,1,1);
	
	_focusrect = rect;
	
	viewport()->update(up);
}

/**
 * Reset the navigator view scale
 */
void NavigatorView::rescale()
{
	resetTransform();

	const qreal padding = 300; // ignore scene padding
	QRectF ss = scene()->sceneRect().adjusted(padding, padding, -padding, -padding);

	qreal x = qreal(width()) / ss.width();
	qreal y = qreal(height()-5) / ss.height();
	qreal min = qMin(x, y);

	scale(min, min);
	centerOn(scene()->sceneRect().center());
	viewport()->update();
}

void NavigatorView::drawForeground(QPainter *painter, const QRectF& rect)
{
	Q_UNUSED(rect);
	QPen pen(QColor(96, 191, 96));
	pen.setCosmetic(true);
	pen.setWidth(2);
	painter->setPen(pen);
	painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
	painter->drawPolygon(_focusrect);
}

/**
 * Construct the navigator dock widget.
 */
Navigator::Navigator(QWidget *parent)
	: QDockWidget(tr("Navigator"), parent)
{
	setObjectName("navigatordock");

	setStyleSheet(defaultDockStylesheet());

	_view = new NavigatorView(this);
	setWidget(_view);

	connect(_view, SIGNAL(focusMoved(const QPoint&)),
			this, SIGNAL(focusMoved(const QPoint&)));

	connect(_view, SIGNAL(wheelZoom(int)), this, SIGNAL(wheelZoom(int)));
}

void Navigator::setScene(QGraphicsScene *scene)
{
	connect(scene, SIGNAL(sceneRectChanged(const QRectF&)), _view, SLOT(rescale()));
	_view->setScene(scene);
	_view->rescale();
}

void Navigator::setViewFocus(const QPolygonF& rect)
{
	_view->setViewFocus(rect);
}

}

