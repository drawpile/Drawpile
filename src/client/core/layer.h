/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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
#ifndef LAYER_H
#define LAYER_H

#include <QColor>

#include "tile.h"

class QImage;
class QSize;
class QDataStream;

namespace paintcore {

class Brush;
struct BrushStamp;
class Point;
class LayerStack;
struct StrokeState;

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
		Layer(LayerStack *owner, int id, const QString& title, const QColor& color, const QSize& size);

		//! Construct a copy of this layer
		Layer(const Layer &layer);

		~Layer();

		//! Get the layer width in pixels
		int width() const { return _width; }

		//! Get the layer height in pixels
		int height() const { return _height; }

		//! Get the layer ID
		int id() const { return id_; }

		//! Get the layer name
		const QString& title() const { return _title; }

		//! Set the layer name
		void setTitle(const QString& title);

		//! Get the layer as an image
		QImage toImage() const;

		//! Get the layer as an image with excess transparency cropped away
		QImage toCroppedImage(int *xOffset, int *yOffset) const;

		//! Adjust layer size
		void resize(int top, int right, int bottom, int left);

		//! Get the color at the specified coordinates
		QColor colorAt(int x, int y) const;

		//! Get the raw pixel value at the specified coordinates
		QRgb pixelAt(int x, int y) const;

		//! Get layer opacity
		int opacity() const { return _opacity; }

		//! Get the effective layer opacity (0 if hidden)
		int effectiveOpacity() const { return hidden() ? 0 : _opacity; }

		//! Set layer opacity
		void setOpacity(int opacity);

		//! Set layer blending mode
		void setBlend(int blend);

		/**
		 * @brief Get the layer blending mode
		 * @return blending mode number
		 */
		int blendmode() const { return _blend; }

		/**
		 * @brief Is this layer hidden?
		 * Hiding a layer is slightly different than setting its opacity
		 * to zero, although the end result is the same. The hidden status
		 * is purely local: setting it will not hide the layer for other
		 * users.
		 */
		bool hidden() const { return _hidden; }

		//! Hide this layer
		void setHidden(bool hide);

		//! Empty this layer
		void makeBlank();

		//! Draw an image onto the layer
		void putImage(int x, int y, QImage image, int mode);

		//! Fill a rectangle
		void fillRect(const QRect &rect, const QColor &color, int blendmode=255);

		//! Dab the layer with a brush
		void dab(int contextId, const Brush& brush, const Point& point, StrokeState &state);

		//! Draw a line using either drawHardLine or drawSoftLine
		void drawLine(int contextId, const Brush& brush, const Point& from, const Point& to, StrokeState &state);

		//! Merge a sublayer with this layer
		void mergeSublayer(int id);

		//! Remove a sublayer
		void removeSublayer(int id);

		//! Merge a layer
		void merge(const Layer *layer, bool sublayers=false);

		//! Optimize layer memory usage
		void optimize();

		//! Get a tile
		const Tile &tile(int x, int y) const {
			Q_ASSERT(x>=0 && x<_xtiles);
			Q_ASSERT(y>=0 && y<_ytiles);
			return _tiles[y*_xtiles+x];
		}

		//! Get an editable reference to a tile
		Tile &rtile(int x, int y) {
			Q_ASSERT(x>=0 && x<_xtiles);
			Q_ASSERT(y>=0 && y<_ytiles);
			return _tiles[y*_xtiles+x];
		}

		//! Get a tile
		const Tile &tile(int index) const { Q_ASSERT(index>=0 && index<_xtiles*_ytiles); return _tiles[index]; }

		//! Get the sublayers
		const QList<Layer*> &sublayers() const { return _sublayers; }

		/**
		 * @brief Is this layer visible
		 * A layer is visible when its opacity is greater than zero AND
		 * it is not explicitly hidden.
		 * @return true if layer is visible
		 */
		bool visible() const { return _opacity > 0 && !_hidden; }

		//! Mark non-empty tiles as dirty
		void markOpaqueDirty(bool forceVisible=false);

		// Disable assignment operator
		Layer& operator=(const Layer&) = delete;

		void toDatastream(QDataStream &out) const;
		static Layer *fromDatastream(LayerStack *owner, QDataStream &in);

	private:
		//! Construct a sublayer
		Layer(LayerStack *owner, int id, const QSize& size);

		QImage padImageToTileBoundary(int leftpad, int toppad, const QImage &original, int mode) const;

		//! Get a sublayer
		Layer *getSubLayer(int id, int blendmode, uchar opacity);

		void directDab(const Brush &brush, const Point& point, StrokeState &state);
		void drawHardLine(const Brush &brush, const Point& from, const Point& to, StrokeState &state);
		void drawSoftLine(const Brush &brush, const Point& from, const Point& to, StrokeState &state);

		QColor getDabColor(const BrushStamp &stamp) const;

		LayerStack *_owner;
		int id_;
		QString _title;
	
		int _width;
		int _height;
		int _xtiles;
		int _ytiles;
		QVector<Tile> _tiles;
		uchar _opacity;
		int _blend;
		bool _hidden;

		QList<Layer*> _sublayers;
};

}

#endif

