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
#ifndef PALETTEBOX_H
#define PALETTEBOX_H

#include <QDockWidget>
#include <QList>
class Ui_PaletteBox;

class Palette;
class LocalPalette;

namespace widgets {

class PaletteBox : public QDockWidget {
	Q_OBJECT
	public:
		PaletteBox(const QString& title, QWidget *parent);
		~PaletteBox();

	signals:
		void colorSelected(const QColor& color);

	private slots:
		void paletteChanged(int index);
		void nameChanged(const QString& name);
		void addPalette();
		void deletePalette();

	private:
		Ui_PaletteBox *ui_;
		QList<LocalPalette*> palettes_;
};

}

#endif

