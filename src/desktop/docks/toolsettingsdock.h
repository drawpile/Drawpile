// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_TOOLSETTINGSDOCK_H
#define DESKTOP_DOCKS_TOOLSETTINGSDOCK_H
#include "desktop/docks/dockbase.h"
#include "libclient/tools/tool.h"

class QStackedWidget;

namespace tools {
class AnnotationSettings;
class BrushSettings;
class ColorPickerSettings;
class FillSettings;
class GradientSettings;
class InspectorSettings;
class LaserPointerSettings;
class PanSettings;
class SelectionSettings;
class ToolController;
class ToolSettings;
class TransformSettings;
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
class ToolSettings final : public DockBase {
	Q_OBJECT
public:
	static constexpr int LASTUSED_COLOR_COUNT = 8;

	ToolSettings(tools::ToolController *ctrl, QWidget *parent = nullptr);
	~ToolSettings() override;

	QColor foregroundColor() const;
	QColor backgroundColor() const;

	//! Get the currently selected tool
	tools::Tool::Type currentTool() const;

	//! Get a tool settings page
	tools::ToolSettings *getToolSettingsPage(tools::Tool::Type tool);

	tools::AnnotationSettings *annotationSettings();
	tools::BrushSettings *brushSettings();
	tools::ColorPickerSettings *colorPickerSettings();
	tools::FillSettings *fillSettings();
	tools::GradientSettings *gradientSettings();
	tools::InspectorSettings *inspectorSettings();
	tools::LaserPointerSettings *laserPointerSettings();
	tools::PanSettings *panSettings();
	tools::SelectionSettings *selectionSettings();
	tools::TransformSettings *transformSettings();
	tools::ZoomSettings *zoomSettings();

	//! Save tool related settings
	void saveSettings();

	bool currentToolAffectsCanvas() const;
	bool currentToolAffectsLayer() const;
	bool currentToolRequiresSelection() const;
	bool isCurrentToolLocked() const;

	void triggerUpdate();

public slots:
	//! Set the active tool
	void setTool(tools::Tool::Type tool);

	//! Set the active tool without touching the previous tool and slot values
	void setToolTemporary(tools::Tool::Type tool);

	//! Select the active tool slot (for those tools that have them)
	void setToolSlot(int idx);

	//! Toggle current tool's eraser mode (if it has one)
	void toggleEraserMode();

	//! Toggle current tool's alpha preserve mode (if it has one)
	void toggleAlphaPreserve();

	//! Quick adjust the parameter specified by the type.
	void quickAdjust(int type, qreal adjustment);

	//! Increase or decrease size for current tool by one step
	void stepAdjustCurrent1(bool increase);

	//! Select the tool previosly set with setTool or setToolSlot
	void setPreviousTool();

	void setForegroundColor(const QColor &color);
	void setBackgroundColor(const QColor &color);

	void setCompatibilityMode(bool compatibilityMode);

	//! Pop up a dialog for changing the foreground color
	void changeForegroundColor();

	//! Pop up a dialog for changing the background color
	void changeBackgroundColor();

	//! Switch tool when eraser is brought near the tablet
	void switchToEraserSlot(bool near);

	//! Switch brush to erase mode when eraser is brought near the tablet
	void switchToEraserMode(bool near);

	//! Swap foreground and background color
	void swapColors();

	//! Set foreground color to black and background color to white
	void resetColors();

	//! Add a color to the active last used colors palette
	void addLastUsedColor(const QColor &color);

	//! Switch to the last used color at the given index
	void setLastUsedColor(int i);

	void startTransformMoveActiveLayer();
	void startTransformMoveMask();
	void startTransformPaste(const QRect &srcBounds, const QImage &image);

signals:
	//! This signal is emitted when the current tool changes its size
	void sizeChanged(int size);

	//! This signal is emitted when tool subpixel drawing mode is changed
	void subpixelModeChanged(bool subpixel, bool square, bool force);

	//! Current foreground color selection changed
	void foregroundColorChanged(const QColor &color);

	//! Current background color selection changed
	void backgroundColorChanged(const QColor &color);

	//! Last used color palette changed
	void lastUsedColorsChanged(const color_widgets::ColorPalette &pal);

	//! Currently active tool was changed
	void toolChanged(tools::Tool::Type tool);

	void activeBrushChanged();

	void showMessageRequested(const QString &message);

	void colorAdjustRequested(int channel, int amount);

private:
	void selectTool(tools::Tool::Type tool);
	void startSelection(int type);
	void startTransformMove(bool onlyMask, bool startMove, bool quickMove);
	void clearTemporaryTransform();
	void quickAdjustCurrent1(qreal adjustment);
	void requestColorAdjustment(int channel, qreal adjustment, qreal max);
	static bool hasBrushCursor(tools::Tool::Type tool);

	struct Private;
	Private *d;
};

}

#endif
