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
#include <QGraphicsView>
#include <QSize>
#include <QDebug>

Navigator::Navigator(QWidget *parent, QGraphicsScene *scene)
	: QDockWidget(tr("Navigator"), parent), scene_(scene), view_(0)
{
	view_ = new QGraphicsView(scene, this);
	view_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
	view_->setAlignment(Qt::AlignCenter);
	view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setWidget(view_);
}

Navigator::~Navigator()
{
	delete view_;
}

void Navigator::setScene(QGraphicsScene *scene)
{
	scene_ = scene;
	connect(scene_, SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(sceneResized()));
	view_->setScene(scene_);
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

void Navigator::setFocus(const QRect& focus)
{
	
}

void Navigator::resizeEvent(QResizeEvent *event)
{
	rescale();
}

void Navigator::sceneResized()
{
	rescale();
}
