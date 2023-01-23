/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2022 Calle Laakkonen

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

#include "desktop/docks/timeline.h"
#include "desktop/docks/titlewidget.h"
#include "libclient/canvas/timelinemodel.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/net/envelopebuilder.h"
#include "rustpile/rustpile.h"

#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>

namespace docks {

Timeline::Timeline(QWidget *parent)
	: QDockWidget(tr("Timeline"), parent)
{
	m_widget = new widgets::TimelineWidget(this);
	connect(m_widget, &widgets::TimelineWidget::timelineEditCommand, this, &Timeline::timelineEditCommand);
	connect(m_widget, &widgets::TimelineWidget::selectFrameRequest, this, &Timeline::setCurrentFrame);
	m_widget->setMinimumHeight(40);
	setWidget(m_widget);

	// Create the title bar widget
	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	m_useTimeline = new QCheckBox(tr("Use manual timeline"));
	connect(m_useTimeline, &QCheckBox::clicked, this, &Timeline::onUseTimelineClicked);

	titlebar->addCustomWidget(m_useTimeline);
	titlebar->addStretch();

	titlebar->addCustomWidget(new QLabel(tr("Frame:")));
	m_currentFrame = new QSpinBox;
	m_currentFrame->setWrapping(true);
	m_currentFrame->setMinimum(1);
	connect(m_currentFrame, QOverload<int>::of(&QSpinBox::valueChanged), this, &Timeline::currentFrameChanged);
	connect(m_currentFrame, QOverload<int>::of(&QSpinBox::valueChanged), m_widget, &widgets::TimelineWidget::setCurrentFrame);
	titlebar->addCustomWidget(m_currentFrame);

	titlebar->addSpace(12);
	titlebar->addCustomWidget(new QLabel(tr("FPS:")));

	m_fps = new QSpinBox;
	m_fps->setMinimum(1);
	m_fps->setMaximum(99);
	connect(m_fps, QOverload<int>::of(&QSpinBox::valueChanged), this, &Timeline::onFpsChanged);

	titlebar->addCustomWidget(m_fps);
}

void Timeline::setTimeline(canvas::TimelineModel *model)
{
	m_widget->setModel(model);
	connect(model, &canvas::TimelineModel::framesChanged, this, &Timeline::onFramesChanged, Qt::QueuedConnection);
}

void Timeline::setFeatureAccess(bool access)
{
	m_useTimeline->setEnabled(access);
	m_widget->setEditable(access);
}

void Timeline::setUseTimeline(bool useTimeline)
{
	m_useTimeline->setChecked(useTimeline);
	m_widget->model()->setManualMode(useTimeline);
}

void Timeline::setFps(int fps)
{
	m_fps->blockSignals(true);
	m_fps->setValue(fps);
	m_fps->blockSignals(false);
}

void Timeline::setCurrentFrame(int frame, int layerId)
{
	m_currentFrame->setValue(frame);

	if(layerId > 0)
		emit layerSelectRequested(layerId);
}

void Timeline::setCurrentLayer(int layerId)
{
	m_widget->setCurrentLayer(layerId);
}

void Timeline::setNextFrame()
{
	m_currentFrame->setValue(m_currentFrame->value() + 1);
	autoSelectLayer();
}

void Timeline::setPreviousFrame()
{
	m_currentFrame->setValue(m_currentFrame->value() - 1);
	autoSelectLayer();
}

void Timeline::autoSelectLayer()
{
	const auto layerId = m_widget->model()->nearestLayerTo(
		m_widget->currentFrame(),
		m_widget->currentLayerId()
	);

	if(layerId > 0)
		emit layerSelectRequested(layerId);
}

int Timeline::currentFrame() const
{
	return m_currentFrame->value();
}

void Timeline::onFramesChanged()
{
	m_currentFrame->setMaximum(qMax(1, m_widget->model()->frames().size()));
	emit currentFrameChanged(m_currentFrame->value());
}

void Timeline::onUseTimelineClicked()
{
	net::EnvelopeBuilder eb;
	rustpile::write_setmetadataint(
		eb,
		0,
		uint8_t(rustpile::MetadataInt::UseTimeline),
		m_useTimeline->isChecked()
	);
	emit timelineEditCommand(eb.toEnvelope());
}

void Timeline::onFpsChanged()
{
	// TODO debounce
	net::EnvelopeBuilder eb;
	rustpile::write_setmetadataint(
		eb,
		0,
		uint8_t(rustpile::MetadataInt::Framerate),
		m_fps->value()
	);
	emit timelineEditCommand(eb.toEnvelope());
}

}
