/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include <QSharedDataPointer>
#include <QColor>

namespace dpcore {

class Point;

struct BrushMaskData : public QSharedData
{
	BrushMaskData() : data(0), dia(0), pressure(-1) { }
	BrushMaskData(const BrushMaskData& other);
	~BrushMaskData() { delete [] data; }

	uchar *data;
	int dia;
	int pressure;
};

//! A brush mask
/**
 * This is an implicitly shared class that holds the alpha map of the
 * brush shape.
 */
class BrushMask
{
	public:
		//! Number of pressure levels supported.
		/**
		 * The limiting factor is the number of bits in the protocol.
		 * Adjust accordingly here. This setting affects brush caching.
		 */
		static const int PRESSURE_LEVELS = 256;

		//! Create an empty brush
		BrushMask() : d(0) { }

		//! Create a new rendered brush
		BrushMask(int dia, qreal pressure);

		//! Is this brush still valid
		bool isFresh(qreal pressure, bool sensitive) const;

		//! Get write access to raw data
		uchar *data() { return d->data; }

		//! Get read only access to raw data
		const uchar *data() const { return d->data; }

		//! Get the diameter of the rendered brush.
		int diameter() const { return d->dia; }

	private:
		QSharedDataPointer<BrushMaskData> d;
};

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
		void setRadius(int radius);
		//! Set radius for light brush
		void setRadius2(int radius);

		//! Set hardness for heavy brush
		void setHardness(qreal hardness);
		//! Set hardness for light brush
		void setHardness2(qreal hardness);

		//! Set opacity for heavy brush
		void setOpacity(qreal opacity);
		//! Set opacity for light brush
		void setOpacity2(qreal opacity);

		//! Set color for heavy brush
		void setColor(const QColor& color);
		//! Set color for light brush
		void setColor2(const QColor& color);

		//! Set spacing hint
		void setSpacing(int spacing);

		//! Set subpixel hint
		void setSubPixel(bool sp);

		//! Set blending mode hint
		void setBlendingMode(int mode);

		//! Get interpolated radius
		int radius(qreal pressure) const;
		//! Get the diameter of the rendered brush
		int diameter(qreal pressure) const;
		//! Get interpolated hardness
		qreal hardness(qreal pressure) const;
		//! Get interpolated opacity
		qreal opacity(qreal pressure) const;
		//! Get interpolated color
		QColor color(qreal pressure) const;
		//! Get spacing hint
		int spacing() const;
		//! Should subpixel rendering be used?
		bool subpixel() const { return subpixel_; }
		//! Get the suggested blending mode
		int blendingMode() const { return blend_; }

		//! Render the brush
		BrushMask render(qreal pressure) const;

		//! Render the brush with an offset
		BrushMask render_subsampled(qreal x, qreal y, qreal pressure) const;

		//! Equality test
		bool operator==(const Brush& brush) const;

		//! Inequality test
		bool operator!=(const Brush& brush) const;

	private:
		//! Check if the brush is sensitive to pressure
		void checkSensitivity();

		int radius1_, radius2_;
		qreal hardness1_, hardness2_;
		qreal opacity1_, opacity2_;
		QColor color1_, color2_;
		int spacing_;
		bool sensitive_;
		bool subpixel_;
		int blend_;

		mutable BrushMask cache_;
};

}

#endif


