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

#include <cmath>

#include "brush.h"
#include "brushmask.h"

namespace paintcore {

namespace {

template<typename T> T square(T x) { return x*x; }

typedef quint64 BrushCacheKey;
static QCache<BrushCacheKey, BrushMaskGenerator> BMG_CACHE(10);

BrushCacheKey brushCacheKey(const Brush &brush) {
	// the cache key includes only the parameters that affect mask generation
	return (brush.size1() << 8) | (brush.size2() << 16) |
			(quint64(brush.hardness1()*255) << 24) | (quint64(brush.hardness2()*255) << 32) |
			(quint64(brush.opacity1()*255) << 40) | (quint64(brush.opacity2()*255) << 48);
}

}

static const int PRESSURE_LEVELS = 256;

inline float int2pressure(int pressure) {
	return pressure / float(PRESSURE_LEVELS-1);
}

inline int pressure2int(float pressure) {
	Q_ASSERT(pressure>=0.0);
	Q_ASSERT(pressure<=1.0);
	return qBound(0, int(pressure * (PRESSURE_LEVELS-1)), PRESSURE_LEVELS-1);
}

const BrushMaskGenerator &BrushMaskGenerator::cached(const Brush &brush)
{
	BrushCacheKey key = brushCacheKey(brush);
	BrushMaskGenerator *bmg = BMG_CACHE[key];
	if(!bmg) {
		bmg = new BrushMaskGenerator(brush);
		BMG_CACHE.insert(key, bmg);
	}
	return *bmg;
}

BrushMaskGenerator::BrushMaskGenerator()
{
}

BrushMaskGenerator::BrushMaskGenerator(const Brush &brush)
	: _cache(10)
{
	buildLUT(brush);
}

BrushStamp BrushMaskGenerator::make(float x, float y, float pressure, bool subpixel) const
{
	BrushStamp s;

	if(subpixel) {

		// optimization: don't bother with a high resolution mask for large brushes
		float radius = _radius.at(_usepressure ? pressure2int(pressure) : 0);

		if(radius < 4)
			s = makeHighresMask(pressure);
		else
			s = makeMask(pressure);


		const float fx = floor(x);
		const float fy = floor(y);
		s.left += fx;
		s.top += fy;

		float xfrac = x-fx;
		float yfrac = y-fy;

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
		s = makeMask(pressure);
		s.left += x;
		s.top += y;
	}

	return s;
}

void _buildLUT(float radius, float opacity, float hardness, int len, uchar *lut)
{
	// GIMP style brush shape
	qreal exponent;
	if ((1.0 - hardness) < 0.0000004)
		exponent = 1000000.0;
	else
		exponent = 0.4 / (1.0 - hardness);

	for(int i=0;i<len;++i,++lut)
		*lut = (1-pow(pow(sqrt(i)/radius, exponent), 2)) * opacity * 255;
}


void BrushMaskGenerator::buildLUT(const Brush &brush)
{
	// First, determine if the brush shape depends on pressure
	_usepressure =
		brush.size1() != brush.size2() ||
		qAbs(brush.hardness1() - brush.hardness2()) >= (1/256.0) ||
		qAbs(brush.opacity1() - brush.opacity2()) >= (1/256.0)
		;

	if(_usepressure) {
		// Build a lookup table for all pressure levels
		uint len = 0;
		_index.reserve(PRESSURE_LEVELS + 1);
		_radius.reserve(PRESSURE_LEVELS);
		_index.append(0);

		for(int i=0;i<PRESSURE_LEVELS;++i) {
			float p = int2pressure(i);
			float radius = brush.fsize(p)/2.0;
			_radius.append(radius);
			len += ceil(radius*radius);
			_index.append(len);

		}
		_lut.resize(len);

		for(int i=0;i<PRESSURE_LEVELS;++i) {
			float p = int2pressure(i);
			_buildLUT(_radius.at(i), brush.opacity(p), brush.hardness(p), _index.at(i+1)-_index.at(i), _lut.data() + _index.at(i));
		}
	} else {
		// Shape not affected by pressure: only one lookup table needed
		float radius = brush.size1()/2.0;
		int len = ceil(radius*radius);
		_lut.resize(len);
		_index.append(len);
		_radius.append(radius);
		_buildLUT(radius, brush.opacity1(), brush.hardness1(), len, _lut.data());
	}
}

BrushStamp BrushMaskGenerator::makeMask(float pressure) const
{
	float r;
	int lut_len;
	const uchar *lut;
	int p;
	if(_usepressure) {
		p = pressure2int(pressure);
		lut = _lut.data() + _index.at(p);
		lut_len = _index.at(p+1) - _index.at(p);
		r = _radius.at(p);
	} else {
		p = 255;
		lut = _lut.data();
		lut_len = _index.at(0);
		r = _radius.at(0);
	}

	// check cache first
	if(_cache.contains(p))
		return *_cache[p];

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
		data[4] = lut[0];

	} else {
		float offset;
		float fudge=1;
		diameter = ceil(r*2) + 2;

		if(diameter%2==0) {
			++diameter;
			offset = -1.0;
		} else {
			offset = -0.5;
		}
		stampOffset = -diameter/2;

		// empirically determined fudge factors to make small brushes look nice
		if(r<1.5)
			fudge=0.9;
		else if(r<2)
			fudge=0.9;
		else if(r<2.5)
			fudge=0.6;
		else if(r<3)
			fudge=0.8;

		data.resize(square(diameter));
		uchar *ptr = data.data();

		for(int y=0;y<diameter;++y) {
			const qreal yy = square(y-r+offset);
			for(int x=0;x<diameter;++x) {
				const int dist = int((square(x-r+offset) + yy) * fudge);
				*(ptr++) = dist<lut_len ? lut[qMin(lut_len-1,dist)] : 0;
			}
		}
	}

