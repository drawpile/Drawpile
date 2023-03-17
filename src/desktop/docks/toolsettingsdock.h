/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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
#ifndef TOOLSETTINGSDOCK_H
#define TOOLSETTINGSDOCK_H

#include "libclient/tools/tool.h"

#include <QDockWidget>

class QStackedWidget;

namespace tools {
	class ToolSettings;
	class ToolController;
}

namespace color_widgets {
	class ColorPalette;
}

namespace docks {

/**
 * @brief Tool settings window
 * A dock widget that displays settings for the currently selected tool.
 */
class ToolSettings final : public QDockWidget
{
Q_OBJECT
public:
	ToolSettings(tools::ToolController *ctrl, QWidget *parent=nullptr);
	~ToolSettings() override;

	//! Get the current foreground color
	QColor foregroundColor() const;

	//! Get the currently selected tool
	tools::Tool::Type currentTool() const;

	//! Get a tool settings page
	tools::ToolSettings *getToolSettingsPage(tools::Tool::Type tool);

	//! Load tool related settings
	void readSettings();

	//! Save tool related settings
	void saveSettings();

public slots:
	//! Set the active tool
	void setTool(tools::Tool::Type tool);

	//! Select the active tool slot (for those tools that have them)
	void setToolSlot(int idx);

	//! Toggle current tool's eraser mode (if it has one)
	void toggleEraserMode();

	//! Toggle current tool's recolor mode (if it has one)
	void toggleRecolorMode();

	//! Quick adjust current tool
	void quickAdjustCurrent1(qreal adjustment);

	//! Increase or decrease size for current tool by one step
	void stepAdjustCurrent1(bool increase);

	//! Select the tool previosly set with setTool or setToolSlot
	void setPreviousTool();

	//! Set foreground color
	void setForegroundColor(const QColor& color);

	//! Pop up a dialog for changing the foreground color
	void changeForegroundColor();

	//! Switch tool when eraser is brought near the tablet
	void eraserNear(bool near);

	//! Swap between the active colors and color history
	void swapLastUsedColors();

	//! Add a color to the active last used colors palette
	void addLastUsedColor(const QColor &color);

signals:
	//! This signal is emitted when the current tool changes its size
	void sizeChanged(int size);

	//! This signal is emitted when tool subpixel drawing mode is changed
	void subpixelModeChanged(bool subpixel, bool square);

	//! Current foreground color selection changed
	void foregroundColorChanged(const QColor &color);

	//! Last used color palette changed
	void lastUsedColorsChanged(const color_widgets::ColorPalette &pal);

	//! Currently active tool was changed
	void toolChanged(tools::Tool::Type tool);

private:
	void selectTool(tools::Tool::Type tool);

	struct Private;
	Private *d;
};

}

#endif

