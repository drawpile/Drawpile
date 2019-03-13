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

#include "tile.h"
#include "rasterop.h"

#include <QImage>
#include <QPainter>

namespace paintcore {

Tile::Tile(const QColor& color, int lastEditedBy)
	: m_data(new TileData)
{
	quint32 *ptr = m_data->pixels;
	quint32 col = qPremultiply(color.rgba());
	for(int i=0;i<LENGTH;++i)
		*(ptr++) = col;
	m_data->lastEditedBy = lastEditedBy;
}

Tile::Tile(const QByteArray &data, int lastEditedBy)
	: m_data(new TileData)
{
	Q_ASSERT(data.length() == BYTES);
	memcpy(m_data->pixels, data.constData(), BYTES);
	m_data->lastEditedBy = lastEditedBy;
}

/**
 * Copy pixel data from (xoff, yoff, min(xoff+SIZE, image.width()), min(yoff+SIZE, image.height()))
 * Pixels outside the image will be set to zero
 *
 * @param image source image
 * @param xoff source image offset
 * @param yoff source image offset
 */
Tile::Tile(const QImage& image, int xoff, int yoff, int lastEditedBy)
	: m_data(new TileData)
{
	Q_ASSERT(xoff>=0 && xoff < image.width());
	Q_ASSERT(yoff>=0 && yoff < image.height());
	Q_ASSERT(image.format() == QImage::Format_ARGB32_Premultiplied);

	const int w = xoff + SIZE > image.width() ? image.width() - xoff : SIZE;
	const int h = yoff + SIZE > image.height() ? image.height() - yoff : SIZE;

	uchar *ptr = reinterpret_cast<uchar*>(m_data->pixels);
	if(w < SIZE || h < SIZE)
		memset(ptr, 0, BYTES);

	const uchar *src = image.scanLine(yoff) + xoff*4;
	for(int y=0;y<h;++y) {
		memcpy(ptr, src, w*4);
		ptr += SIZE*4;
		src += image.bytesPerLine();
	}
	m_data->lastEditedBy = lastEditedBy;
}

void Tile::fillChecker(quint32 *data, const QColor& dark, const QColor& light)
{
	const int HALF = SIZE/2;
	const quint32 d = qPremultiply(dark.rgba());
	const quint32 l = qPremultiply(light.rgba());
	quint32 *q1 = data, *q2 = data+HALF, *q3 = data + SIZE*HALF, *q4 = data + SIZE*(HALF)+HALF;
	for(int y=0;y<HALF;++y) {
		for(int x=0;x<HALF;++x) {
			*(q1++) = d;
			*(q2++) = l;
			*(q3++) = l;
			*(q4++) = d;
		}
		q1 += HALF; q2 += HALF; q3 += HALF; q4 += HALF;
	}
}

Tile Tile::ZebraBlock(const QColor &dark, const QColor &light, const int stripe)
{
	Q_ASSERT(stripe>0 && SIZE % stripe == 0); // must be a divisor of SIZE for the blocks to be tilable

	Tile t;
	quint32 *pixels = t.data();
	const quint32 colors[] {
		qPremultiply(dark.rgba()),
		qPremultiply(light.rgba())
	};
	for(int y=0;y<SIZE;++y) {
		for(int x=0;x<SIZE;++x, ++pixels) {
			*pixels = colors[((x+y) / stripe) % 2];
		}
	}
	return t;
}

void Tile::copyTo(quint32 *data) const
{
	if(isNull())
		memset(data, 0, BYTES);
	else
		memcpy(data, constData(), BYTES);
}

void Tile::copyToImage(QImage& image, int x, int y) const {
	int w = 4*(image.width()-x<SIZE ? image.width()-x : SIZE);
	int h = image.height()-y<SIZE ? image.height()-y : SIZE;
	uchar *targ = image.bits() + y * image.bytesPerLine() + x * 4;

	if(isNull()) {
		for(int y=0;y<h;++y) {
			memset(targ, 0, w);
			targ += image.bytesPerLine();
		}
	} else {
		const quint32 *ptr = constData();
		for(int y=0;y<h;++y) {
			memcpy(targ, ptr, w);
			targ += image.bytesPerLine();
			ptr += SIZE;
		}
	}
}

/**
 * @param values array of alpha values
 * @param color composite color
 * @param x offset in the tile
 * @param y offset in the tile
 * @param w values in tile (must be < SIZE)
 * @param h values in tile (must be < SIZE)
 * @param skip values to skip to reach the next line
 */
void Tile::composite(BlendMode::Mode mode, const uchar *values, const QColor& color, int x, int y, int w, int h, int skip)
{
	Q_ASSERT(x>=0 && x<SIZE && y>=0 && y<SIZE);
	Q_ASSERT((x+w)<=SIZE && (y+h)<=SIZE);
	compositeMask(mode, data() + y * SIZE + x,
			color.rgba(), values, w, h, skip, SIZE-w);
}

/**
 * @param weights array of weights
 * @param x x offset in tile
 * @param y y offset in tile
 * @param w width of composition rectangle
 * @param h height of composition rectangle
 * @param offset number of bytes to skip in weights to get to the next line
 * @return [sum of weights, red, green, blue, alpha]
 */
std::array<quint32, 5> Tile::weightedAverage(const uchar *weights, int x, int y, int w, int h, int skip) const
{
	Q_ASSERT(x>=0 && x<SIZE && y>=0 && y<SIZE);
	Q_ASSERT((x+w)<=SIZE && (y+h)<=SIZE);

	if(isNull()) {
		quint32 weightsum=0;
		for(int y=0;y<h;++y) {
			for(int x=0;x<w;++x,++weights) {
				weightsum += *weights;
			}
			weights += skip;
		}

		return {{weightsum, 0, 0, 0, 0}};

	} else {
		return sampleMask(constData() + y * SIZE + x, weights,
			w, h, skip, SIZE-w);
	}
}

/**
 * @param tile the tile which will be composited over this tile
 * @param opacity opacity modifier of tile
 * @param blend blending mode
 */
void Tile::merge(const Tile &tile, uchar opacity, BlendMode::Mode blend)
{
	if(!tile.isNull())
		compositePixels(blend, data(), tile.constData(), SIZE*SIZE, opacity);
}

/**
 * @return true if every pixel of this tile has an alpha value of zero
 */
bool Tile::isBlank() const
{
	if(isNull())
		return true;

	const quint32 *pixel = constData();
	const quint32 *end = pixel + LENGTH;
	while(pixel<end) {
		// Note: colors are premultiplied so alpha=0 => rgb=0
		if(*pixel)
			return false;
		++pixel;
	}
	return true;
}

QColor Tile::solidColor() const
{
	if(isNull())
		return Qt::transparent;

	const quint32 *pixel = constData();
	const quint32 *end = pixel + LENGTH;
	const quint32 first = *(pixel++);
	while(pixel<end) {
		if(*pixel != first)
			return QColor();
		++pixel;
	}

	return QColor::fromRgba(qUnpremultiply(first));
}

void Tile::setLastEditedBy(int id)
{
	if(!m_data) {
		m_data = new TileData;
		memset(m_data->pixels, 0, BYTES);
	}
	m_data->lastEditedBy = id;
}

quint32 *Tile::data() {
	if(!m_data) {
		m_data = new TileData;
		memset(m_data->pixels, 0, BYTES);
		m_data->lastEditedBy = 0;
	}
	return m_data->pixels;
}

bool Tile::equals(const Tile &other) const
{
	// Check if the tiles are both the same or both blank
	if(*this == other || (isNull() && other.isBlank()) || (other.isNull() && isBlank()))
		return true;

	// If both are not blank, either being null means they can't be the same
	if(isNull() || other.isNull())
		return false;

	// Both are not null: check content
	const quint32 *d1 = m_data->pixels;
	const quint32 *d2 = other.m_data->pixels;
	for(int i=0;i<LENGTH;++i) {
		if(*(d1++) != *(d2++))
			return false;
	}

	return true;
}

QDataStream &operator<<(QDataStream &ds, const Tile &t)
{
	const QColor solid = t.solidColor();
	if(solid.isValid()) {
		ds << quint8(1) << solid.rgba();
	} else {
		ds << quint8(0);
		ds.writeRawData(reinterpret_cast<const char*>(t.constData()), Tile::BYTES);
	}
	return ds;
}

QDataStream &operator>>(QDataStream &ds, Tile &t)
{
	quint8 isSolid;
	ds >> isSolid;
	if(isSolid) {
		QRgb color;
		ds >> color;
		if(qAlpha(color) == 0)
			t = Tile();
		else
			t = Tile(QColor::fromRgba(color));
	} else {
		ds.readRawData(reinterpret_cast<char*>(t.data()), Tile::BYTES);
	}
	return ds;
}

#ifndef NDEBUG
QAtomicInt TileData::_count;
TileData::TileData() { _count.fetchAndAddOrdered(1); }
TileData::TileData(const TileData &td) : QSharedData() { memcpy(pixels, td.pixels, sizeof pixels); _count.fetchAndAddOrdered(1); }
TileData::~TileData() { _count.fetchAndAddOrdered(-1); }
#endif

}
