// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/zoomsettings.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/zoom.h"
#include <QButtonGroup>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>

namespace tools {

namespace props {
static const ToolProperties::RangedValue<int> zoomMode{
	QStringLiteral("zoommode"), 1, 0, 1};
}

ZoomSettings::ZoomSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

void ZoomSettings::setActions(
	QAction *zoomIn, QAction *resetZoom, QAction *zoomToFit, QAction *zoomOut)
{
	m_zoomInButton->setDefaultAction(zoomIn);
	m_resetZoomButton->setDefaultAction(resetZoom);
	m_zoomToFitButton->setDefaultAction(zoomToFit);
	m_zoomOutButton->setDefaultAction(zoomOut);
}

ToolProperties ZoomSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::zoomMode, m_zoomModeGroup->checkedId());
	return cfg;
}

void ZoomSettings::restoreToolSettings(const ToolProperties &cfg)
{
	QAbstractButton *button =
		m_zoomModeGroup->button(cfg.value(props::zoomMode));
	if(button) {
		button->setChecked(true);
	}
}

void ZoomSettings::pushSettings()
{
	ToolController *ctrl = controller();
	ZoomTool *tool = static_cast<ZoomTool *>(ctrl->getTool(Tool::ZOOM));
	tool->setScrub(m_zoomModeGroup->checkedId() != 0);
}

QWidget *ZoomSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);
	headerLayout->addStretch(1);

	m_zoomInButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_zoomInButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	headerLayout->addWidget(m_zoomInButton, 3);

	m_resetZoomButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	m_resetZoomButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	headerLayout->addWidget(m_resetZoomButton, 3);

	m_zoomToFitButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	m_zoomToFitButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	headerLayout->addWidget(m_zoomToFitButton, 3);

	m_zoomOutButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_zoomOutButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	headerLayout->addWidget(m_zoomOutButton, 3);

	headerLayout->addStretch(1);

	QWidget *widget = new QWidget(parent);
	QFormLayout *form = new QFormLayout(widget);
	form->setContentsMargins(3, 3, 3, 3);
	form->setSpacing(3);

	QHBoxLayout *modeLayout = new QHBoxLayout;
	modeLayout->setContentsMargins(0, 0, 0, 0);
	modeLayout->setSpacing(0);

	m_modeDiscreteButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_modeDiscreteButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_modeDiscreteButton->setText(tr("Discrete"));
	m_modeDiscreteButton->setToolTip(tr("Zoom in steps and by drawing boxes."));
	m_modeDiscreteButton->setStatusTip(m_modeDiscreteButton->toolTip());
	m_modeDiscreteButton->setCheckable(true);
	m_modeDiscreteButton->setChecked(true);
	modeLayout->addWidget(m_modeDiscreteButton);

	m_modeScrubButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_modeScrubButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	//: Refers to zooming in and out by dragging up and down.
	m_modeScrubButton->setText(tr("Scrub"));
	m_modeScrubButton->setToolTip(tr("Zoom by dragging up and down."));
	m_modeScrubButton->setStatusTip(m_modeScrubButton->toolTip());
	m_modeScrubButton->setCheckable(true);
	modeLayout->addWidget(m_modeScrubButton);

	form->addRow(tr("Mode:"), modeLayout);

	m_zoomModeGroup = new QButtonGroup(this);
	m_zoomModeGroup->addButton(m_modeDiscreteButton, 0);
	m_zoomModeGroup->addButton(m_modeScrubButton, 1);
	connect(
		m_zoomModeGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&ZoomSettings::pushSettings);

	return widget;
}

}
