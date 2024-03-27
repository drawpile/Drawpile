// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_SELECTION_H
#define TOOLSETTINGS_SELECTION_H

#include "desktop/toolwidgets/toolsettings.h"
#include "libclient/tools/selection.h"

class Ui_SelectionSettings;

namespace canvas {
class CanvasModel;
}

namespace view {
class CanvasWrapper;
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

	void setCanvasView(view::CanvasWrapper *canvasView) { m_canvasView = canvasView; }

	void setForeground(const QColor &) override {}

	int getSize() const override { return 0; }
	bool getSubpixelMode() const override { return false; }

	void setControlsEnabled(bool enabled);
	void setCompatibilityMode(bool compatibilityMode);

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

	void updateSelectionMode(canvas::Selection::AdjustmentMode mode);
	void cutSelection();

	Ui_SelectionSettings *m_ui;
	view::CanvasWrapper *m_canvasView;
};

}

#endif
