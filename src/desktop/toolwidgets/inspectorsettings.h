// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLSETTINGS_INSPECTOR_H
#define TOOLSETTINGS_INSPECTOR_H

#include "desktop/toolwidgets/toolsettings.h"

class Ui_InspectorSettings;

namespace canvas {
class UserListModel;
}

namespace tools {

/**
 * @brief Canvas inspector (a moderation tool)
 */
class InspectorSettings final : public ToolSettings {
	Q_OBJECT
public:
	InspectorSettings(ToolController *ctrl, QObject *parent = nullptr);
	~InspectorSettings() override;

	QString toolType() const override { return QStringLiteral("inspector"); }

	void setUserList(canvas::UserListModel *userlist) { m_userlist = userlist; }

	bool isShowTiles() const;

	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

public slots:
	void onCanvasInspected(int lastEditedBy);
	void pushSettings() override;

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	Ui_InspectorSettings *m_ui;
	canvas::UserListModel *m_userlist;
};

}

#endif
