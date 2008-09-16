/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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
#include <QImage>

#include <cmath>
#include "point.h"
#include "brush.h"
//#include "../../config.h"

namespace dpcore {

/**
 * \brief Linear interpolation
 * This is used to figure out correct radius, opacity, hardness and
 * color values.
 */
inline qreal interpolate(qreal a, qreal b, qreal alpha)
{
	return a*alpha + b*(1-alpha);
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
	: radius1_(radius), radius2_(radius),
	hardness1_(hardness), hardness2_(hardness),
	opacity1_(opacity), opacity2_(opacity),
	color1_(color), color2_(color), spacing_(spacing),
	sensitive_(false), cache_(new BrushCache())
{
	Q_ASSERT(radius>=0);
	Q_ASSERT(hardness>=0 && hardness <=1);
	Q_ASSERT(opacity>=0 && opacity <=1);
	Q_ASSERT(spacing>=0 && spacing <=100);
}

/**
 * Set the radius for pressure=1.0
 * @param radius brush radius. 0 means single pixel brush
 * @pre 0 <= radius
 */
void Brush::setRadius(int radius)
{
	Q_ASSERT(radius>=0);
	radius1_ = radius;
	checkSensitivity();
	cache_->invalidate();
}

/**
 * Set the hardness for pressure=1.0
 * @param hardness brush hardness
 * @pre 0 <= hardness <= 1
 */
void Brush::setHardness(qreal hardness)
{
	Q_ASSERT(hardness>=0 && hardness<=1);
	hardness1_ = hardness;
	checkSensitivity();
	cache_->invalidate();
}

/**
 * Set the opacity for pressure=1.0
 * @param opacity brush opacity
 * @pre 0 <= opacity <= 1
 */
void Brush::setOpacity(qreal opacity)
{
	Q_ASSERT(opacity>=0 && opacity<=1);
	opacity1_ = opacity;
	checkSensitivity();
	cache_->invalidate();
}

/**
 * Set the color for pressure=1.0
 * @param color brush color.
 */
void Brush::setColor(const QColor& color)
{
	color1_ = color;
}


/**
 * Set the radius for pressure=0.0
 * @param radius brush radius. 0 means single pixel brush
 * @pre 0 <= radius
 */
void Brush::setRadius2(int radius)
{
	Q_ASSERT(radius>=0);
	radius2_  = radius;
	checkSensitivity();
	cache_->invalidate();
}

/**
 * Set the hardness for pressure=0.0
 * @param hardness brush hardness
 * @pre 0 <= hardness <= 1
 */
void Brush::setHardness2(qreal hardness)
{
	Q_ASSERT(hardness>=0 && hardness<=1);
	hardness2_ = hardness;
	checkSensitivity();
	cache_->invalidate();
}

/**
 * Set the opacity for pressure=0.0
 * @param opacity brush hardness
 * @pre 0 <= opacity <= 1
 */
void Brush::setOpacity2(qreal opacity)
{
	Q_ASSERT(opacity>=0 && opacity<=1);
	opacity2_ = opacity;
	checkSensitivity();
	cache_->invalidate();
}

/**
 * Set the color for pressure=0.0
 * @param color brush color.
 */
void Brush::setColor2(const QColor& color)
{
	color2_ = color;
}

/**
 * @param spacing brush spacing is a percentage of brush radius
 * @pre 0 <= spacing <= 100
 */
void Brush::setSpacing(int spacing)
{
	Q_ASSERT(spacing >= 0 && spacing <= 100);
	spacing_ = spacing;
}

/**
 * Sets sensitive_ if brush can change its shape or color according
 * to the pressure. Insensitive brushes can be cached more easily.
 */
void Brush::checkSensitivity()
{
	sensitive_ = radius1_ != radius2_ ||
			qAbs(hardness1_ - hardness2_) >= 1.0/256.0 ||
			qAbs(opacity1_ - opacity2_) >= 1.0/256.0;
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
	Q_ASSERT(pressure>=0 && pressure<=1);
	return unsigned(ceil(interpolate(radius1_, radius2_, pressure)));
}

/**
 * Get the diameter of the brush for certain pressure.
 * @param pressure pen pressure
 * @return diameter
 * @post 0 < RESULT
 */
int Brush::diameter(qreal pressure) const
{
	int rad = radius(pressure);
	return rad==0?1:rad*2;
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
	Q_ASSERT(pressure>=0 && pressure<=1);
	return interpolate(hardness1_, hardness2_, pressure);
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
	Q_ASSERT(pressure>=0 && pressure<=1);
	return interpolate(opacity1_, opacity2_, pressure);
}

/**
 * Get the brush color for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return color
 * @pre 0 <= pressure <= 1
 */
QColor Brush::color(qreal pressure) const
{
	Q_ASSERT(pressure>=0 && pressure<=1);
	
	return QColor(
			qRound(interpolate(color1_.red(), color2_.red(), pressure)),
			qRound(interpolate(color1_.green(), color2_.green(), pressure)),
			qRound(interpolate(color1_.blue(), color2_.blue(), pressure))
			);


}

/**
 * Spacing is not used internally by the brush, but is provided here because
 * it is closely related to how the brush is drawn.
 * @return spacing as a percentage of brush radius
 * @post 0 <= RESULT <= 100
 */
int Brush::spacing() const
{
	return spacing_;
}

/**
 * Returns the value of each pixel of the brush. It is up to you to blend
 * the color in. A pointer to internal memory is returned, which will be
 * invalidated when render is called with a different pressure or when
 * the brush object is destroyed.
 * @param pressure brush pressue [0..1]
 * @return diameter^2 pixel values
 */
const uchar *Brush::render(qreal pressure) const {
	const int dia = diameter(pressure);
	const qreal o = opacity(pressure);

	// Return cache if we have one
	if(cache_.constData()!=0 && cache_->isFresh(pressure, sensitive_))
		return cache_->data();

	const qreal rad = dia/2.0;

	// Compute a lookup table
	static const int OVERSAMPLE=2;
	uchar lookup[int(rad+0.5)*OVERSAMPLE];
	const int grad = int((1 - hardness(pressure)) * rad * OVERSAMPLE);
	int i=0;
	for(; i < grad ; ++i)
		lookup[i] = int(255 * i/qreal(grad) * o);
	for(; i < rad*OVERSAMPLE; ++i)
		lookup[i] = int(255 * o);

	// Render the brush
	uchar *values = cache_->newCache(dia*dia, pressure);
	uchar *ptr = values;
	for(qreal y=-rad;y<rad;++y) {
		const qreal yy = y*y;
		for(qreal x=-rad;x<rad;++x) {
			const qreal dist = int(sqrt(x*x + yy));
			*(ptr++) = (dist<rad?lookup[int(rad*OVERSAMPLE-dist*OVERSAMPLE)]:0);
		}
	}
	return values;
}

bool Brush::operator==(const Brush& brush) const
{
	return radius1_ == brush.radius1_ && radius2_ == brush.radius2_ &&
			qAbs(hardness1_ - brush.hardness1_) <= 1.0/256.0 &&
			qAbs(hardness2_ - brush.hardness2_) <= 1.0/256.0 &&
			qAbs(opacity1_ - brush.opacity1_) <= 1.0/256.0 &&
			qAbs(opacity2_ - brush.opacity2_) <= 1.0/256.0 &&
			color1_ == brush.color1_ &&
			color2_ == brush.color2_ &&
			spacing_ == brush.spacing_;
}

bool Brush::operator!=(const Brush& brush) const
{
	return !(*this == brush);
}

BrushCache::BrushCache(uchar *data, int len, qreal pressure)
	: data_(data), len_(len), pressure_(int(PRESSURE_LEVELS*pressure)) { }

BrushCache::BrushCache(const BrushCache& other)
	: QSharedData(other), len_(other.len_), pressure_(other.pressure_)
{
	if(len_<=0)
		data_ = 0;
	else {
		data_ = new uchar[len_];
		memcpy(data_, other.data_, len_);
	}
}

BrushCache::~BrushCache()
{
	delete [] data_;
}

bool BrushCache::isFresh(qreal pressure, bool sensitive) const {
	if(sensitive)
		return len_>0 && int(pressure*PRESSURE_LEVELS)==pressure_;
	else
		return len_>0;
}

uchar *BrushCache::newCache(int len, qreal pressure) {
	if(len > len_) {
		pressure_ = int(pressure * PRESSURE_LEVELS);
		len_ = len;
		delete data_;
		data_ = new uchar[len];
	}
	return data_;
}

}
