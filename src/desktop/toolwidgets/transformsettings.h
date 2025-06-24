// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_TRANSFORMSETTINGS_H
#define DESKTOP_TOOLWIDGETS_TRANSFORMSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

class BlendModeManager;
class KisSliderSpinBox;
class QAction;
class QButtonGroup;
class QComboBox;
class QPushButton;

namespace canvas {
class CanvasModel;
}

namespace view {
class CanvasWrapper;
}

namespace widgets {
class GroupedToolButton;
}

namespace tools {

class TransformTool;

class TransformSettings final : public ToolSettings {
	Q_OBJECT
public:
	TransformSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("transform"); }

	bool affectsCanvas() override { return true; }
	bool affectsLayer() override { return false; }

	void setActions(
		QAction *mirror, QAction *flip, QAction *rotatecw, QAction *rotateccw,
		QAction *shrinktoview, QAction *stamp);

	void setCanvasView(view::CanvasWrapper *canvasView)
	{
		m_canvasView = canvasView;
	}

	void setCompatibilityMode(bool compatibilityMode) override;

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void pushSettings() override;
	void toggleEraserMode() override;
	void toggleAlphaPreserve() override;
	void toggleBlendMode(int blendMode) override;

	void quickAdjust2(qreal adjustment) override;
	void stepAdjust2(bool increase) override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

signals:
	void blendModeChanged(int blendMode);
	void opacityChanged(qreal opacity);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	void mirror();
	void flip();
	void rotateCw();
	void rotateCcw();
	void shrinkToView();
	void stamp();

	void setModel(canvas::CanvasModel *canvas);
	void updateEnabled();
	void updateEnabledFrom(canvas::CanvasModel *canvas);
	void updateHandles(int mode);
	void showHandles();

	void updateConstrain(bool constrain);
	void updateCenter(bool center);

	void updateOpacity(int value);
	void setOpacity(qreal opacity);

	TransformTool *tool();

	QWidget *m_headerWidget = nullptr;
	widgets::GroupedToolButton *m_mirrorButton = nullptr;
	widgets::GroupedToolButton *m_flipButton = nullptr;
	widgets::GroupedToolButton *m_rotateCwButton = nullptr;
	widgets::GroupedToolButton *m_rotateCcwButton = nullptr;
	widgets::GroupedToolButton *m_shrinkToViewButton = nullptr;
	QButtonGroup *m_previewGroup = nullptr;
	widgets::GroupedToolButton *m_fastButton = nullptr;
	widgets::GroupedToolButton *m_accurateButton = nullptr;
	widgets::GroupedToolButton *m_pseudoFastButton = nullptr;
	QButtonGroup *m_interpolationGroup = nullptr;
	widgets::GroupedToolButton *m_showHandlesButton = nullptr;
	widgets::GroupedToolButton *m_scaleButton = nullptr;
	widgets::GroupedToolButton *m_distortButton = nullptr;
	QButtonGroup *m_handlesGroup = nullptr;
	widgets::GroupedToolButton *m_alphaPreserveButton = nullptr;
	QComboBox *m_blendModeCombo = nullptr;
	widgets::GroupedToolButton *m_constrainButton = nullptr;
	widgets::GroupedToolButton *m_centerButton = nullptr;
	KisSliderSpinBox *m_opacitySlider = nullptr;
	QPushButton *m_applyButton = nullptr;
	QPushButton *m_cancelButton = nullptr;
	BlendModeManager *m_blendModeManager = nullptr;
	view::CanvasWrapper *m_canvasView = nullptr;
	qreal m_quickAdjust2 = 0.0;
	bool m_handleButtonsVisible = true;
};

}

#endif
