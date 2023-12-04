// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/key_frame.h>
}
#include "desktop/dialogs/keyframepropertiesdialog.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/net/message.h"
#include <QApplication>
#include <QClipboard>
#include <QDrag>
#include <QHash>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <algorithm>

namespace widgets {

enum class TrackAction { None, ToggleVisible, ToggleOnionSkin };

struct TimelineWidget::Target {
	int uiTrackIndex;
	int trackId;
	int frameIndex;
	bool onHeader;
	TrackAction action;
};

struct Exposure {
	int start;
	int length;
	bool hasFrameAtEnd;

	bool isValid() const { return start != -1 && length != -1; }
};

struct TimelineWidget::Private {
	canvas::CanvasModel *canvas = nullptr;

	bool haveActions = false;
	Actions actions;
	QMenu *trackMenu = nullptr;
	QMenu *frameMenu = nullptr;

	QIcon visibleIcon;
	QIcon hiddenIcon;
	QIcon onionSkinOnIcon;
	QIcon onionSkinOffIcon;
	QIcon uselessIcon;
	int headerWidth = 64;
	int rowHeight = 10;
	int columnWidth = 10;
	int xScroll = 0;
	int yScroll = 0;
	int currentTrackId = 0;
	int currentFrame = 0;
	int nextTrackId = 0;
	Target hoverTarget = {-1, 0, -1, false, TrackAction::None};
	bool pressedOnHeader = false;
	bool editable = false;
	Drag drag = Drag::None;
	Drag dragHover = Drag::None;
	Qt::DropAction dropAction;
	QPoint dragOrigin;
	QPoint dragPos;

	int selectedLayerId = 0;
	QHash<QPair<int, int>, int> layerIdByKeyFrame;

	QScrollBar *verticalScroll;
	QScrollBar *horizontalScroll;

	const QVector<canvas::TimelineTrack> &getTracks() const
	{
		if(canvas) {
			return canvas->timeline()->tracks();
		} else {
			static const QVector<canvas::TimelineTrack> emptyTracks;
			return emptyTracks;
		}
	}

	int trackCount() const { return getTracks().size(); }

	int frameCount() const
	{
		return canvas ? canvas->metadata()->frameCount() : 0;
	}

	int framerate() const
	{
		return canvas ? canvas->metadata()->framerate() : 0;
	}

	int trackIdByIndex(int index) const
	{
		const QVector<canvas::TimelineTrack> &tracks = getTracks();
		return index >= 0 && index < tracks.size() ? tracks[index].id : 0;
	}

	int trackIdByUiIndex(int index) const
	{
		const QVector<canvas::TimelineTrack> &tracks = getTracks();
		int count = tracks.size();
		return index >= 0 && index < count ? tracks[count - index - 1].id : 0;
	}

	int trackIndexById(int trackId) const
	{
		const QVector<canvas::TimelineTrack> &tracks = getTracks();
		int trackCount = tracks.size();
		for(int i = 0; i < trackCount; ++i) {
			if(tracks[i].id == trackId) {
				return i;
			}
		}
		return -1;
	}

	const canvas::TimelineTrack *trackById(int trackId) const
	{
		int i = trackIndexById(trackId);
		return i == -1 ? nullptr : &getTracks()[i];
	}

	const canvas::TimelineTrack *currentTrack() const
	{
		return trackById(currentTrackId);
	}

	const canvas::TimelineTrack *hoverTrack() const
	{
		return trackById(hoverTarget.trackId);
	}

	const canvas::TimelineKeyFrame *keyFrameBy(int trackId, int frame) const
	{
		const canvas::TimelineTrack *track = trackById(trackId);
		if(track) {
			const QVector<canvas::TimelineKeyFrame> &keyFrames =
				track->keyFrames;
			int keyFrameCount = keyFrames.size();
			for(int i = 0; i < keyFrameCount; ++i) {
				const canvas::TimelineKeyFrame &keyFrame = keyFrames[i];
				if(keyFrame.frameIndex == frame) {
					return &keyFrame;
				}
			}
		}
		return nullptr;
	}

	const canvas::TimelineKeyFrame *currentKeyFrame() const
	{
		return keyFrameBy(currentTrackId, currentFrame);
	}

	const canvas::TimelineKeyFrame *nextKeyFrame() const
	{
		return keyFrameBy(currentTrackId, currentFrame + 1);
	}

	const canvas::TimelineKeyFrame *previousKeyFrame() const
	{
		return keyFrameBy(currentTrackId, currentFrame - 1);
	}

	const canvas::TimelineKeyFrame *hoverKeyFrame() const
	{
		return keyFrameBy(hoverTarget.trackId, hoverTarget.frameIndex);
	}

	const canvas::TimelineKeyFrame *beforeHoverKeyFrame() const
	{
		const canvas::TimelineKeyFrame *hover = hoverKeyFrame();
		return hover ? keyFrameBy(hoverTarget.trackId, hover->frameIndex - 1)
					 : nullptr;
	}

	const canvas::TimelineKeyFrame *currentVisibleKeyFrame() const
	{
		const canvas::TimelineTrack *track = trackById(currentTrackId);
		if(track) {
			const QVector<canvas::TimelineKeyFrame> &keyFrames =
				track->keyFrames;
			int keyFrameCount = keyFrames.size();
			int bestFrameIndex = -1;
			const canvas::TimelineKeyFrame *bestKeyFrame = nullptr;
			for(int i = 0; i < keyFrameCount; ++i) {
				const canvas::TimelineKeyFrame &keyFrame = keyFrames[i];
				int frameIndex = keyFrame.frameIndex;
				if(frameIndex <= currentFrame && frameIndex > bestFrameIndex) {
					bestKeyFrame = &keyFrame;
					bestFrameIndex = frameIndex;
				}
			}
			return bestKeyFrame;
		}
		return nullptr;
	}

	Exposure currentExposure()
	{
		const canvas::TimelineTrack *track = trackById(currentTrackId);
		if(!track) {
			return {-1, -1, false};
		}

		const QVector<canvas::TimelineKeyFrame> &keyFrames = track->keyFrames;
		int keyFrameCount = keyFrames.size();
		if(keyFrameCount == 0) {
			return {-1, -1, false};
		}

		int start = 0;
		for(int i = 0; i < keyFrameCount; ++i) {
			int frameIndex = keyFrames[i].frameIndex;
			if(frameIndex <= currentFrame && frameIndex > start) {
				start = frameIndex;
			}
		}

		int invalidIndex = frameCount();
		int lastIndex = invalidIndex;
		for(int i = 0; i < keyFrameCount; ++i) {
			int frameIndex = keyFrames[i].frameIndex;
			if(frameIndex > start && frameIndex < lastIndex) {
				lastIndex = frameIndex;
			}
		}
		if(lastIndex == invalidIndex) {
			return {-1, -1, false};
		}

		int length = lastIndex - start;
		bool hasFrameAtEnd =
			keyFrameBy(currentTrackId, invalidIndex - 1) != nullptr;
		return {start, length, hasFrameAtEnd};
	}

	QVector<int> gatherCurrentExposureFrames(int start)
	{
		const canvas::TimelineTrack *track = trackById(currentTrackId);
		QVector<int> frameIndexes;
		if(track) {
			const QVector<canvas::TimelineKeyFrame> &keyFrames =
				track->keyFrames;
			int keyFrameCount = keyFrames.size();
			for(int i = 0; i < keyFrameCount; ++i) {
				int frameIndex = keyFrames[i].frameIndex;
				if(frameIndex > start) {
					frameIndexes.append(frameIndex);
				}
			}
		}
		return frameIndexes;
	}

