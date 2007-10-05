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
	viewport()->setMouseTracking(true);
}

void NavigatorView::mousePressEvent(QMouseEvent *event)
{
	moveFocus(mapToScene(event->pos()).toPoint());
	dragging_ = true;
}

void NavigatorView::mouseMoveEvent(QMouseEvent *event)
{
	if (dragging_)
		moveFocus(mapToScene(event->pos()).toPoint());
}

void NavigatorView::mouseReleaseEvent(QMouseEvent *event)
{
	dragging_ = false;
}

void NavigatorView::moveFocus(const QPoint& pt)
{
	emit focusMoved(pt.x(), pt.y());
}

void NavigatorView::setFocus(const QRectF& rect)
{
	QList<QRectF> up;
	up.append(rect.adjusted(-5,-5,5,5));
	up.append(rect_.adjusted(-5,-5,5,5));
	
	rect_ = rect;
	
	// update whole navigator
	updateScene(up);
}

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
	painter->drawRect(rect_);
}

Navigator::Navigator(QWidget *parent, QGraphicsScene *scene)
	: QDockWidget(tr("Navigator"), parent), view_(0), scene_(scene), layout_(0)
{
	view_ = new NavigatorView(scene, this);
	
	view_->setInteractive(false); //?
	
	view_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
	view_->setAlignment(Qt::AlignCenter);
	
	view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	
	view_->setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing
		|QGraphicsView::DontSavePainterState);
	
	if (scene_)
		setScene(scene_);
	
	layout_ = new NavigatorLayout(this, view_);
	
	setWidget(layout_);
}

Navigator::~Navigator()
{
	//delete view_; // ?
}

NavigatorView* Navigator::getView()
{
	return view_;
}

NavigatorLayout* Navigator::getLayout()
{
	return layout_;
}

void Navigator::setScene(QGraphicsScene *scene)
{
	scene_ = scene;
	//disconnect(view_, SLOT(rescale()));
	connect(scene, SIGNAL(sceneRectChanged(const QRectF&)), view_, SLOT(rescale()));
	view_->setScene(scene);
	view_->rescale();
}

void Navigator::resizeEvent(QResizeEvent *event)
{
	view_->rescale();
}

NavigatorLayout::NavigatorLayout(QWidget *parent, NavigatorView *view)
	: QWidget(parent), m_zoomInButton(0), m_zoomOutButton(0)
{
	QVBoxLayout *vbox = new QVBoxLayout(this);
	vbox->setContentsMargins(0,0,0,0);
	QHBoxLayout *hbox = new QHBoxLayout();
	
	// add slider between the two buttons for somewhat nicer adjusting
	/** @todo Replace with icons */
	m_zoomOutButton = new QPushButton(tr("Zoom out"), this);
	m_zoomInButton = new QPushButton(tr("Zoom in"), this);
	
	hbox->addWidget(m_zoomOutButton);
	hbox->addWidget(m_zoomInButton);
	
	vbox->addWidget(view);
	vbox->addLayout(hbox);
	
	setLayout(vbox);
}

QPushButton* NavigatorLayout::getZoomIn()
{
	return m_zoomInButton;
}

QPushButton* NavigatorLayout::getZoomOut()
{
	return m_zoomOutButton;
}
