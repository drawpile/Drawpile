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

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif
//! HSV color triangle widget
/**
 */
class PLUGIN_EXPORT ColorTriangle : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor color READ color WRITE setColor)
	Q_PROPERTY(int hue READ hue)
	Q_PROPERTY(int saturation READ saturation)
	Q_PROPERTY(int value READ value)
	public:
		ColorTriangle(QWidget *parent=0,const QColor& color = Qt::white);

		int hue() const;
		int saturation() const;
		int value() const;

		QColor color() const;

	public slots:
		void setColor(const QColor& color);

	signals:
		void colorChanged(const QColor& color);

	protected:
		void paintEvent(QPaintEvent *event);
		void resizeEvent(QResizeEvent *event);
		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);

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
		qreal inner_,innert_;
		int hx_, hy_, sx_, sy_, vx_, vy_;
		QPixmap wheel_;
		QPixmap triangle_;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

