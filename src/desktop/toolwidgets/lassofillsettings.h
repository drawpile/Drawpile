// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_LASSOFILLSETTINGS_H
#define DESKTOP_TOOLWIDGETS_LASSOFILLSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

class BlendModeManager;
class KisSliderSpinBox;
class QAction;
class QActionGroup;
class QCheckBox;
class QComboBox;
class QMenu;
class QPushButton;

namespace widgets {
class GroupedToolButton;
}

namespace tools {

class LassoFillSettings final : public ToolSettings {
	Q_OBJECT
public:
	LassoFillSettings(ToolController *ctrl, QObject *parent = nullptr);

	void setActions(QAction *automaticAlphaPreserve, QAction *maskSelection);

	QString toolType() const override { return QStringLiteral("lassofill"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return true; }
	bool isLocked() override { return !m_featureAccess; }

	void setFeatureAccess(bool featureAccess);
	void setCompatibilityMode(bool compatibilityMode) override;

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void toggleEraserMode() override;
	void toggleAlphaPreserve() override;
	void toggleBlendMode(int blendMode) override;

	void pushSettings() override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	void updateStabilizationMode(QAction *action);
	int getCurrentStabilizationMode() const;

	void setButtonState(bool pending);

	QWidget *m_headerWidget = nullptr;
	QMenu *m_headerMenu = nullptr;
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
	BlendModeManager *m_blendModeManager = nullptr;
	bool m_featureAccess = true;
};

}

#endif
