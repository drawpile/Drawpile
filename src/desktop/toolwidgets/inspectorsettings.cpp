// SPDX-License-Identifier: GPL-3.0-or-later

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

