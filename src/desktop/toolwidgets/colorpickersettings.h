/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2016 Calle Laakkonen

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
#ifndef TOOLSETTINGS_COLORPICKER_H
#define TOOLSETTINGS_COLORPICKER_H

#include "desktop/toolwidgets/toolsettings.h"

namespace color_widgets {
	class Swatch;
}
class QCheckBox;
class QSpinBox;

namespace tools {

/**
 * @brief Color picker history
 */
class ColorPickerSettings : public ToolSettings {
Q_OBJECT
public:
	ColorPickerSettings(ToolController *ctrl, QObject *parent=nullptr);
	~ColorPickerSettings();

	QString toolType() const override { return QStringLiteral("picker"); }

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setForeground(const QColor &color) override { Q_UNUSED(color); }
	void quickAdjust1(qreal adjustment) override;
	void stepAdjust1(bool increase) override;

	int getSize() const override;
	bool getSubpixelMode() const override { return false; }

public slots:
	void addColor(const QColor &color);
	void pushSettings() override;
	void openColorDialog();
	void selectColorFromDialog(const QColor &color);

signals:
	void colorSelected(const QColor &color);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	color_widgets::Swatch *m_palettewidget;
	QCheckBox *m_layerpick;
	QSpinBox *m_size;
	qreal m_quickAdjust1 = 0.0;
};

}

#endif


