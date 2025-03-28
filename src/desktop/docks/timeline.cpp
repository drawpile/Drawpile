// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/timeline.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/main.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/canvas/acl.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/net/message.h"
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <functional>

namespace docks {

Timeline::Timeline(QWidget *parent)
	: DockBase(tr("Timeline"), QString(), QIcon::fromTheme("keyframe"), parent)
	, m_widget{new widgets::TimelineWidget{this}}
	, m_frameSpinner{nullptr}
	, m_framerateSpinner{nullptr}
	, m_framerateDebounce{dpApp().settings().debounceDelayMs()}
	, m_featureAccessEnabled{true}
	, m_locked{false}
{
	m_widget->setMinimumHeight(40);
	setWidget(m_widget);
	connect(
		m_widget, &widgets::TimelineWidget::timelineEditCommands, this,
		&Timeline::timelineEditCommands);
	connect(
		m_widget, &widgets::TimelineWidget::trackSelected, this,
		&Timeline::trackSelected);
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
		canvas->aclState(), &canvas::AclState::resetLockChanged, this,
		&Timeline::setDisabled);
	connect(
		canvas->aclState(), &canvas::AclState::localLockChanged, this,
		&Timeline::setLocked);

	const canvas::DocumentMetadata *metadata = canvas->metadata();
	connect(
		metadata, &canvas::DocumentMetadata::framerateChanged, this,
		&Timeline::setFramerate);
	connect(
		metadata, &canvas::DocumentMetadata::frameCountChanged, this,
		&Timeline::setFrameCount);

	setFramerate(metadata->framerate());
	setFrameCount(metadata->frameCount());
}

void Timeline::setActions(
	const widgets::TimelineWidget::Actions &actions, QAction *layerViewNormal,
	QAction *layerViewCurrentFrame, QAction *showFlipbook)
{
	m_widget->setActions(actions);
	setUpTitleWidget(
		actions, layerViewNormal, layerViewCurrentFrame, showFlipbook);
}

int Timeline::currentTrackId() const
{
	return m_widget->currentTrackId();
}

int Timeline::currentFrame() const
{
	return m_widget->currentFrame();
}

void Timeline::updateKeyFrameColorMenuIcon()
{
	m_widget->updateKeyFrameColorMenuIcon();
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
		m_frameSpinner->setSuffix(QStringLiteral("/%1").arg(frameCount));
		updateFrame(m_widget->currentFrame());
	}
}

void Timeline::setCurrentLayer(int layerId)
{
	m_widget->setCurrentLayer(layerId);
}

void Timeline::setFeatureAccess(bool access)
{
	m_featureAccessEnabled = access;
	updateControlsEnabled(access, m_locked);
}

void Timeline::setCurrentFrame(int frame)
{
	emit frameSelected(frame);
	updateFrame(frame);
}

void Timeline::setLocked(bool locked)
{
	m_locked = locked;
	updateControlsEnabled(m_featureAccessEnabled, locked);
}

void Timeline::setUpTitleWidget(
	const widgets::TimelineWidget::Actions &actions, QAction *layerViewNormal,
	QAction *layerViewCurrentFrame, QAction *showFlipbook)
{
	using widgets::GroupedToolButton;
	docks::TitleWidget *titlebar =
		qobject_cast<docks::TitleWidget *>(actualTitleBarWidget());

	m_frameSpinner = new QSpinBox{titlebar};
	m_frameSpinner->setWrapping(true);
	titlebar->addCustomWidget(m_frameSpinner);
	connect(
		m_frameSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this,
		[this](int value) {
			m_widget->setCurrentFrame(value - 1);
		});

	addTitleButton(
		titlebar, actions.frameCountSet, GroupedToolButton::NotGrouped);

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
		titlebar, actions.keyFrameCreateLayer, GroupedToolButton::GroupLeft);
	addTitleButton(
		titlebar, actions.keyFrameSetLayer, GroupedToolButton::GroupCenter);
	addTitleButton(
		titlebar, actions.keyFrameSetEmpty, GroupedToolButton::GroupCenter);
	addTitleButton(
		titlebar, actions.keyFrameProperties, GroupedToolButton::GroupCenter);
	addTitleButton(
		titlebar, actions.keyFrameDelete, GroupedToolButton::GroupRight);

	titlebar->addStretch();

	addTitleButton(titlebar, showFlipbook, GroupedToolButton::NotGrouped);

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

void Timeline::updateControlsEnabled(bool access, bool locked)
{
	m_widget->updateControlsEnabled(access, locked);
}

void Timeline::updateFrame(int frame)
{
	if(m_frameSpinner) {
		QSignalBlocker blocker{m_frameSpinner};
		m_frameSpinner->setValue(frame + 1);
	}
}

}
