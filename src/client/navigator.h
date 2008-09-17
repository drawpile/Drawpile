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

#ifndef Navigator_H
#define Navigator_H

#include <QDockWidget> // inherited by Navigator
#include <QGraphicsView> // inherited by NavigatorView
#include <QRectF>

class Ui_NaviBox;

namespace widgets {

//! Navigator graphics view
class NavigatorView
	: public QGraphicsView
{
	Q_OBJECT
public:
	NavigatorView(QWidget *parent);
	
signals:
	//! Signal rectangle movement
	void focusMoved(const QPoint& to);
	
public slots:
	//! Set rectangle
	void setFocus(const QRectF& rect);
	
	//! Re-scale the viewport
	void rescale();
	
protected:
	//! Draw rectangle
	void drawForeground(QPainter *painter, const QRectF& rect);
	
	//! Drag rectangle if button is pressed
	void mouseMoveEvent(QMouseEvent *event);
	//! Move rectangle and enable dragging
	void mousePressEvent(QMouseEvent *event);
	//! Disable dragging
	void mouseReleaseEvent(QMouseEvent *event);
	
private:
	QRectF focusRect_;
	
	//! Is dragging?
	bool dragging_;
};

//! Navigator dock widget
class Navigator
	: public QDockWidget
{
	Q_OBJECT
public:
	Navigator(QWidget *parent, QGraphicsScene *scene);
	~Navigator();
	
	//! Set associated graphics scene
	void setScene(QGraphicsScene *scene);

public slots:
	//! Move the view focus rectangle
	void setViewFocus(const QRectF& rect);

signals:
	//! A zoom in button was pressed
	void zoomIn();
	//! A zoom out button was pressed
	void zoomOut();
	//! The view focus rectangle was moved
	void focusMoved(const QPoint& to);
	
protected:
	//! React to widget resizing
	void resizeEvent(QResizeEvent *event);
	
private:
	Ui_NaviBox *ui_;
};

}

#endif // Navigator_H
