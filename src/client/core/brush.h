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
#ifndef BRUSH_H
#define BRUSH_H

#include <QColor>

#include "blendmodes.h"

namespace paintcore {

/**
 * @brief A brush for drawing onto a layer
 *
 * This class contains the parameters defining a brush.
 */
class Brush
{
public:
	//! Construct a brush
	Brush(int size=1, qreal hardness=1.0, qreal opacity=1.0,
			const QColor& color=Qt::black, int spacing=25);

	//! Set size for heavy brush
	void setSize(int size) { Q_ASSERT(size>0); _size1 = size; }
	//! Set size for light brush
	void setSize2(int size) { Q_ASSERT(size>0); _size2 = size; }

	int size1() const { return _size1; }
	int size2() const { return _size2; }

	//! Set hardness for heavy brush
	void setHardness(qreal hardness) { Q_ASSERT(hardness>=0 && hardness<=1); _hardness1 = hardness; }
	//! Set hardness for light brush
	void setHardness2(qreal hardness) { Q_ASSERT(hardness>=0 && hardness<=1); _hardness2 = hardness; }

	qreal hardness1() const { return _hardness1; }
	qreal hardness2() const { return _hardness2; }

	//! Set opacity for heavy brush
	void setOpacity(qreal opacity) { Q_ASSERT(opacity>=0 && opacity<=1); _opacity1 = opacity; }
	//! Set opacity for light brush
	void setOpacity2(qreal opacity) { Q_ASSERT(opacity>=0 && opacity<=1); _opacity2 = opacity; }

	qreal opacity1() const { return _opacity1; }
	qreal opacity2() const { return _opacity2; }

	//! Set color for heavy brush
	void setColor(const QColor& color) { _color = color; }

	//! Set smudging pressure for heavy brush
	void setSmudge(qreal smudge) { Q_ASSERT(smudge>=0 && smudge<=1); _smudge1 = smudge; }
	//! Set smudging pressure for light brush
	void setSmudge2(qreal smudge) { Q_ASSERT(smudge>=0 && smudge<=1); _smudge2 = smudge; }

	qreal smudge1() const { return _smudge1; }
	qreal smudge2() const { return _smudge2; }

	const QColor &color() const { return _color; }

	void setSpacing(int spacing) { Q_ASSERT(spacing >= 0 && spacing <= 100); _spacing = spacing; }
	int spacing() const { return _spacing; }

	//! Set smudge colir resampling frequency (0 resamples on every dab)
	void setResmudge(int resmudge) { Q_ASSERT(resmudge >= 0); _resmudge = resmudge; }
	int resmudge() const { return _resmudge; }

	void setSubpixel(bool sp) { _subpixel = sp; }
	bool subpixel() const { return _subpixel; }

	void setIncremental(bool incremental) { _incremental = incremental; }
	bool incremental() const { return _incremental; }

	void setBlendingMode(BlendMode::Mode mode) { _blend = mode; }
	BlendMode::Mode blendingMode() const { return _blend; }

	//! Get interpolated size
	qreal fsize(qreal pressure) const;
	//! Get interpolated hardness
	qreal hardness(qreal pressure) const;
	//! Get interpolated opacity
	qreal opacity(qreal pressure) const;
	//! Get interpolated smudging pressure
	qreal smudge(qreal pressure) const;
	//! Get the dab spacing distance
	qreal spacingDist(qreal pressure) const;

	//! Does opacity vary with pressure?
	bool isOpacityVariable() const;

	//! Get a color with transparency for approximating strokes with solid lines
	QColor approxColor() const;

	bool operator==(const Brush& brush) const;
	bool operator!=(const Brush& brush) const;

private:
	int _size1, _size2;
	qreal _hardness1, _hardness2;
	qreal _opacity1, _opacity2;
	qreal _smudge1, _smudge2;
	QColor _color;
	int _spacing;
	int _resmudge;
	BlendMode::Mode _blend;
	bool _subpixel;
	bool _incremental;
};

struct StrokeState {
	// stroked distance
	qreal distance;

	// number of dabs since last smudge color sampling
	int smudgeDistance;

	// the smudged color. This is used instead of the normal
	// brush color when smudging is enabled, so initialize it to the brush
	// color at the start of the stroke.
	QColor smudgeColor;

	StrokeState() : distance(0), smudgeDistance(0) { }
	explicit StrokeState(const Brush &b) : distance(0), smudgeDistance(0), smudgeColor(b.color()) { }
};

}

Q_DECLARE_TYPEINFO(paintcore::Brush, Q_MOVABLE_TYPE);

#endif


