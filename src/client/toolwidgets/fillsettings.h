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
#ifndef TOOLSETTINGS_FILL_H
#define TOOLSETTINGS_FILL_H

#include "toolsettings.h"

class Ui_FillSettings;

namespace tools {

/**
 * @brief Settings for the flood fill tool
 */
class FillSettings : public QObject, public ToolSettings {
	Q_OBJECT
public:
	FillSettings(const QString &name, const QString &title, ToolController *ctrl);
	~FillSettings();

	void quickAdjust1(float adjustment) override;
	void setForeground(const QColor &color) override;

	virtual int getSize() const override { return 0; }
	virtual bool getSubpixelMode() const override { return false; }

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

private slots:
	void updateTool();

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_FillSettings * _ui;
};

}

#endif