	QModelIndex layerIndexById(int layerId) const
	{
		if(canvas) {
			canvas::LayerListModel *layers = canvas->layerlist();
			return layers->layerIndex(layerId);
		}
		return QModelIndex{};
	}

	QString layerTitleById(int layerId) const
	{
		if(canvas) {
			QModelIndex layerIndex = layerIndexById(layerId);
			if(layerIndex.isValid()) {
				return layerIndex.data(canvas::LayerListModel::TitleRole)
					.toString();
			}
		}
		return QString{};
	}

	bool isLayerVisibleInCurrentTrack(int layerId)
	{
		return canvas &&
			   canvas->paintEngine()
				   ->viewCanvasState()
				   .getLayersVisibleInTrackFrame(currentTrackId, currentFrame)
				   .contains(layerId);
	}

	void setSelectedLayerId(int layerId)
	{
		selectedLayerId = layerId;
		if(isLayerVisibleInCurrentTrack(layerId)) {
			layerIdByKeyFrame.insert({currentTrackId, currentFrame}, layerId);
		}
	}

	int guessLayerIdToSelect()
	{
		QPair<int, int> key = {currentTrackId, currentFrame};
		if(isLayerVisibleInCurrentTrack(selectedLayerId)) {
			layerIdByKeyFrame.insert(key, selectedLayerId);
			return 0;
		}

		int lastLayerId = layerIdByKeyFrame.value(key, 0);
		if(lastLayerId != 0 && isLayerVisibleInCurrentTrack(lastLayerId)) {
			return lastLayerId;
		}

		const canvas::TimelineKeyFrame *keyFrame = currentVisibleKeyFrame();
		return keyFrame ? keyFrame->layerId : 0;
	}

	int bodyWidth() const { return columnWidth * frameCount() - xScroll; }
	int bodyHeight() const { return rowHeight * trackCount() - yScroll; }

	QRect frameHeaderRect() const
	{
		return QRect{headerWidth, 0, bodyWidth() + 1, rowHeight};
	}

	QRect trackSidebarRect() const
	{
		return QRect{0, rowHeight, headerWidth, bodyHeight() + 1};
	}

	int trackDropIndex(int y) const
	{
		return qBound(
			0, (y + rowHeight / 2 + yScroll) / rowHeight - 1, trackCount());
	}

	const QIcon &getVisibilityIcon(const canvas::TimelineTrack &track)
	{
		return track.hidden ? hiddenIcon : visibleIcon;
	}

