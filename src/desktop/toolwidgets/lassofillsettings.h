// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_LASSOFILLSETTINGS_H
#define DESKTOP_TOOLWIDGETS_LASSOFILLSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

class KisSliderSpinBox;
class QActionGroup;
class QAction;
class QCheckBox;
class QComboBox;
class QPushButton;

namespace widgets {
class GroupedToolButton;
}

namespace tools {

class LassoFillSettings final : public ToolSettings {
	Q_OBJECT
public:
	LassoFillSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("lassofill"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return true; }
	bool isLocked() override { return !m_featureAccess; }

	void setFeatureAccess(bool featureAccess);

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void toggleEraserMode() override;
	void toggleAlphaPreserve() override;

	void pushSettings() override;

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	void initBlendModeOptions(bool compatibilityMode);
	void updateAlphaPreserve(bool alphaPreserve);
	void updateBlendMode(int index);
	void selectBlendMode(int blendMode);
	int getCurrentBlendMode() const;

	void updateStabilizationMode(QAction *action);
	int getCurrentStabilizationMode() const;

	void setAutomaticAlphaPerserve(bool automaticAlphaPreserve);

	void setButtonState(bool pending);

	KisSliderSpinBox *m_opacitySpinner = nullptr;
	KisSliderSpinBox *m_stabilizerSpinner = nullptr;
	KisSliderSpinBox *m_smoothingSpinner = nullptr;
	widgets::GroupedToolButton *m_stabilizerButton = nullptr;
	QActionGroup *m_stabilizationModeGroup = nullptr;
	QAction *m_stabilizerAction = nullptr;
	QAction *m_smoothingAction = nullptr;
	widgets::GroupedToolButton *m_alphaPreserveButton = nullptr;
	QComboBox *m_blendModeCombo = nullptr;
	QCheckBox *m_antiAliasCheckBox;
	QPushButton *m_applyButton = nullptr;
	QPushButton *m_cancelButton = nullptr;
	int m_previousMode;
	int m_previousEraseMode;
	bool m_automaticAlphaPreserve = true;
	bool m_featureAccess = true;
};

}

#endif
