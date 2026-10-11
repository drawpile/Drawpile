// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLSETTINGS_ZOOM_H
#define TOOLSETTINGS_ZOOM_H
#include "desktop/toolwidgets/toolsettings.h"

class QAction;
class QButtonGroup;

namespace widgets {
class GroupedToolButton;
}

namespace tools {

class ZoomSettings final : public ToolSettings {
	Q_OBJECT
public:
	ZoomSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("zoom"); }

	bool affectsCanvas() override { return false; }
	bool affectsLayer() override { return false; }

	void setActions(
		QAction *zoomIn, QAction *resetZoom, QAction *zoomToFit,
		QAction *zoomOut);

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;
	void pushSettings() override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	QWidget *m_headerWidget = nullptr;
	widgets::GroupedToolButton *m_zoomInButton = nullptr;
	widgets::GroupedToolButton *m_resetZoomButton = nullptr;
	widgets::GroupedToolButton *m_zoomToFitButton = nullptr;
	widgets::GroupedToolButton *m_zoomOutButton = nullptr;
	widgets::GroupedToolButton *m_modeDiscreteButton = nullptr;
	widgets::GroupedToolButton *m_modeScrubButton = nullptr;
	QButtonGroup *m_zoomModeGroup = nullptr;
};

}

#endif
