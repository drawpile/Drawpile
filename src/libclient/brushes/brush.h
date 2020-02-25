/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2020 Calle Laakkonen

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
#include "../../rustpile/rustpile.h"

#include <QColor>
#include <QMetaType>

class QJsonObject;
class QDataStream;

namespace brushes {

/**
 * @brief The parameters for Drawpile's classic soft and pixel brushes
 *
 * TODO this is now a wrapper around rustpile ClassicBrush. We should transition
 * to using it directly everywhere.
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
	ClassicBrush();
	ClassicBrush(const rustpile::ClassicBrush &brush) : m_brush(brush) {}

	void setSize(int size) { m_brush.size.max = qMax(1, size); }
	void setSize2(int size) { m_brush.size.min = qMax(1, size); }

	int size1() const { return m_brush.size.max; }
	int size2() const { return m_brush.size.min; }

	void setHardness(qreal hardness) { m_brush.hardness.max = qBound(0.0, hardness, 1.0); }
	void setHardness2(qreal hardness) { m_brush.hardness.min = qBound(0.0, hardness, 1.0); }

	qreal hardness1() const { return m_brush.hardness.max; }
	qreal hardness2() const { return m_brush.hardness.min; }

	void setOpacity(qreal opacity) { m_brush.opacity.max = qBound(0.0, opacity, 1.0); }
	void setOpacity2(qreal opacity) { m_brush.opacity.min = qBound(0.0, opacity, 1.0); }

	qreal opacity1() const { return m_brush.opacity.max; }
	qreal opacity2() const { return m_brush.opacity.min; }

	void setColor(const QColor& color) { m_brush.color = {float(color.redF()), float(color.greenF()), float(color.blueF()), float(color.alphaF())}; }
	QColor color() const { return QColor::fromRgbF(m_brush.color.r, m_brush.color.g, m_brush.color.b, m_brush.color.a); }
	rustpile::Color rpColor() const { return m_brush.color; }

	void setSmudge(qreal smudge) { m_brush.smudge.max = qBound(0.0, smudge, 1.0); }
	void setSmudge2(qreal smudge) { m_brush.smudge.min = qBound(0.0, smudge, 1.0); }

	qreal smudge1() const { return m_brush.smudge.max; }
	qreal smudge2() const { return m_brush.smudge.min; }

	void setSpacing(qreal spacing) { m_brush.spacing = qMax(0.01, spacing); }
	qreal spacing() const { return m_brush.spacing; }

	//! Set smudge color resampling frequency (0 resamples on every dab)
	void setResmudge(int resmudge) { m_brush.resmudge = qMax(0, resmudge); }
	int resmudge() const { return m_brush.resmudge; }

	bool subpixel() const { return m_brush.shape == rustpile::ClassicBrushShape::RoundSoft; }

	void setIncremental(bool incremental) { m_brush.incremental = incremental; }
	bool incremental() const { return m_brush.incremental; }

	void setBlendingMode(paintcore::BlendMode::Mode mode) { m_brush.mode = rustpile::Blendmode(mode); }
	paintcore::BlendMode::Mode blendingMode() const { return paintcore::BlendMode::Mode(m_brush.mode); }
	bool isEraser() const { return blendingMode() == paintcore::BlendMode::MODE_ERASE || blendingMode() == paintcore::BlendMode::MODE_COLORERASE; }

	void setShape(rustpile::ClassicBrushShape shape) { m_brush.shape = shape; }
	rustpile::ClassicBrushShape shape() const { return m_brush.shape; }

	void setColorPickMode(bool colorpick) { m_brush.colorpick = colorpick; }
	bool isColorPickMode() const { return m_brush.colorpick; }

	qreal size(qreal pressure) const { return useSizePressure() ? lerp(size1(), size2(), pressure) : size1(); }
	qreal hardness(qreal pressure) const { return useHardnessPressure() ? lerp(hardness1(), hardness2(), pressure) : hardness1(); }
	qreal opacity(qreal pressure) const { return useOpacityPressure() ? lerp(opacity1(), opacity2(), pressure) : opacity1(); }
	qreal smudge(qreal pressure) const { return useSmudgePressure() ? lerp(smudge1(), smudge2(), pressure) : smudge1(); }
	qreal spacingDist(qreal pressure) const { return spacing() * size(pressure); }

	void setSizePressure(bool p) { m_brush.size_pressure = p; }
	bool useSizePressure() const { return m_brush.size_pressure; }

	void setHardnessPressure(bool p) { m_brush.hardness_pressure = p; }
	bool useHardnessPressure() const { return m_brush.hardness_pressure; }

	void setOpacityPressure(bool p) { m_brush.opacity_pressure = p; }
	bool useOpacityPressure() const { return m_brush.opacity_pressure; }

	void setSmudgePressure(bool p) { m_brush.smudge_pressure = p; }
	bool useSmudgePressure() const { return m_brush.smudge_pressure; }

	QJsonObject toJson() const;
	static ClassicBrush fromJson(const QJsonObject &json);

	const rustpile::ClassicBrush &brush() const { return m_brush; }

#if 0
	friend QDataStream &operator<<(QDataStream &out, const ClassicBrush &myObj);
	friend QDataStream &operator>>(QDataStream &in, ClassicBrush &myObj);
#endif

private:
	static inline qreal lerp(qreal a, qreal b, qreal alpha) {
		Q_ASSERT(alpha >=0 && alpha<=1);
		return (a-b) * alpha + b;
	}

	rustpile::ClassicBrush m_brush;
#if 0
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
#endif
};

}

Q_DECLARE_METATYPE(brushes::ClassicBrush)
Q_DECLARE_TYPEINFO(brushes::ClassicBrush, Q_MOVABLE_TYPE);

#endif


