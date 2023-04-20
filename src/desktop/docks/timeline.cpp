// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/timeline.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/drawdance/message.h"

#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>

namespace docks {

Timeline::Timeline(QWidget *parent)
	: QDockWidget{tr("Timeline"), parent}
	, m_featureAccessEnabled{true}
{
	m_widget = new widgets::TimelineWidget(this);
	m_widget->setMinimumHeight(40);
	setWidget(m_widget);
	connect(
		m_widget, &widgets::TimelineWidget::timelineEditCommands, this,
		&Timeline::timelineEditCommands);
	connect(
		m_widget, &widgets::TimelineWidget::frameSelected, this,
		&Timeline::frameSelected);
	connect(
		m_widget, &widgets::TimelineWidget::trackHidden, this,
		&Timeline::trackHidden);
	connect(
		m_widget, &widgets::TimelineWidget::trackOnionSkinEnabled, this,
		&Timeline::trackOnionSkinEnabled);

	TitleWidget *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);
	titlebar->addStretch();
}

void Timeline::setTimeline(canvas::TimelineModel *model)
{
	m_widget->setModel(model);
	connect(
		model->canvas(), &canvas::CanvasModel::compatibilityModeChanged, this,
		&Timeline::setCompatibilityMode);
	setCompatibilityMode(model->canvas()->isCompatibilityMode());
}

void Timeline::setActions(const widgets::TimelineWidget::Actions &actions)
{
	m_widget->setActions(actions);
}

void Timeline::setFrameCount(int frameCount)
{
	m_widget->model()->setFrameCount(frameCount);
}

void Timeline::setCurrentLayer(int layerId)
{
	m_widget->setCurrentLayer(layerId);
}

int Timeline::currentFrame() const
{
	return m_widget->currentFrame();
}

void Timeline::setFeatureAccess(bool access)
{
	m_featureAccessEnabled = access;
	updateControlsEnabled(access, isCompatibilityMode());
}

void Timeline::setCompatibilityMode(bool compatibilityMode)
{
	updateControlsEnabled(m_featureAccessEnabled, compatibilityMode);
}

bool Timeline::isCompatibilityMode() const
{
	canvas::TimelineModel *model = m_widget->model();
	return model && model->canvas()->isCompatibilityMode();
}

void Timeline::updateControlsEnabled(bool access, bool compatibilityMode)
{
	m_widget->updateControlsEnabled(access, compatibilityMode);
}

}
