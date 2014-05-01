/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
	return (brush.radius1() << 8) | (brush.radius2() << 16) |
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
		brush.radius1() != brush.radius2() ||
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
			float radius = brush.fradius(p);
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
		// Shape not affected by pressure: only lookup table needed
		float radius = brush.fradius(1.0);
		int len = ceil(radius*radius);
		_lut.resize(len);
		_index.append(len);
		_radius.append(radius);
		_buildLUT(radius, brush.opacity1(), brush.hardness1(), len, _lut.data());
	}
}

BrushMask BrushMaskGenerator::make(float pressure) const
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
	BrushMask *cached = _cache[p];
	if(cached)
		return *cached;

	const int diameter = int(r*2) + 1;

	QVector<uchar> data;
	data.resize(square(diameter));
	uchar *ptr = data.data();

	for(int y=0;y<diameter;++y) {
		const qreal yy = square(y-r+0.5);
		for(int x=0;x<diameter;++x) {
			const int dist = int(square(x-r+0.5) + yy);
			*(ptr++) = dist<lut_len ? lut[dist] : 0;
		}
	}

	BrushMask *bm = new BrushMask(diameter, data);
	_cache.insert(p, bm);

	return *bm;
}

BrushMask BrushMaskGenerator::make(float xfrac, float yfrac, float pressure) const
{
	Q_ASSERT(xfrac>=0 && xfrac<=1);
	Q_ASSERT(yfrac>=0 && yfrac<=1);

	const BrushMask mask = make(pressure);
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
