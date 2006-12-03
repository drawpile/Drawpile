/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen, based on the GTK+ color selector (C) The Free Software Foundation

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

#ifndef COLORTRIANGLE_H
#define COLORTRIANGLE_H

#include <QWidget>
#include <QPixmap>
#include <QtDesigner/QDesignerExportWidget>

#ifndef NO_WIDGETS_NAMESPACE
namespace widgets {
#endif

//! HSV color triangle widget
/**
 */
class QDESIGNER_WIDGET_EXPORT ColorTriangle : public QWidget {
	Q_OBJECT
	public:
		ColorTriangle(QWidget *parent=0,const QColor& color = Qt::white);

		int hue() const;
		int saturation() const;
		int value() const;

		QColor color() const;

	public slots:
		void setHue(int hue);
		void setSaturation(int saturation);
		void setValue(int value);
		void setRed(int red);
		void setGreen(int green);
		void setBlue(int blue);
		void setColor(const QColor& color);

	signals:
		void hueChanged(int hue);
		void saturationChanged(int hue);
		void valueChanged(int hue);
		void redChanged(int red);
		void greenChanged(int green);
		void blueChanged(int blue);
		void colorChanged(const QColor& color);

	protected:
		void paintEvent(QPaintEvent *event);
		void resizeEvent(QResizeEvent *event);
		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
	private:
		//! Update the entire color triangle
		void updateColorTriangle();

		//! Get current hue as angle
		qreal hueAngle() const;

		//! Computes the vertices of the saturation/value triangle
		void updateVertices();

		//! Computes whether a point is inside the hue ring
		bool isInRing(qreal x, qreal y) const;

		//! Computes whether a point is inside the saturation/value triangle
		bool isInTriangle(qreal x, qreal y) const;

		//! Set hue based on mouse coordinates
		void setHue(qreal x, qreal y);

		//! Set saturation and value based on the mouse coordinates
		void setSv(qreal x, qreal y);

		//! Generate a hue ring
		void makeRing();

		//! Generate saturation/value triangle
		void makeTriangle();

		enum {NODRAG, DRAGHUE, DRAGSV} mode_;

		qreal hue_;
		qreal saturation_;
		qreal value_;

		int xoff_,yoff_;
		int diameter_;
		qreal center_;
		qreal outer_;
		qreal inner_;
		int hx_, hy_, sx_, sy_, vx_, vy_;
		QPixmap wheel_;
		QPixmap triangle_;
};

#ifndef NO_WIDGETS_NAMESPACE
}
#endif

#endif

