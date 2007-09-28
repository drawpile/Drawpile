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

#ifndef Navigator_H
#define Navigator_H

#include <QDockWidget>
#include <QGraphicsView>

class QGraphicsScene;

//! Navigator graphics view
class NavigatorView
	: public QGraphicsView
{
	Q_OBJECT
public:
	NavigatorView(QGraphicsScene *scene, QWidget *parent);
	~NavigatorView();
	
public slots:
	void setFocus(const QPoint& pt);
	
protected:
	QGraphicsRectItem *rect_;
	
	// mouse dragging
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	
	bool dragging_;
};

//! 
class NavigatorLayout
	: public QWidget
{
	Q_OBJECT
public:
	NavigatorLayout(QWidget *parent, QGraphicsScene *scene);
	
	NavigatorView* navigatorView();
	
signals:
	//! Zoom in
	void zoomIn();
	//! Zoom out
	void zoomOut();
	
protected slots:
	void zoomInButtonAction();
	void zoomOutButtonAction();
	
protected:
	NavigatorView *view_;
};

//! Navigator dock widget
class Navigator
	: public QDockWidget
{
	Q_OBJECT
protected:
	//! Viewport
	NavigatorView *view_;
	QGraphicsScene *scene_;
public:
	//! Ctor
	Navigator(QWidget *parent, QGraphicsScene *scene);
	//! Dtor
	~Navigator();
signals:
	//! Signaled when user moves the rectangle to some other spot
	void focusMoved(const QRect& focus);
	
public slots:
	//! Enable/disable delayed update
	void delayedUpdate(bool enable);
	//! Update 
	void update();
	//! Set associated graphics scene
	void setScene(QGraphicsScene *scene);
	//! Set view rectangle to different spot
	void setFocus(const QRect& focus);
	//! Re-scale the viewport
	void rescale();
	//! Should be called when scene is resized
	void sceneResized();
	
protected:
	//! React to widget resizing
	void resizeEvent(QResizeEvent *event);
	
	NavigatorLayout *layout_;
	
	bool delayed_;
};

#endif // Navigator_H
