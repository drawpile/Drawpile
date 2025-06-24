// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_GRADIENTSETTINGS_H
#define DESKTOP_TOOLWIDGETS_GRADIENTSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"
#include "libclient/utils/debouncetimer.h"

class BlendModeManager;
class KisDoubleSliderSpinBox;
class KisSliderSpinBox;
class QAction;
class QButtonGroup;
class QComboBox;
class QLabel;
class QMenu;
class QPushButton;
class QStackedWidget;

namespace widgets {
class GroupedToolButton;
}

namespace tools {

class GradientSettings final : public ToolSettings {
	Q_OBJECT
public:
	GradientSettings(ToolController *ctrl, QObject *parent = nullptr);

	void setActions(QAction *automaticAlphaPreserve);

	QString toolType() const override { return QStringLiteral("gradient"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return true; }
	bool isLocked() override { return !m_featureAccess; }
	bool requiresSelection() override { return true; }

	void setForeground(const QColor &) override { updateColor(); }
	void setBackground(const QColor &) override { updateColor(); }
	void setCompatibilityMode(bool compatibilityMode) override;
	void setFeatureAccess(bool featureAccess);

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void pushSettings() override;
	void toggleEraserMode() override;
	void toggleAlphaPreserve() override;
	void toggleBlendMode(int blendMode) override;

	void quickAdjust2(qreal adjustment) override;
	void quickAdjust3(qreal adjustment) override;
	void stepAdjust2(bool increase) override;
	void stepAdjust3(bool increase) override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

	void setActions(QAction *selectAll, QAction *selectLayerBounds);
	void setSelectionValid(bool selectionValid);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	enum class Gradient {
		ForegroundToTransparent,
		TransparentToForeground,
		ForegroundToBackground,
		BackgroundToForeground,
	};

	static void checkGroupButton(QButtonGroup *group, int id);

	void updateColor();

	void setButtonState(bool pending);

	QWidget *m_headerWidget = nullptr;
	QMenu *m_headerMenu = nullptr;
	QStackedWidget *m_stack = nullptr;
	QButtonGroup *m_gradientGroup = nullptr;
	KisSliderSpinBox *m_fgOpacitySpinner = nullptr;
	KisSliderSpinBox *m_bgOpacitySpinner = nullptr;
	QButtonGroup *m_shapeGroup = nullptr;
	KisDoubleSliderSpinBox *m_focusSpinner = nullptr;
	QButtonGroup *m_spreadGroup = nullptr;
	widgets::GroupedToolButton *m_alphaPreserveButton = nullptr;
	QComboBox *m_blendModeCombo = nullptr;
	QPushButton *m_applyButton = nullptr;
	QPushButton *m_cancelButton = nullptr;
	QPushButton *m_selectAllButton = nullptr;
	QPushButton *m_selectLayerBoundsButton = nullptr;
	BlendModeManager *m_blendModeManager = nullptr;
	DebounceTimer m_colorDebounce;
	qreal m_quickAdjust2 = 0.0;
	qreal m_quickAdjust3 = 0.0;
	bool m_featureAccess = true;
};

}

#endif
