/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

class QImage;
class QColor;
class QPainter;
class QRectF;

namespace dpcore {

class Brush;
class Point;
class Tile;

//! A drawing layer/tile manager
/**
 * A layer is made up of multiple tiles.
 * Although images of arbitrary size can be created, the real image size is
 * always a multiple of Tile::SIZE. This is just simplyfo some algorithms.
 */
class Layer {
	public:
		//! Construct a layer from an image
		Layer(const QImage& image);

		//! Construct an empty layer
		Layer(const QColor& color, const QSize& size);

		~Layer();

		//! Get the layer width in pixels
		int width() const { return width_; }

		//! Get the layer height in pixels
		int height() const { return height_; }

		//! Get the layer as an image
		QImage toImage() const;

		//! Resize this layer
		//void resize(const QSize& newsize);

		//! Paint an area of this layer
		void paint(const QRectF& rect, QPainter *painter); 

		//! Get the color at the specified coordinate
		QColor colorAt(int x, int y);

		//! Dab the layer with a brush
		void dab(const Brush& brush, const Point& point);

		//! Draw a line using either drawHardLine or drawSoftLine
		void drawLine(const Brush& brush, const Point& from, const Point& to, qreal *distance);

		//! Draw a line using the brush
		void drawHardLine(const Brush& brush, const Point& from, const Point& to, qreal *distance);

		//! Draw a line using the brush.
		void drawSoftLine(const Brush& brush, const Point& from, const Point& to, qreal *distance);

		//! Merge a layer
		void merge(int x, int y, const Layer *layer);

		//! Fill the layer with a checker pattern
		void fillChecker(const QColor& dark, const QColor& light);

	private:
		int width_;
		int height_;
		int xtiles_;
		int ytiles_;
		Tile **tiles_;
};

}

#endif

