/*
   Copyright (C) 2008 Calle Laakkonen, 2007 M.K.A.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "navigator.h"
using widgets::NavigatorView; // to satisfy ui_navibox
#include "ui_navibox.h"

#include <QMouseEvent>
#include <QGraphicsScene>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSize>
#include <QDebug>

namespace widgets {

/** @todo change viewportUpdateMode to manual updating after every pen-up */
NavigatorView::NavigatorView(QWidget *parent)
	: QGraphicsView(parent), dragging_(false)
{
	viewport()->setMouseTracking(true);
	setInteractive(false);
	
	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	setAlignment(Qt::AlignCenter);
	
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	
	setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing
		|QGraphicsView::DontSavePainterState);
}

/**
 * Start dragging the view focus
 */
void NavigatorView::mousePressEvent(QMouseEvent *event)
{
	emit focusMoved(mapToScene(event->pos()).toPoint());
	dragging_ = true;
}

/**
 * Drag the view focus
 */
void NavigatorView::mouseMoveEvent(QMouseEvent *event)
{
	if (dragging_)
		emit focusMoved(mapToScene(event->pos()).toPoint());
}

/**
 * Stop dragging the view focus
 */
void NavigatorView::mouseReleaseEvent(QMouseEvent *event)
{
	dragging_ = false;
}

/**
 * The focus rectangle represents the visible area in the
 * main viewport.
 */
void NavigatorView::setFocus(const QRectF& rect)
{
	QList<QRectF> up;
	up.append(rect.adjusted(-5,-5,5,5));
	up.append(focusRect_.adjusted(-5,-5,5,5));
	
	focusRect_ = rect;
	
	// update whole navigator
	updateScene(up);
}

/**
 * Reset the navigator view scale
 */
void NavigatorView::rescale()
{
	resetTransform();
	
	QRectF ss = scene()->sceneRect();
	// we shouldn't need to use view_
	qreal x = qreal(width()) / ss.width();
	qreal y = qreal(height()-5) / ss.height();
	qreal min = (x < y ? x : y);
	
	scale(min, min);
}

/**
 * @todo the color should be user defined
 * @todo rectangle line style should be user defined as well
 */
void NavigatorView::drawForeground(QPainter *painter, const QRectF& rect)
{
	QPen pen(Qt::black);
	pen.setStyle(Qt::DashLine);
	painter->setPen(pen);
	painter->drawRect(focusRect_);
}

/**
 * Construct the navigator dock widget.
 */
Navigator::Navigator(QWidget *parent, QGraphicsScene *scene)
	: QDockWidget(tr("Navigator"), parent)
{
	setObjectName("navigatordock");
	
	QWidget *w = new QWidget(this);
	ui_ = new Ui_NaviBox();
	ui_->setupUi(w);

	connect(ui_->view, SIGNAL(focusMoved(const QPoint&)),
			this, SIGNAL(focusMoved(const QPoint&)));

	setScene(scene);
	
	connect(ui_->zoomin, SIGNAL(pressed()), this, SIGNAL(zoomIn()));
	connect(ui_->zoomout, SIGNAL(pressed()), this, SIGNAL(zoomOut()));

	setWidget(w);
}

Navigator::~Navigator()
{
	delete ui_;
}

void Navigator::setScene(QGraphicsScene *scene)
{
	if (scene)
	{
		connect(scene, SIGNAL(sceneRectChanged(const QRectF&)), ui_->view, SLOT(rescale()));
		ui_->view->setScene(scene);
		ui_->view->rescale();
	}
	else
	{
		disconnect(ui_->view, SLOT(rescale()));
		ui_->view->setScene(scene);
	}
}

void Navigator::resizeEvent(QResizeEvent *event)
{
	ui_->view->rescale();
}

void Navigator::setViewFocus(const QRectF& rect)
{
	ui_->view->setFocus(rect);
}

}

