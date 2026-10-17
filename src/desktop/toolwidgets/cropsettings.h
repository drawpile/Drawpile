// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_CROPSETTINGS_H
#define DESKTOP_TOOLWIDGETS_CROPSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

class KisSliderSpinBox;
class QAction;
class QActionGroup;
class QButtonGroup;
class QCheckBox;
class QPushButton;

namespace widgets {
class GroupedToolButton;
}

namespace tools {

class CropSettings final : public ToolSettings {
	Q_OBJECT
public:
	CropSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("crop"); }

	bool affectsCanvas() override { return false; }
	bool affectsLayer() override { return false; }

	void pushSettings() override;
	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	static constexpr int POSITION_SPINNER_MIN = -9999999;
	static constexpr int POSITION_SPINNER_MAX = 9999999;
	static constexpr int SIZE_SPINNER_MIN = 1;
	static constexpr int SIZE_SPINNER_MAX = 9999999;

	void showCropColorDialog();
	void setCropGuides(int cropGuides);
	void updateCropGuides(QAction *action);
	void setCropArea(QRect cropArea);
	void pushCropArea();

	static void setSpinner(
		KisSliderSpinBox *spinner, bool enabled, int value, int min, int max);

	QWidget *m_headerWidget = nullptr;
	QButtonGroup *m_headerGroup = nullptr;
	QActionGroup *m_guidesGroup = nullptr;
	KisSliderSpinBox *m_xSpinner = nullptr;
	KisSliderSpinBox *m_ySpinner = nullptr;
	widgets::GroupedToolButton *m_centerButton = nullptr;
	KisSliderSpinBox *m_widthSpinner = nullptr;
	KisSliderSpinBox *m_heightSpinner = nullptr;
	widgets::GroupedToolButton *m_constrainButton = nullptr;
	QCheckBox *m_constrainToCanvasBox = nullptr;
	QPushButton *m_cancelButton = nullptr;
};

}

#endif
