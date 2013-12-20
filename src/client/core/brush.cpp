/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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

#include <QtGlobal>

#include <cmath>
#include "point.h"
#include "brush.h"

namespace paintcore {

/**
 * \brief Linear interpolation
 * This is used to figure out correct radius, opacity, hardness and
 * color values.
 */
inline qreal interpolate(qreal a, qreal b, qreal alpha)
{
	Q_ASSERT(alpha>=0 && alpha<=1);
	//return a*alpha + b*(1-alpha);
	return (a-b) * alpha + b;
}

/**
 * A brush with the specified settings is constructed. The brush is
 * pressure insensitive by default.
 * @param radius brush radius. Zero means a single pixel brush
 * @param hardness brush hardness
 * @param opacity brush opacity
 * @param color brush color
 * @param spacing brush spacing hint, as percentage of brush radius
 */
Brush::Brush(int radius, qreal hardness, qreal opacity, const QColor& color, int spacing)
	: _radius1(radius), _radius2(radius),
	_hardness1(hardness), _hardness2(hardness),
	_opacity1(opacity), _opacity2(opacity),
	_color1(color), _color2(color), _spacing(spacing), _blend(1),
	_subpixel(false), _incremental(true)
{
	Q_ASSERT(radius>=0);
	Q_ASSERT(hardness>=0 && hardness <=1);
	Q_ASSERT(opacity>=0 && opacity <=1);
	Q_ASSERT(spacing>=0 && spacing <=100);
}

/**
 * Get the brush radius for certain pressure.
 * @param pressure pen pressure
 * @return radius
 * @pre 0 <= pressure <= 1
 * @post 0 <= RESULT
 */
int Brush::radius(qreal pressure) const
{
	return ceil(interpolate(radius1(), radius2(), pressure));
}

/**
 * @brief Get the real radius for the given pressure
 * @param pressure
 * @return radius
 * @pre 0 <= pressure <= 1
 * @post 0.5 <= RESULT
 */
qreal Brush::fradius(qreal pressure) const
{
	return qMax(0.5, interpolate(radius1(), radius2(), pressure));
}

/**
 * Get the diameter of the brush for certain pressure.
 * @param pressure pen pressure
 * @return diameter
 * @post 0 < RESULT
 */
int Brush::diameter(qreal pressure) const
{
	return qMax(0.5, interpolate(radius1(), radius2(), pressure))*2;
}

/**
 * Get the brush hardness for certain pressure.
 * @param pressure pen pressure
 * @return hardness
 * @pre 0 <= pressure <= 1
 * @post 0 <=  RESULT <= 1
 */
qreal Brush::hardness(qreal pressure) const
{
	return interpolate(hardness1(), hardness2(), pressure);
}

/**
 * Get the brush opacity for certain pressure.
 * @param pressure pen pressure
 * @return opacity
 * @pre 0 <= pressure <= 1
 * @post 0 <= RESULT <= 1
 */
qreal Brush::opacity(qreal pressure) const
{
	return interpolate(opacity1(), opacity2(), pressure);
}

/**
 * Get the brush color for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return color
 * @pre 0 <= pressure <= 1
 */
QColor Brush::color(qreal pressure) const
{
	return QColor(
		qRound(interpolate(color1().red(), color2().red(), pressure)),
		qRound(interpolate(color1().green(), color2().green(), pressure)),
		qRound(interpolate(color1().blue(), color2().blue(), pressure))
	);
}

bool Brush::isOpacityVariable() const
{
	return qAbs(opacity1() - opacity2()) > (1/256.0);
}

#if 0

/**
 * Returns the value of each pixel of the brush. It is up to you to blend
 * the color in. A pointer to internal memory is returned, which will be
 * invalidated when render is called with a different pressure or when
 * the brush object is destroyed.
 * @param pressure brush pressue [0..1]
 * @return diameter^2 pixel values
 */
BrushMask Brush::render(qreal pressure) const {
	const int dia = diameter(pressure)+1;

	// Re-render the cache if not up to date
	if(!cache_.isFresh(pressure, sensitive_)) {
		const qreal o = opacity(pressure);
		if(dia==2) {
			// Special case: smallest brush
			cache_ = BrushMask(dia, pressure);
			uchar *ptr = cache_.data();
			*ptr = o*255;
			*(ptr+1) = 0;
			*(ptr+2) = 0;
			*(ptr+3) = 0;
		} else {
			const qreal R = interpolate(radius1_, radius2_, pressure);
			const qreal rr = R*R;

			// GIMP style brush shape
			const qreal hard = hardness(pressure);
			qreal exponent;
			if ((1.0 - hard) < 0.0000004)
				exponent = 1000000.0;
			else
				exponent = 0.4 / (1.0 - hard);

			const int lut_len = ceil(rr);
			uchar lut[lut_len];
			for(int i=0;i<lut_len;++i)
				lut[i] = (1-pow(pow(sqrt(i)/R, exponent), 2)) * o * 255;

			cache_ = BrushMask(dia, pressure);
			uchar *ptr = cache_.data();

			for(int y=0;y<dia;++y) {
				const qreal yy = (y-R+0.5)*(y-R+0.5);
				for(int x=0;x<dia;++x) {
					const int dist = int((x-R+0.5)*(x-R+0.5) + yy);
					*(ptr++) = dist<lut_len ? lut[dist] : 0;
				}
			}
		}
	}

	return cache_;
}

/**
 * A convolution operation is performed on the brush mask, shifting it
 * south-east by x and y amount.
 *
 * @param x horizontal offset [0..1]
 * @param y vertical offset [0..1]
 * @param pressure brush pressure
 * @return resampled brush mask
 */
BrushMask Brush::render_subsampled(qreal x, qreal y, qreal pressure) const
{
	Q_ASSERT(x>= 0 && x<= 1);
	Q_ASSERT(y>= 0 && y<= 1);
	const BrushMask rb = render(pressure);
	if(x==0 && y==0)
		return rb;
	const int dia = rb.diameter();
	BrushMask b(dia, pressure);

	qreal kernel[] = {x*y, (1.0-x)*y, x*(1.0-y), (1.0-x)*(1.0-y)};
	Q_ASSERT(fabs(kernel[0]+kernel[1]+kernel[2]+kernel[3]-1.0)<0.001);
	const uchar *src = rb.data();
	uchar *ptr = b.data();

#if 0
	for(int y=-1;y<dia-1;++y) {
		const int Y = y*dia;
		for(int x=-1;x<dia-1;++x) {
			Q_ASSERT(Y+dia+x+1<dia*dia);
			*(ptr++) =
				(Y<0?0:(x<0?0:src[Y+x]*kernel[0]) + src[Y+x+1]*kernel[1]) +
				(x<0?0:src[Y+dia+x]*kernel[2]) + src[Y+dia+x+1]*kernel[3];
		}
	}
#else
	// Unrolled version of the above
	*(ptr++) = uchar(src[0] * kernel[3]);
	for(int x=0;x<dia-1;++x)
		*(ptr++) = uchar(src[x]*kernel[2] + src[x+1]*kernel[3]);
	for(int y=0;y<dia-1;++y) {
		const int Y = y*dia;
		*(ptr++) = uchar(src[Y]*kernel[1] + src[Y+dia]*kernel[3]);
		for(int x=0;x<dia-1;++x)
			*(ptr++) = uchar(src[Y+x]*kernel[0] + src[Y+x+1]*kernel[1] +
				src[Y+dia+x]*kernel[2] + src[Y+dia+x+1]*kernel[3]);
	}
#endif
	return b;
}

#endif

bool Brush::operator==(const Brush& brush) const
{
	return radius1() == brush.radius1() && radius2() == brush.radius2() &&
			qAbs(hardness1() - brush.hardness1()) <= 1.0/256.0 &&
			qAbs(hardness2() - brush.hardness2()) <= 1.0/256.0 &&
			qAbs(opacity1() - brush.opacity1()) <= 1.0/256.0 &&
			qAbs(opacity2() - brush.opacity2()) <= 1.0/256.0 &&
			color1() == brush.color1() &&
			color2() == brush.color2() &&
			spacing() == brush.spacing() &&
			subpixel() == brush.subpixel() &&
			incremental() == brush.incremental() &&
			blendingMode() == brush.blendingMode();
}

bool Brush::operator!=(const Brush& brush) const
{
	return !(*this == brush);
}

#if 0

/**
 * Copy the data from an existing brush. A brush data is guaranteed
 * to contain a pixel buffer.
 */
BrushMaskData::BrushMaskData(const BrushMaskData& other)
	: dia(other.dia), pressure(other.pressure)
{
	data = new uchar[dia*dia];
	memcpy(data, other.data, dia*dia);
}

/**
 * The newly created brush is uninitialized, so remember to actually fill
 * it with something!
 * The pressure value is used only for isFresh.
 * @param dia diameter of the new brush
 * @param pressure the pressure value at which the brush was rendered
 */
BrushMask::BrushMask(int dia, qreal pressure)
	: d(new BrushMaskData)
{
	Q_ASSERT(dia>0);
	d->data = new uchar[dia*dia];
	d->dia = dia;
	d->pressure = int(pressure*PRESSURE_LEVELS);
}

/**
 * This is used when determining if a cached brush is still usable.
 * A brush is considred fresh if:
 * 1. it contains data
 * 2. it's pressure value is the same as @parma pressure iff sensitive is true.
 */
bool BrushMask::isFresh(qreal pressure, bool sensitive) const {
	if(sensitive)
		return d.constData() && int(pressure*PRESSURE_LEVELS)==d->pressure;
	else
		return d.constData() && d->pressure>=0;
}
#endif

}
