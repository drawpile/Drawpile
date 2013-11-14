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
#ifndef GRADIENTSLIDER_H
#define GRADIENTSLIDER_H

#include <QAbstractSlider>

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

//! Gradient slider
/**
 * A slider that displays either a simple two color gradient or
 * a HSV gradient.
 */
class PLUGIN_EXPORT GradientSlider : public QAbstractSlider {
	Q_OBJECT
	Q_PROPERTY(Mode mode READ mode WRITE setMode)
	Q_PROPERTY(QColor color1 READ color1 WRITE setColor1)
	Q_PROPERTY(QColor color2 READ color2 WRITE setColor2)
	Q_PROPERTY(qreal colorSaturation READ colorSaturation WRITE setColorSaturation)
	Q_PROPERTY(qreal colorValue READ colorValue WRITE setColorValue)
	Q_ENUMS(Mode)
	public:
		enum Mode {Color, Hsv};

		GradientSlider(QWidget *parent=0);

		void setMode(Mode mode);
		Mode mode() const { return mode_; }

		const QColor& color1() const { return color1_; }
		const QColor& color2() const { return color2_; }
		qreal colorSaturation() const { return saturation_; }
		qreal colorValue() const { return value_; }

	public slots:
		//! Set the first color (used in simple gradient mode)
		void setColor1(const QColor& color);
		//! Set the second color (used in simple gradient mode
		void setColor2(const QColor& color);
		//! Set saturation (used in HSV mode)
		void setColorSaturation(qreal saturation);
		//! Set (color) value (used in HSV mode)
		void setColorValue(qreal value);

	protected:
		void paintEvent(QPaintEvent *);
		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);

	private:
		void setPosition(int x,int y);

		// Colors for simple color mode
		QColor color1_;
		QColor color2_;

		// Saturation and value for HSV mode
		qreal saturation_,value_;

		Mode mode_;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

