/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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
#ifndef BRUSH_H
#define BRUSH_H

#include <QColor>

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
	Brush(int radius=0, qreal hardness=1.0, qreal opacity=1.0,
			const QColor& color=Qt::black, int spacing=25);

	//! Set radius heavy brush
	void setRadius(int radius) { Q_ASSERT(radius>=0); _radius1  = radius; }
	//! Set radius for light brush
	void setRadius2(int radius) { Q_ASSERT(radius>=0); _radius2  = radius; }

	int radius1() const { return _radius1; }
	int radius2() const { return _radius2; }

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
	void setColor(const QColor& color) { _color1 = color; }
	//! Set color for light brush
	void setColor2(const QColor& color) { _color2 = color; }

	const QColor &color1() const { return _color1; }
	const QColor &color2() const { return _color2; }

	void setSpacing(int spacing) { Q_ASSERT(spacing >= 0 && spacing <= 100); _spacing = spacing; }
	int spacing() const { return _spacing; }

	void setSubpixel(bool sp) { _subpixel = sp; }
	bool subpixel() const { return _subpixel; }

	void setIncremental(bool incremental) { _incremental = incremental; }
	bool incremental() const { return _incremental; }

	void setBlendingMode(int mode) { _blend = mode; }
	int blendingMode() const { return _blend; }

	//! Get interpolated radius
	int radius(qreal pressure) const;
	qreal fradius(qreal pressure) const;
	//! Get the diameter of the rendered brush
	int diameter(qreal pressure) const;
	//! Get interpolated hardness
	qreal hardness(qreal pressure) const;
	//! Get interpolated opacity
	qreal opacity(qreal pressure) const;
	//! Get interpolated color
	QColor color(qreal pressure) const;

	//! Does opacity vary with pressure?
	bool isOpacityVariable() const;

	//! Will two or more dabs of this brush have the same result as just one?
	bool isIdempotent() const;

	bool operator==(const Brush& brush) const;
	bool operator!=(const Brush& brush) const;

private:
	int _radius1, _radius2;
	qreal _hardness1, _hardness2;
	qreal _opacity1, _opacity2;
	QColor _color1, _color2;
	int _spacing;
	int _blend;
	bool _subpixel;
	bool _incremental;
};

}

#endif


