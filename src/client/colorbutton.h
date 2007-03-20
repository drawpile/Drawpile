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

class PLUGIN_EXPORT ColorButton : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor color READ color WRITE setColor)
	public:
		ColorButton(QWidget *parent=0,const QColor& color = Qt::black);
		~ColorButton() {}

		QColor color() const { return color_; }

	public slots:
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
};

#ifndef DESIGNER_PLUGIN
}
#endif

#endif

