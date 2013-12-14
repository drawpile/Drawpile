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
#ifndef TILE_H
#define TILE_H

#include <QSharedDataPointer>

class QColor;
class QImage;

namespace dpcore {

/// Shared tile data
struct TileData : public QSharedData {
	quint32 data[64*64];
};

/**
 * @brief A piece of an image
 * Each tile is a square of size SIZE*SIZE. The pixel format is 32-bit ARGB.
 *
 */
class Tile {
	public:
		//! The tile width and height
		static const int SIZE = 64;

		//! The length of the tile data in bytes
		static const int BYTES = SIZE * SIZE * sizeof(quint32);

		/** @brief Round i upwards to SIZE boundary
		 * @param i coordinate
		 * @return i rounded up to nearest multiple of SIZE
		 */
		static int roundUp(int i) {
			return (i + SIZE-1)/SIZE*SIZE;
		}
		
		/**
		 * @brief Round i down to SIZE boundary
		 * @param i coordinate
		 * @return i rounded down to nearest multiple of SIZE
		 */
		static int roundDown(int i) {
			return (i/SIZE) * SIZE;
		}

		//! Construct a null tile
		Tile();

		//! Construct a tile filled with the given color
		Tile(const QColor& color);

		//! Construct a tile from an image
		Tile(const QImage& image, int xoff=0, int yoff=0);

		//! Get a pixel value from this tile
		quint32 pixel(int x, int y) const {
			Q_ASSERT(x>=0 && x<SIZE);
			Q_ASSERT(y>=0 && y<SIZE);
			if(_data)
				return *(_data->data + y * SIZE + x);
			return 0;
		}

		//! Composite values multiplied by color onto this tile
		void composite(int mode, const uchar *values, const QColor& color, int x, int y, int w, int h, int offset);

		//! Composite another tile with this tile
		bool merge(const Tile &tile, uchar opacity, int blend);

		//! Copy the contents of this tile onto the given spot on an image
		void copyToImage(QImage& image, int x, int y) const;

		//! Fill this tile with a checker pattern
		void fillChecker(const QColor& dark, const QColor& light);

		//! Fill this tile with a solid color
		void fillColor(const QColor& color);

		//! Make this a null tile
		void blank();

		//! Get read access to the raw pixel data
		const quint32 *data() const { Q_ASSERT( _data); return _data->data; }

		//! Copy the contents of this tile
		void copyTo(quint32 *data) const;

		/**
		 * @brief is this a null tile?
		 *
		 * Null tiles have no pixel data and should be considered
		 * to be completely transparent.
		 * @return true if there is no pixel data
		 */
		bool isNull() const { return !_data; }

		//! Check if this tile is completely transparent
		bool isBlank() const;

		//! Make this a null tile if it is completely transparent
		void optimize();

		//! Fill a tile sized memory buffer with a checker pattenr
		static void fillChecker(quint32 *data, const QColor& dark, const QColor& light);

	private:
		quint32 *getOrCreateData() {
			if(!_data) {
				_data = new TileData;
				for(int i=0;i<SIZE*SIZE;++i)
					_data->data[i] = 0;
			}
			return _data->data;
		}

		quint32 *getOrCreateUninitializedData() {
			if(!_data)
				_data = new TileData;
			return _data->data;
		}

		QSharedDataPointer<TileData> _data;
};

}

#endif

