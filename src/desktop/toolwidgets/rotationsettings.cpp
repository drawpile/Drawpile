// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/rotationsettings.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/tools/rotation.h"
#include "libclient/tools/toolcontroller.h"
#include <QButtonGroup>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>

namespace tools {

namespace props {
static const ToolProperties::RangedValue<int> rotationMode{
	QStringLiteral("rotationmode"), 0, 0, 2};
}

RotationSettings::RotationSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

void RotationSettings::setActions(
	QAction *rotateCcw, QAction *resetRotation, QAction *rotateCw)
{
	m_rotateCcwButton->setDefaultAction(rotateCcw);
	m_resetRotationButton->setDefaultAction(resetRotation);
	m_rotateCwButton->setDefaultAction(rotateCw);
}

ToolProperties RotationSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::rotationMode, m_rotationModeGroup->checkedId());
	return cfg;
}

void RotationSettings::restoreToolSettings(const ToolProperties &cfg)
{
	QAbstractButton *button =
		m_rotationModeGroup->button(cfg.value(props::rotationMode));
	if(button) {
		button->setChecked(true);
	}
}

QWidget *RotationSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);
	headerLayout->addStretch(1);

	m_rotateCcwButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_rotateCcwButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	headerLayout->addWidget(m_rotateCcwButton, 3);

	m_resetRotationButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	m_resetRotationButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	headerLayout->addWidget(m_resetRotationButton, 3);

	m_rotateCwButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_rotateCwButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	headerLayout->addWidget(m_rotateCwButton, 3);

	headerLayout->addStretch(1);

	QWidget *widget = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(widget);

	m_rotationModeGroup = new QButtonGroup(this);

	QRadioButton *normalRadio = new QRadioButton(tr("Normal rotation"));
	QRadioButton *noSnapRadio = new QRadioButton(tr("Free rotation"));
	QRadioButton *discreteRadio = new QRadioButton(tr("Ratchet rotation"));
	normalRadio->setToolTip(tr("Rotates smoothly, snaps to 0°"));
	noSnapRadio->setToolTip(tr("Rotates smoothly, never snaps"));
	discreteRadio->setToolTip(tr("Rotates in 15° increments"));
	normalRadio->setStatusTip(normalRadio->toolTip());
	noSnapRadio->setStatusTip(noSnapRadio->toolTip());
	discreteRadio->setStatusTip(discreteRadio->toolTip());
	layout->addWidget(normalRadio);
	layout->addWidget(noSnapRadio);
	layout->addWidget(discreteRadio);
	m_rotationModeGroup->addButton(normalRadio, int(RotationMode::Normal));
	m_rotationModeGroup->addButton(noSnapRadio, int(RotationMode::NoSnap));
	m_rotationModeGroup->addButton(discreteRadio, int(RotationMode::Discrete));
	normalRadio->setChecked(true);

	layout->addStretch();

	connect(
		m_rotationModeGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&RotationSettings::pushSettings);

	return widget;
}

void RotationSettings::pushSettings()
{
	ToolController *ctrl = controller();
	RotationTool *tool =
		static_cast<RotationTool *>(ctrl->getTool(Tool::ROTATION));
	tool->setRotationMode(RotationMode(m_rotationModeGroup->checkedId()));
}

}
