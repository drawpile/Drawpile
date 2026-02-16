// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOOLWIDGETS_ROTATIONSETTINGS_H
#define DESKTOP_TOOLWIDGETS_ROTATIONSETTINGS_H
#include "desktop/toolwidgets/toolsettings.h"

class QAction;
class QButtonGroup;

namespace widgets {
class GroupedToolButton;
}

namespace tools {

class RotationSettings final : public ToolSettings {
	Q_OBJECT
public:
	RotationSettings(ToolController *ctrl, QObject *parent = nullptr);

	QString toolType() const override { return QStringLiteral("rotation"); }

	bool affectsCanvas() override { return false; }
	bool affectsLayer() override { return false; }

	void
	setActions(QAction *rotateCcw, QAction *resetRotation, QAction *rotateCw);

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;
	void pushSettings() override;

	QWidget *getHeaderWidget() override { return m_headerWidget; }

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	QWidget *m_headerWidget = nullptr;
	widgets::GroupedToolButton *m_rotateCcwButton = nullptr;
	widgets::GroupedToolButton *m_resetRotationButton = nullptr;
	widgets::GroupedToolButton *m_rotateCwButton = nullptr;
	QButtonGroup *m_rotationModeGroup = nullptr;
};

}

#endif
