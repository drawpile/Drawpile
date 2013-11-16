/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#include <QColor>

class QImage;
class QSize;

namespace dpcore {

class Brush;
class Point;
class Tile;
class LayerStack;

/**
 * @brief A drawing layer/tile manage
 * 
 * A layer is made up of multiple tiles.
 * Although images of arbitrary size can be created, the true layer size is
 * always a multiple of Tile::SIZE.
 */
class Layer {
	public:
		//! Construct a layer filled with solid color
		Layer(LayerStack *owner, int id, const QString& name, const QColor& color, const QSize& size);

		~Layer();

		//! Get the layer width in pixels
		int width() const { return width_; }

		//! Get the layer height in pixels
		int height() const { return height_; }

		//! Get the layer ID
		int id() const { return id_; }

		//! Get the layer name
		const QString& name() const { return name_; }

		//! Set the layer name
		void setName(const QString& name);

		//! Get the layer as an image
		QImage toImage() const;

		//! Resize this layer
		//void resize(const QSize& newsize);

		//! Get the color at the specified coordinate
		QColor colorAt(int x, int y) const;

		//! Get layer opacity
		int opacity() const { return opacity_; }

		//! Set layer opacity
		void setOpacity(int opacity);

		//! Is this layer hidden?
		/**
		 * Hiding a layer is slightly different than setting its opacity
		 * to zero, although the end result is the same. The hidden status
		 * is purely local: setting it will not hide the layer for other
		 * users.
		 */
		bool hidden() const { return hidden_; }

		//! Hide this layer
		void setHidden(bool hide);

		//! Draw an image onto the layer
		void putImage(int x, int y, QImage image, bool blend);

		//! Dab the layer with a brush
		void dab(const Brush& brush, const Point& point);

		//! Draw a line using either drawHardLine or drawSoftLine
		void drawLine(const Brush& brush, const Point& from, const Point& to, qreal &distance);

		//! Draw a line using the brush
		void drawHardLine(const Brush& brush, const Point& from, const Point& to, qreal &distance);

		//! Draw a line using the brush.
		void drawSoftLine(const Brush& brush, const Point& from, const Point& to, qreal &distance);

		//! Merge a layer
		void merge(int x, int y, const Layer *layer);

		//! Fill the layer with a checker pattern
		void fillChecker(const QColor& dark, const QColor& light);

		//! Fill the layer with solid color
		void fillColor(const QColor& color);

		//! Optimize layer memory usage
		void optimize();

		//! Get a tile
		const Tile *tile(int x, int y) const {
			Q_ASSERT(x>=0 && x<xtiles_);
			Q_ASSERT(y>=0 && y<ytiles_);
			return tiles_[y*xtiles_+x];
		}

		//! Get a tile
		const Tile *tile(int index) const { Q_ASSERT(index>=0 && index<xtiles_*ytiles_); return tiles_[index]; }

		/**
		 * @brief Is this layer visible
		 * A layer is visible when its opacity is greater than zero AND
		 * it is not explicitly hidden.
		 * @return true if layer is visible
		 */
		bool visible() const { return opacity_ > 0 && !hidden_; }

		//! Return a (temporary use) copy of the given layer
		// TODO needed?
		static Layer *scratchCopy(const Layer *src);

	private:
		QImage padImageToTileBoundary(int leftpad, int toppad, const QImage &original, bool alpha) const;
		
		LayerStack *owner_;
		int id_;
		QString name_;
	
		int width_;
		int height_;
		int xtiles_;
		int ytiles_;
		Tile **tiles_;
		uchar opacity_;
		bool hidden_;
};

}

#endif

