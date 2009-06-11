/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2009 Calle Laakkonen

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

#include <QDebug>
#include <QImage>
#include <QPainter>

#include "tile.h"
#include "rasterop.h"

namespace dpcore {

Tile::Tile(const QColor& color, int x, int y)
	: x_(x), y_(y), data_(new quint32[SIZE*SIZE])
{
	quint32 *ptr = data_;
	quint32 col = color.rgba();
	for(int i=0;i<SIZE*SIZE;++i)
		*(ptr++) = col;
}

Tile::Tile(int x, int y)
	: x_(x), y_(y), data_(new quint32[SIZE*SIZE])
{
	memset(data_, 0, SIZE*SIZE*sizeof(quint32));
}

Tile::Tile(const Tile *src)
	: x_(src->x_), y_(src->y_), data_(new quint32[SIZE*SIZE])
{
	memcpy(data_, src->data_, BYTES);
}

/**
 * Copy all pixel data from (x*SIZE, y*SIZE, (x+1)*SIZE, (y+1)*SIZE).
 * Pixels outside the source image are set to zero
 */
Tile::Tile(const QImage& image, int x, int y)
	: x_(x), y_(y), data_(new quint32[SIZE*SIZE])
{
	const uchar *pixels = image.scanLine(y*SIZE) + x*SIZE*4;
	uchar *data = reinterpret_cast<uchar*>(data_);
	const int w = 4*((image.width() - x*SIZE)<SIZE?image.width()-x*SIZE:SIZE);
	const int h = (image.height() - y*SIZE)<SIZE?image.height()-y*SIZE:SIZE;
	for(int yy=0;yy<h;++yy) {
		memcpy(data, pixels, w);
		memset(data+w, 0, SIZE*4-w);
		pixels += image.bytesPerLine();
		data += SIZE*4;
	}
	memset(data, 0, (SIZE-h)*SIZE*4);
}

Tile::~Tile() {
	delete [] data_;
}

void Tile::fillChecker(quint32 *data, const QColor& dark, const QColor& light)
{
	const int HALF = SIZE/2;
	quint32 d = dark.rgba();
	quint32 l = light.rgba();
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

void Tile::fillChecker(const QColor& dark, const QColor& light)
{
	fillChecker(data_, dark, light);
}

void Tile::fillColor(const QColor& color)
{
	const quint32 c = color.rgba();
	quint32 *ptr = data_;
	for(int i=0;i<SIZE*SIZE;++i)
		*(ptr++) = c;
}

void Tile::copyToImage(QImage& image) const {
	int w = 4*(image.width()-x_*SIZE<SIZE?image.width()-x_*SIZE:SIZE);
	int h = image.height()-y_*SIZE<SIZE?image.height()-y_*SIZE:SIZE;
	const quint32 *ptr = data_;
	uchar *targ = image.bits() + (y_ * SIZE) * image.bytesPerLine() + (x_ * SIZE) * 4;
	for(int y=0;y<h;++y) {
		memcpy(targ, ptr, w);
		targ += image.bytesPerLine();
		ptr += SIZE;
	}
}

/**
 * X and Y coordinates must be in range [0..SIZE[
 * @param x
 * @param y
 */
quint32 Tile::pixel(int x,int y) const {
	return *(data_ + y * SIZE + x);
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
void Tile::composite(int mode, const uchar *values, const QColor& color, int x, int y, int w, int h, int skip)
{
	Q_ASSERT(x>=0 && x<SIZE && y>=0 && y<SIZE);
	Q_ASSERT((x+w)<=SIZE && (y+h)<=SIZE);
	compositeMask(mode, data_ + y * SIZE + x,
			color.rgba(), values, w, h, skip, SIZE-w);
}

/**
 * @param tile the tile which will be composited over this tile
 * @param opacity opacity modifier of tile
 */
void Tile::merge(const Tile *tile, uchar opacity)
{
	if(tile!=0)
		compositePixels(1, data_, tile->data_, SIZE*SIZE, opacity);
}

}

