/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
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
	InspectorSettings(ToolController *ctrl, QObject *parent=nullptr);
	~InspectorSettings() override;

	QString toolType() const override { return QStringLiteral("inspector"); }

	void setForeground(const QColor &color) override { Q_UNUSED(color); }

	int getSize() const override { return 0; }
	bool getSubpixelMode() const override { return false; }

	void setUserList(canvas::UserListModel *userlist) { m_userlist = userlist; }

public slots:
	void onCanvasInspected(int lastEditedBy);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private:
	Ui_InspectorSettings *m_ui;
	canvas::UserListModel *m_userlist;
};

}

#endif

