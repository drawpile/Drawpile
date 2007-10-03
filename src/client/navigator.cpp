/*
   Copyright (C) 2007 M.K.A.

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

#include <QGraphicsScene>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSize>
#include <QDebug>

/** @todo change viewportUpdateMode to manual updating after every pen-up */
NavigatorView::NavigatorView(QGraphicsScene *scene, QWidget *parent)
	: QGraphicsView(scene, parent), dragging_(false)
{
	rect_ = new QGraphicsRectItem();
	viewport()->setMouseTracking(true);
}

NavigatorView::~NavigatorView()
{
	delete rect_;
}

void NavigatorView::mousePressEvent(QMouseEvent *event)
{
	setFocus(mapToScene(event->pos()).toPoint());
	dragging_ = true;
}

void NavigatorView::mouseMoveEvent(QMouseEvent *event)
{
	if (dragging_)
		setFocus(mapToScene(event->pos()).toPoint());
}

void NavigatorView::mouseReleaseEvent(QMouseEvent *event)
{
	dragging_ = false;
}

void NavigatorView::setFocus(const QPoint& pt)
{
	qDebug() << "set Focus to" << pt;
	//emit focusMoved(QRect(foofoo));
}

Navigator::Navigator(QWidget *parent, QGraphicsScene *scene)
	: QDockWidget(tr("Navigator"), parent), view_(0), scene_(scene), layout_(0), delayed_(false)
{
	view_ = new NavigatorView(scene, this);
	
	view_->setInteractive(false); //?
	
	view_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
	view_->setAlignment(Qt::AlignCenter);
	
	view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	
	view_->setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing
		|QGraphicsView::DontSavePainterState);
	
	//delayedUpdate(false);
	
	if (scene_)
		setScene(scene_);
	
	layout_ = new NavigatorLayout(this, view_);
	
	setWidget(layout_);
}

Navigator::~Navigator()
{
	//delete view_; // ?
}

void Navigator::setRenderHint(QPainter::RenderHint hints)
{
	view_->setRenderHint(hints);
}

void Navigator::setScene(QGraphicsScene *scene)
{
	scene_ = scene;
	disconnect(this, SLOT(sceneResized()));
	connect(scene, SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(sceneResized()));
	view_->setScene(scene);
	rescale();
}

void Navigator::rescale()
{
	view_->resetTransform();
	
	QRectF ss = scene_->sceneRect();
	// we shouldn't need to use view_
	qreal x = qreal(view_->width()) / ss.width();
	qreal y = qreal(view_->height()-5) / ss.height();
	qreal min = (x < y ? x : y);
	
	view_->scale(min, min);
}

void Navigator::delayedUpdate(bool enable)
{
	if (enable)
	{
		//connect(emitter_, SIGNAL(penUp()), this, SLOT(update()));
		view_->setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
	}
	else
	{
		disconnect(this, SLOT(update()));
		view_->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
	}
}

void Navigator::update()
{
	qDebug() << "update";
}

void Navigator::setFocus(const QRect& focus)
{
	//view_->setFocus(*);
}

void Navigator::resizeEvent(QResizeEvent *event)
{
	rescale();
}

void Navigator::sceneResized()
{
	rescale();
}

void Navigator::catchZoomIn()
{
	emit zoomIn();
}

void Navigator::catchZoomOut()
{
	emit zoomOut();
}


NavigatorLayout::NavigatorLayout(QWidget *parent, NavigatorView *view)
	: QWidget(parent)
{
	QVBoxLayout *vbox = new QVBoxLayout(this);
	vbox->setContentsMargins(0,0,0,0);
	QHBoxLayout *hbox = new QHBoxLayout();
	
	/** @todo Replace with icons */
	QPushButton *zoomOutButton = new QPushButton("Zoom out", this);
	QPushButton *zoomInButton = new QPushButton("Zoom in", this);
	
	// not very nice, but works
	connect(zoomInButton, SIGNAL(released()), static_cast<Navigator*>(parent), SLOT(catchZoomIn()));
	connect(zoomOutButton, SIGNAL(released()), static_cast<Navigator*>(parent), SLOT(catchZoomOut()));
	
	hbox->addWidget(zoomOutButton);
	hbox->addWidget(zoomInButton);
	
	vbox->addWidget(view);
	vbox->addLayout(hbox);
	
	setLayout(vbox);
}
