/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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
#ifndef BRUSHSLIDER_H
#define BRUSHSLIDER_H

#include <QAbstractSlider>

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

//! Brush slider
/**
 * A slider for intuitively changing brush parameters.
 */
class PLUGIN_EXPORT BrushSlider : public QAbstractSlider {
	Q_OBJECT
	Q_PROPERTY(Style style READ style WRITE setStyle)
	Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
	Q_ENUMS(Style)
	public:
		enum Style {Size, Opacity, Hardness, Spacing};

		BrushSlider(QWidget *parent=0);

		void setStyle(Style style);
		Style style() const { return style_; }

		void setSuffix(const QString& suffix);
		const QString& suffix() const { return suffix_; }

	protected:
		void paintEvent(QPaintEvent *);
		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);

	private:
		void drawCircle(QPainter& painter, qreal rad, qreal x, qreal y, qreal value);
		void setPosition(int x,int y);

		Style style_;
		QString suffix_;
		enum ClickZone {BUTTON, SLIDER, HANDLE};
		ClickZone click_;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif


