/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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
#ifndef DP_CANVASITEM_H
#define DP_CANVASITEM_H

#include <QGraphicsObject>

namespace dpcore {
	class Layer;
	class LayerStack;
	class Brush;
	class Point;
}

namespace drawingboard {

//! A drawing board item item for QGraphicsScene
/**
 * The board item provides an interface to a LayerStack for QGraphicsScene.
 * Methods are provided for drawing lines and points with a Brush object.
 */
class CanvasItem : public QGraphicsObject
{
	Q_OBJECT
	public:
		//! Construct an empty board
		CanvasItem(QGraphicsItem *parent=0);

		//! Get the image
		dpcore::LayerStack *image() const { return _image; }

		/** reimplematation */
		QRectF boundingRect() const;

	public slots:
		void refreshImage(const QRect &area);

	protected:
		/** reimplementation */
		void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);

	private:
		dpcore::LayerStack *_image;
};

}

#endif


