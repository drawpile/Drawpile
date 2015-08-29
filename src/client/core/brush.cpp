/*
   Drawpile - a collaborative drawing program.

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
 * This is used to figure out correct size, opacity, hardness and
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
 * @param size brush size. Zero means a single pixel brush
 * @param hardness brush hardness
 * @param opacity brush opacity
 * @param color brush color
 * @param spacing brush spacing hint, as percentage of brush size
 */
Brush::Brush(int size, qreal hardness, qreal opacity, const QColor& color, int spacing)
	: _size1(size), _size2(size),
	_hardness1(hardness), _hardness2(hardness),
	_opacity1(opacity), _opacity2(opacity),
	_smudge1(0), _smudge2(0),
	_color(color),_spacing(spacing), _resmudge(0), _blend(BlendMode::MODE_NORMAL),
	_subpixel(false), _incremental(true)
{
	Q_ASSERT(size>0);
	Q_ASSERT(hardness>=0 && hardness <=1);
	Q_ASSERT(opacity>=0 && opacity <=1);
	Q_ASSERT(spacing>=0 && spacing <=100);
}

/**
 * @brief Get the real size for the given pressure
 * @param pressure
 * @return size
 * @pre 0 <= pressure <= 1
 * @post 0.5 <= RESULT
 */
qreal Brush::fsize(qreal pressure) const
{
	//return qMax(0.5, interpolate(size1(), size2(), pressure));
	return interpolate(size1(), size2(), pressure);
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
 * Get color smudging pressure for the given stylus pressure
 * @param pressure pen pressure
 * @return opacity
 * @pre 0 <= pressure <= 1
 * @post 0 <= RESULT <= 1
 */
qreal Brush::smudge(qreal pressure) const
{
	return interpolate(smudge1(), smudge2(), pressure);
}

qreal Brush::spacingDist(qreal pressure) const
{
	return spacing() / 100.0 * fsize(pressure);
}

bool Brush::isOpacityVariable() const
{
	return qAbs(opacity1() - opacity2()) > (1/256.0);
}

bool Brush::operator==(const Brush& brush) const
{
	return size1() == brush.size1() && size2() == brush.size2() &&
			qAbs(hardness1() - brush.hardness1()) <= 1.0/256.0 &&
			qAbs(hardness2() - brush.hardness2()) <= 1.0/256.0 &&
			qAbs(opacity1() - brush.opacity1()) <= 1.0/256.0 &&
			qAbs(opacity2() - brush.opacity2()) <= 1.0/256.0 &&
			qAbs(smudge1() - brush.smudge1()) <= 1.0/256.0 &&
			qAbs(smudge2() - brush.smudge2()) <= 1.0/256.0 &&
			color() == brush.color() &&
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
