/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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
#ifndef DP_BRUSHES_BRUSH_H
#define DP_BRUSHES_BRUSH_H

#include "../core/blendmodes.h"

#include <QColor>

namespace brushes {

/**
 * @brief The parameters for Drawpile's classic soft and pixel brushes
 *
 * Pressure sensitive brush parameters come in pairs: x1 and x2,
 * where 1 is the value at full pressure and 2 is the value at zero pressure.
 *
 * Pressure sensitive brush parameters are:
 *
 *  - Size - the brush diameter
 *  - Hardness - hardness of the brush shape
 *  - Opacity - brush opacity
 *  - Smudge - smudging factor (how much color is picked up)
 *
 *  Constant parameters are:
 *
 *  - Resmudge - how often to sample smudge color (every resmudge dabs)
 *  - Spacing - dab spacing as a percentage of brush diameter
 *  - Subpixel mode (boolean)
 *  - Incremental mode (boolean)
 *  - Blending mode
 *  - Shape selection (round/square)
 */
class ClassicBrush
{
public:
	//! Construct a brush
	ClassicBrush(int size=1, qreal hardness=1.0, qreal opacity=1.0,
			const QColor& color=Qt::black, int spacing=25)
		: m_size1(size), m_size2(size),
		m_hardness1(hardness), m_hardness2(hardness),
		m_opacity1(opacity), m_opacity2(opacity),
		m_smudge1(0), m_smudge2(0),
		m_color(color), m_spacing(spacing), m_resmudge(0),
		m_blend(paintcore::BlendMode::MODE_NORMAL),
		m_subpixel(false), m_incremental(true),
		m_square(false)
	{
	}

	void setSize(int size) { Q_ASSERT(size>0); m_size1 = size; }
	void setSize2(int size) { Q_ASSERT(size>0); m_size2 = size; }

	int size1() const { return m_size1; }
	int size2() const { return m_size2; }

	void setHardness(qreal hardness) { Q_ASSERT(hardness>=0 && hardness<=1); m_hardness1 = hardness; }
	void setHardness2(qreal hardness) { Q_ASSERT(hardness>=0 && hardness<=1); m_hardness2 = hardness; }

	qreal hardness1() const { return m_hardness1; }
	qreal hardness2() const { return m_hardness2; }

	void setOpacity(qreal opacity) { Q_ASSERT(opacity>=0 && opacity<=1); m_opacity1 = opacity; }
	void setOpacity2(qreal opacity) { Q_ASSERT(opacity>=0 && opacity<=1); m_opacity2 = opacity; }

	qreal opacity1() const { return m_opacity1; }
	qreal opacity2() const { return m_opacity2; }

	void setColor(const QColor& color) { m_color = color; }
	const QColor &color() const { return m_color; }

	void setSmudge(qreal smudge) { Q_ASSERT(smudge>=0 && smudge<=1); m_smudge1 = smudge; }
	void setSmudge2(qreal smudge) { Q_ASSERT(smudge>=0 && smudge<=1); m_smudge2 = smudge; }

	qreal smudge1() const { return m_smudge1; }
	qreal smudge2() const { return m_smudge2; }

	void setSpacing(int spacing) { Q_ASSERT(spacing > 0); m_spacing = spacing; }
	int spacing() const { return m_spacing; }

	//! Set smudge colir resampling frequency (0 resamples on every dab)
	void setResmudge(int resmudge) { Q_ASSERT(resmudge >= 0); m_resmudge = resmudge; }
	int resmudge() const { return m_resmudge; }

	void setSubpixel(bool sp) { m_subpixel = sp; }
	bool subpixel() const { return m_subpixel; }

	void setIncremental(bool incremental) { m_incremental = incremental; }
	bool incremental() const { return m_incremental; }

	void setBlendingMode(paintcore::BlendMode::Mode mode) { m_blend = mode; }
	paintcore::BlendMode::Mode blendingMode() const { return m_blend; }
	bool isEraser() const { return m_blend == paintcore::BlendMode::MODE_ERASE; }

	void setSquare(bool square) { m_square = square; }
	bool isSquare() const { return m_square; }

	qreal size(qreal pressure) const { return lerp(size1(), size2(), pressure); }
	qreal hardness(qreal pressure) const { return lerp(hardness1(), hardness2(), pressure); }
	qreal opacity(qreal pressure) const { return lerp(opacity1(), opacity2(), pressure); }
	qreal smudge(qreal pressure) const { return lerp(smudge1(), smudge2(), pressure); }
	qreal spacingDist(qreal pressure) const { return spacing() / 100.0 * size(pressure); }

	//! Does opacity vary with pressure?
	bool isOpacityVariable() const { return qAbs(opacity1() - opacity2()) > (1/256.0); }

private:
	static inline qreal lerp(qreal a, qreal b, qreal alpha) {
		Q_ASSERT(alpha >=0 && alpha<=1);
		return (a-b) * alpha + b;
	}

	int m_size1, m_size2;
	qreal m_hardness1, m_hardness2;
	qreal m_opacity1, m_opacity2;
	qreal m_smudge1, m_smudge2;
	QColor m_color;
	int m_spacing;
	int m_resmudge;
	paintcore::BlendMode::Mode m_blend;
	bool m_subpixel;
	bool m_incremental;
	bool m_square;
};

}

Q_DECLARE_TYPEINFO(brushes::ClassicBrush, Q_MOVABLE_TYPE);

#endif


