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

#include "toolsettings.h"
#include "utils/palette.h"

namespace widgets {
	class PaletteWidget;
}
class QCheckBox;
class QSpinBox;

namespace tools {

/**
 * @brief Color picker history
 */
class ColorPickerSettings : public QObject, public ToolSettings {
Q_OBJECT
public:
	ColorPickerSettings(const QString &name, const QString &title, ToolController *ctrl);
	~ColorPickerSettings();

	virtual ToolProperties saveToolSettings() override;
	virtual void restoreToolSettings(const ToolProperties &cfg) override;

	virtual void setForeground(const QColor &color) override { Q_UNUSED(color); }
	virtual void quickAdjust1(float adjustment) override;

	virtual int getSize() const override;
	virtual bool getSubpixelMode() const override { return false; }



public slots:
	void addColor(const QColor &color);

private slots:
	void updateTool();

signals:
	void colorSelected(const QColor &color);

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Palette m_palette;
	widgets::PaletteWidget *m_palettewidget;
	QCheckBox *m_layerpick;
	QSpinBox *m_size;
};

}

#endif


