/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#include "../../config.h"

namespace drawingboard {

#define ADJUST_BY_PRESSURE(var1, var2, pressure) \
	var1 * pressure + var2 * (1.0-pressure)

/**
 * A brush with the specified settings is constructed. The brush is
 * pressure insensitive by default.
 * @param radius brush radius. Zero means a single pixel brush
 * @param hardness brush hardness
 * @param opacity brush opacity
 * @param color brush color
 */
Brush::Brush(int radius, qreal hardness, qreal opacity, const QColor& color)
	: radius1_(radius), radius2_(radius),
	hardness1_(hardness), hardness2_(hardness),
	opacity1_(opacity), opacity2_(opacity),
	color1_(color), color2_(color),
	sensitive_(false), cachepressure_(-1)
{
	Q_ASSERT(radius>=0);
	Q_ASSERT(hardness>=0 && hardness <=1);
	Q_ASSERT(opacity>=0 && opacity <=1);
}

/**
 * Set the radius for pressure=1.0
 * @param radius brush radius. 0 means single pixel brush
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
 * @param hardness brush hardness. Range is [-1..1]
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
 * @param opacity brush opacity. Range is [0..1]
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


void Brush::setRadius2(int radius)
{
	Q_ASSERT(radius>=0);
	radius2_  = radius;
	checkSensitivity();
	cachepressure_ = -1;
}

void Brush::setHardness2(qreal hardness)
{
	Q_ASSERT(hardness>=0 && hardness<=1);
	hardness2_ = hardness;
	checkSensitivity();
	cachepressure_ = -1;
}

void Brush::setOpacity2(qreal opacity)
{
	Q_ASSERT(opacity>=0 && opacity<=1);
	opacity2_ = opacity;
	checkSensitivity();
	cachepressure_ = -1;
}

void Brush::setColor2(const QColor& color)
{
	color2_ = color;
}

/**
 * Check if brush is sensitive to pressure.
 */
void Brush::checkSensitivity()
{
	sensitive_ = radius1_ != radius2_ ||
			fabs(hardness1_ - hardness2_) >= 1.0/256.0 ||
			fabs(opacity1_ - opacity2_) >= 1.0/256.0;
}

/**
 * Get the brush radius for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return radius
 */
int Brush::radius(qreal pressure) const
{
	Q_ASSERT(pressure>=0 && pressure<=1);
	return unsigned(ceil(ADJUST_BY_PRESSURE(radius1_, radius2_, pressure)));
}

/**
 * Get the brush hardness for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return hardness
 */
qreal Brush::hardness(qreal pressure) const
{
	Q_ASSERT(pressure>=0 && pressure<=1);
	return ADJUST_BY_PRESSURE(opacity1_, opacity2_, pressure);
}

/**
 * Get the brush opacity for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return opacity
 */
qreal Brush::opacity(qreal pressure) const
{
	return ADJUST_BY_PRESSURE(opacity1_, opacity2_, pressure);
}

/**
 * Get the brush color for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return color
 */
QColor Brush::color(qreal pressure) const
{
	Q_ASSERT(pressure>=0 && pressure<=1);
	
	const int r = qRound(ADJUST_BY_PRESSURE(color1_.red(), color2_.red(), pressure));
	const int g = qRound(ADJUST_BY_PRESSURE(color1_.green(), color2_.green(), pressure));
	const int b = qRound(ADJUST_BY_PRESSURE(color1_.blue(), color2_.blue(), pressure));
	
	return QColor(r,g,b);
}

/**
 * A brush is basically an image filled with a single color and an alpha
 * channel that defines its shape.
 * The alpha channel is produced with the formula \f$a(x,y) = (1-\frac{x^2+y^2}{r^2}^{2r^{2h-1}})*o\f$.
 * getBrush will cache the previously used brush.
 * @param pressure pen pressure. Range is [0..1]
 * @return brush image
 */
void Brush::updateCache() const
{
	const int rad = radius(cachepressure_);
	const qreal o = opacity(cachepressure_);
	qreal hard = pow(2*rad,2*hardness(cachepressure_)-1);
	if(hard<0.01) hard=0.01;

	const int dia = rad*2;
	cache_.resize(dia*dia);

	// 1/radius^2
	const qreal rr = 1.0/qreal(rad*rad);

	// Only one quarter of the pixels are unique.
	// Quarter 1 is top left, quarter 2 is top right,
	// quarter 3 is bottom left and quarter 4 is bottom right.
	const unsigned int scanline = dia - 1;
	const unsigned int scanline15= dia + rad;
	const unsigned int scanline2 = rad;
	unsigned short *q1 = cache_.data();
	unsigned short *q2 = q1 + scanline;
	unsigned short *q3 = q1 + dia*(dia-1);
	unsigned short *q4 = q3 + scanline;
	for(int y=rad;y!=0;--y) {
		const qreal yy = y * y;
		for(int x=rad;x!=0;--x) {
			const unsigned short a = qBound(0, int((1-pow( ((x*x)+(yy))*(rr) ,hard)) * o * 256), 256);
			
			*(q1++) = a;
			*(q2--) = a;
			*(q3++) = a;
			*(q4--) = a;
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
	const unsigned int offx = cx<0 ? -cx : 0;
	const unsigned int offy = cy<0 ? -cy : 0;
	uchar *dest = image.bits() + ((cy+offy)*image.width()+cx+offx)*4;
	
	// for avoiding
	#define CALCULATE_COLOR(color, target, alpha) \
		alpha * (color - target) / 2560
	
	// Special case, single pixel brush
	if(dia==0) {
		if(offx==0 && offy==0 &&
				pos.x() < image.width() && pos.y() < image.height()) {
			const int a = int(opacity(pos.pressure())*256);
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
							fabs(pos.pressure()-cachepressure_) > 1.0/256.0))) {
		cachepressure_ = pos.pressure();
		updateCache();
	}
	
	const unsigned short *src = cache_.constData() + offy*dia + offx;
	const unsigned int nextline = (image.width() - dia + offx) * 4;
	
	const int w = (cx+dia)>image.width()?image.width()-cx:dia;
	const int h = (cy+dia)>image.height()?image.height()-cy:dia;
	
	// Composite brush on image
	for(int y=offy;y<h;++y) {
		for(int x=offx;x<w;++x) {
			const int a = *(src++);
			#ifdef TESTING
			if (a == 0) {
				dest += 4;
				continue;
			}
			#endif // TESTING
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
		fabs(hardness1_ - brush.hardness1_) >= 1.0/256.0 ||
		fabs(hardness2_ - brush.hardness2_) >= 1.0/256.0 ||
		fabs(opacity1_ - brush.opacity1_) >= 1.0/256.0 ||
		fabs(opacity2_ - brush.opacity2_) >= 1.0/256.0 ||
		cachepressure_<0 || brush.cachepressure_>=0) {
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
	
	return *this;
}

bool Brush::operator==(const Brush& brush) const
{
	return radius1_ == brush.radius1_ && radius2_ == brush.radius2_ &&
			fabs(hardness1_ - brush.hardness1_) <= 1.0/256.0 &&
			fabs(hardness2_ - brush.hardness2_) <= 1.0/256.0 &&
			fabs(opacity1_ - brush.opacity1_) <= 1.0/256.0 &&
			fabs(opacity2_ - brush.opacity2_) <= 1.0/256.0 &&
			color1_ == brush.color1_ &&
			color2_ == brush.color2_;
}

bool Brush::operator!=(const Brush& brush) const
{
	return !(*this == brush);
}

#undef ADJUST_BY_PRESSURE // remove macro function

}
