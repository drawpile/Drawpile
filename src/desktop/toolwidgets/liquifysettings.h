// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_LIQUIFYSETTINGS_H
#define DESKTOP_TOOLWIDGETS_LIQUIFYSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

class KisSliderSpinBox;
class QMenu;
class QPushButton;
class QStackedWidget;

namespace canvas {
class CanvasModel;
}

namespace widgets {
class GroupedToolButton;
}

namespace tools {

class LiquifySettings final : public ToolSettings {
	Q_OBJECT
public:
	LiquifySettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("liquify"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return true; }
	bool isLocked() override { return !m_featureAccess; }
	bool requiresSelection() override { return true; }

	int getSize() const override;
	bool getSubpixelMode() const override { return true; }

	void setFeatureAccess(bool featureAccess);

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void pushSettings() override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

Q_SIGNALS:
	void sizeChanged(int size);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	// Too hard of a hardness causes funky artifacts at the edges of e.g.
	// rotation or expansion. We cap it at a limit that still has smooth edges.
	static constexpr qreal HARDNESS_MULTIPLIER = (0.8 / 0.99);

	enum class Operation {
		Move,
		RotateLeft,
		RotateRight,
		Expand,
		Shrink,
		Smoothe,
		Erase,
		Last = Erase,
	};

	void loadDefaultSettings();

	void setModel(canvas::CanvasModel *canvas);
	void updateEnabled();
	void updateEnabledFrom(canvas::CanvasModel *canvas);

	QWidget *m_headerWidget = nullptr;
	QMenu *m_headerMenu = nullptr;
	QStackedWidget *m_stack = nullptr;
	QButtonGroup *m_operationGroup = nullptr;
	KisSliderSpinBox *m_sizeSlider = nullptr;
	widgets::GroupedToolButton *m_sizePressureButton = nullptr;
	KisSliderSpinBox *m_amountSlider = nullptr;
	widgets::GroupedToolButton *m_amountPressureButton = nullptr;
	KisSliderSpinBox *m_hardnessSlider = nullptr;
	widgets::GroupedToolButton *m_hardnessPressureButton = nullptr;
	KisSliderSpinBox *m_spacingSlider = nullptr;
	QButtonGroup *m_interpolationGroup = nullptr;
	QPushButton *m_applyButton = nullptr;
	QPushButton *m_cancelButton = nullptr;
	qreal m_lastEmittedSize = 0.0;
	bool m_featureAccess = true;
};

}

#endif
