/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#ifndef LAYER_H
#define LAYER_H

#include <QGraphicsItem>


namespace drawingboard {

class Brush;
class Point;

//! A drawing layer item item for QGraphicsScene
/**
 * The layer item provides a modifiable image item for QGraphicsScene.
 * Methods are provided for drawing lines and points with a Brush object.
 *
 */
class Layer : public QGraphicsItem
{
	public:
		//! Construct an empty layer
		Layer(QGraphicsItem *parent=0, QGraphicsScene *scene=0);

		//! Construct a layer from a QImage
		Layer(const QImage& image, QGraphicsItem *parent=0, QGraphicsScene *scene=0);

		//! Set layer contents
		void setImage(const QImage& image);

		//! Get layer contents
		QImage image() const;

		//! Draw a line between two points with interpolated pressure values
		void drawLine(const Point& point1, const Point& point2,
				const Brush& brush, int *distance=0);

		//! Draw a single point
		void drawPoint(const Point& point, const Brush& brush);

		/** reimplematation */
		QRectF boundingRect() const;

		/** reimplementation */
		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
			 QWidget *);

	private:

		QImage image_;

		int plastx_, plasty_;
};

}

#endif


