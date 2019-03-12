/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2019 Calle Laakkonen

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
#ifndef TILE_H
#define TILE_H

#include "blendmodes.h"

#include <QSharedDataPointer>

#ifndef NDEBUG
#include <QAtomicInt>
#endif

#include <array>

class QColor;
class QImage;
class QDataStream;

namespace paintcore {

/// Shared tile data
struct TileData : public QSharedData {
	quint32 pixels[64*64]; // the pixel data
	int lastEditedBy;     // ID of the user who last edited this tile

#ifndef NDEBUG // Debug tool for measuring memory usage
	TileData();
	TileData(const TileData &td);
	~TileData();

	static int globalCount() { return _count.load() ; }
	static float megabytesUsed() { return globalCount() * sizeof(pixels) / float(1024*1024); }
private:
	static QAtomicInt _count;
#endif
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

		//! The length of the tile in 32 bit words
		static const int LENGTH = SIZE * SIZE;

		//! The length of the tile data in bytes
		static const int BYTES = LENGTH * sizeof(quint32);

		/** @brief Round i upwards to SIZE boundary
		 * @param i coordinate
		 * @return i rounded up to nearest multiple of SIZE
		 */
		static constexpr int roundUp(int i) {
			return (i + SIZE-1)/SIZE*SIZE;
		}
		
		/**
		 * @brief Round i down to SIZE boundary
		 * @param i coordinate
		 * @return i rounded down to nearest multiple of SIZE
		 */
		static constexpr int roundDown(int i) {
			return (i/SIZE) * SIZE;
		}

		/**
		 * @brief Round to the number of tiles needed
		 * @param i
		 * @return
		 */
		static constexpr int roundTiles(int i) {
			return i<0 ?
				((i - SIZE + 1) / SIZE)
				:
				((i + SIZE - 1) / SIZE)
				;
		}

		//! Construct a null tile
		Tile() : m_data(nullptr) { }

		//! Construct a tile filled with the given color
		explicit Tile(const QColor& color, int lastEditedBy=0);

		//! Construct a tile from raw data
		explicit Tile(const QByteArray &data, int lastEditedBy=0);

		//! Construct a tile from an image
		Tile(const QImage& image, int xoff, int yoff, int lastEditedBy=0);

		//! Get a pixel value from this tile.
		quint32 pixel(int x, int y) const {
			Q_ASSERT(x>=0 && x<SIZE);
			Q_ASSERT(y>=0 && y<SIZE);
			if(m_data)
				return m_data->pixels[y * SIZE + x];
			return 0;
		}

		//! Get the ID of the user who last edited this tile
		int lastEditedBy() const { return m_data ? m_data->lastEditedBy : 0; }

		//! Set the last edited by tag
		void setLastEditedBy(int id);

		//! Composite values multiplied by color onto this tile
		void composite(BlendMode::Mode mode, const uchar *values, const QColor& color, int x, int y, int w, int h, int skip);

		//! Get a weighted average of the tile pixels
		std::array<quint32, 5> weightedAverage(const uchar *weights, int x, int y, int w, int h, int skip) const;

		//! Composite another tile with this tile
		void merge(const Tile &tile, uchar opacity, BlendMode::Mode mode);

		//! Copy the contents of this tile onto the given spot on an image
		void copyToImage(QImage& image, int x, int y) const;

		//! Get read access to the raw pixel data (tile must not be a null tile)
		const quint32 *constData() const { Q_ASSERT(m_data); return m_data->pixels; }

		//! Get read/write access to the raw pixel data
		quint32 *data();

		//! Copy the contents of this tile
		void copyTo(quint32 *data) const;

		/**
		 * @brief is this a null tile?
		 *
		 * Aside from constData(), null tiles behave exactly like
		 * blank tiles.
		 * @return true if there is no pixel data
		 */
		bool isNull() const { return !m_data; }

		//! Check if this tile is completely transparent
		bool isBlank() const;

		/**
		 * @brief Is this tile filled with a single solid color?
		 *
		 * @return tile color or an invalid color if not solid
		 */
		QColor solidColor() const;

		//! Fill a tile sized memory buffer with a checker pattern
		static void fillChecker(quint32 *data, const QColor& dark, const QColor& light);

		//! Construct a tile filled with a censor block pattern
		static Tile CensorBlock(const QColor& dark, const QColor& light);

		/**
		 * @brief Check if these two tiles have identical content
		 */
		bool equals(const Tile &t) const;

		/**
		 * @brief Return true if the two tiles point to the same data
		 *
		 * This is an identity comparison. This will return false even
		 * if the tiles have identical contents but have different data pointers.
		 * @param other
		 * @return true if tiles share data pointers
		 */
		bool operator==(const Tile &other) const { return m_data == other.m_data; }
		bool operator!=(const Tile &other) const { return m_data != other.m_data; }
		friend QDataStream &operator>>(QDataStream&, Tile&);

	private:
		QSharedDataPointer<TileData> m_data;
};

QDataStream &operator<<(QDataStream&, const Tile&);

}

Q_DECLARE_TYPEINFO(paintcore::Tile, Q_MOVABLE_TYPE);


#endif