	const QIcon &getOnionSkinIcon(const canvas::TimelineTrack &track)
	{
		return track.onionSkin ? onionSkinOnIcon : onionSkinOffIcon;
	}
};

TimelineWidget::TimelineWidget(QWidget *parent)
	: QWidget{parent}
	, d{new Private}
{
	setAcceptDrops(true);
	setMouseTracking(true);
	d->visibleIcon = QIcon::fromTheme("view-visible");
	d->hiddenIcon = QIcon::fromTheme("view-hidden");
	d->onionSkinOnIcon = QIcon::fromTheme("onion-on");
	d->onionSkinOffIcon = QIcon::fromTheme("onion-off");
	d->uselessIcon = QIcon::fromTheme("edit-delete");
	d->verticalScroll = new QScrollBar(Qt::Vertical, this);
	d->horizontalScroll = new QScrollBar(Qt::Horizontal, this);
	connect(
		QApplication::clipboard(), &QClipboard::dataChanged, this,
		&TimelineWidget::updatePasteAction);
	connect(
		d->horizontalScroll, &QScrollBar::valueChanged, this,
		&TimelineWidget::setHorizontalScroll);
	connect(
		d->verticalScroll, &QScrollBar::valueChanged, this,
		&TimelineWidget::setVerticalScroll);
	updateScrollbars();
}

TimelineWidget::~TimelineWidget()
{
	delete d;
}

void TimelineWidget::setCanvas(canvas::CanvasModel *canvas)
{
	d->canvas = canvas;
	connect(
		canvas->timeline(), &canvas::TimelineModel::tracksChanged, this,
		&TimelineWidget::updateTracks);
	connect(
		canvas->metadata(), &canvas::DocumentMetadata::frameCountChanged, this,
		&TimelineWidget::updateFrameCount);
	updateTracks();
	updateFrameCount();
}

void TimelineWidget::setActions(const Actions &actions)
{
	Q_ASSERT(!d->haveActions);
	d->haveActions = true;
	d->actions = actions;

	d->trackMenu = new QMenu{this};
	d->frameMenu = new QMenu{this};
	d->frameMenu->addAction(actions.keyFrameSetLayer);
	d->frameMenu->addAction(actions.keyFrameSetEmpty);
	d->frameMenu->addAction(actions.keyFrameCut);
	d->frameMenu->addAction(actions.keyFrameCopy);
	d->frameMenu->addAction(actions.keyFramePaste);
	d->frameMenu->addAction(actions.keyFrameProperties);
	d->frameMenu->addAction(actions.keyFrameDelete);
	d->frameMenu->addSeparator();
	d->frameMenu->addAction(actions.keyFrameExposureIncrease);
	d->frameMenu->addAction(actions.keyFrameExposureDecrease);
	d->frameMenu->addSeparator();
	d->frameMenu->addMenu(actions.animationLayerMenu);
	d->frameMenu->addMenu(actions.animationGroupMenu);
	d->frameMenu->addMenu(actions.animationDuplicateMenu);
	d->frameMenu->addSeparator();
	for(QMenu *menu : {d->trackMenu, d->frameMenu}) {
		menu->addAction(actions.trackAdd);
		menu->addAction(actions.trackDuplicate);
		menu->addAction(actions.trackRetitle);
		menu->addAction(actions.trackDelete);
		menu->addAction(actions.trackVisible);
		menu->addAction(actions.trackOnionSkin);
		menu->addSeparator();
		menu->addAction(actions.frameCountSet);
		menu->addAction(actions.framerateSet);
		menu->addSeparator();
		menu->addAction(actions.frameNext);
		menu->addAction(actions.framePrev);
		menu->addAction(actions.trackAbove);
		menu->addAction(actions.trackBelow);
	}

	connect(
		actions.keyFrameSetLayer, &QAction::triggered, this,
		&TimelineWidget::setKeyFrameLayer);
	connect(
		actions.keyFrameSetEmpty, &QAction::triggered, this,
		&TimelineWidget::setKeyFrameEmpty);
	connect(
		actions.keyFrameCut, &QAction::triggered, this,
		&TimelineWidget::cutKeyFrame);
	connect(
		actions.keyFrameCopy, &QAction::triggered, this,
		&TimelineWidget::copyKeyFrame);
	connect(
		actions.keyFramePaste, &QAction::triggered, this,
		&TimelineWidget::pasteKeyFrame);
	connect(
		actions.keyFrameProperties, &QAction::triggered, this,
		&TimelineWidget::showKeyFrameProperties);
	connect(
		actions.keyFrameDelete, &QAction::triggered, this,
		&TimelineWidget::deleteKeyFrame);
	connect(
		actions.keyFrameExposureIncrease, &QAction::triggered, this,
		&TimelineWidget::increaseKeyFrameExposure);
	connect(
		actions.keyFrameExposureDecrease, &QAction::triggered, this,
		&TimelineWidget::decreaseKeyFrameExposure);
	connect(
		actions.trackAdd, &QAction::triggered, this, &TimelineWidget::addTrack);
	connect(
		actions.trackVisible, &QAction::triggered, this,
		&TimelineWidget::toggleTrackVisible);
	connect(
		actions.trackOnionSkin, &QAction::triggered, this,
		&TimelineWidget::toggleTrackOnionSkin);
	connect(
		actions.trackDuplicate, &QAction::triggered, this,
		&TimelineWidget::duplicateTrack);
	connect(
		actions.trackRetitle, &QAction::triggered, this,
		&TimelineWidget::retitleTrack);
	connect(
		actions.trackDelete, &QAction::triggered, this,
		&TimelineWidget::deleteTrack);
	connect(
		actions.frameCountSet, &QAction::triggered, this,
		&TimelineWidget::setFrameCount);
	connect(
		actions.framerateSet, &QAction::triggered, this,
		&TimelineWidget::setFramerate);
	connect(
		actions.frameNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.framePrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);
	connect(
		actions.trackAbove, &QAction::triggered, this,
		&TimelineWidget::trackAbove);
	connect(
		actions.trackBelow, &QAction::triggered, this,
		&TimelineWidget::trackBelow);
	connect(
		actions.keyFrameCreateLayerNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.keyFrameCreateLayerPrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);
	connect(
		actions.keyFrameCreateGroupNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.keyFrameCreateGroupPrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);
	connect(
		actions.keyFrameDuplicateNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.keyFrameDuplicatePrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);

	updateActions();
}

void TimelineWidget::setCurrentFrame(int frame)
{
	setCurrent(d->currentTrackId, frame, true, true);
}

void TimelineWidget::setCurrentTrack(int trackId)
{
	setCurrent(trackId, d->currentFrame, true, true);
}

void TimelineWidget::setCurrentLayer(int layerId)
{
	d->setSelectedLayerId(layerId);
	updateActions();
}

void TimelineWidget::updateControlsEnabled(
	bool access, bool locked, bool compatibilityMode)
{
	d->editable = access && !locked && !compatibilityMode;
	updateActions();
	update();
}

canvas::CanvasModel *TimelineWidget::canvas() const
{
	return d->canvas;
}

int TimelineWidget::frameCount() const
{
	return d->frameCount();
}

int TimelineWidget::currentTrackId() const
{
	return d->currentTrackId;
}

int TimelineWidget::currentFrame() const
{
	return d->currentFrame;
}

void TimelineWidget::changeFramerate(int framerate)
{
	emitCommand([&](uint8_t contextId) {
		return net::makeSetMetadataIntMessage(
			contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE, framerate);
	});
}

void TimelineWidget::changeFrameCount(int frameCount)
{
	emitCommand([&](uint8_t contextId) {
		return net::makeSetMetadataIntMessage(
			contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT, frameCount);
	});
}

bool TimelineWidget::event(QEvent *event)
{
	if(event->type() == QEvent::ToolTip) {
		const canvas::TimelineKeyFrame *keyFrame = d->hoverKeyFrame();
		QString tip;
		if(keyFrame) {
			int layerId = keyFrame->layerId;
			const QString &keyFrameTitle = keyFrame->title;
			const canvas::TimelineKeyFrame *previousKeyFrame =
				d->beforeHoverKeyFrame();
			bool useless = previousKeyFrame &&
						   keyFrame->hasSameContentAs(*previousKeyFrame);
			if(layerId == 0) {
				if(useless) {
					tip = keyFrameTitle.isEmpty()
							  ? tr("Blank key frame (duplicate)")
							  : tr("Blank key frame %1 (duplicate)")
									.arg(keyFrameTitle);
				} else {
					tip = keyFrameTitle.isEmpty()
							  ? tr("Blank key frame")
							  : tr("Blank key frame %1").arg(keyFrameTitle);
				}
			} else {
				QString layerTitle = d->layerTitleById(layerId);
				if(useless) {
					tip =
						keyFrameTitle.isEmpty()
							? tr("Key frame on %1 (duplicate)").arg(layerTitle)
							: tr("Key frame %1 on %2 (duplicate)")
								  .arg(keyFrameTitle)
								  .arg(layerTitle);
				} else {
					tip = keyFrameTitle.isEmpty()
							  ? tr("Key frame on %1").arg(layerTitle)
							  : tr("Key frame %1 on %2")
									.arg(keyFrameTitle)
									.arg(layerTitle);
				}
			}
		} else if(d->hoverTarget.action == TrackAction::ToggleVisible) {
			tip = tr("Toggle visibility");
		} else if(d->hoverTarget.action == TrackAction::ToggleOnionSkin) {
			tip = tr("Toggle onion skin");
		}
		setToolTip(tip);
	}
	return QWidget::event(event);
}

void TimelineWidget::paintEvent(QPaintEvent *)
{
	if(!d->canvas) {
		return;
	}

	const QVector<canvas::TimelineTrack> tracks = d->getTracks();
	int trackCount = tracks.size();
	int frameCount = d->frameCount();
	int currentTrackIndex = d->trackIndexById(d->currentTrackId);
	int currentFrame = d->currentFrame;

	int w = width() - d->verticalScroll->width();
	int h = height() - d->horizontalScroll->height();
	int rowHeight = d->rowHeight;
	int columnWidth = d->columnWidth;
	int headerWidth = d->headerWidth;
	int xScroll = d->xScroll;
	int yScroll = d->yScroll;

	QPainter painter{this};
	QPalette pal = palette();
	if(!d->editable) {
		pal.setCurrentColorGroup(QPalette::Disabled);
	}

	QColor textColor = pal.windowText().color();
	QColor highlightedTextColor = pal.highlightedText().color();
	QColor outlineColor = pal.window().color().lightness() < 128
							  ? pal.light().color()
							  : pal.dark().color();
	QColor bodyColor = pal.alternateBase().color();
	QColor gridColor = bodyColor.lightness() < 128 ? bodyColor.lighter(150)
												   : bodyColor.darker(150);

	// Body background.
	QRect bodyRect{headerWidth, rowHeight, w - headerWidth, h - rowHeight};
	painter.setClipRect(bodyRect);
	painter.setPen(Qt::NoPen);
	painter.setBrush(bodyColor);
	painter.drawRect(bodyRect);

	// Selected row and column.
	if(trackCount != 0) {
		QColor selectedColor = pal.highlight().color();
		selectedColor.setAlphaF(0.5f);
		painter.setBrush(selectedColor);
		if(currentFrame != -1) {
			int x = headerWidth + currentFrame * columnWidth - xScroll;
			painter.drawRect(x, bodyRect.top(), columnWidth, bodyRect.height());
		}
		if(currentTrackIndex != -1) {
			int i = trackCount - currentTrackIndex - 1;
			int y = rowHeight + i * rowHeight - yScroll;
			painter.drawRect(bodyRect.left(), y, bodyRect.width(), rowHeight);
		}
	}

	// Key frames.
	const canvas::TimelineKeyFrame *currentVisibleKeyFrame =
		d->currentVisibleKeyFrame();
	for(int i = 0; i < trackCount; ++i) {
		int y = rowHeight + i * rowHeight - yScroll;
		const canvas::TimelineTrack &track = tracks[trackCount - i - 1];
		painter.setOpacity(track.hidden ? 0.5 : 1.0);
		const QVector<canvas::TimelineKeyFrame> &keyFrames = track.keyFrames;
		int keyFrameCount = keyFrames.size();
		for(int j = 0; j < keyFrameCount; ++j) {
			const canvas::TimelineKeyFrame &keyFrame = keyFrames[j];
			int frame = keyFrame.frameIndex;

			int x = headerWidth + frame * columnWidth - xScroll;
			bool isSelected =
				currentFrame == frame && d->currentTrackId == track.id;
			QBrush brush =
				isSelected ? pal.highlightedText() : pal.windowText();
			if(keyFrame.layerId == 0) {
				brush.setStyle(Qt::DiagCrossPattern);
			} else {
				bool sameContentAsCurrent =
					currentVisibleKeyFrame &&
					&keyFrame != currentVisibleKeyFrame &&
					keyFrame.hasSameContentAs(*currentVisibleKeyFrame);
				if(sameContentAsCurrent) {
					brush.setStyle(Qt::Dense3Pattern);
				}
			}
			painter.setBrush(brush);
			painter.drawRect(x, y, columnWidth, rowHeight);

			if(keyFrame.layerId != 0) {
				bool useless =
					j > 0 && keyFrame.hasSameContentAs(keyFrames[j - 1]);
				if(useless) {
					int iconSize = qMin(columnWidth, rowHeight) - 1;
					d->uselessIcon.paint(
						&painter, x - (iconSize - columnWidth) / 2 + 1,
						y - (iconSize - rowHeight) / 2 + 1, iconSize, iconSize);
				}

				int until = j < keyFrameCount - 1 ? keyFrames[j + 1].frameIndex
												  : frameCount;
				int count = until - frame - 1;
				if(count > 0) {
					QColor trailingColor = brush.color();
					trailingColor.setAlphaF(0.3f);
					brush.setColor(trailingColor);
					brush.setStyle(Qt::Dense4Pattern);
					painter.setBrush(brush);
					painter.drawRect(
						x + columnWidth, y, count * columnWidth, rowHeight);
				}
			}
		}
	}
	painter.setOpacity(1.0);

	// Body grid or no tracks message.
	if(trackCount == 0) {
		painter.setPen(textColor);
		painter.drawText(
			bodyRect,
			tr("There's no tracks yet.\n"
			   "Add one using the ＋ button above\n"
			   "or via Animation ▸ New Track."),
			Qt::AlignHCenter | Qt::AlignVCenter);
	} else {
		painter.setPen(gridColor);
		painter.setBrush(Qt::NoBrush);
		int firstX = columnWidth - (xScroll % columnWidth);
		for(int x = firstX; x < bodyRect.width(); x += columnWidth) {
			int xw = x + headerWidth;
			painter.drawLine(xw, bodyRect.top(), xw, bodyRect.bottom());
		}
		int firstY = rowHeight - (yScroll % rowHeight);
		for(int y = firstY; y < bodyRect.height(); y += rowHeight) {
			int yh = y + rowHeight;
			painter.drawLine(bodyRect.left(), yh, bodyRect.right(), yh);
		}
	}

	// Outlines around the sides.
	painter.setPen(outlineColor);
	painter.setClipRect(QRect{}, Qt::NoClip);
	painter.drawLine(0, 0, w, 0);
	painter.drawLine(0, rowHeight, w, rowHeight);
	painter.drawLine(0, 0, 0, h);
	painter.drawLine(headerWidth, 0, headerWidth, h);

	// Frame numbers along the top.
	painter.setClipRect(d->frameHeaderRect());
	for(int i = 0; i < frameCount; ++i) {
		int x = headerWidth + i * columnWidth - xScroll;
		bool isSelected = d->currentFrame == i;
		if(isSelected) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(pal.highlight());
			painter.drawRect(x + 1, 1, columnWidth - 1, rowHeight - 1);
		}
		painter.setPen(isSelected ? highlightedTextColor : textColor);
		painter.drawText(
			x, 0, columnWidth, rowHeight, Qt::AlignCenter | Qt::AlignVCenter,
			QString::number(i + 1));
		painter.setPen(outlineColor);
		painter.drawLine(x + columnWidth, 0, x + columnWidth, rowHeight);
	}

