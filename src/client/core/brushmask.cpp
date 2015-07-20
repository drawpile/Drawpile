/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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

#include "brushmask.h"

#include <QCache>

#include <cmath>

namespace paintcore {

namespace {

template<typename T> T square(T x) { return x*x; }

typedef QVector<float> LUT;
static const int LUT_RADIUS = 128;
static QCache<int, LUT> LUT_CACHE;

// Generate a lookup table for Gimp style exponential brush shape
// The value at r² (where r is distance from brush center, scaled to LUT_RADIUS) is
// the opaqueness of the pixel.
LUT makeGimpStyleBrushLUT(float hardness)
{
	qreal exponent;
	if ((1.0 - hardness) < 0.0000004)
		exponent = 1000000.0;
	else
		exponent = 0.4 / (1.0 - hardness);

	LUT lut(square(LUT_RADIUS));
	for(int i=0;i<lut.size();++i)
		lut[i] = 1-pow(pow(sqrt(i)/LUT_RADIUS, exponent), 2);

	return lut;
}

LUT cachedGimpStyleBrushLUT(float hardness)
{
	const int h = hardness * 100;
	Q_ASSERT(h>=0 && h<=100);
	if(!LUT_CACHE.contains(h))
		LUT_CACHE.insert(h, new LUT(makeGimpStyleBrushLUT(hardness)));

	return *LUT_CACHE[h];
}

BrushStamp makeMask(const Brush &brush, float pressure)
{
	const float r = brush.fsize(pressure) / 2.0f;
	const float opacity = brush.opacity(pressure) * 255;

	// generate mask
	QVector<uchar> data;
	int diameter;
	int stampOffset;

	if(r<1) {
		// special case for single pixel brush
		diameter=3;
		stampOffset = -1;
		data.resize(3*3);
		data.fill(0);
		data[4] = opacity;

	} else {
		const LUT lut = cachedGimpStyleBrushLUT(brush.hardness(pressure));
		const float lut_scale = square((LUT_RADIUS-1) / r);

		float offset;
		float fudge=1;
		diameter = ceil(r*2) + 2;

		if(diameter%2==0) {
			++diameter;
			offset = -1.0;

			if(r<8)
				fudge = 0.9;
		} else {
			offset = -0.5;
		}
		stampOffset = -diameter/2;

		// empirically determined fudge factors to make small brushes look nice
		if(r<2.5)
			fudge=0.8;

		else if(r<4)
			fudge=0.8;

		data.resize(square(diameter));
		uchar *ptr = data.data();

		for(int y=0;y<diameter;++y) {
			const qreal yy = square(y-r+offset);
			for(int x=0;x<diameter;++x) {
				const int dist = int((square(x-r+offset) + yy) * fudge * lut_scale);
				*(ptr++) = dist<lut.size() ? lut.at(dist) * opacity : 0;
			}
		}
	}

	return BrushStamp(stampOffset, stampOffset, BrushMask(diameter, data));
}

BrushStamp makeHighresMask(const Brush &brush, float pressure)
{
	// we calculate a double sized brush and downsample
	const float r = brush.fsize(pressure);
	const float opacity = brush.opacity(pressure) * (255 / 4); // opacity of each subsample

	int diameter = ceil(r) + 2; // abstract brush is double size, but target diameter is normal
	float offset = (ceil(r) - r) / -2;

	if(diameter%2==0) {
		++diameter;
		offset += -2.5;
	} else {
		offset += -1.5;
	}
	const int stampOffset = -diameter/2;

	const LUT lut = cachedGimpStyleBrushLUT(brush.hardness(pressure));
	const float lut_scale = square((LUT_RADIUS-1) / r);

	QVector<uchar> data(square(diameter));
	uchar *ptr = data.data();

	for(int y=0;y<diameter;++y) {
		const qreal yy0 = square(y*2-r+offset);
		const qreal yy1 = square(y*2+1-r+offset);

		for(int x=0;x<diameter;++x) {
			const qreal xx0 = square(x*2-r+offset);
			const qreal xx1 = square(x*2+1-r+offset);

			const int dist00 = int((xx0 + yy0) * lut_scale);
			const int dist01 = int((xx0 + yy1) * lut_scale);
			const int dist10 = int((xx1 + yy0) * lut_scale);
			const int dist11 = int((xx1 + yy1) * lut_scale);

			*(ptr++) =
					((dist00<lut.size() ? lut.at(dist00) : 0) +
					(dist01<lut.size() ? lut.at(dist01) : 0) +
					(dist10<lut.size() ? lut.at(dist10) : 0) +
					(dist11<lut.size() ? lut.at(dist11) : 0)) * opacity
					;

		}
	}

	return BrushStamp(stampOffset, stampOffset, BrushMask(diameter, data));
}

BrushMask offsetMask(const BrushMask &mask, float xfrac, float yfrac)
{
#ifndef NDEBUG
	if(xfrac<0 || xfrac>1 || yfrac<0 || yfrac>1)
		qWarning("offsetMask(mask, %f, %f): offset out of bounds!", xfrac, yfrac);
#endif

	const int diameter = mask.diameter();

	const qreal kernel[] = {
		xfrac*yfrac,
		(1.0-xfrac)*yfrac,
		xfrac*(1.0-yfrac),
		(1.0-xfrac)*(1.0-yfrac)
	};
#ifndef NDEBUG
	const qreal kernelsum = fabs(kernel[0]+kernel[1]+kernel[2]+kernel[3]-1.0);
	if(kernelsum>0.001)
		qWarning("offset kernel sum error=%f", kernelsum);
#endif

	const uchar *src = mask.data();

	QVector<uchar> data(square(diameter));
	uchar *ptr = data.data();

#if 0
	for(int y=-1;y<diameter-1;++y) {
		const int Y = y*diameter;
		for(int x=-1;x<diameter-1;++x) {
			Q_ASSERT(Y+diameter+x+1<diameter*diameter);
			*(ptr++) =
				(Y<0?0:(x<0?0:src[Y+x]*kernel[0]) + src[Y+x+1]*kernel[1]) +
				(x<0?0:src[Y+diameter+x]*kernel[2]) + src[Y+diameter+x+1]*kernel[3];
		}
	}
#else
	// Unrolled version of the above
	*(ptr++) = uchar(src[0] * kernel[3]);
	for(int x=0;x<diameter-1;++x)
		*(ptr++) = uchar(src[x]*kernel[2] + src[x+1]*kernel[3]);
	for(int y=0;y<diameter-1;++y) {
		const int Y = y*diameter;
		*(ptr++) = uchar(src[Y]*kernel[1] + src[Y+diameter]*kernel[3]);
		for(int x=0;x<diameter-1;++x)
			*(ptr++) = uchar(src[Y+x]*kernel[0] + src[Y+x+1]*kernel[1] +
				src[Y+diameter+x]*kernel[2] + src[Y+diameter+x+1]*kernel[3]);
	}
#endif

	return BrushMask(diameter, data);
}

}

BrushStamp makeGimpStyleBrushStamp(const Brush &brush, const Point &point)
{
	BrushStamp s;

	if(brush.subpixel()) {
		// optimization: don't bother with a high resolution mask for large brushes
		float radius = brush.fsize(point.pressure());

		if(radius < 8)
			s = makeHighresMask(brush, point.pressure());
		else
			s = makeMask(brush, point.pressure());

		const float fx = floor(point.x());
		const float fy = floor(point.y());
		s.left += fx;
		s.top += fy;

		float xfrac = point.x()-fx;
		float yfrac = point.y()-fy;

		if(xfrac<0.5) {
			xfrac += 0.5;
			s.left--;
		} else
			xfrac -= 0.5;

		if(yfrac<0.5) {
			yfrac += 0.5;
			s.top--;
		} else
			yfrac -= 0.5;

		s.mask = offsetMask(s.mask, xfrac, yfrac);

	} else {
		s = makeMask(brush, point.pressure());
		s.left += point.x();
		s.top += point.y();
	}

	return s;
}

}
