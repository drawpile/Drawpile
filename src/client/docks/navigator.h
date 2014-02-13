/*
   Copyright (C) 2008-2014 Calle Laakkonen, 2007 M.K.A.

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

class Ui_NaviBox;

namespace docks {

//! Navigator graphics view
class NavigatorView
	: public QGraphicsView
{
	Q_OBJECT
public:
	NavigatorView(QWidget *parent);

	void setViewFocus(const QPolygonF& rect);
	
public slots:
	void rescale();

signals:
	void focusMoved(const QPoint& to);
	
protected:
	void drawForeground(QPainter *painter, const QRectF& rect);
	void resizeEvent(QResizeEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

private:
	QPolygonF _focusrect;
	bool _dragging;
};

//! Navigator dock widget
class Navigator
	: public QDockWidget
{
	Q_OBJECT
public:
	Navigator(QWidget *parent);
	~Navigator();
	
	//! Set associated graphics scene
	void setScene(QGraphicsScene *scene);

public slots:
	//! Move the view focus rectangle
	void setViewFocus(const QPolygonF& rect);

	//! Set the transform controls
	void setViewTransform(qreal zoom, qreal angle);

signals:
	void zoomIn();
	void zoomOut();
	void focusMoved(const QPoint& to);
	void angleChanged(qreal angle);
	
private:
	Ui_NaviBox *_ui;
};

}

#endif // Navigator_H