	// Tracks along the side.
	painter.setClipRect(d->trackSidebarRect());
	for(int i = 0; i < trackCount; ++i) {
		int y = rowHeight + i * rowHeight - yScroll;
		const canvas::TimelineTrack &track = tracks[trackCount - i - 1];
		bool isSelected = track.id == d->currentTrackId;
		if(isSelected) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(pal.highlight());
			painter.drawRect(1, y + 1, headerWidth - 1, rowHeight - 1);
		}

		int x = TRACK_PADDING;
		int yIcon = y + (rowHeight - ICON_SIZE) / 2;
		qreal opacity = track.hidden ? 0.3 : 1.0;
		qreal onionSkinOpacity = opacity * (track.onionSkin ? 1.0 : 0.3);
		painter.setPen(isSelected ? highlightedTextColor : textColor);
		painter.setOpacity(opacity);
		d->getVisibilityIcon(track).paint(
			&painter, x, yIcon, ICON_SIZE, ICON_SIZE);

		x += TRACK_PADDING + ICON_SIZE;
		painter.setOpacity(onionSkinOpacity);
		d->getOnionSkinIcon(track).paint(
			&painter, x, yIcon, ICON_SIZE, ICON_SIZE);

		x += TRACK_PADDING + ICON_SIZE;
		painter.setOpacity(opacity);
		painter.drawText(
			x, y, headerWidth - x - TRACK_PADDING, rowHeight, Qt::AlignVCenter,
			track.title);

		painter.setOpacity(1.0);
		painter.setPen(outlineColor);
		painter.drawLine(0, y + rowHeight, headerWidth, y + rowHeight);
	}
	painter.setOpacity(1.0);
	painter.setClipRect(QRect{}, Qt::NoClip);

	if(d->dragHover == Drag::Track) {
		painter.setPen(textColor);
		int y = d->trackDropIndex(d->dragPos.y()) * rowHeight + rowHeight -
				d->yScroll;
		painter.drawLine(0, y, headerWidth, y);
	} else if(d->dragHover == Drag::KeyFrame) {
		Target target = getMouseTarget(d->dragPos);
		QBrush brush = pal.highlightedText();
		brush.setStyle(Qt::Dense5Pattern);
		painter.setBrush(brush);
		int x = headerWidth + target.frameIndex * columnWidth - xScroll;
		int y = rowHeight + target.uiTrackIndex * rowHeight - yScroll;
		painter.drawRect(x, y, columnWidth, rowHeight);
	}
}

