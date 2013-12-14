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

#include <QDebug>
#include <QImage>
#include <QPainter>

#include "tile.h"
#include "rasterop.h"

namespace dpcore {

Tile::Tile() :
	_data(0)
{
}

Tile::Tile(const QColor& color)
	: _data(new TileData)
{
	quint32 *ptr = _data->data;
	quint32 col = color.rgba();
	for(int i=0;i<SIZE*SIZE;++i)
		*(ptr++) = col;
}

/**
 * -Copy all pixel data from (x*SIZE-xoff, y*SIZE-yoff, (x+1)*SIZE-xoff, (y+1)*SIZE-yoff).
 * -Pixels outside the source image are set to zero
 * Copy pixel data from (xoff, yoff, min(xoff+SIZE, image.width()), min(yoff+SIZE, image.height()))
 * Pixels outside the image will be set to zero
 *
 * @param image source image
 * @param xoff source image offset
 * @param yoff source image offset
 */
Tile::Tile(const QImage& image, int xoff, int yoff)
	: _data(new TileData)
{
#if 0
	// Tile top-left coordinates relative to layer origin
	const int x = xi * SIZE;
	const int y = yi * SIZE;

	// Work area inside the tile
	const int top = yoff < y ? 0 : yoff - y;
	const int left = xoff < x ? 0 : xoff - x;
	const int bottom = (yoff + image.height() - y) > SIZE ? SIZE : (yoff + image.height() - y);
	const int right = (xoff + image.width() - x) > SIZE ? SIZE : (xoff + image.width() - x);

	// If we are not writing the whole tile, initialize memory first
	if(top || left || bottom<SIZE || right<SIZE) 
		memset(_data->data, 0, BYTES);

	// Copy pixels from source area
	uchar *dest = reinterpret_cast<uchar*>(_data->data) + (SIZE * 4 * top) + (4 * left);
	for(int yy=top;yy<bottom;++yy) {
		const uchar *pixels = image.scanLine(y + yy - yoff) + (x+left-xoff)*4;
		memcpy(dest, pixels, (right-left)*4);
		dest += SIZE*4;
	}
#else
	Q_ASSERT(xoff>=0 && xoff < image.width());
	Q_ASSERT(yoff>=0 && yoff < image.height());
	Q_ASSERT(image.format() == QImage::Format_ARGB32);

	const int w = xoff + SIZE > image.width() ? image.width() - xoff : SIZE;
	const int h = yoff + SIZE > image.height() ? image.height() - yoff : SIZE;

	uchar *ptr = reinterpret_cast<uchar*>(_data->data);
	memset(ptr, 0, BYTES);

	const uchar *src = image.scanLine(yoff) + xoff*4;
	for(int y=0;y<h;++y) {
		memcpy(ptr, src, w*4);
		ptr += SIZE*4;
		src += image.bytesPerLine();
	}
#endif
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
	fillChecker(getOrCreateUninitializedData(), dark, light);
}

void Tile::fillColor(const QColor& color)
{
	const quint32 c = color.rgba();
	quint32 *ptr = getOrCreateUninitializedData();
	for(int i=0;i<SIZE*SIZE;++i)
		*(ptr++) = c;
}

void Tile::blank()
{
	_data = 0;
}

void Tile::copyTo(quint32 *data) const
{
	if(isNull()) {
		for(int i=0;i<SIZE*SIZE;++i)
			*(data++) = 0;
	} else {
		const quint32 *ptr = _data->data;
		for(int i=0;i<SIZE*SIZE;++i,++data,++ptr)
			*data = *ptr;
	}

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
		const quint32 *ptr = _data->data;
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
void Tile::composite(int mode, const uchar *values, const QColor& color, int x, int y, int w, int h, int skip)
{
	Q_ASSERT(x>=0 && x<SIZE && y>=0 && y<SIZE);
	Q_ASSERT((x+w)<=SIZE && (y+h)<=SIZE);
	compositeMask(mode, getOrCreateData() + y * SIZE + x,
			color.rgba(), values, w, h, skip, SIZE-w);
}

/**
 * @param tile the tile which will be composited over this tile
 * @param opacity opacity modifier of tile
 * @param blend blending mode
 * @return true if there were any pixels to merge
 */
bool Tile::merge(const Tile &tile, uchar opacity, int blend)
{
	if(!tile.isNull()) {
		compositePixels(blend, getOrCreateData(), tile.data(), SIZE*SIZE, opacity);
		return true;
	}
	return false;
}

/**
 * @return true if every pixel of this tile has an alpha value of zero
 */
bool Tile::isBlank() const
{
	if(isNull())
		return true;

	const quint32 *pixel = _data->data;
	const quint32 *end = pixel + SIZE*SIZE;
	while(pixel<end) {
		if((*pixel & 0xff000000))
			return false;
		++pixel;
	}
	return true;
}

void Tile::optimize()
{
	if(!isNull() && isBlank())
		_data = 0;
}

}
