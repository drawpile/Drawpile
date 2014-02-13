/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#ifndef COLORBOX_H
#define COLORBOX_H

#include <QDockWidget>

class Ui_ColorBox;

namespace docks {

class ColorBox : public QDockWidget {
	Q_OBJECT
	public:
		enum Mode {RGB, HSV};

		ColorBox(const QString& title, Mode mode, QWidget *parent);
		~ColorBox();

	public slots:
		void setColor(const QColor& color);

	signals:
		void colorChanged(const QColor& color);

	private slots:
		void updateColor();

	private:
		void updateSliders();
		Ui_ColorBox *ui_;
		bool updating_;
		Mode mode_;
};

}

#endif