void TimelineWidget::resizeEvent(QResizeEvent *event)
{
	Q_UNUSED(event);

	const QSize hsh = d->horizontalScroll->sizeHint();
	const QSize vsh = d->verticalScroll->sizeHint();

	d->horizontalScroll->setGeometry(
		0, height() - hsh.height(), width(), hsh.height());
	d->verticalScroll->setGeometry(
		width() - vsh.width(), 0, vsh.width(), height() - hsh.height());
	d->horizontalScroll->setPageStep(width());
	d->verticalScroll->setPageStep(height());

	updateScrollbars();
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
	QPoint mousePos = compat::mousePos(*event);
	Target target = getMouseTarget(mousePos);
	d->hoverTarget = target;

	Drag dragType = d->drag;
	bool shouldDrag = d->editable &&
					  ((dragType == Drag::Track && d->currentTrack()) ||
					   (dragType == Drag::KeyFrame && d->currentKeyFrame())) &&
					  (mousePos - d->dragOrigin).manhattanLength() <
						  QApplication::startDragDistance();
	if(shouldDrag) {
		d->drag = Drag::None;

		QPixmap pixmap;
		QMimeData *mimeData;
		if(dragType == Drag::Track) {
			const canvas::TimelineTrack *track = d->currentTrack();
			const QPalette &pal = palette();
			pixmap = QPixmap{d->headerWidth, d->rowHeight};
			pixmap.fill(pal.highlight().color());
			QPainter painter{&pixmap};
			painter.setPen(pal.highlightedText().color());
			painter.drawText(
				4, 0, d->headerWidth - 8, d->rowHeight, Qt::AlignVCenter,
				track->title);

			mimeData = new QMimeData;
			mimeData->setProperty("dragType", int(Drag::Track));
			mimeData->setProperty("trackId", track->id);
		} else if(dragType == Drag::KeyFrame) {
			const canvas::TimelineKeyFrame *keyFrame = d->currentKeyFrame();
			const QPalette &pal = palette();
			pixmap = QPixmap{d->columnWidth, d->rowHeight};
			QBrush brush = pal.highlightedText();
			if(keyFrame->layerId == 0) {
				brush.setStyle(Qt::DiagCrossPattern);
				pixmap.fill(Qt::transparent);
			}
			QPainter painter{&pixmap};
			painter.setBrush(brush);
			painter.drawRect(pixmap.rect());

			mimeData = new QMimeData;
			mimeData->setProperty("dragType", int(Drag::KeyFrame));
			mimeData->setProperty("trackId", d->currentTrackId);
			mimeData->setProperty("frameIndex", keyFrame->frameIndex);
		} else {
			qWarning("Unknown drag type %d", int(dragType));
			return;
		}

		QDrag *drag = new QDrag{this};
		drag->setMimeData(mimeData); // QDrag takes ownership of the QMimeData.
		drag->setPixmap(pixmap);
		drag->exec(d->dropAction); // Qt takes ownership of the QDrag.
	} else if(d->pressedOnHeader && target.frameIndex != -1) {
		setCurrent(0, target.frameIndex, true, true);
	}
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
	Target target = getMouseTarget(compat::mousePos(*event));
	applyMouseTarget(event, target, true);

	Qt::MouseButton button = event->button();
	Drag drag = Drag::None;
	Qt::DropAction dropAction = Qt::MoveAction;
	if(button == Qt::LeftButton) {
		d->pressedOnHeader = target.onHeader;
		if(target.action != TrackAction::None) {
			executeTargetAction(target);
			event->accept();
		} else if(target.trackId != 0 && target.frameIndex == -1) {
			drag = Drag::Track;
		} else if(d->keyFrameBy(target.trackId, target.frameIndex)) {
			drag = Drag::KeyFrame;
		}
	} else if(button == Qt::MiddleButton) {
		if(d->keyFrameBy(target.trackId, target.frameIndex)) {
			drag = Drag::KeyFrame;
			dropAction = Qt::CopyAction;
		}
	} else if(button == Qt::RightButton) {
		if(target.trackId != 0 && target.frameIndex != -1 && d->frameMenu) {
			d->frameMenu->popup(compat::globalPos(*event));
		} else if(d->trackMenu) {
			d->trackMenu->popup(compat::globalPos(*event));
		}
	}

	d->drag = drag;
	if(drag != Drag::None) {
		d->dragOrigin = compat::mousePos(*event);
		d->dropAction = dropAction;
	}
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	Target target = getMouseTarget(compat::mousePos(*event));
	applyMouseTarget(event, target, true);

	if(event->button() == Qt::LeftButton) {
		if(target.action != TrackAction::None) {
			executeTargetAction(target);
		} else if(target.frameIndex == -1 && target.trackId != 0) {
			retitleTrack();
		} else if(target.frameIndex != -1 && target.trackId != 0) {
			showKeyFrameProperties();
		}
	}
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if(event->button() == Qt::LeftButton) {
		d->pressedOnHeader = false;
		d->drag = Drag::None;
	}
}

void TimelineWidget::wheelEvent(QWheelEvent *event)
{
	QPoint pd = event->pixelDelta() * (event->inverted() ? -1 : 1);
	if(event->modifiers() & Qt::ShiftModifier) {
		pd = QPoint{pd.y(), pd.x()};
	}

	int x = qMax(0, d->horizontalScroll->value() - pd.x());
	int y = qMax(0, d->verticalScroll->value() - pd.y());
	d->horizontalScroll->setValue(x - (x % d->columnWidth));
	d->verticalScroll->setValue(y - (y % d->rowHeight));
}

void TimelineWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->source() != this) {
		return;
	}

	int dragType = event->mimeData()->property("dragType").toInt();
	if(dragType == int(Drag::Track) || dragType == int(Drag::KeyFrame)) {
		event->acceptProposedAction();
	}
}

void TimelineWidget::dragMoveEvent(QDragMoveEvent *event)
{
	if(event->source() != this) {
		return;
	}

	int dragType = event->mimeData()->property("dragType").toInt();
	if(dragType == int(Drag::Track)) {
		QRect rect{0, 0, d->headerWidth, height()};
		if(event->source() == this && event->answerRect().intersects(rect)) {
			event->accept(rect);
			d->dragHover = Drag::Track;
			d->dragPos = compat::dropPos(*event);
		} else {
			event->ignore();
			d->dragHover = Drag::None;
		}
	} else if(dragType == int(Drag::KeyFrame)) {
		QRect rect{
			d->headerWidth, d->rowHeight,
			qMin(width() - d->headerWidth, d->bodyWidth()),
			qMin(height() - d->rowHeight, d->bodyHeight())};
		if(event->source() == this && event->answerRect().intersects(rect)) {
			event->accept(rect);
			d->dragHover = Drag::KeyFrame;
			d->dragPos = compat::dropPos(*event);
		} else {
			event->ignore();
			d->dragHover = Drag::None;
		}
	}
	update();
}

void TimelineWidget::dragLeaveEvent(QDragLeaveEvent *)
{
	d->dragHover = Drag::None;
	update();
}

void TimelineWidget::dropEvent(QDropEvent *event)
{
	if(event->source() != this) {
		return;
	}

	d->dragHover = Drag::None;
	update();

	const QMimeData *mimeData = event->mimeData();
	int dragType = mimeData->property("dragType").toInt();
	if(dragType == int(Drag::Track)) {
		QPoint pos = compat::dropPos(*event);
		int target = d->trackCount() - d->trackDropIndex(pos.y()) - 1;
		QVector<uint16_t> trackIds;
		for(const canvas::TimelineTrack &track : d->getTracks()) {
			trackIds.append(track.id);
		}

		int trackId = mimeData->property("trackId").toInt();
		int source = trackIds.indexOf(trackId);
		if(target < source) {
			++target;
		}
		if(source == -1 || source == target) {
			return;
		}
		trackIds.move(source, target);

		emitCommand([&](uint8_t contextId) {
			return net::makeTrackOrderMessage(contextId, trackIds);
		});
	} else if(dragType == int(Drag::KeyFrame)) {
		int sourceTrackId = mimeData->property("trackId").toInt();
		int sourceFrameIndex = mimeData->property("frameIndex").toInt();
		Target target = getMouseTarget(compat::dropPos(*event));
		bool isValid = target.trackId != 0 && target.frameIndex != -1 &&
					   (sourceTrackId != target.trackId ||
						sourceFrameIndex != target.frameIndex);
		if(isValid) {
			applyMouseTarget(nullptr, target, false);
			emitCommand([&](uint8_t contextId) {
				if(event->dropAction() == Qt::CopyAction) {
					return net::makeKeyFrameSetMessage(
						contextId, target.trackId, target.frameIndex,
						sourceTrackId, sourceFrameIndex,
						DP_MSG_KEY_FRAME_SET_SOURCE_KEY_FRAME);
				} else {
					return net::makeKeyFrameDeleteMessage(
						contextId, sourceTrackId, sourceFrameIndex,
						target.trackId, target.frameIndex);
				}
			});
		}
	}
}

void TimelineWidget::setKeyFrameLayer()
{
	if(!d->editable) {
		return;
	}
	if(d->selectedLayerId > 0) {
		setKeyFrame(d->selectedLayerId);
	}
}

void TimelineWidget::setKeyFrameEmpty()
{
	if(!d->editable) {
		return;
	}
	setKeyFrame(0);
}

void TimelineWidget::cutKeyFrame()
{
	copyKeyFrame();
	deleteKeyFrame();
}

