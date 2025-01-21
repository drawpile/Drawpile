// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/inspectorsettings.h"
#include "libclient/canvas/userlist.h"
#include "libclient/tools/inspector.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "ui_inspectorsettings.h"

namespace tools {

namespace props {
static const ToolProperties::Value<bool> tiles{QStringLiteral("tiles"), false};
}

InspectorSettings::InspectorSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
	, m_ui(nullptr)
	, m_userlist(nullptr)
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
	connect(
		m_ui->showTilesBox, &QCheckBox::clicked, this,
		&InspectorSettings::pushSettings);
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

bool InspectorSettings::isShowTiles() const
{
	return m_ui->showTilesBox->isChecked();
}

ToolProperties InspectorSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::tiles, m_ui->showTilesBox->isChecked());
	return cfg;
}

void InspectorSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_ui->showTilesBox->setChecked(cfg.value(props::tiles));
	pushSettings();
}

void InspectorSettings::pushSettings()
{
	Inspector *tool =
		static_cast<Inspector *>(controller()->getTool(Tool::INSPECTOR));
	tool->setShowTiles(m_ui->showTilesBox->isChecked());
}

}
