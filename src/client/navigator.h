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

class QGraphicsScene;
class QGraphicsView;

class Navigator
	: public QDockWidget
{
	Q_OBJECT
private:
	QGraphicsScene *scene_;
	QGraphicsView *view_;
public:
	Navigator(QWidget *parent, QGraphicsScene *scene);
	~Navigator();
signals:
	void focusMoved(const QRect& focus);
public slots:
	void setScene(QGraphicsScene *scene);
	void setFocus(const QRect& focus);
	void rescale();
	void sceneResized();
private:
	void resizeEvent(QResizeEvent *event);
};

#endif // Navigator_H
