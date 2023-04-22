// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/timeline.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/drawdance/message.h"

#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <functional>

namespace docks {

Timeline::Timeline(QWidget *parent)
	: QDockWidget{tr("Timeline"), parent}
	, m_widget{new widgets::TimelineWidget{this}}
	, m_frameSpinner{nullptr}
	, m_framerateSpinner{nullptr}
	, m_frameCountSpinner{nullptr}
	, m_framerateDebounce{DEBOUNCE_DELAY_MS}
	, m_frameCountDebounce{DEBOUNCE_DELAY_MS}
	, m_featureAccessEnabled{true}
{
	m_widget->setMinimumHeight(40);
	setWidget(m_widget);
	connect(
		m_widget, &widgets::TimelineWidget::timelineEditCommands, this,
		&Timeline::timelineEditCommands);
	connect(
		m_widget, &widgets::TimelineWidget::frameSelected, this,
		&Timeline::setCurrentFrame);
	connect(
		m_widget, &widgets::TimelineWidget::layerSelected, this,
		&Timeline::layerSelected);
	connect(
		m_widget, &widgets::TimelineWidget::trackHidden, this,
		&Timeline::trackHidden);
	connect(
		m_widget, &widgets::TimelineWidget::trackOnionSkinEnabled, this,
		&Timeline::trackOnionSkinEnabled);

	TitleWidget *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);
}

void Timeline::setCanvas(canvas::CanvasModel *canvas)
{
	m_widget->setCanvas(canvas);

	connect(
		canvas, &canvas::CanvasModel::compatibilityModeChanged, this,
		&Timeline::setCompatibilityMode);

	const canvas::DocumentMetadata *metadata = canvas->metadata();
	connect(
		metadata, &canvas::DocumentMetadata::framerateChanged, this,
		&Timeline::setFramerate);
	connect(
		metadata, &canvas::DocumentMetadata::frameCountChanged, this,
		&Timeline::setFrameCount);

	setCompatibilityMode(canvas->isCompatibilityMode());
	setFramerate(metadata->framerate());
	setFrameCount(metadata->frameCount());
}

void Timeline::setActions(
	const widgets::TimelineWidget::Actions &actions, QAction *layerViewNormal,
	QAction *layerViewCurrentFrame)
{
	m_widget->setActions(actions);
	setUpTitleWidget(actions, layerViewNormal, layerViewCurrentFrame);
}

int Timeline::currentFrame() const
{
	return m_widget->currentFrame();
}

void Timeline::setFramerate(int framerate)
{
	if(m_framerateSpinner) {
		QSignalBlocker blocker{m_framerateSpinner};
		m_framerateSpinner->setValue(framerate);
	}
}

void Timeline::setFrameCount(int frameCount)
{
	if(m_frameSpinner) {
		QSignalBlocker blocker{m_frameSpinner};
		m_frameSpinner->setRange(1, frameCount);
		updateFrame(m_widget->currentFrame());
	}
	if(m_frameCountSpinner) {
		QSignalBlocker blocker{m_frameCountSpinner};
		m_frameCountSpinner->setValue(frameCount);
	}
}

void Timeline::setCurrentLayer(int layerId)
{
	m_widget->setCurrentLayer(layerId);
}

void Timeline::setFeatureAccess(bool access)
{
	m_featureAccessEnabled = access;
	updateControlsEnabled(access, isCompatibilityMode());
}

void Timeline::setCurrentFrame(int frame)
{
	emit frameSelected(frame);
	updateFrame(frame);
}

void Timeline::setCompatibilityMode(bool compatibilityMode)
{
	updateControlsEnabled(m_featureAccessEnabled, compatibilityMode);
}

void Timeline::setUpTitleWidget(
	const widgets::TimelineWidget::Actions &actions, QAction *layerViewNormal,
	QAction *layerViewCurrentFrame)
{
	using widgets::GroupedToolButton;
	docks::TitleWidget *titlebar =
		qobject_cast<docks::TitleWidget *>(titleBarWidget());

	m_frameSpinner = new QSpinBox{titlebar};
	m_frameSpinner->setWrapping(true);
	titlebar->addCustomWidget(m_frameSpinner);
	connect(
		m_frameSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		[this](int value) {
			m_widget->setCurrentFrame(value - 1);
		});

	titlebar->addCustomWidget(new QLabel{" / ", titlebar});

	m_frameCountSpinner = new QSpinBox{titlebar};
	m_frameCountSpinner->setRange(1, 9999);
	titlebar->addCustomWidget(m_frameCountSpinner);
	connect(
		m_frameCountSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		&m_frameCountDebounce, &DebounceTimer::setInt);
	connect(
		&m_frameCountDebounce, &DebounceTimer::intChanged, m_widget,
		&widgets::TimelineWidget::changeFrameCount);

	titlebar->addStretch();

	addTitleButton(titlebar, layerViewNormal, GroupedToolButton::GroupLeft);
	addTitleButton(
		titlebar, layerViewCurrentFrame, GroupedToolButton::GroupRight);

	titlebar->addStretch();

	addTitleButton(titlebar, actions.trackAdd, GroupedToolButton::GroupLeft);
	addTitleButton(
		titlebar, actions.trackDuplicate, GroupedToolButton::GroupCenter);
	addTitleButton(
		titlebar, actions.trackRetitle, GroupedToolButton::GroupCenter);
	addTitleButton(
		titlebar, actions.trackDelete, GroupedToolButton::GroupRight);

	titlebar->addStretch();

	addTitleButton(
		titlebar, actions.keyFrameSetLayer, GroupedToolButton::GroupLeft);
	addTitleButton(
		titlebar, actions.keyFrameSetEmpty, GroupedToolButton::GroupCenter);
	addTitleButton(
		titlebar, actions.keyFrameProperties, GroupedToolButton::GroupCenter);
	addTitleButton(
		titlebar, actions.keyFrameDelete, GroupedToolButton::GroupRight);

	titlebar->addStretch();

	m_framerateSpinner = new QSpinBox{titlebar};
	m_framerateSpinner->setRange(1, 999);
	m_framerateSpinner->setSuffix(tr(" FPS"));
	titlebar->addCustomWidget(m_framerateSpinner);
	connect(
		m_framerateSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		&m_framerateDebounce, &DebounceTimer::setInt);
	connect(
		&m_framerateDebounce, &DebounceTimer::intChanged, m_widget,
		&widgets::TimelineWidget::changeFramerate);

	titlebar->addSpace(4);
}

void Timeline::addTitleButton(
	docks::TitleWidget *titlebar, QAction *action,
	widgets::GroupedToolButton::GroupPosition position)
{
	widgets::GroupedToolButton *button =
		new widgets::GroupedToolButton{position, titlebar};
	button->setDefaultAction(action);
	titlebar->addCustomWidget(button);
}

bool Timeline::isCompatibilityMode() const
{
	canvas::CanvasModel *canvas = m_widget->canvas();
	return canvas && canvas->isCompatibilityMode();
}

void Timeline::updateControlsEnabled(bool access, bool compatibilityMode)
{
	m_widget->updateControlsEnabled(access, compatibilityMode);
}

void Timeline::updateFrame(int frame)
{
	if(m_frameSpinner) {
		QSignalBlocker blocker{m_frameSpinner};
		m_frameSpinner->setValue(frame + 1);
	}
}

}