void TimelineWidget::copyKeyFrame()
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineKeyFrame *keyFrame = d->currentKeyFrame();
	if(!keyFrame) {
		return;
	}

	QJsonArray layerVisibilityJson;
	using LayersIt = QHash<int, bool>::const_iterator;
	const LayersIt end = keyFrame->layerVisibility.constEnd();
	for(LayersIt it = keyFrame->layerVisibility.constBegin(); it != end; ++it) {
		layerVisibilityJson.append(QJsonObject{
			{"layerId", it.key()},
			{"visible", it.value()},
		});
	}

	QJsonDocument doc{QJsonObject{
		{"layerId", keyFrame->layerId},
		{"title", keyFrame->title},
		{"layerVisibility", layerVisibilityJson},
	}};

	QMimeData *mimeData = new QMimeData;
	mimeData->setData(KEY_FRAME_MIME_TYPE, doc.toJson(QJsonDocument::Compact));
	QApplication::clipboard()->setMimeData(mimeData);
}

void TimelineWidget::pasteKeyFrame()
{
	if(!d->editable) {
		return;
	}

	int trackId = d->currentTrackId;
	int frame = d->currentFrame;
	if(trackId == 0 || frame == -1) {
		return;
	}

	const QMimeData *mimeData = QApplication::clipboard()->mimeData();
	if(!mimeData->hasFormat(KEY_FRAME_MIME_TYPE)) {
		return;
	}

	QJsonParseError err;
	QJsonDocument doc =
		QJsonDocument::fromJson(mimeData->data(KEY_FRAME_MIME_TYPE), &err);
	if(!doc.isObject()) {
		qWarning(
			"Error parsing key frame on clipboard: %s",
			qUtf8Printable(err.errorString()));
		return;
	}

	QJsonObject keyFrameJson = doc.object();
	setKeyFrame(keyFrameJson["layerId"].toInt());

	QString title = keyFrameJson["title"].toString();
	QHash<int, bool> layerVisibility;
	for(const QJsonValue &value : keyFrameJson["layerVisibility"].toArray()) {
		QJsonObject obj = value.toObject();
		layerVisibility.insert(obj["layerId"].toInt(), obj["visible"].toBool());
	}
	setKeyFrameProperties(trackId, frame, {}, {}, title, layerVisibility);
}

void TimelineWidget::showKeyFrameProperties()
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineKeyFrame *keyFrame = d->currentKeyFrame();
	if(!keyFrame) {
		return;
	}

	int trackId = d->currentTrackId;
	int frame = d->currentFrame;
	dialogs::KeyFramePropertiesDialog *dlg = nullptr;
	for(QObject *child : children()) {
		dialogs::KeyFramePropertiesDialog *c =
			qobject_cast<dialogs::KeyFramePropertiesDialog *>(child);
		if(c && c->trackId() == trackId && c->frame() == frame) {
			dlg = c;
			break;
		}
	}
	if(!dlg) {
		dlg = new dialogs::KeyFramePropertiesDialog{trackId, frame, this};
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		connect(
			dlg, &dialogs::KeyFramePropertiesDialog::keyFramePropertiesChanged,
			this, &TimelineWidget::keyFramePropertiesChanged);
	}

	dlg->setKeyFrameTitle(keyFrame->title);
	dlg->setKeyFrameLayers(d->canvas->layerlist()->toKeyFrameLayerModel(
		keyFrame->layerId, keyFrame->layerVisibility));

	utils::showWindow(dlg);
	dlg->raise();
	dlg->activateWindow();
}

void TimelineWidget::keyFramePropertiesChanged(
	int trackId, int frame, const QString &title,
	const QHash<int, bool> &layerVisibility)
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineKeyFrame *keyFrame = d->keyFrameBy(trackId, frame);
	if(!keyFrame) {
		return;
	}

	setKeyFrameProperties(
		trackId, frame, keyFrame->title, keyFrame->layerVisibility, title,
		layerVisibility);
}

void TimelineWidget::deleteKeyFrame()
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineKeyFrame *keyFrame = d->currentKeyFrame();
	if(keyFrame) {
		emitCommand([&](uint8_t contextId) {
			return net::makeKeyFrameDeleteMessage(
				contextId, d->currentTrackId, keyFrame->frameIndex, 0, 0);
		});
	}
}

void TimelineWidget::increaseKeyFrameExposure()
{
	changeFrameExposure(1);
}

void TimelineWidget::decreaseKeyFrameExposure()
{
	changeFrameExposure(-1);
}

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

	d->nextTrackId = trackId;
	const canvas::TimelineTrack *track = d->currentTrack();
	emitCommand([&](uint8_t contextId) {
		return net::makeTrackCreateMessage(
			contextId, trackId, track ? track->id : 0, 0,
			timeline->getAvailableTrackName(tr("Track")));
	});
}

void TimelineWidget::toggleTrackVisible(bool visible)
{
	if(d->currentTrackId != 0) {
		emit trackHidden(d->currentTrackId, !visible);
	}
}

void TimelineWidget::toggleTrackOnionSkin(bool onionSkin)
{
	if(d->currentTrackId != 0) {
		emit trackOnionSkinEnabled(d->currentTrackId, onionSkin);
	}
}

void TimelineWidget::duplicateTrack()
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineTrack *source = d->currentTrack();
	if(!source) {
		return;
	}

	const canvas::TimelineModel *timeline = d->canvas->timeline();
	int trackId = timeline->getAvailableTrackId();
	if(trackId == 0) {
		qWarning("Couldn't find a free ID for a new track");
		return;
	}

	d->nextTrackId = trackId;
	emitCommand([&](uint8_t contextId) {
		return net::makeTrackCreateMessage(
			contextId, trackId, source->id, source->id,
			timeline->getAvailableTrackName(source->title));
	});
}

void TimelineWidget::retitleTrack()
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineTrack *source = d->currentTrack();
	if(!source) {
		return;
	}

	bool ok;
	QString title = QInputDialog::getText(
		this, tr("Rename Track"), tr("Track Name"), QLineEdit::Normal,
		source->title, &ok);
	if(ok && !(title = title.trimmed()).isEmpty()) {
		emitCommand([&](uint8_t contextId) {
			return net::makeTrackRetitleMessage(
				contextId, d->currentTrackId, title);
		});
	}
}

void TimelineWidget::deleteTrack()
{
	int trackId = currentTrackId();
	if(trackId == 0) {
		return;
	}

	int trackIndex = d->trackIndexById(trackId);
	d->nextTrackId = d->trackIdByIndex(trackIndex - 1);
	if(d->nextTrackId == 0) {
		d->nextTrackId = d->trackIdByIndex(trackIndex + 1);
	}

	emitCommand([&](uint8_t contextId) {
		return net::makeTrackDeleteMessage(contextId, trackId);
	});
}

void TimelineWidget::setFrameCount()
{
	if(!d->editable) {
		return;
	}

	bool ok;
	int frameCount = QInputDialog::getInt(
		this, tr("Change Frame Count"),
		tr("Frame Count (key frames beyond this point will be deleted)"),
		d->frameCount(), 1, 9999, 1, &ok);
	if(ok) {
		changeFrameCount(frameCount);
	}
}

void TimelineWidget::setFramerate()
{
	if(!d->editable) {
		return;
	}

	bool ok;
	int framerate = QInputDialog::getInt(
		this, tr("Change Framerate"), tr("Frames Per Second (FPS)"),
		d->framerate(), 1, 999, 1, &ok);
	if(ok) {
		changeFramerate(framerate);
	}
}

void TimelineWidget::nextFrame()
{
	int targetFrame = d->currentFrame + 1;
	setCurrentFrame(targetFrame < d->frameCount() ? targetFrame : 0);
}

