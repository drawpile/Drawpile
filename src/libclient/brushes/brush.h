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
#include <QMetaType>

class QJsonObject;

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
 *  - Spacing - dab spacing (1.0 = spacing is one diameter from center to center)
 *  - Subpixel mode (boolean)
 *  - Incremental mode (boolean)
 *  - Blending mode
 *  - Shape selection (round/square)
 */
class ClassicBrush
{
public:
	enum Shape {
		ROUND_PIXEL,
		SQUARE_PIXEL,
		ROUND_SOFT
	};

	void setSize(int size) { m_size1 = qMax(1, size); }
	void setSize2(int size) { m_size2 = qMax(1, size); }

	int size1() const { return m_size1; }
	int size2() const { return m_size2; }

	void setHardness(qreal hardness) { m_hardness1 = qBound(0.0, hardness, 1.0); }
	void setHardness2(qreal hardness) { m_hardness2 = qBound(0.0, hardness, 1.0); }

	qreal hardness1() const { return m_hardness1; }
	qreal hardness2() const { return m_hardness2; }

	void setOpacity(qreal opacity) { m_opacity1 = qBound(0.0, opacity, 1.0); }
	void setOpacity2(qreal opacity) { m_opacity2 = qBound(0.0, opacity, 1.0); }

	qreal opacity1() const { return m_opacity1; }
	qreal opacity2() const { return m_opacity2; }

	void setColor(const QColor& color) { m_color = color; }
	const QColor &color() const { return m_color; }

	void setSmudge(qreal smudge) { m_smudge1 = qBound(0.0, smudge, 1.0); }
	void setSmudge2(qreal smudge) { m_smudge2 = qBound(0.0, smudge, 1.0); }

	qreal smudge1() const { return m_smudge1; }
	qreal smudge2() const { return m_smudge2; }

	void setSpacing(qreal spacing) { m_spacing = qMax(0.01, spacing); }
	qreal spacing() const { return m_spacing; }

	//! Set smudge color resampling frequency (0 resamples on every dab)
	void setResmudge(int resmudge) { m_resmudge = qMax(0, resmudge); }
	int resmudge() const { return m_resmudge; }

	bool subpixel() const { return m_shape == ROUND_SOFT; }

	void setIncremental(bool incremental) { m_incremental = incremental; }
	bool incremental() const { return m_incremental; }

	void setBlendingMode(paintcore::BlendMode::Mode mode) { m_blend = mode; }
	paintcore::BlendMode::Mode blendingMode() const { return m_blend; }
	bool isEraser() const { return m_blend == paintcore::BlendMode::MODE_ERASE || m_blend == paintcore::BlendMode::MODE_COLORERASE; }

	void setShape(Shape shape) { m_shape = shape; }
	Shape shape() const { return m_shape; }

	void setColorPickMode(bool colorpick) { m_colorpick = colorpick; }
	bool isColorPickMode() const { return m_colorpick; }

	qreal size(qreal pressure) const { return m_sizePressure ? lerp(size1(), size2(), pressure) : size1(); }
	qreal hardness(qreal pressure) const { return m_hardnessPressure ? lerp(hardness1(), hardness2(), pressure) : hardness1(); }
	qreal opacity(qreal pressure) const { return m_opacityPressure ? lerp(opacity1(), opacity2(), pressure) : opacity1(); }
	qreal smudge(qreal pressure) const { return m_smudgePressure ? lerp(smudge1(), smudge2(), pressure) : smudge1(); }
	qreal spacingDist(qreal pressure) const { return spacing() * size(pressure); }

	void setSizePressure(bool p) { m_sizePressure = p; }
	bool useSizePressure() const { return m_sizePressure; }

	void setHardnessPressure(bool p) { m_hardnessPressure = p; }
	bool useHardnessPressure() const { return m_hardnessPressure; }

	void setOpacityPressure(bool p) { m_opacityPressure = p; }
	bool useOpacityPressure() const { return m_opacityPressure; }

	void setSmudgePressure(bool p) { m_smudgePressure = p; }
	bool useSmudgePressure() const { return m_smudgePressure; }

	QJsonObject toJson() const;
	static ClassicBrush fromJson(const QJsonObject &json);

private:
	static inline qreal lerp(qreal a, qreal b, qreal alpha) {
		Q_ASSERT(alpha >=0 && alpha<=1);
		return (a-b) * alpha + b;
	}

	Shape m_shape = ROUND_PIXEL;
	paintcore::BlendMode::Mode m_blend = paintcore::BlendMode::MODE_NORMAL;

	int m_size1 = 10, m_size2 = 1;
	int m_resmudge = 0;

	qreal m_hardness1 = 1, m_hardness2 = 0;
	qreal m_opacity1 = 1, m_opacity2 = 0;
	qreal m_smudge1 = 0, m_smudge2 = 0;
	qreal m_spacing = 0.1;

	QColor m_color;

	bool m_incremental = true;
	bool m_colorpick = false;
	bool m_sizePressure = false;
	bool m_hardnessPressure = false;
	bool m_opacityPressure = false;
	bool m_smudgePressure = false;
};

}

Q_DECLARE_METATYPE(brushes::ClassicBrush)
Q_DECLARE_TYPEINFO(brushes::ClassicBrush, Q_MOVABLE_TYPE);

#endif


