/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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
#ifndef TOOLSETTINGS_ZOOM_H
#define TOOLSETTINGS_ZOOM_H

#include "desktop/toolwidgets/toolsettings.h"

namespace tools {

/**
 * @brief Zoom tool options
 */
class ZoomSettings : public ToolSettings {
	Q_OBJECT
public:
	ZoomSettings(ToolController *ctrl, QObject *parent=nullptr);

	QString toolType() const override { return QStringLiteral("zoom"); }

	void setForeground(const QColor &color) override { Q_UNUSED(color); }

	int getSize() const override { return 0; }
	bool getSubpixelMode() const override { return false; }

signals:
	void resetZoom();
	void fitToWindow();

protected:
	QWidget *createUiWidget(QWidget *parent) override;
};

}

#endif


