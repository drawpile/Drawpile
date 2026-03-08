// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/timeline.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/canvas/acl.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/net/message.h"
#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QWidgetAction>

namespace docks {

Timeline::Timeline(QWidget *parent)
	: DockBase(tr("Timeline"), QString(), QIcon::fromTheme("keyframe"), parent)
	, m_widget(new widgets::TimelineWidget(this))
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
		&Timeline::frameSelected);
	connect(
		m_widget, &widgets::TimelineWidget::layerSelected, this,
		&Timeline::layerSelected);
	connect(
		m_widget, &widgets::TimelineWidget::blankLayerSelected, this,
		&Timeline::blankLayerSelected);
	connect(
		m_widget, &widgets::TimelineWidget::trackHidden, this,
		&Timeline::trackHidden);
	connect(
		m_widget, &widgets::TimelineWidget::trackOnionSkinEnabled, this,
		&Timeline::trackOnionSkinEnabled);
	connect(
		m_widget, &widgets::TimelineWidget::frameViewModeRequested, this,
		&Timeline::frameViewModeRequested);

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
		&Timeline::updateFramerateText);
	updateFramerateText(metadata->framerate());
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

void Timeline::setCurrentLayer(int layerId)
{
	m_widget->setCurrentLayer(layerId);
}

void Timeline::setFeatureAccess(bool access)
{
	m_featureAccessEnabled = access;
	updateControlsEnabled(access, m_locked);
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

	QPushButton *playButton = new QPushButton(showFlipbook->icon(), tr("Play"));
	titlebar->addCustomWidget(playButton);
	connect(playButton, &QPushButton::clicked, showFlipbook, &QAction::trigger);

	titlebar->addStretch();

	addTitleButton(
		titlebar, actions.timelineToolNormal, GroupedToolButton::GroupLeft);
	addTitleButton(
		titlebar, actions.timelineToolExposure, GroupedToolButton::GroupRight);

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
		titlebar, actions.keyFrameDeleteLayer, GroupedToolButton::GroupRight);

	titlebar->addStretch();

	addTitleButton(titlebar, layerViewNormal, GroupedToolButton::GroupLeft);
	addTitleButton(
		titlebar, layerViewCurrentFrame, GroupedToolButton::GroupCenter);

	widgets::GroupedToolButton *zoomButton =
		new widgets::GroupedToolButton(GroupedToolButton::GroupRight);
	zoomButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
	zoomButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	zoomButton->setPopupMode(QToolButton::InstantPopup);
	zoomButton->setToolTip(tr("Zoom"));
	titlebar->addCustomWidget(zoomButton);

	QMenu *zoomMenu = new QMenu(zoomButton);
	zoomButton->setMenu(zoomMenu);
	zoomMenu->addAction(actions.timelineZoomReset);

	zoomMenu->addSection(tr("Column width:"));
	m_zoomSlider = new KisSliderSpinBox;
	m_zoomSlider->setRange(
		widgets::TimelineWidget::MIN_COLUMN_WIDTH,
		widgets::TimelineWidget::MAX_COLUMN_WIDTH);
	connect(
		m_widget, &widgets::TimelineWidget::columnWidthChanged, m_zoomSlider,
		&KisSliderSpinBox::setValue);
	connect(
		m_zoomSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		m_widget, &widgets::TimelineWidget::setColumnWidth);
	m_zoomSlider->setValue(m_widget->columnWidth());

	QWidgetAction *zoomAction = new QWidgetAction(zoomMenu);
	zoomAction->setDefaultWidget(m_zoomSlider);
	zoomMenu->addAction(zoomAction);

	titlebar->addStretch();

	m_propertiesButton = new QPushButton;
	m_propertiesButton->setIcon(actions.animationProperties->icon());
	m_propertiesButton->setStatusTip(actions.animationProperties->text());
	m_propertiesButton->setToolTip(actions.animationProperties->text());
	titlebar->addCustomWidget(m_propertiesButton);
	connect(
		m_propertiesButton, &QPushButton::clicked, actions.animationProperties,
		&QAction::trigger);

	titlebar->addSpace(4);

	canvas::CanvasModel *canvas = m_widget->canvas();
	if(canvas) {
		updateFramerateText(canvas->metadata()->framerate());
	}
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

void Timeline::updateFramerateText(double framerate)
{
	if(m_propertiesButton) {
		QLocale locale;
		QString text = locale.toString(framerate, 'f', 2);
		if(text.endsWith(QStringLiteral("00"))) {
			text = locale.toString(qRound(framerate));
		} else if(text.endsWith(QStringLiteral("0"))) {
			text.chop(1);
		}
		m_propertiesButton->setText(tr("%1 FPS").arg(text));
	}
}

}
