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
	bool isLocked() override;

	void setAction(QAction *starttransform);

	void setPutImageAllowed(bool putImageAllowed)
	{
		m_putImageAllowed = putImageAllowed;
	}

	QWidget *getHeaderWidget() override { return m_headerWidget; }

public slots:
	void pushSettings() override;

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	enum Area { Continuous, Similar };

	void setModel(canvas::CanvasModel *canvas);
	void updateEnabled();
	void updateEnabledFrom(canvas::CanvasModel *canvas);

	QWidget *m_headerWidget = nullptr;
	QButtonGroup *m_headerGroup = nullptr;
	QWidget *m_selectionContainer = nullptr;
	QCheckBox *m_antiAliasCheckBox = nullptr;
	QWidget *m_magicWandContainer = nullptr;
	KisSliderSpinBox *m_toleranceSlider = nullptr;
	KisSliderSpinBox *m_expandSlider = nullptr;
	KisSliderSpinBox *m_featherSlider = nullptr;
	KisSliderSpinBox *m_closeGapsSlider = nullptr;
	QButtonGroup *m_sourceGroup = nullptr;
	QButtonGroup *m_areaGroup = nullptr;
	QPushButton *m_startTransformButton = nullptr;
	bool m_putImageAllowed = true;
};

}

#endif
