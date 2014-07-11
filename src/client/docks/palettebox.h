/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PALETTEBOX_H
#define PALETTEBOX_H

#include <QDockWidget>
#include <QList>

#include "utils/palette.h"

class Ui_PaletteBox;

namespace docks {

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
	bool isUniquePaletteName(const QString& name, int excludeIdx) const;

	Ui_PaletteBox *_ui;
	QList<Palette> _palettes;
};

}

#endif

