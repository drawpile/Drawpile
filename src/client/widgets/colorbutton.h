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
#ifndef COLORBUTTON_H
#define COLORBUTTON_H

#include <QWidget>

#ifndef DESIGNER_PLUGIN
namespace widgets {
#define PLUGIN_EXPORT
#else
#include <QtDesigner/QDesignerExportWidget>
#define PLUGIN_EXPORT QDESIGNER_WIDGET_EXPORT
#endif

//! A button for selecting a color
class PLUGIN_EXPORT ColorButton : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor color READ color WRITE setColor)
	Q_PROPERTY(bool setAlpha READ alpha WRITE setAlpha)
	public:
		ColorButton(QWidget *parent=0,const QColor& color = Qt::black);
		~ColorButton() {}

		//! Get the selected color
		QColor color() const { return color_; }

		//! Allow setting of alpha value
		void setAlpha(bool use);

		//! Set alpha value?
		bool alpha() const { return setAlpha_; }

	public slots:
		//! Set color selection
		void setColor(const QColor& color);

	signals:
		void colorChanged(const QColor& color);

	protected:
		void paintEvent(QPaintEvent *);
		void mousePressEvent(QMouseEvent *);
		void mouseReleaseEvent(QMouseEvent *);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);

	private:
		QColor color_;
		bool isdown_;
		bool setAlpha_;
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

