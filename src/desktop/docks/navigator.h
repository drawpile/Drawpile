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

#ifndef Navigator_H
#define Navigator_H

#include <QDockWidget>
#include <QGraphicsView>

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
	void wheelZoom(int steps);
	
protected:
	void drawForeground(QPainter *painter, const QRectF& rect);
	void resizeEvent(QResizeEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);

private:
	QPolygonF _focusrect;
	int _zoomWheelDelta;
	bool _dragging;
};

//! Navigator dock widget
class Navigator
	: public QDockWidget
{
	Q_OBJECT
public:
	Navigator(QWidget *parent);
	
	//! Set associated graphics scene
	void setScene(QGraphicsScene *scene);

public slots:
	//! Move the view focus rectangle
	void setViewFocus(const QPolygonF& rect);

signals:
	void focusMoved(const QPoint& to);
	void wheelZoom(int steps);
	
private:
	NavigatorView *m_view;
};

}

#endif // Navigator_H