void TimelineWidget::prevFrame()
{
	int targetFrame = d->currentFrame - 1;
	setCurrentFrame(targetFrame >= 0 ? targetFrame : d->frameCount() - 1);
}

void TimelineWidget::trackAbove()
{
	int targetTrack = d->trackIndexById(d->currentTrackId) + 1;
	setCurrentTrack(
		d->trackIdByIndex(targetTrack < d->trackCount() ? targetTrack : 0));
}

void TimelineWidget::trackBelow()
{
	int targetTrack = d->trackIndexById(d->currentTrackId) - 1;
	setCurrentTrack(d->trackIdByIndex(
		targetTrack >= 0 ? targetTrack : d->trackCount() - 1));
}

void TimelineWidget::updateTracks()
{
	if(!d->canvas) {
		return;
	}

	const QFontMetrics fm = fontMetrics();
	d->rowHeight = qMax(fm.height() * 3 / 2, ICON_SIZE);
	d->columnWidth = fm.horizontalAdvance(QStringLiteral("999")) + 8;

	d->verticalScroll->setSingleStep(d->rowHeight);
	d->horizontalScroll->setSingleStep(d->columnWidth);

	int maxTrackAdvance = 0;
	int nextTrackId = 0;
	int currentTrackId = 0;
	int anyTrackId = 0;
	for(const canvas::TimelineTrack &track : d->getTracks()) {
		int trackAdvance = fm.horizontalAdvance(track.title);
		if(trackAdvance > maxTrackAdvance) {
			maxTrackAdvance = trackAdvance;
		}

		if(d->nextTrackId != 0 && track.id == d->nextTrackId) {
			nextTrackId = d->nextTrackId;
			d->nextTrackId = 0;
		}
		if(d->currentTrackId == track.id) {
			currentTrackId = d->currentTrackId;
		}
		if(anyTrackId == 0) {
			anyTrackId = track.id;
		}
	}
	d->headerWidth = maxTrackAdvance + TRACK_PADDING * 4 + ICON_SIZE * 2;
	int effectiveTrackId = nextTrackId != 0		 ? nextTrackId
						   : currentTrackId != 0 ? currentTrackId
												 : anyTrackId;
	setCurrent(effectiveTrackId, d->currentFrame, false, false);
	updateActions();
	updateScrollbars();
	update();
}

void TimelineWidget::updateFrameCount()
{
	setCurrent(d->currentTrackId, d->currentFrame, false, false);
	updateActions();
	updateScrollbars();
	update();
}

void TimelineWidget::setHorizontalScroll(int pos)
{
	d->xScroll = pos;
	update();
}

void TimelineWidget::setVerticalScroll(int pos)
{
	d->yScroll = pos;
	update();
}

void TimelineWidget::updatePasteAction()
{
	if(d->canvas && d->haveActions) {
		const QMimeData *mimeData = QApplication::clipboard()->mimeData();
		d->actions.keyFramePaste->setEnabled(
			d->editable && d->frameCount() > 0 && d->currentTrack() &&
			d->currentFrame != -1 && mimeData &&
			mimeData->hasFormat(KEY_FRAME_MIME_TYPE));
	}
}

void TimelineWidget::setCurrent(
	int trackId, int frame, bool triggerUpdate, bool selectLayer)
{
	bool needsUpdate = false;

	if(d->trackIndexById(trackId) != -1) {
		d->currentTrackId = trackId;
		emit trackSelected(trackId);
		needsUpdate = true;
	}

	int actualFrame = qBound(0, frame, qMax(0, d->frameCount() - 1));
	if(actualFrame != d->currentFrame) {
		const canvas::TimelineKeyFrame *prevKeyFrame =
			d->currentVisibleKeyFrame();
		d->currentFrame = actualFrame;
		emit frameSelected(frame);
		needsUpdate = true;
		if(d->currentVisibleKeyFrame() == prevKeyFrame) {
			selectLayer = false;
		}
	}

	if(needsUpdate && triggerUpdate) {
		if(selectLayer) {
			int layerIdToSelect = d->guessLayerIdToSelect();
			if(layerIdToSelect != 0) {
				emit layerSelected(layerIdToSelect);
			}
		}
		updateActions();
		update();
	}
}

void TimelineWidget::setKeyFrame(int layerId)
{
	emitCommand([&](uint8_t contextId) {
		return net::makeKeyFrameSetMessage(
			contextId, d->currentTrackId, d->currentFrame, layerId, 0,
			DP_MSG_KEY_FRAME_SET_SOURCE_LAYER);
	});
}

void TimelineWidget::setKeyFrameProperties(
	int trackId, int frame, const QString &prevTitle,
	const QHash<int, bool> prevLayerVisibility, const QString &title,
	const QHash<int, bool> layerVisibility)
{
	bool titleChanged = prevTitle != title;
	bool layersChanged = prevLayerVisibility != layerVisibility;

	if(titleChanged || layersChanged) {
		uint8_t contextId = d->canvas->localUserId();
		net::Message messages[3];
		int fill = 0;
		messages[fill++] = net::makeUndoPointMessage(contextId);
		if(titleChanged) {
			messages[fill++] = net::makeKeyFrameRetitleMessage(
				contextId, trackId, frame, title);
		}
		if(layersChanged) {
			QVector<uint16_t> layers;
			layers.reserve(layerVisibility.size() * 2);
			using LayersIt = QHash<int, bool>::const_iterator;
			const LayersIt end = layerVisibility.constEnd();
			for(LayersIt it = layerVisibility.constBegin(); it != end; ++it) {
				layers.append(it.key());
				layers.append(
					it.value() ? DP_KEY_FRAME_LAYER_REVEALED
							   : DP_KEY_FRAME_LAYER_HIDDEN);
			}
			messages[fill++] = net::makeKeyFrameLayerAttributesMessage(
				contextId, trackId, frame, layers);
		}
		emit timelineEditCommands(fill, messages);
	}
}

void TimelineWidget::changeFrameExposure(int direction)
{
	Q_ASSERT(direction == -1 || direction == 1);
	if(!d->editable) {
		return;
	}

	Exposure exposure = d->currentExposure();
	if(!exposure.isValid()) {
		return;
	}

	bool forward = direction == 1;
	if(forward ? exposure.hasFrameAtEnd : exposure.length <= 1) {
		return;
	}

	QVector<int> frameIndexes = d->gatherCurrentExposureFrames(exposure.start);
	if(frameIndexes.isEmpty()) {
		return;
	}

	// Order these correctly so we don't move frames over each other.
	if(forward) {
		std::sort(frameIndexes.rbegin(), frameIndexes.rend());
	} else {
		std::sort(frameIndexes.begin(), frameIndexes.end());
	}

	uint8_t contextId = d->canvas->localUserId();
	QVector<net::Message> messages;
	messages.reserve(frameIndexes.size() + 1);
	messages.append(net::makeUndoPointMessage(contextId));
	for(int frameIndex : frameIndexes) {
		messages.append(net::makeKeyFrameDeleteMessage(
			contextId, d->currentTrackId, frameIndex, d->currentTrackId,
			frameIndex + direction));
	}
	emit timelineEditCommands(messages.size(), messages.constData());
}

