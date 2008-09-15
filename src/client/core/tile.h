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
#ifndef TILE_H
#define TILE_H

#include <QPixmap>

class QColor;
class QImage;
class QPainter;

namespace dpcore {

//! A piece of an image
/**
 * Each tile is a square of size SIZE*SIZE. Only one 32-bit RGBA is supported.
 *
 */
class Tile {
	public:
		static const int SIZE = 64;

		//! Round i upwards to SIZE boundary
		static int roundTo(int i) {
			return (i + SIZE-1)/SIZE*SIZE;
		}

		//! Construct a blank tile
		Tile(const QColor& color, int x, int y);

		//! Construct a tile from an image
		Tile(const QImage& image, int x, int y);

		//! Get tile X index
		int x() const { return x_; }

		//! Get tile Y index
		int y() const { return y_; }

		//! Get a pixel value from this tile
		quint32 pixel(int x, int y) const;

		//! Composite values multiplied by color onto this tile
		void composite(const uchar *values, const QColor& color, int x, int y, int w, int h, int offset);

		//! Copy the contents of this tile onto the appropriate spot on an image
		void copyToImage(QImage& image) const;

		//! Paint this tile
		void paint(QPainter *painter, const QPoint& target) const;

	private:
		int x_, y_;
		quint32 *data_;
		mutable QPixmap cache_;
};

}

#endif

