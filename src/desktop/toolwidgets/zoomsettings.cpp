// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/zoomsettings.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/zoom.h"
#include <QButtonGroup>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QWidget>

namespace tools {

namespace props {
static const ToolProperties::RangedValue<int> zoomMode{
	QStringLiteral("zoommode"), 1, 0, 1},
	scrubDirection{QStringLiteral("scrubDirection"), 0, 0, 1};
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
	cfg.setValue(props::scrubDirection, m_scrubDirectionGroup->checkedId());
	return cfg;
}

void ZoomSettings::restoreToolSettings(const ToolProperties &cfg)
{
	QAbstractButton *modeButton =
		m_zoomModeGroup->button(cfg.value(props::zoomMode));
	if(modeButton) {
		modeButton->setChecked(true);
	}

	QAbstractButton *directionButton =
		m_scrubDirectionGroup->button(cfg.value(props::scrubDirection));
	if(directionButton) {
		directionButton->setChecked(true);
	}
}

void ZoomSettings::pushSettings()
{
	ToolController *ctrl = controller();
	ZoomTool *tool = static_cast<ZoomTool *>(ctrl->getTool(Tool::ZOOM));
	tool->setScrub(m_zoomModeGroup->checkedId() != 0);
	tool->setScrubVertical(m_scrubDirectionGroup->checkedId() != 0);
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

	m_modeScrubButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_modeScrubButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	//: Zoom tool mode, refers to clicking and dragging to scrub zoom.
	m_modeScrubButton->setText(tr("Drag"));
	m_modeScrubButton->setToolTip(tr("Zoom by clicking and dragging."));
	m_modeScrubButton->setStatusTip(m_modeScrubButton->toolTip());
	m_modeScrubButton->setCheckable(true);
	modeLayout->addWidget(m_modeScrubButton);

	m_modeSelectButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_modeSelectButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	//: Zoom tool mode, refers to drawing a rectangle to zoom into.
	m_modeSelectButton->setText(tr("Select"));
	m_modeSelectButton->setToolTip(tr("Zoom by selecting an area."));
	m_modeSelectButton->setStatusTip(m_modeSelectButton->toolTip());
	m_modeSelectButton->setCheckable(true);
	m_modeSelectButton->setChecked(true);
	modeLayout->addWidget(m_modeSelectButton);

	form->addRow(tr("Mode:"), modeLayout);

	m_zoomModeGroup = new QButtonGroup(this);
	m_zoomModeGroup->addButton(m_modeScrubButton, 1);
	m_zoomModeGroup->addButton(m_modeSelectButton, 0);
	connect(
		m_zoomModeGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&ZoomSettings::pushSettings);

	QHBoxLayout *directionLayout = new QHBoxLayout;
	directionLayout->setContentsMargins(0, 0, 0, 0);
	directionLayout->setSpacing(0);

	m_scrubHorizontalButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_scrubHorizontalButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_scrubHorizontalButton->setText(tr("Horizontal"));
	m_scrubHorizontalButton->setToolTip(tr("Drag left and right to zoom."));
	m_scrubHorizontalButton->setStatusTip(m_scrubHorizontalButton->toolTip());
	m_scrubHorizontalButton->setCheckable(true);
	m_scrubHorizontalButton->setChecked(true);
	directionLayout->addWidget(m_scrubHorizontalButton);

	m_scrubVerticalButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_scrubVerticalButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	//: Zoom tool mode, refers to drawing a rectangle to zoom into.
	m_scrubVerticalButton->setText(tr("Vertical"));
	m_scrubVerticalButton->setToolTip(tr("Drag up and down to zoom."));
	m_scrubVerticalButton->setStatusTip(m_scrubVerticalButton->toolTip());
	m_scrubVerticalButton->setCheckable(true);
	directionLayout->addWidget(m_scrubVerticalButton);

	form->addRow(tr("Direction:"), directionLayout);

	m_scrubDirectionGroup = new QButtonGroup(this);
	m_scrubDirectionGroup->addButton(m_scrubHorizontalButton, 0);
	m_scrubDirectionGroup->addButton(m_scrubVerticalButton, 1);
	connect(
		m_scrubDirectionGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&ZoomSettings::pushSettings);

	return widget;
}

}
