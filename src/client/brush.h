/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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

#include "../config.h"

class QImage;
#include <QVector>
#include <QColor>

namespace drawingboard {

class Point;

//! A brush for drawing onto a layer
/**
 * This class produces an image that can be used as a brush.
 * Two sets of parameters are set and the image is created by
 * interpolating between them.
 */
class Brush
{
	public:
		//! Construct a brush
		Brush(int radius=8, qreal hardness=0, qreal opacity=1.0,
				const QColor& color=Qt::black, int spacing=25);

		//! Set radius heavy brush
		void setRadius(int radius) NOTHROW;
		//! Set radius for light brush
		void setRadius2(int radius) NOTHROW;

		//! Set hardness for heavy brush
		void setHardness(qreal hardness) NOTHROW;
		//! Set hardness for light brush
		void setHardness2(qreal hardness) NOTHROW;

		//! Set opacity for heavy brush
		void setOpacity(qreal opacity) NOTHROW;
		//! Set opacity for light brush
		void setOpacity2(qreal opacity) NOTHROW;

		//! Set color for heavy brush
		void setColor(const QColor& color) NOTHROW;
		//! Set color for light brush
		void setColor2(const QColor& color) NOTHROW;

		//! Set spacing hint
		void setSpacing(int spacing) NOTHROW;

		//! Get interpolated radius
		int radius(qreal pressure) const NOTHROW;
		//! Get interpolated hardness
		qreal hardness(qreal pressure) const NOTHROW;
		//! Get interpolated opacity
		qreal opacity(qreal pressure) const NOTHROW;
		//! Get interpolated color
		QColor color(qreal pressure) const NOTHROW;
		//! Get spacing hint
		int spacing() const NOTHROW;

		//! Draw the brush on an image
		void draw(QImage &image, const Point& pos) const;

		//! Copy operator
		Brush& operator=(const Brush& brush) NOTHROW;

		//! Equality test
		bool operator==(const Brush& brush) const NOTHROW;

		//! Inequality test
		bool operator!=(const Brush& brush) const NOTHROW;

	private:
		//! called by draw()
		void drawPixel(uchar *dest, const QColor& color, int alpha) const NOTHROW;
		
		//! Normal composition, called by drawPixel()
		void normalComposition(int color, uchar& target, int alpha) const NOTHROW;
		
		//! Update the brush cache
		void updateCache() const;

		//! Check if the brush is sensitive to pressure
		void checkSensitivity() NOTHROW;

		int radius1_, radius2_;
		qreal hardness1_, hardness2_;
		qreal opacity1_, opacity2_;
		QColor color1_, color2_;
		int spacing_;
		bool sensitive_;

		mutable QVector<ushort> cache_;
		mutable qreal cachepressure_;
};

}

#endif


