/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2019 Calle Laakkonen

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
#ifndef BRUSHPALETTEDOCK_H
#define BRUSHPALETTEDOCK_H

#include "tools/tool.h"

#include <QDockWidget>

namespace tools {
	class ToolProperties;
	class ToolSettings;
}

namespace docks {

/**
 * @brief A brush palette dock
 * 
 * This dock displays a list of brush presets to choose from.
 */
class BrushPalette : public QDockWidget
{
Q_OBJECT
public:
	BrushPalette(QWidget *parent=nullptr);
	~BrushPalette();

	void connectBrushSettings(tools::ToolSettings *toolSettings);

private slots:
	void addBrush();
	void overwriteBrush();
	void deleteBrush();
	void addFolder();
	void deleteFolder();

private:
	struct Private;
	Private *d;
};

}

#endif