	BrushStamp *bs = new BrushStamp(stampOffset, stampOffset, BrushMask(diameter, data));
	_cache.insert(p, bs);

	return *bs;
}

BrushStamp BrushMaskGenerator::makeHighresMask(float pressure) const
{
	float r;
	int lut_len;
	const uchar *lut;
	if(_usepressure) {
		int p = pressure2int(pressure);
		lut = _lut.data() + _index.at(p);
		lut_len = _index.at(p+1) - _index.at(p);
		r = _radius.at(p);
	} else {
		lut = _lut.data();
		lut_len = _index.at(0);
		r = _radius.at(0);
	}

	// we calculate a double sized brush and downsample
	r *= 2;

	// generate mask
	int diameter;
	int stampOffset;

	float offset;
	float fudge=1;
	diameter = ceil(r) + 2; // abstract brush is double size, but target diameter is normal

	if(diameter%2==0) {
		++diameter;
		offset = -2.5;
	} else {
		offset = -1.5;
	}
	stampOffset = -diameter/2;

	// empirically determined fudge factors to make small brushes look nice
	if(r<1.3)
		fudge=2;

	QVector<uchar> data(square(diameter));
	uchar *ptr = data.data();

	for(int y=0;y<diameter;++y) {
		const qreal yy0 = square(y*2-r+offset);
		const qreal yy1 = square(y*2+1-r+offset);

		for(int x=0;x<diameter;++x) {
			const qreal xx0 = square(x*2-r+offset);
			const qreal xx1 = square(x*2+1-r+offset);

			const int dist00 = int((xx0 + yy0) * fudge / 4.0);
			const int dist01 = int((xx0 + yy1) * fudge / 4.0);
			const int dist10 = int((xx1 + yy0) * fudge / 4.0);
			const int dist11 = int((xx1 + yy1) * fudge / 4.0);

			*(ptr++) =
					((dist00<lut_len ? lut[dist00] : 0) +
					(dist01<lut_len ? lut[dist01] : 0) +
					(dist10<lut_len ? lut[dist10] : 0) +
					(dist11<lut_len ? lut[dist11] : 0)) / 4
					;

		}
	}

	return BrushStamp(stampOffset, stampOffset, BrushMask(diameter, data));
}

BrushMask BrushMaskGenerator::offsetMask(const BrushMask &mask, float xfrac, float yfrac) const
{
	Q_ASSERT(xfrac>=0 && xfrac<=1);
	Q_ASSERT(yfrac>=0 && yfrac<=1);

	const int diameter = mask.diameter();

	const qreal kernel[] = {
		xfrac*yfrac,
		(1.0-xfrac)*yfrac,
		xfrac*(1.0-yfrac),
		(1.0-xfrac)*(1.0-yfrac)
	};
	Q_ASSERT(fabs(kernel[0]+kernel[1]+kernel[2]+kernel[3]-1.0)<0.001);
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
