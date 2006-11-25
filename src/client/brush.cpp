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

Brush::Brush(int diameter, qreal hardness, qreal opacity, const QColor& color)
	: diameter1_(diameter), diameter2_(1),
	hardness1_(hardness), hardness2_(hardness),
	opacity1_(opacity), opacity2_(opacity),
	color1_(color), color2_(color),
	cachepressure_(-1)
{
}

/**
 * Set the diameter for pressure=1.0
 * @param diameter brush diameter. Must be greater than 0
 */
void Brush::setDiameter(int diameter)
{
	diameter1_ = diameter;
	cachepressure_ = -1;
}

/**
 * Set the hardness for pressure=1.0
 * @param radius brush hardness. Range is [-1..1]
 */
void Brush::setHardness(qreal hardness)
{
	hardness1_ = hardness;
	cachepressure_ = -1;
}

/**
 * Set the opacity for pressure=1.0
 * Opacity is not used internally by this class, but is provided as a service.
 * @param opacity brush opacity. Range is [0..1]
 */
void Brush::setOpacity(qreal opacity)
{
	opacity1_ = opacity;
}

/**
 * Set the color for pressure=1.0
 * @param color brush color.
 */
void Brush::setColor(const QColor& color)
{
	color1_ = color;
	cachepressure_ = -1;
}


void Brush::setDiameter2(int diameter)
{
	diameter2_ = diameter;
	cachepressure_ = -1;
}

void Brush::setHardness2(qreal hardness)
{
	hardness2_ = hardness;
	cachepressure_ = -1;
}

void Brush::setOpacity2(qreal opacity)
{
	opacity2_ = opacity;
}

void Brush::setColor2(const QColor& color)
{
	color2_ = color;
	cachepressure_ = -1;
}

/**
 * Get the brush diameter for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return diameter
 */
int Brush::diameter(qreal pressure) const
{
	return int(ceil(diameter2_ + (diameter1_ - diameter2_) * pressure));
}

/**
 * Get the brush radius for certain pressure.
 * @param pressure pen pressure. Range is [0..1]
 * @return radius
 */
qreal Brush::radius(qreal pressure) const
{
	qreal rad1 = diameter1_ / 2.0;
	qreal rad2 = diameter2_ / 2.0;
	return qRound(rad2 + (rad1 - rad2) * pressure);
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
 * A brush is basically a pixmap filled with a single color and an alpha
 * channel that defines its shape.
 * The alpha channel is produced with the formula \f$a(x,y) = 1-\frac{x^2+y^2}{r^2}+h\f$.
 * getBrush will cache the previously used brush.
 * @param pressure pen pressure. Range is [0..1]
 * @return brush pixmap
 */
QPixmap Brush::getBrush(qreal pressure) const
{
	if(fabs(pressure - cachepressure_) > 1.0/256.0) {
		// Regenerate brush (we currently support 256 levels of pressure)

		int dia = diameter(pressure);
		qreal rad = radius(pressure);
		qreal hard = hardness(pressure);

		QImage brush(dia,dia,QImage::Format_ARGB32);

		// Fill the brush with color
		brush.fill(color(pressure).rgb());

		// Set brush alpha
		qreal rr = 1.0/(rad*rad);
		for(int y=0;y<dia;++y) {
			uchar *data = brush.scanLine(y)+3;
			qreal yy = (y-rad) * (y-rad);
			for(int x=0;x<dia;++x,data+=4) {
				qreal xx = (x-rad) * (x-rad);
				qreal intensity = (1-(xx + yy)*rr) + hard;

				if(intensity<0) intensity=0;
				else if(intensity>1) intensity=1;

				*data = qRound(intensity*255);
			}
		}
		cache_ = QPixmap::fromImage(brush);
		cachepressure_ = pressure;
	}
	return cache_;
}

}

