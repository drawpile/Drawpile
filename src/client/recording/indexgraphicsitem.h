/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef INDEXGRAPHICSITEM_H
#define INDEXGRAPHICSITEM_H

#include <QGraphicsItem>
#include <QBrush>
#include <QPen>
#include <QPixmap>

namespace recording {
	class Index;
}

class IndexGraphicsItem : public QGraphicsItem
{
public:
	explicit IndexGraphicsItem(QGraphicsItem *parent=0);

	static const int STEP_WIDTH = 16;
	static const int ITEM_HEIGHT = 16;
	static const int VERTICAL_PADDING = 2;

	/**
	 * @brief Populate a graphics scene with index items
	 * @param scene
	 * @param index
	 */
	static void buildScene(QGraphicsScene *scene, const recording::Index &index);

	QRectF boundingRect() const;

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);

private:
	QRectF _rect;
	QBrush _brush;
	QPen _pen;
	QPixmap _icon;
};

#endif // INDEXGRAPHICSITEM_H
