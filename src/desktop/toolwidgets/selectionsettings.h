// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_SELECTION_H
#define TOOLSETTINGS_SELECTION_H

#include "desktop/toolwidgets/toolsettings.h"
#include "libclient/tools/selection.h"

class Ui_SelectionSettings;

namespace canvas {
class CanvasModel;
}

namespace widgets {
class CanvasView;
}

namespace tools {

/**
 * @brief Settings and actions for canvas selections (rectangular and freeform)
 *
 * Unlike most tool settings widgets, this one also includes buttons to trigger
 * various actions on the active selection (e.g. flip/mirror.)
 */
class SelectionSettings final : public ToolSettings {
	Q_OBJECT
public:
	SelectionSettings(ToolController *ctrl, QObject *parent = nullptr);
	~SelectionSettings() override;

	QString toolType() const override { return QStringLiteral("selection"); }

	/**
	 * @brief Set the view widget
	 *
	 * This is used to get the current view rectangle needed by fitToScreen()
	 */
	void setView(widgets::CanvasView *view) { m_view = view; }

	void setForeground(const QColor &) override {}

	int getSize() const override { return 0; }
	bool getSubpixelMode() const override { return false; }

	void setControlsEnabled(bool enabled);

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

public slots:
	void pushSettings() override;

private slots:
	void flipSelection();
	void mirrorSelection();
	void fitToScreen();
	void resetSize();
	void scale();
	void rotateShear();
	void distort();
	void modelChanged(canvas::CanvasModel *model);
	void selectionChanged(canvas::Selection *selection);
	void selectionClosed();
	void selectionAdjustmentModeChanged(canvas::Selection::AdjustmentMode mode);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	tools::RectangleSelection *getRectangleSelectionTool();
	tools::PolygonSelection *getPolygonSelectionTool();

	void updateSelectionMode(canvas::Selection::AdjustmentMode mode);
	void cutSelection();

	Ui_SelectionSettings *m_ui;
	widgets::CanvasView *m_view;
};

}

#endif
