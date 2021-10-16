/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2021 Calle Laakkonen

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
#ifndef COLORBOX_H
#define COLORBOX_H

#include <QDockWidget>

namespace docks {

class ColorBox : public QDockWidget {
	Q_OBJECT
public:
	ColorBox(const QString& title, QWidget *parent);
	~ColorBox();

public slots:
	void setColor(const QColor& color);
	void addLastUsedColor(const QColor &color);
	void swapLastUsedColors();

signals:
	void colorChanged(const QColor& color);

private slots:
	void paletteChanged(int index);
	void addPalette();
	void copyPalette();
	void deletePalette();
	void renamePalette();
	void paletteRenamed();
	void exportPalette();
	void importPalette();

	void paletteClicked(int index);
	void paletteDoubleClicked(int index);
	void paletteRightClicked(int index);

	void updateFromRgbSliders();
	void updateFromRgbSpinbox();
	void updateFromHsvSliders();
	void updateFromHsvSpinbox();

	void updateSettings();

private:
	QWidget *createPalettePage();
	QWidget *createSlidersPage();
	QWidget *createWheelPage();
	void createSliderPage();
	void createSpinnerPage();

	struct Private;
	Private *d;
};

}

#endif