void TimelineWidget::updateActions()
{
	if(!d->haveActions || !d->canvas) {
		return;
	}

	bool timelineEditable = d->editable;
	d->actions.frameCountSet->setEnabled(timelineEditable);
	d->actions.framerateSet->setEnabled(timelineEditable);

	const canvas::TimelineTrack *track = d->currentTrack();
	bool trackEditable = timelineEditable && track;
	d->actions.trackAdd->setEnabled(timelineEditable);
	d->actions.trackVisible->setEnabled(track);
	setCheckedSignalBlocked(d->actions.trackVisible, track && !track->hidden);
	d->actions.trackOnionSkin->setEnabled(track);
	setCheckedSignalBlocked(
		d->actions.trackOnionSkin, track && track->onionSkin);
	d->actions.trackDuplicate->setEnabled(trackEditable);
	d->actions.trackRetitle->setEnabled(trackEditable);
	d->actions.trackDelete->setEnabled(trackEditable);

	bool haveMultipleFrames = d->frameCount() > 1;
	d->actions.frameNext->setEnabled(haveMultipleFrames);
	d->actions.framePrev->setEnabled(haveMultipleFrames);

	bool haveMultipleTracks = d->trackCount() > 1;
	d->actions.trackAbove->setEnabled(haveMultipleTracks);
	d->actions.trackBelow->setEnabled(haveMultipleTracks);

	bool keyFrameSettable = trackEditable && d->frameCount() > 0;

	const canvas::TimelineKeyFrame *currentKeyFrame = d->currentKeyFrame();
	d->actions.keyFrameSetEmpty->setEnabled(
		keyFrameSettable &&
		(!currentKeyFrame || currentKeyFrame->layerId != 0));

	bool haveKeyFrameLayer = false;
	bool isSameKeyFrameLayer = false;
	QString keyFrameSetLayerText;
	if(d->selectedLayerId > 0) {
		QModelIndex layerIndex = d->layerIndexById(d->selectedLayerId);
		if(layerIndex.isValid()) {
			int layerId =
				layerIndex.data(canvas::LayerListModel::IdRole).toInt();
			QString layerTitle =
				layerIndex.data(canvas::LayerListModel::TitleRole).toString();
			keyFrameSetLayerText = tr("Set Key Frame to %1").arg(layerTitle);
			haveKeyFrameLayer = true;
			isSameKeyFrameLayer =
				currentKeyFrame && layerId == currentKeyFrame->layerId;
		}
	}

	d->actions.keyFrameSetLayer->setEnabled(
		keyFrameSettable && haveKeyFrameLayer && !isSameKeyFrameLayer);
	if(haveKeyFrameLayer) {
		d->actions.keyFrameSetLayer->setText(keyFrameSetLayerText);
	} else {
		d->actions.keyFrameSetLayer->setText(
			tr("Set Key Frame to Current Layer"));
	}

	bool keyFrameCreatable = keyFrameSettable && !currentKeyFrame;
	d->actions.keyFrameCreateLayer->setEnabled(keyFrameCreatable);
	d->actions.keyFrameCreateGroup->setEnabled(keyFrameCreatable);

	bool nextKeyFrameCreatable = keyFrameSettable &&
								 d->currentFrame < d->frameCount() - 1 &&
								 !d->nextKeyFrame();
	d->actions.keyFrameCreateLayerNext->setEnabled(nextKeyFrameCreatable);
	d->actions.keyFrameCreateGroupNext->setEnabled(nextKeyFrameCreatable);

	bool prevKeyFrameCreatable =
		keyFrameSettable && d->currentFrame > 0 && !d->previousKeyFrame();
	d->actions.keyFrameCreateLayerPrev->setEnabled(prevKeyFrameCreatable);
	d->actions.keyFrameCreateGroupPrev->setEnabled(prevKeyFrameCreatable);

	bool keyFrameDuplicatable =
		currentKeyFrame && currentKeyFrame->layerId != 0;
	d->actions.keyFrameDuplicateNext->setEnabled(
		nextKeyFrameCreatable && keyFrameDuplicatable);
	d->actions.keyFrameDuplicatePrev->setEnabled(
		prevKeyFrameCreatable && keyFrameDuplicatable);

	bool keyFrameEditable = keyFrameSettable && currentKeyFrame;
	d->actions.keyFrameCut->setEnabled(keyFrameEditable);
	d->actions.keyFrameCopy->setEnabled(keyFrameEditable);
	d->actions.keyFrameProperties->setEnabled(keyFrameEditable);
	d->actions.keyFrameDelete->setEnabled(keyFrameEditable);

	bool canIncreaseExposure = false;
	bool canDecreaseExposure = false;
	if(timelineEditable && track) {
		Exposure exposure = d->currentExposure();
		if(exposure.isValid()) {
			canIncreaseExposure = !exposure.hasFrameAtEnd;
			canDecreaseExposure = exposure.length > 1;
		}
	}
	d->actions.keyFrameExposureIncrease->setEnabled(canIncreaseExposure);
	d->actions.keyFrameExposureDecrease->setEnabled(canDecreaseExposure);

	updatePasteAction();
}

void TimelineWidget::updateScrollbars()
{
	if(!d->canvas) {
		return;
	}
	QSize hsh = d->horizontalScroll->sizeHint();
	QSize vsh = d->verticalScroll->sizeHint();
	d->horizontalScroll->setMaximum(qMax(
		0, (d->headerWidth + vsh.width() + d->frameCount() * d->columnWidth) -
			   width()));
	d->verticalScroll->setMaximum(qMax(
		0, (d->rowHeight + hsh.height() + d->trackCount() * d->rowHeight) -
			   height()));
}

TimelineWidget::Target TimelineWidget::getMouseTarget(const QPoint &pos) const
{
	Target target{-1, 0, -1, false, TrackAction::None};
	if(d->canvas) {
		int x = pos.x();
		int headerWidth = d->headerWidth;
		if(x > headerWidth) {
			int frameIndex = (x - headerWidth + d->xScroll) / d->columnWidth;
			if(frameIndex >= 0 && frameIndex < d->frameCount()) {
				target.frameIndex = frameIndex;
			}
		}

		int y = pos.y();
		int rowHeight = d->rowHeight;
		if(y > rowHeight) {
			target.uiTrackIndex = (y - rowHeight + d->yScroll) / rowHeight;
			target.trackId = d->trackIdByUiIndex(target.uiTrackIndex);
			int tp = TRACK_PADDING;
			int tph = TRACK_PADDING / 2;
			int mid = tph + ICON_SIZE + tp;
			if(x >= tph && x < mid) {
				target.action = TrackAction::ToggleVisible;
			} else if(x >= mid && x < tp * 3 + ICON_SIZE * 2) {
				target.action = TrackAction::ToggleOnionSkin;
			}
		} else {
			target.onHeader = true;
		}
	}
	return target;
}

void TimelineWidget::applyMouseTarget(
	QMouseEvent *event, const Target &target, bool press)
{
	bool action = press && target.action != TrackAction::None;
	if(event && ((target.trackId != 0 && target.frameIndex != -1) || action)) {
		event->accept();
	}

	bool right = event && event->button() == Qt::RightButton;
	if(!action || right) {
		int trackId = target.trackId == 0 ? d->currentTrackId : target.trackId;
		int frame =
			target.frameIndex == -1 ? d->currentFrame : target.frameIndex;
		setCurrent(trackId, frame, true, event && !right);
	}
}

void TimelineWidget::executeTargetAction(const Target &target)
{
	const canvas::TimelineTrack *track = d->trackById(target.trackId);
	if(track) {
		if(target.action == TrackAction::ToggleVisible) {
			emit trackHidden(track->id, !track->hidden);
		} else if(target.action == TrackAction::ToggleOnionSkin) {
			emit trackOnionSkinEnabled(track->id, !track->onionSkin);
		} else {
			qWarning("Unknown track action %d", int(target.action));
		}
	}
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

void TimelineWidget::setCheckedSignalBlocked(QAction *action, bool checked)
{
	QSignalBlocker blocker{action};
	action->setChecked(checked);
}

}
