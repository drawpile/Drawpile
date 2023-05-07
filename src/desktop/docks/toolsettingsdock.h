// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGSDOCK_H
#define TOOLSETTINGSDOCK_H

#include "libclient/tools/tool.h"

#include <QDockWidget>

class QStackedWidget;

namespace tools {
	class AnnotationSettings;
	class BrushSettings;
	class ColorPickerSettings;
	class InspectorSettings;
	class LaserPointerSettings;
	class SelectionSettings;
	class ToolController;
	class ToolSettings;
	class ZoomSettings;
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

	tools::AnnotationSettings *annotationSettings();
	tools::BrushSettings *brushSettings();
	tools::ColorPickerSettings *colorPickerSettings();
	tools::InspectorSettings *inspectorSettings();
	tools::LaserPointerSettings *laserPointerSettings();
	tools::SelectionSettings *selectionSettings();
	tools::ZoomSettings *zoomSettings();

	//! Save tool related settings
	void saveSettings();

	bool isCurrentToolLocked() const;

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

	void activeBrushChanged();

private:
	void selectTool(tools::Tool::Type tool);

	struct Private;
	Private *d;
};

}

#endif

