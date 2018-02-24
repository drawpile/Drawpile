/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2017 Calle Laakkonen

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

class Ui_ColorBox;
class Palette;

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
	void paletteNameChanged(const QString& name);
	void addPalette();
	void copyPalette();
	void deletePalette();
	void toggleWriteProtect();
	void exportPalette();
	void importPalette();

	void updateFromRgbSliders();
	void updateFromRgbSpinbox();
	void updateFromHsvSliders();
	void updateFromHsvSpinbox();

private:
	Ui_ColorBox *_ui;
	Palette *m_lastused;
	Palette *m_lastusedAlt;

	QAction *m_deletePalette;
	QAction *m_writeprotectPalette;
	QAction *m_exportPalette;
	QAction *m_importPalette;
	bool _updating;

};

}

#endif

