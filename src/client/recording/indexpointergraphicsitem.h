/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef INDEXPOINTERGRAPHICSITEM_H
#define INDEXPOINTERGRAPHICSITEM_H

#include <QGraphicsItem>
#include <QPolygon>

class IndexPointerGraphicsItem : public QGraphicsItem
{
public:
	explicit IndexPointerGraphicsItem(int height, QGraphicsItem *parent=0);

	QRectF boundingRect() const;

	void setIndex(int i);

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);

private:
	QRectF _rect;
};

#endif
