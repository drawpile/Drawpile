/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2022 Calle Laakkonen

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
#ifndef COLORPALETTEDOCK_H
#define COLORPALETTEDOCK_H

#include <QDockWidget>

namespace color_widgets {
	class ColorPalette;
}

namespace docks {

class ColorPaletteDock final : public QDockWidget {
	Q_OBJECT
public:
	ColorPaletteDock(const QString& title, QWidget *parent);
	~ColorPaletteDock() override;

public slots:
	void setColor(const QColor& color);

signals:
	void colorSelected(const QColor& color);

private slots:
	void paletteChanged(int index);
	void addPalette();
	void copyPalette();
	void deletePalette();
	void renamePalette();
	void paletteRenamed();
	void setPaletteReadonly(bool readonly);
	void exportPalette();
	void importPalette();

	void paletteClicked(int index);
	void paletteDoubleClicked(int index);
	void paletteRightClicked(int index);

private:
	struct Private;
	Private *d;
};

int findPaletteColor(const color_widgets::ColorPalette &pal, const QColor &color);

}

#endif

