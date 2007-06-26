/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
#include "../config.h"

namespace drawingboard {

/**
 * \brief Linear interpolation
 * This is used to figure out correct radius, opacity, hardness and
 * color values.
 */
static inline qreal interpolate(qreal a, qreal b, qreal alpha)
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
	sensitive_(false), cachepressure_(-1)
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
	cachepressure_ = -1;
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
	cachepressure_ = -1;
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
	cachepressure_ = -1;
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
	cachepressure_ = -1;
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
	cachepressure_ = -1;
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
	cachepressure_ = -1;
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
 * A brush is basically an image filled with a single color and an alpha
 * channel that defines its shape.
 * @return brush image
 */
void Brush::updateCache() const
{
	const int rad = radius(cachepressure_);
	const qreal o = opacity(cachepressure_);
	const int oversample = 2;

	cache_.resize(rad*rad*4);

	// Compute a lookup table
	ushort lookup[rad*oversample];
	const int grad = int((1 - hardness(cachepressure_)) * rad * oversample);
	int i=0;
	for(; i < grad ; ++i)
		lookup[i] = int(256 * i/qreal(grad) * o);
	for(; i < rad*oversample; ++i)
		lookup[i] = int(256 * o);

	// Generate an alpha map for the brush.
	// Only one quarter of the pixels are unique.
	// Quarter 1 is top left, quarter 2 is top right,
	// quarter 3 is bottom left and quarter 4 is bottom right.
	const uint scanline = rad*2 - 1;
	const uint scanline15= rad*3;
	const uint scanline2 = rad;
	ushort *q1 = cache_.data();
	ushort *q2 = q1 + scanline;
	ushort *q3 = q1 + (rad*2) * ((rad*2)-1);
	ushort *q4 = q3 + scanline;
	for(qreal y=-rad+0.5;y<=0;++y) {
		const qreal yy = y*y;
		for(qreal x=-rad+0.5;x<=0;++x) {
			const int dist = int(sqrt(x*x + yy)+.5);
			const ushort a = (dist<rad ? lookup[rad*oversample-dist*oversample] : 0);
			*(q1++) = *(q2--) = *(q3++) = *(q4--) = a;
		}
		q1 += scanline2;
		q2 += scanline15;
		q3 -= scanline15;
		q4 -= scanline2;
	}
}

/**
 * The brush will be drawn centered at \a pos
 * @param image image to draw on
 * @param pos coordinates with pressure
 */
void Brush::draw(QImage &image, const Point& pos) const
{
	const int rad = radius(pos.pressure());
	const int dia = rad * 2;
	const int cx = pos.x()-rad;
	const int cy = pos.y()-rad;

	const QColor col = color(pos.pressure());
	const int red = col.red();
	const int green = col.green();
	const int blue= col.blue();

	// Make sure we are inside the image
	const uint offx = cx<0 ? -cx : 0;
	const uint offy = cy<0 ? -cy : 0;
	uchar *dest = image.bits() + ((cy+offy)*image.width()+cx+offx)*4;
	
	// for avoiding retyping same calculation over and over
	#define CALCULATE_COLOR(color, target, alpha) \
		alpha * (color - target) / 256
	
	// Special case, single pixel brush
	if(dia==0) {
		if(offx==0 && offy==0 &&
				pos.x() < image.width() && pos.y() < image.height()) {
			const int a = int(opacity(pos.pressure())* hardness(pos.pressure())*256);
			#ifdef IS_BIG_ENDIAN
			++dest;
			*dest += CALCULATE_COLOR(red, *dest, a); ++dest;
			*dest += CALCULATE_COLOR(green, *dest, a); ++dest;
			*dest += CALCULATE_COLOR(blue, *dest, a); ++dest;
			#else
			*dest += CALCULATE_COLOR(blue, *dest, a); ++dest;
			*dest += CALCULATE_COLOR(green, *dest, a); ++dest;
			*dest += CALCULATE_COLOR(red, *dest, a);
			#endif
		}
		return;
	}

	// Update brush cache if out of date
	if(cachepressure_<0 ||
			(sensitive_ && (rad!=radius(cachepressure_) ||
							qAbs(pos.pressure()-cachepressure_) > 1.0/256.0))) {
		cachepressure_ = pos.pressure();
		updateCache();
	}
	
	const ushort *src = cache_.constData() + offy*dia + offx;
	const uint nextline = (image.width() - dia + offx) * 4;
	
	const int w = (cx+dia)>image.width()?image.width()-cx:dia;
	const int h = (cy+dia)>image.height()?image.height()-cy:dia;
	
	// Composite brush on image
	for(int y=offy;y<h;++y) {
		for(int x=offx;x<w;++x) {
			const int a = *(src++);
			#ifdef IS_BIG_ENDIAN
			++dest;
			*dest += CALCULATE_COLOR(red, *dest, a); ++dest;
			*dest += CALCULATE_COLOR(green, *dest, a); ++dest;
			*dest += CALCULATE_COLOR(blue, *dest, a); ++dest;
			#else
			*dest += CALCULATE_COLOR(blue, *dest, a); ++dest;
			*dest += CALCULATE_COLOR(green, *dest, a); ++dest;
			*dest += CALCULATE_COLOR(red, *dest, a); ++dest;
			++dest;
			#endif
		}
		dest += nextline + (dia-w)*4;
		src += offx + dia-w;
	}
	
	#undef CALCULATE_COLOR // remove macro function
}

/**
 * This copy operator tries to preserve the brush cache.
 * If both brushes contain equal parameters (color excluded)
 * and the brush that is being overwritten (but not the other one)
 * has a cached brush image, then the old cache is preserved.
 * @param brush brush to replace this with
 */
Brush& Brush::operator=(const Brush& brush)
{
	if(radius1_ != brush.radius1_ || radius2_ != brush.radius2_ ||
		qAbs(hardness1_ - brush.hardness1_) >= 1.0/256.0 ||
		qAbs(hardness2_ - brush.hardness2_) >= 1.0/256.0 ||
		qAbs(opacity1_ - brush.opacity1_) >= 1.0/256.0 ||
		qAbs(opacity2_ - brush.opacity2_) >= 1.0/256.0 ||
		cachepressure_<0 || brush.cachepressure_>=0)
	{
		cachepressure_ = brush.cachepressure_;
		cache_ = brush.cache_;
	}
	
	radius1_ = brush.radius1_ ;
	radius2_ = brush.radius2_ ;
	hardness1_ = brush.hardness1_;
	hardness2_ = brush.hardness2_;
	opacity1_ = brush.opacity1_;
	opacity2_ = brush.opacity2_;
	color1_ = brush.color1_;
	color2_ = brush.color2_;
	sensitive_ = brush.sensitive_;
	spacing_ = brush.spacing_;
	
	return *this;
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

}
