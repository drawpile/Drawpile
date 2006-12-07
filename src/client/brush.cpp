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
#include "brush.h"

namespace drawingboard {

Brush::Brush(int radius, qreal hardness, qreal opacity, const QColor& color)
	: radius1_(radius), radius2_(radius),
	hardness1_(hardness), hardness2_(hardness),
	opacity1_(opacity), opacity2_(opacity),
	color1_(color), color2_(color),
	sensitive_(false), cachepressure_(-1)
{
}

/**
 * Set the radius for pressure=1.0
 * @param radius brush radius. 0 means single pixel brush
 */
void Brush::setRadius(int radius)
{
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
	radius2_  = radius;
	checkSensitivity();
	cachepressure_ = -1;
}

void Brush::setHardness2(qreal hardness)
{
	hardness2_ = hardness;
	checkSensitivity();
	cachepressure_ = -1;
}

void Brush::setOpacity2(qreal opacity)
{
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
			fabs(hardness1_ - hardness2_) >= 0.01 ||
			fabs(opacity1_ - opacity2_) >= 0.01;
}

/**
 * Get the brush radius for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return radius
 */
int Brush::radius(qreal pressure) const
{
	return int(ceil(radius2_ + (radius1_ - radius2_) * pressure));
}

/**
 * Get the brush hardness for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return hardness
 */
qreal Brush::hardness(qreal pressure) const
{
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
			int a = int(intensity*255);

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

void Brush::draw(QImage &image, const QPoint& pos, qreal pressure) const
{
	const int dia = radius(pressure) * 2;
	const QColor col = color(pressure);

	const int red = col.red();
	const int green = col.green();
	const int blue= col.blue();

	// Make sure we are inside the image
	int offx = 0;
	int offy = 0;
	if(pos.x()<0)
		offx = -pos.x();
	if(pos.y()<0)
		offy = -pos.y();
	uchar *dest = image.bits() + ((pos.y()+offy)*image.width()+pos.x()+offx)*4;

	// Special case, single pixel brush
	if(dia==0) {
		const qreal a = opacity(pressure);
		*dest = int(*dest * (1-a) + blue * a + 0.5);  ++dest;
		*dest = int(*dest * (1-a) + green * a + 0.5); ++dest;
		*dest = int(*dest * (1-a) + red * a + 0.5);
		return;
	}

	// Update brush cache if out of date
	if(cachepressure_<0 ||
			(sensitive_ && fabs(pressure - cachepressure_) > 1.0/256.0)) {
		cachepressure_ = pressure;
		updateCache();
	}

	const uchar *src = cache_.constData() + offy*dia + offx;
	const unsigned int nextline = (image.width() - dia + offx) * 4;

	const int w = (pos.x()+dia)>image.width()?image.width()-pos.x():dia;
	const int h = (pos.y()+dia)>image.height()?image.height()-pos.y():dia;

	// Composite brush on image
	for(int y=offy;y<h;++y) {
		for(int x=offx;x<w;++x) {
			const int a = *(src++);
			*dest = a*(blue - *dest) / 256 + *dest; ++dest;
			*dest = a*(green - *dest) / 256 + *dest; ++dest;
			*dest = a*(red - *dest) / 256 + *dest; ++dest;
			++dest;
		}
		dest += nextline + (dia-w)*4;
		src += offx + dia-w;
	}
}

/**
 * This copy operator tries to preserve the brush cache.
 * If both brushes contain equal parameters (opacity excluded)
 * and the brush that is being overwritten (but not the other one)
 * has a cached brush image, then the old cache is preserved.
 * @param brush brush to replace this with
 */
Brush& Brush::operator=(const Brush& brush)
{
	bool isdifferent =
		radius1_ != brush.radius1_ || radius2_ != brush.radius2_ ||
		fabs(hardness1_ - brush.hardness1_) >= 0.01 ||
		fabs(hardness2_ - brush.hardness2_) >= 0.01 ||
		fabs(opacity1_ - brush.opacity1_) >= 0.01 ||
		fabs(opacity2_ - brush.opacity2_) >= 0.01;
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
			fabs(hardness1_ - brush.hardness1_) <= 0.01 &&
			fabs(hardness2_ - brush.hardness2_) <= 0.01 &&
			fabs(opacity1_ - brush.opacity1_) <= 0.01 &&
			fabs(opacity2_ - brush.opacity2_) <= 0.01 &&
			color1_ == brush.color1_ &&
			color2_ == brush.color2_;
}

bool Brush::operator!=(const Brush& brush) const
{
	return radius1_ != brush.radius1_ || radius2_ != brush.radius2_ ||
			fabs(hardness1_ - brush.hardness1_) >= 0.01 ||
			fabs(hardness2_ - brush.hardness2_) >= 0.01 ||
			fabs(opacity1_ - brush.opacity1_) >= 0.01 ||
			fabs(opacity2_ - brush.opacity2_) >= 0.01 ||
			color1_ != brush.color1_ ||
			color2_ != brush.color2_;
}

}
