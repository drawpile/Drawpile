// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_SELECTION_H
#define DESKTOP_TOOLWIDGETS_SELECTION_H
#include "desktop/toolwidgets/toolsettings.h"

class KisSliderSpinBox;
class QAction;
class QButtonGroup;
class QCheckBox;
class QLabel;
class QPushButton;

namespace canvas {
class CanvasModel;
}

namespace widgets {
class ExpandShrinkSpinner;
class GroupedToolButton;
}

namespace tools {

class SelectionSettings final : public ToolSettings {
	Q_OBJECT
public:
	SelectionSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("selection"); }

	void setActiveTool(tools::Tool::Type tool) override;

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return false; }

	void stepAdjust1(bool increase) override;

	int getSize() const override;
	bool isSquare() const override { return true; }
	bool requiresOutline() const override { return true; }

	void setAction(QAction *starttransform);
	void setActionEnabled(bool enabled);

	QWidget *getHeaderWidget() override { return m_headerWidget; }

public slots:
	void pushSettings() override;

signals:
	void pixelSizeChanged(int size);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	enum Area { Continuous, Similar };

	void updateSize(int size);
	void updateTolerance();
	static bool isSizeUnlimited(int size);
	static int calculatePixelSize(int size, bool unlimited);

	void setDragState(bool dragging, int tolerance);

	QWidget *m_headerWidget = nullptr;
	QButtonGroup *m_headerGroup = nullptr;
	QWidget *m_selectionContainer = nullptr;
	QCheckBox *m_antiAliasCheckBox = nullptr;
	QWidget *m_magicWandContainer = nullptr;
	KisSliderSpinBox *m_sizeSlider = nullptr;
	KisSliderSpinBox *m_opacitySlider = nullptr;
	KisSliderSpinBox *m_toleranceSlider = nullptr;
	widgets::ExpandShrinkSpinner *m_expandShrink = nullptr;
	KisSliderSpinBox *m_featherSlider = nullptr;
	KisSliderSpinBox *m_closeGapsSlider = nullptr;
	QButtonGroup *m_sourceGroup = nullptr;
	QButtonGroup *m_areaGroup = nullptr;
	QPushButton *m_startTransformButton = nullptr;
	int m_toleranceBeforeDrag = -1;
	bool m_isMagicWand = false;
};

}

#endif
