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

#include <cmath>
#include "point.h"
#include "brush.h"
#include "../../config.h"

namespace drawingboard {

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
unsigned int Brush::radius(qreal pressure) const
{
	Q_ASSERT(pressure>=0 && pressure<=1);
	return unsigned(ceil(radius2_ + (radius1_ - radius2_) * pressure));
}

/**
 * Get the brush hardness for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return hardness
 */
qreal Brush::hardness(qreal pressure) const
{
	Q_ASSERT(pressure>=0 && pressure<=1);
	return hardness2_ + (hardness1_ - hardness2_) * pressure;
}

/**
 * Get the brush opacity for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return opacity
 */
qreal Brush::opacity(qreal pressure) const
{
	return opacity2_ + (opacity1_ - opacity2_) * pressure;
}

/**
 * Get the brush color for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return color
 */
QColor Brush::color(qreal pressure) const
{
	Q_ASSERT(pressure>=0 && pressure<=1);
	int r = qRound(color2_.red() + (color1_.red() - color2_.red()) * pressure);
	int g = qRound(color2_.green() + (color1_.green() - color2_.green()) * pressure);
	int b = qRound(color2_.blue() + (color1_.blue() - color2_.blue()) * pressure);
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
	unsigned int scanline = dia - 1;
	unsigned int scanline15= dia + rad;
	unsigned int scanline2 = rad;
	uchar *q1 = cache_.data();
	uchar *q2 = q1 + scanline;
	uchar *q3 = q1 + dia*(dia-1);
	uchar *q4 = q3 + scanline;
	for(int y=0;y<rad;++y) {
		qreal yy = (y-rad+0.5) * (y-rad+0.5);
		for(int x=0;x<rad;++x) {
			qreal xx = (x-rad+0.5) * (x-rad+0.5);
			qreal intensity = (1-pow( (xx+yy)*(rr) ,hard)) * o;

			if(intensity<0) intensity=0;
			else if(intensity>1) intensity=1;
			const unsigned int a = int(intensity*255);

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

void Brush::draw(QImage &image, const Point& pos) const
{
	const unsigned int dia = radius(pos.pressure()) * 2;
	const QColor col = color(pos.pressure());

	const unsigned int red = col.red();
	const unsigned int green = col.green();
	const unsigned int blue= col.blue();

	// Make sure we are inside the image
	const unsigned int offx = pos.x()<0 ? -pos.x() : 0;
	const unsigned int offy = pos.y()<0 ? -pos.y() : 0;
	uchar *dest = image.bits() + ((pos.y()+offy)*image.width()+pos.x()+offx)*4;

	// Special case, single pixel brush
	if(dia==0) {
		if(offx==0 && offy==0 &&
				pos.x() < image.width() && pos.y() < image.height()) {
			const qreal a = opacity(pos.pressure());
#ifdef IS_BIG_ENDIAN
			++dest;
			*dest = int(*dest * (1-a) + red * a + 0.5);
			*dest = int(*dest * (1-a) + green * a + 0.5); ++dest;
			*dest = int(*dest * (1-a) + blue * a + 0.5);  ++dest;
#else
			*dest = int(*dest * (1-a) + blue * a + 0.5);  ++dest;
			*dest = int(*dest * (1-a) + green * a + 0.5); ++dest;
			*dest = int(*dest * (1-a) + red * a + 0.5);
#endif
		}
		return;
	}

	// Update brush cache if out of date
	if(cachepressure_<0 ||
			(sensitive_ && (dia!=radius(cachepressure_)*2 ||
							fabs(pos.pressure()-cachepressure_) > 1.0/256.0))) {
		cachepressure_ = pos.pressure();
		updateCache();
	}

	const uchar *src = cache_.constData() + offy*dia + offx;
	const unsigned int nextline = (image.width() - dia + offx) * 4;

	const unsigned int w = (pos.x()+dia)>unsigned(image.width())?image.width()-pos.x():dia;
	const unsigned int h = (pos.y()+dia)>unsigned(image.height())?image.height()-pos.y():dia;

	// Composite brush on image
	for(unsigned int y=offy;y<h;++y) {
		for(unsigned int x=offx;x<w;++x) {
			const unsigned int a = *(src++);
#ifdef IS_BIG_ENDIAN
			++dest;
			*dest = a*(red - *dest) / 256 + *dest; ++dest;
			*dest = a*(green - *dest) / 256 + *dest; ++dest;
			*dest = a*(blue - *dest) / 256 + *dest; ++dest;
#else
			*dest = a*(blue - *dest) / 256 + *dest; ++dest;
			*dest = a*(green - *dest) / 256 + *dest; ++dest;
			*dest = a*(red - *dest) / 256 + *dest; ++dest;
			++dest;
#endif
		}
		dest += nextline + (dia-w)*4;
		src += offx + dia-w;
	}
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
	bool isdifferent =
		radius1_ != brush.radius1_ || radius2_ != brush.radius2_ ||
		fabs(hardness1_ - brush.hardness1_) >= 1.0/256.0 ||
		fabs(hardness2_ - brush.hardness2_) >= 1.0/256.0 ||
		fabs(opacity1_ - brush.opacity1_) >= 1.0/256.0 ||
		fabs(opacity2_ - brush.opacity2_) >= 1.0/256.0;
	radius1_ = brush.radius1_ ;
	radius2_ = brush.radius2_ ;
	hardness1_ = brush.hardness1_;
	hardness2_ = brush.hardness2_;
	opacity1_ = brush.opacity1_;
	opacity2_ = brush.opacity2_;
	color1_ = brush.color1_;
	color2_ = brush.color2_;
	sensitive_ = brush.sensitive_;

	if(isdifferent || cachepressure_<0 || brush.cachepressure_>=0) {
		cachepressure_ = brush.cachepressure_;
		cache_ = brush.cache_;
	}
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
	return radius1_ != brush.radius1_ || radius2_ != brush.radius2_ ||
			fabs(hardness1_ - brush.hardness1_) >= 1.0/256.0 ||
			fabs(hardness2_ - brush.hardness2_) >= 1.0/256.0 ||
			fabs(opacity1_ - brush.opacity1_) >= 1.0/256.0 ||
			fabs(opacity2_ - brush.opacity2_) >= 1.0/256.0 ||
			color1_ != brush.color1_ ||
			color2_ != brush.color2_;
}

}
