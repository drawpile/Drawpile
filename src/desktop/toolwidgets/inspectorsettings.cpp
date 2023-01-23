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

#include "desktop/toolwidgets/inspectorsettings.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/canvas/userlist.h"

#include "ui_inspectorsettings.h"

namespace tools {

InspectorSettings::InspectorSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), m_ui(nullptr), m_userlist(nullptr)
{
}

InspectorSettings::~InspectorSettings()
{
	delete m_ui;
}

QWidget *InspectorSettings::createUiWidget(QWidget *parent)
{
	auto widget = new QWidget(parent);
	m_ui = new Ui_InspectorSettings;
	m_ui->setupUi(widget);

	return widget;
}

void InspectorSettings::onCanvasInspected(int lastEditedBy)
{

	if(m_userlist) {
		const canvas::User u = m_userlist->getUserById(lastEditedBy);

		m_ui->lblAvatar->setPixmap(u.avatar);
		m_ui->lblUsername->setText(u.name);

	} else {
		m_ui->lblUsername->setText(QString("User %1").arg(lastEditedBy));
	}
}

}

