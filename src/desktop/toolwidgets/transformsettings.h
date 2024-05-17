// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_TRANSFORMSETTINGS_H
#define DESKTOP_TOOLWIDGETS_TRANSFORMSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

class QAction;
class QButtonGroup;
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

	void setCompatibilityMode(bool compatibilityMode);

	void setActions(
		QAction *mirror, QAction *flip, QAction *rotatecw, QAction *rotateccw,
		QAction *shrinktoview, QAction *stamp);

	void setCanvasView(view::CanvasWrapper *canvasView)
	{
		m_canvasView = canvasView;
	}

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void pushSettings() override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

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

	TransformTool *tool();

	QWidget *m_headerWidget = nullptr;
	widgets::GroupedToolButton *m_mirrorButton = nullptr;
	widgets::GroupedToolButton *m_flipButton = nullptr;
	widgets::GroupedToolButton *m_rotateCwButton = nullptr;
	widgets::GroupedToolButton *m_rotateCcwButton = nullptr;
	widgets::GroupedToolButton *m_shrinkToViewButton = nullptr;
	QButtonGroup *m_interpolationGroup = nullptr;
	widgets::GroupedToolButton *m_scaleButton = nullptr;
	widgets::GroupedToolButton *m_distortButton = nullptr;
	QButtonGroup *m_handlesGroup = nullptr;
	QPushButton *m_applyButton = nullptr;
	QPushButton *m_cancelButton = nullptr;
	view::CanvasWrapper *m_canvasView = nullptr;
};

}

#endif
