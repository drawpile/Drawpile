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
#ifndef BRUSH_H
#define BRUSH_H

#include <QPixmap>

namespace drawingboard {

//! A brush for drawing onto a layer
/**
 * This class produces a pixmap that can be used as a brush.
 * Two sets of parameters are set and the pixmap is created by
 * interpolating between them.
 */
class Brush
{
	public:
		Brush(int radius=8, qreal hardness=0, qreal opacity=1.0,
				const QColor& color=Qt::black);

		//! Set radius heavy brush
		void setRadius(int radius);
		//! Set radius for light brush
		void setRadius2(int radius);

		//! Set hardness for heavy brush
		void setHardness(qreal hardness);
		//! Set hardness for light brush
		void setHardness2(qreal hardness);

		//! Set opacity for heavy brush (doesn't affect the brush pixmap)
		void setOpacity(qreal opacity);
		//! Set opacity for light brush (doesn't affect the brush pixmap)
		void setOpacity2(qreal opacity);

		//! Set color for heavy brush
		void setColor(const QColor& color);
		//! Set color for light brush
		void setColor2(const QColor& color);

		//! Get interpolated radius
		int radius(qreal pressure) const;
		//! Get interpolated hardness
		qreal hardness(qreal pressure) const;
		//! Get interpolated opacity
		qreal opacity(qreal pressure) const;
		//! Get interpolated color
		QColor color(qreal pressure) const;

		//! Get a brush pixmap to paint with
		QPixmap getBrush(qreal pressure) const;

		//! Copy operator
		Brush& operator=(const Brush& brush);
	private:
		void checkSensitivity();

		int radius1_, radius2_;
		qreal hardness1_, hardness2_;
		qreal opacity1_, opacity2_;
		QColor color1_, color2_;
		bool sensitive_;

		mutable QPixmap cache_;
		mutable qreal cachepressure_;
};

}

#endif


