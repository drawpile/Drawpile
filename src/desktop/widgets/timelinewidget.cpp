// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/timelinewidget.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/timelinemodel.h"
#include <QHeaderView>

namespace widgets {

struct TimelineWidget::Private {
	canvas::CanvasModel *canvas = nullptr;
	bool editable = true;

	int frameCount() const
	{
		return canvas ? canvas->metadata()->frameCount() : 0;
	}
};

TimelineWidget::TimelineWidget(QWidget *parent)
	: QTableView(parent)
	, d(new Private)
{
	setCornerButtonEnabled(false);
	setSelectionBehavior(QAbstractItemView::SelectItems);
	setSelectionMode(QAbstractItemView::ExtendedSelection);

	setAcceptDrops(true);
	setDragEnabled(true);
	setDropIndicatorShown(true);
	setDefaultDropAction(Qt::MoveAction);
	setDragDropMode(QAbstractItemView::DragDrop);

	setHorizontalHeader(new QHeaderView(Qt::Horizontal, this));
	setVerticalHeader(new QHeaderView(Qt::Vertical, this));

	setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
	setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	utils::bindKineticScrolling(this);
}

TimelineWidget::~TimelineWidget()
{
	delete d;
}

canvas::CanvasModel *TimelineWidget::canvas() const
{
	return d->canvas;
}

void TimelineWidget::setCanvas(canvas::CanvasModel *canvas)
{
	d->canvas = canvas;
	setModel(d->canvas->timeline());
}

void TimelineWidget::setActions(const Actions &actions)
{
	connect(
		actions.trackAdd, &QAction::triggered, this, &TimelineWidget::addTrack);
}

void TimelineWidget::setCurrentFrameIndex(int frame) {}

void TimelineWidget::setCurrentTrackId(int trackId) {}

void TimelineWidget::setCurrentTrackIndex(int trackIndex) {}

void TimelineWidget::setCurrentLayer(int layerId) {}

void TimelineWidget::updateControlsEnabled(bool access, bool locked)
{
	d->editable = access && !locked;
	setEnabled(d->editable);
}

void TimelineWidget::updateKeyFrameColorMenuIcon() {}

int TimelineWidget::frameCount() const
{
	return d->frameCount();
}

int TimelineWidget::currentTrackId() const
{
	return 0;
}

int TimelineWidget::currentFrame() const
{
	return -1;
}

void TimelineWidget::changeFramerate(int framerate) {}

void TimelineWidget::changeFrameCount(int frameCount) {}

void TimelineWidget::addTrack()
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineModel *timeline = d->canvas->timeline();
	int trackId = timeline->getAvailableTrackId();
	if(trackId == 0) {
		qWarning("Couldn't find a free ID for a new track");
		return;
	}

	// d->nextTrackId = trackId;
	const canvas::TimelineTrack *track = nullptr; // d->currentTrack();
	emitCommand([&](uint8_t contextId) {
		return net::makeTrackCreateMessage(
			contextId, trackId, track ? track->id : 0, 0,
			timeline->getAvailableTrackName(tr("Track")));
	});
}

void TimelineWidget::emitCommand(
	std::function<net::Message(uint8_t)> getMessage)
{
	if(d->editable) {
		uint8_t contextId = d->canvas->localUserId();
		net::Message messages[] = {
			net::makeUndoPointMessage(contextId),
			getMessage(contextId),
		};
		emit timelineEditCommands(DP_ARRAY_LENGTH(messages), messages);
	}
}

}
