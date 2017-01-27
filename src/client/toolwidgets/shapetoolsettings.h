/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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
#ifndef TOOLSETTINGS_SHAPETOOLS_H
#define TOOLSETTINGS_SHAPETOOLS_H

#include "toolsettings.h"

class Ui_SimpleSettings;

namespace tools {

/**
 * @brief Settings for tools without pressure sensitivity
 *
 * This class is used for the shape drawing tools.
 */
class SimpleSettings : public ToolSettings {
public:
	enum Type {Line, Rectangle, Ellipse};

	SimpleSettings(const QString &name, const QString &title, const QString &icon, Type type, bool sp, ToolController *ctrl);
	~SimpleSettings();

	tools::Tool::Type toolType() const override;

	void pushSettings() override;
	void setForeground(const QColor& color) override;
	void quickAdjust1(float adjustment) override;

	int getSize() const override;
	bool getSubpixelMode() const override;

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

protected:
	virtual QWidget *createUiWidget(QWidget *parent);

private:
	Ui_SimpleSettings *m_ui;
	Type m_type;
	bool m_subpixel;
};

}

#endif

