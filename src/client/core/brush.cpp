/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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
	return qMax(1, qRound(interpolate(radius1(), radius2(), pressure)));
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

qreal Brush::spacingDist(qreal pressure) const
{
	return spacing() * radius(pressure)/100.0;
}

bool Brush::isOpacityVariable() const
{
	return qAbs(opacity1() - opacity2()) > (1/256.0);
}

bool Brush::isIdempotent() const
{
	return opacity1() == 1.0 && opacity2() == 1.0 &&
			hardness1() == 1.0 && hardness2() == 1.0 &&
			(blendingMode() == 0 || blendingMode() == 1);
}

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

}
