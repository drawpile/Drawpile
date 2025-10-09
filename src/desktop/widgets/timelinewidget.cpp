// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/key_frame.h>
}
#include "desktop/dialogs/animationpropertiesdialog.h"
#include "desktop/dialogs/keyframepropertiesdialog.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/drawdance/documentmetadata.h"
#include "libclient/net/message.h"
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDrag>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <algorithm>
#include <limits>

namespace widgets {

enum class TargetAction {
	None,
	ToggleVisible,
	ToggleOnionSkin,
	ToggleCamera,
	CameraMenu,
};

enum class TargetHeader { None, Header, RangeFirst, RangeLast };

struct TimelineWidget::Target {
	int uiTrackIndex;
	int trackId;
	int frameIndex;
	bool onCamera;
	TargetHeader header;
	TargetAction action;
};

struct Exposure {
	int start;
	int length;
	bool hasFrameAtEnd;

	bool isValid() const { return start != -1 && length != -1; }
	bool canIncrease() const { return isValid() && !hasFrameAtEnd; }
	bool canDecrease() const { return isValid() && length > 1; }

	bool canChangeInDirection(bool forward) const
	{
		return forward ? canIncrease() : canDecrease();
	}
};

struct TimelineWidget::Private {
	TimelineWidget *const q;
	canvas::CanvasModel *canvas = nullptr;

	bool haveActions = false;
	Actions actions;
	QMenu *trackMenu = nullptr;
	QMenu *frameMenu = nullptr;
	QActionGroup *cameraActionGroup = nullptr;

	QIcon visibleIcon;
	QIcon hiddenIcon;
	QIcon onionSkinOnIcon;
	QIcon onionSkinOffIcon;
	QIcon uselessIcon;
	QIcon cameraIcon;
	QIcon noCameraIcon;
	QIcon ellipsisIcon;
	int headerWidth = 64;
	int rowHeight = 10;
	int columnWidth = 10;
	int xScroll = 0;
	int yScroll = 0;
	int currentTrackId = 0;
	int currentCameraId = 0;
	int currentFrame = 0;
	int nextTrackId = 0;
	int nextCameraId = 0;
	Target hoverTarget = {
		-1, 0, -1, false, TargetHeader::None, TargetAction::None};
	TargetHeader pressedHeader = TargetHeader::None;
	bool editable = false;
	Drag drag = Drag::None;
	Drag dragHover = Drag::None;
	Qt::DropAction dropAction;
	QPoint dragOrigin;
	QPoint dragPos;

	int selectedLayerId = 0;
	QHash<QPair<int, int>, int> layerIdByKeyFrame;

	QScrollBar *verticalScroll = nullptr;
	QScrollBar *horizontalScroll = nullptr;

	Private(TimelineWidget *widget)
		: q(widget)
	{
	}

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

	const QVector<canvas::TimelineCamera> &getCameras() const
	{
		if(canvas) {
			return canvas->timeline()->cameras();
		} else {
			static const QVector<canvas::TimelineCamera> emptyCameras;
			return emptyCameras;
		}
	}

	int cameraCount() const { return getCameras().size(); }

	int frameCount() const
	{
		return canvas ? canvas->metadata()->frameCount() : 0;
	}

	const canvas::TimelineCamera *currentCamera() const
	{
		return cameraById(currentCameraId);
	}

	int currentFrameRangeFirst() const
	{
		if(canvas) {
			const canvas::TimelineCamera *camera = currentCamera();
			if(camera && camera->rangeValid()) {
				return camera->rangeFirst();
			} else {
				return canvas->metadata()->frameRangeFirst();
			}
		} else {
			return -1;
		}
	}

	int currentFrameRangeLast() const
	{
		if(canvas) {
			const canvas::TimelineCamera *camera = currentCamera();
			if(camera && camera->rangeValid()) {
				return camera->rangeLast();
			} else {
				return canvas->metadata()->frameRangeLast();
			}
		} else {
			return -1;
		}
	}

	int scrollableFrameCount() const
	{
		if(canvas) {
			canvas::DocumentMetadata *metadata = canvas->metadata();
			int frameCount = metadata->frameCount();
			int lastFrame = qMax(
				metadata->frameRangeLast(),
				canvas->timeline()->lastFrameIndex());
			return qMin(frameCount, lastFrame + EXTRA_VISIBLE_FRAME_COUNT);
		} else {
			return 0;
		}
	}

	int visibleFrameCount(bool includeMinimumVisibleFrameCount = true) const
	{
		if(canvas) {
			canvas::DocumentMetadata *metadata = canvas->metadata();
			int frameCount = metadata->frameCount();
			int lastFrame = qMax(
				metadata->frameRangeLast(),
				canvas->timeline()->lastFrameIndex());
			int w = q->width() - verticalScroll->width() - headerWidth;
			int minimumVisibleFrameCount = w / columnWidth + 1;
			int softFrameCount = lastFrame + minimumVisibleFrameCount;
			if(softFrameCount > frameCount) {
				return frameCount;
			}

			if(includeMinimumVisibleFrameCount &&
			   minimumVisibleFrameCount > softFrameCount) {
				softFrameCount = minimumVisibleFrameCount;
			}

			return qMin(frameCount, softFrameCount);
		} else {
			return 0;
		}
	}

	double effectiveFramerate() const
	{
		return canvas ? canvas->metadata()->framerate() : 0.0;
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
		if(trackId > 0) {
			const QVector<canvas::TimelineTrack> &tracks = getTracks();
			int trackCount = tracks.size();
			for(int i = 0; i < trackCount; ++i) {
				if(tracks[i].id == trackId) {
					return i;
				}
			}
		}
		return -1;
	}

	int cameraIndexById(int cameraId) const
	{
		if(cameraId > 0) {
			const QVector<canvas::TimelineCamera> &cameras = getCameras();
			int cameraCount = cameras.size();
			for(int i = 0; i < cameraCount; ++i) {
				if(cameras[i].id() == cameraId) {
					return i;
				}
			}
		}
		return -1;
	}

	const canvas::TimelineTrack *trackById(int trackId) const
	{
		int i = trackIndexById(trackId);
		return i == -1 ? nullptr : &getTracks()[i];
	}

	const canvas::TimelineCamera *cameraById(int cameraId) const
	{
		int i = cameraIndexById(cameraId);
		return i == -1 ? nullptr : &getCameras()[i];
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

	Exposure currentExposureByTrackId(int trackId)
	{
		const canvas::TimelineTrack *track = trackById(trackId);
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
		bool hasFrameAtEnd = keyFrameBy(trackId, invalidIndex - 1) != nullptr;
		return {start, length, hasFrameAtEnd};
	}

	Exposure currentExposure()
	{
		return currentExposureByTrackId(currentTrackId);
	}

	QVector<int> gatherCurrentExposureFramesByTrackId(int trackId, int start)
	{
		const canvas::TimelineTrack *track = trackById(trackId);
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

	QVector<int> gatherCurrentExposureFrames(int start)
	{
		return gatherCurrentExposureFramesByTrackId(currentTrackId, start);
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

	bool guessLayerIdToSelect(int &outLayerId)
	{
		QPair<int, int> key = {currentTrackId, currentFrame};
		if(isLayerVisibleInCurrentTrack(selectedLayerId)) {
			layerIdByKeyFrame.insert(key, selectedLayerId);
			return false;
		}

		int lastLayerId = layerIdByKeyFrame.value(key, 0);
		if(lastLayerId != 0 && isLayerVisibleInCurrentTrack(lastLayerId)) {
			outLayerId = lastLayerId;
			return true;
		}

		const canvas::TimelineKeyFrame *keyFrame = currentVisibleKeyFrame();
		if(keyFrame) {
			outLayerId = keyFrame->layerId;
			return true;
		}

		return false;
	}

	int bodyWidth() const
	{
		return columnWidth * visibleFrameCount() - xScroll;
	}

	int fullBodyWidth() const { return columnWidth * frameCount() - xScroll; }

	int bodyHeight() const { return rowHeight * trackCount() - yScroll; }

	QRect frameHeaderRect() const
	{
		return QRect(headerWidth, 0, bodyWidth() + 1, rowHeight);
	}

	QRect cameraSidebarRect() const
	{
		return QRect{
			0, currentCameraId == 0 ? 0 : rowHeight, headerWidth, rowHeight};
	}

	QRect trackSidebarRect() const
	{
		return QRect{
			0, currentCameraId == 0 ? rowHeight : rowHeight * 2, headerWidth,
			bodyHeight() + 1};
	}

	qreal rangeHandleInsetX() const { return qreal(columnWidth) / 6.0; }

	int trackDropIndex(int y) const
	{
		return qBound(
			0, (y + rowHeight / 2 + yScroll) / rowHeight - 1, trackCount());
	}

	const QIcon &getVisibilityIcon(bool hidden)
	{
		return hidden ? hiddenIcon : visibleIcon;
	}

	const QIcon &getOnionSkinIcon(bool onionSkin)
	{
		return onionSkin ? onionSkinOnIcon : onionSkinOffIcon;
	}

	const QIcon &getCameraIcon(bool camera)
	{
		return camera ? cameraIcon : noCameraIcon;
	}
};

TimelineWidget::TimelineWidget(QWidget *parent)
	: QWidget(parent)
	, d(new Private(this))
{
	setAcceptDrops(true);
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	d->visibleIcon = QIcon::fromTheme("view-visible");
	d->hiddenIcon = QIcon::fromTheme("view-hidden");
	d->onionSkinOnIcon = QIcon::fromTheme("onion-on");
	d->onionSkinOffIcon = QIcon::fromTheme("onion-off");
	d->uselessIcon = QIcon::fromTheme("edit-delete");
	d->cameraIcon = QIcon::fromTheme("camera-video");
	d->noCameraIcon = QIcon::fromTheme("drawpile_nocamera");
	d->ellipsisIcon = QIcon::fromTheme("drawpile_ellipsis_vertical");
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
		canvas->timeline(), &canvas::TimelineModel::modelChanged, this,
		&TimelineWidget::updateTracks);
	connect(
		canvas->metadata(), &canvas::DocumentMetadata::frameCountChanged, this,
		&TimelineWidget::updateFrameCount);
	connect(
		canvas->metadata(), &canvas::DocumentMetadata::frameRangeChanged, this,
		&TimelineWidget::updateFrameRange);
	updateTracks();
	updateFrameCount();
}

void TimelineWidget::setActions(const Actions &actions)
{
	Q_ASSERT(!d->haveActions);
	d->haveActions = true;
	d->actions = actions;

	d->trackMenu = new QMenu(this);
	d->frameMenu = new QMenu(this);
	d->frameMenu->addAction(actions.keyFrameSetLayer);
	d->frameMenu->addAction(actions.keyFrameSetEmpty);
	d->frameMenu->addAction(actions.keyFrameCut);
	d->frameMenu->addAction(actions.keyFrameCopy);
	d->frameMenu->addAction(actions.keyFramePaste);
	d->frameMenu->addMenu(actions.animationKeyFrameColorMenu);
	d->frameMenu->addAction(actions.keyFrameProperties);
	d->frameMenu->addAction(actions.keyFrameDelete);
	d->frameMenu->addSeparator();
	d->frameMenu->addAction(actions.keyFrameExposureIncrease);
	d->frameMenu->addAction(actions.keyFrameExposureIncreaseVisible);
	d->frameMenu->addAction(actions.keyFrameExposureDecrease);
	d->frameMenu->addAction(actions.keyFrameExposureDecreaseVisible);
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
		menu->addAction(actions.frameNext);
		menu->addAction(actions.framePrev);
		menu->addAction(actions.keyFrameNext);
		menu->addAction(actions.keyFramePrev);
		menu->addAction(actions.trackAbove);
		menu->addAction(actions.trackBelow);
		menu->addSeparator();
		menu->addAction(actions.animationProperties);
	}

	connect(
		d->frameMenu, &QMenu::aboutToShow, this,
		&TimelineWidget::updateKeyFrameColorMenuIcon);
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
		actions.animationKeyFrameColorMenu, &QMenu::triggered, this,
		&TimelineWidget::setKeyFrameColor);
	connect(
		actions.keyFrameDelete, &QAction::triggered, this,
		&TimelineWidget::deleteKeyFrame);
	connect(
		actions.keyFrameExposureIncrease, &QAction::triggered, this,
		&TimelineWidget::increaseKeyFrameExposure);
	connect(
		actions.keyFrameExposureIncreaseVisible, &QAction::triggered, this,
		&TimelineWidget::increaseKeyFrameExposureVisible);
	connect(
		actions.keyFrameExposureDecrease, &QAction::triggered, this,
		&TimelineWidget::decreaseKeyFrameExposure);
	connect(
		actions.keyFrameExposureDecreaseVisible, &QAction::triggered, this,
		&TimelineWidget::decreaseKeyFrameExposureVisible);
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
		actions.cameraAdd, &QAction::triggered, this,
		&TimelineWidget::addCamera);
	connect(
		actions.cameraDuplicate, &QAction::triggered, this,
		&TimelineWidget::duplicateCamera);
	connect(
		actions.cameraDelete, &QAction::triggered, this,
		&TimelineWidget::deleteCamera);
	connect(
		actions.animationProperties, &QAction::triggered, this,
		&TimelineWidget::showAnimationProperties);
	connect(
		actions.frameNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.framePrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);
	connect(
		actions.keyFrameNext, &QAction::triggered, this,
		&TimelineWidget::nextKeyFrame);
	connect(
		actions.keyFramePrev, &QAction::triggered, this,
		&TimelineWidget::prevKeyFrame);
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

void TimelineWidget::updateControlsEnabled(bool access, bool locked)
{
	d->editable = access && !locked;
	updateActions();
	update();
}

void TimelineWidget::updateKeyFrameColorMenuIcon()
{
	QMenu *menu = d->actions.animationKeyFrameColorMenu;
	QColor markerColor = menu->property("markercolor").value<QColor>();
	QColor iconColor = menu->property("iconcolor").value<QColor>();
	if(markerColor != iconColor) {
		menu->setProperty("iconcolor", markerColor);
		menu->setIcon(utils::makeColorIcon(16, markerColor));
	}
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
		} else {
			switch(d->hoverTarget.action) {
			case TargetAction::ToggleVisible:
				tip = tr("Toggle visibility");
				break;
			case TargetAction::ToggleOnionSkin:
				tip = tr("Toggle onion skin");
				break;
			case TargetAction::ToggleCamera:
				tip = tr("Toggle camera visibility on canvas");
				break;
			case TargetAction::CameraMenu:
				tip = tr("Camera menu");
				break;
			case TargetAction::None:
				break;
			}
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
	int visibleFrameCount = d->visibleFrameCount();
	int currentTrackIndex = d->trackIndexById(d->currentTrackId);
	int currentFrame = d->currentFrame;

	int w = width() - d->verticalScroll->width();
	int h = height() - d->horizontalScroll->height();
	int rowHeight = d->rowHeight;
	int columnWidth = d->columnWidth;
	int headerWidth = d->headerWidth;
	int xScroll = d->xScroll;
	int yScroll = d->yScroll;

	int frameRangeFirst = d->currentFrameRangeFirst();
	int frameRangeLast = d->currentFrameRangeLast();
	if(d->pressedHeader == TargetHeader::RangeFirst) {
		frameRangeFirst = qMin(currentFrame, frameRangeLast);
	} else if(d->pressedHeader == TargetHeader::RangeLast) {
		frameRangeLast = qMax(currentFrame, frameRangeFirst);
	}

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

	// Camera.
	const canvas::TimelineCamera *currentCamera = d->currentCamera();
	QRect cameraSidebarRect = d->cameraSidebarRect();
	int cameraOffsetY = cameraSidebarRect.y();

	// Key frames.
	const canvas::TimelineKeyFrame *currentVisibleKeyFrame =
		d->currentVisibleKeyFrame();
	for(int i = 0; i < trackCount; ++i) {
		int y = cameraOffsetY + rowHeight + i * rowHeight - yScroll;
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
			QBrush brush;
			if(keyFrame.color.isValid()) {
				brush = isSelected ? keyFrame.color.lighter() : keyFrame.color;
			} else {
				brush = isSelected ? pal.highlightedText() : pal.windowText();
			}
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
												  : visibleFrameCount;
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

	// Body grid
	if(trackCount != 0) {
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
	bool nextInRange = 0 >= frameRangeFirst && 0 <= frameRangeLast;
	for(int i = 0; i < visibleFrameCount; ++i) {
		bool currentInRange = nextInRange;
		nextInRange = (i + 1) >= frameRangeFirst && (i + 1) <= frameRangeLast;

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

		if(currentInRange || nextInRange) {
			painter.setPen(outlineColor);
			painter.drawLine(x + columnWidth, 0, x + columnWidth, rowHeight);
		}
	}

	// Camera and tracks along the side.
	painter.setClipRect(cameraSidebarRect);
	{
		int y = cameraOffsetY - yScroll;
		int x = TRACK_PADDING;
		int yIcon = y + qRound(qreal(rowHeight - ICON_SIZE) / 2.0);
		qreal visibilityOpacity = 1.0;
		qreal cameraOpacity = 1.0;
		painter.setPen(textColor);
		painter.setOpacity(cameraOpacity);
		d->getCameraIcon(currentCamera)
			.paint(&painter, x, yIcon, ICON_SIZE, ICON_SIZE);

		x += TRACK_PADDING + ICON_SIZE;
		painter.setOpacity(visibilityOpacity);
		painter.drawText(
			x, y, headerWidth - x - TRACK_PADDING, rowHeight, Qt::AlignVCenter,
			currentCamera ? currentCamera->title() : noCameraTitle());

		painter.setOpacity(1.0);
		d->ellipsisIcon.paint(
			&painter, cameraSidebarRect.right() - TRACK_PADDING - ICON_SIZE,
			yIcon, ICON_SIZE, ICON_SIZE);

		painter.setPen(outlineColor);
		painter.drawLine(0, y + rowHeight, headerWidth, y + rowHeight);
	}

	painter.setClipRect(d->trackSidebarRect());
	for(int i = 0; i < trackCount; ++i) {
		int y = cameraOffsetY + rowHeight + i * rowHeight - yScroll;
		const canvas::TimelineTrack &track = tracks[trackCount - i - 1];
		bool isSelected = track.id == d->currentTrackId;
		if(isSelected) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(pal.highlight());
			painter.drawRect(1, y + 1, headerWidth - 1, rowHeight - 1);
		}

		int x = TRACK_PADDING;
		int yIcon = y + qRound(qreal(rowHeight - ICON_SIZE) / 2.0);
		qreal opacity = track.hidden ? 0.3 : 1.0;
		qreal onionSkinOpacity = opacity * (track.onionSkin ? 1.0 : 0.3);
		painter.setPen(isSelected ? highlightedTextColor : textColor);
		painter.setOpacity(opacity);
		d->getVisibilityIcon(track.hidden)
			.paint(&painter, x, yIcon, ICON_SIZE, ICON_SIZE);

		x += TRACK_PADDING + ICON_SIZE;
		painter.setOpacity(onionSkinOpacity);
		d->getOnionSkinIcon(track.onionSkin)
			.paint(&painter, x, yIcon, ICON_SIZE, ICON_SIZE);

		x += TRACK_PADDING + ICON_SIZE;
		painter.setOpacity(opacity);
		painter.drawText(
			x, y, headerWidth - x - TRACK_PADDING, rowHeight, Qt::AlignVCenter,
			track.title);

		painter.setOpacity(1.0);
		painter.setPen(outlineColor);
		painter.drawLine(0, y + rowHeight, headerWidth, y + rowHeight);
	}
	painter.setClipRect(QRect(), Qt::NoClip);

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

	if(!d->canvas->isCompatibilityMode()) {
		QRect coverRect(headerWidth, 0, d->bodyWidth() + 1, h);
		painter.setClipRect(coverRect);
		painter.setPen(Qt::NoPen);
		painter.setBrush(bodyColor);
		painter.setOpacity(0.6);

		QRect leftCoverRect(
			coverRect.topLeft(),
			QPoint(
				headerWidth + frameRangeFirst * columnWidth - xScroll - 1,
				coverRect.bottom()));
		painter.drawRect(leftCoverRect);

		QRect rightCoverRect(
			QPoint(
				headerWidth + (frameRangeLast + 1) * columnWidth - xScroll + 1,
				coverRect.top()),
			coverRect.bottomRight());
		painter.drawRect(rightCoverRect);

		painter.setBrush(pal.highlightedText());
		painter.setOpacity(0.4);

		qreal leftHandle = qreal(leftCoverRect.right()) + 1.0;
		qreal rightHandle = qreal(rightCoverRect.left()) - 1.0;
		qreal handleBottom = qreal(rowHeight);
		qreal handleInsetX = d->rangeHandleInsetX();
		qreal handleInsetY = qreal(rowHeight) / 6.0;
		painter.drawPolygon(QPolygonF({
			QPointF(leftHandle, 0.0),
			QPointF(leftHandle, handleBottom),
			QPointF(leftHandle + handleInsetX, handleBottom - handleInsetY),
			QPointF(leftHandle + handleInsetX, handleInsetY),
		}));
		painter.drawPolygon(QPolygonF({
			QPointF(rightHandle, 0.0),
			QPointF(rightHandle, handleBottom),
			QPointF(rightHandle - handleInsetX, handleBottom - handleInsetY),
			QPointF(rightHandle - handleInsetX, handleInsetY),
		}));
	}

	if(trackCount == 0) {
		painter.setOpacity(1.0);
		painter.setClipRect(QRect(), Qt::NoClip);
		painter.setPen(textColor);
		painter.setBrush(Qt::NoBrush);
		painter.drawText(
			bodyRect,
			tr("There's no tracks yet.\n"
			   "Add one using the ＋ button above\n"
			   "or via Animation ▸ New Track."),
			Qt::AlignHCenter | Qt::AlignVCenter);
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

void TimelineWidget::keyPressEvent(QKeyEvent *event)
{
	switch(event->key()) {
	case Qt::Key_Left:
		event->accept();
		prevFrame();
		break;
	case Qt::Key_Up:
		event->accept();
		trackAbove();
		break;
	case Qt::Key_Right:
		event->accept();
		nextFrame();
		break;
	case Qt::Key_Down:
		event->accept();
		trackBelow();
		break;
	default:
		QWidget::keyPressEvent(event);
		break;
	}
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
	QPoint mousePos = compat::mousePos(*event);
	Target target = getMouseTarget(mousePos);
	d->hoverTarget = target;

	Drag dragType = d->drag;
	if((target.header == TargetHeader::RangeFirst ||
		target.header == TargetHeader::RangeLast) &&
	   event->buttons() == Qt::NoButton) {
		setCursor(Qt::SizeHorCursor);
	} else if(
		d->pressedHeader != TargetHeader::RangeFirst &&
		d->pressedHeader != TargetHeader::RangeLast) {
		setCursor(Qt::ArrowCursor);
	}

	bool shouldDrag = d->editable &&
					  ((dragType == Drag::Track && d->currentTrack()) ||
					   (dragType == Drag::KeyFrame && d->currentKeyFrame())) &&
					  (mousePos - d->dragOrigin).manhattanLength() >=
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
#ifdef __EMSCRIPTEN__
		if(QDrag::needsAsync()) {
			drag->execAsync(d->dropAction); // Qt takes ownership of the QDrag.
			return;
		}
#else
		drag->exec(d->dropAction); // Qt takes ownership of the QDrag.
#endif
	} else if(
		d->pressedHeader != TargetHeader::None && target.frameIndex != -1) {
		setCurrent(0, target.frameIndex, true, true);
	}
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
	QPoint pos = compat::mousePos(*event);
	Target target = getMouseTarget(pos);
	applyMouseTarget(event, target, true);

	Qt::MouseButton button = event->button();
	Drag drag = Drag::None;
	Qt::DropAction dropAction = Qt::MoveAction;
	if(button == Qt::LeftButton) {
		if(target.action != TargetAction::None) {
			executeTargetAction(target, pos);
			event->accept();
		} else if(target.trackId != 0 && target.frameIndex == -1) {
			drag = Drag::Track;
		} else if(d->keyFrameBy(target.trackId, target.frameIndex)) {
			drag = Drag::KeyFrame;
		} else {
			d->pressedHeader = target.header;
			if(target.header == TargetHeader::RangeFirst ||
			   target.header == TargetHeader::RangeLast) {
				setCursor(Qt::SizeHorCursor);
			}
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
	QPoint pos = compat::mousePos(*event);
	Target target = getMouseTarget(pos);
	applyMouseTarget(event, target, true);

	if(event->button() == Qt::LeftButton) {
		if(target.action != TargetAction::None) {
			executeTargetAction(target, pos);
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
		if(d->pressedHeader == TargetHeader::RangeFirst) {
			int frameRangeFirst =
				qBound(0, d->currentFrame, d->currentFrameRangeLast());
			setAnimationProperties(-1.0, frameRangeFirst, -1);
		} else if(d->pressedHeader == TargetHeader::RangeLast) {
			int frameRangeLast = qBound(
				d->currentFrameRangeFirst(), d->currentFrame, d->frameCount());
			setAnimationProperties(-1.0, -1, frameRangeLast);
		}
		d->pressedHeader = TargetHeader::None;
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
			qMin(width() - d->headerWidth, d->fullBodyWidth()),
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
		{"title", keyFrame->titleWithColor()},
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

void TimelineWidget::setKeyFrameColor(QAction *action)
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineKeyFrame *keyFrame = d->currentKeyFrame();
	if(!keyFrame) {
		return;
	}

	QColor color = action->property("markercolor").value<QColor>();
	if(color != keyFrame->color) {
		setKeyFrameProperties(
			d->currentTrackId, keyFrame->frameIndex, keyFrame->titleWithColor(),
			keyFrame->layerVisibility,
			canvas::TimelineKeyFrame::makeTitleWithColor(
				keyFrame->title, color),
			keyFrame->layerVisibility);
	}
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
	dlg->setKeyFrameColor(keyFrame->color);
	dlg->setKeyFrameLayers(d->canvas->layerlist()->toKeyFrameLayerModel(
		keyFrame->layerId, keyFrame->layerVisibility));

	utils::showWindow(dlg);
	dlg->raise();
	dlg->activateWindow();
}

void TimelineWidget::keyFramePropertiesChanged(
	int trackId, int frame, const QColor &color, const QString &title,
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
		trackId, frame, keyFrame->titleWithColor(), keyFrame->layerVisibility,
		canvas::TimelineKeyFrame::makeTitleWithColor(title, color),
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
	changeFrameExposure(1, false);
}

void TimelineWidget::increaseKeyFrameExposureVisible()
{
	changeFrameExposure(1, true);
}

void TimelineWidget::decreaseKeyFrameExposure()
{
	changeFrameExposure(-1, false);
}

void TimelineWidget::decreaseKeyFrameExposureVisible()
{
	changeFrameExposure(-1, true);
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

	utils::getInputText(
		this, tr("Rename Track"), tr("Track Name"), source->title,
		[this](const QString &text) {
			QString title = text.trimmed();
			if(!title.isEmpty() && d->currentTrack()) {
				emitCommand([&](uint8_t contextId) {
					return net::makeTrackRetitleMessage(
						contextId, d->currentTrackId, title);
				});
			}
		});
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

void TimelineWidget::addCamera()
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineModel *timeline = d->canvas->timeline();
	int cameraId = timeline->getAvailableCameraId();
	if(cameraId == 0) {
		qWarning("Couldn't find a free ID for a new camera");
		return;
	}

	d->nextCameraId = cameraId;
	emitCommand([&](uint8_t contextId) {
		return net::makeCameraCreateMessage(
			contextId, cameraId, 0,
			timeline->getAvailableCameraName(tr("Camera")));
	});
}

void TimelineWidget::duplicateCamera()
{
	if(!d->editable) {
		return;
	}

	const canvas::TimelineCamera *source = d->currentCamera();
	if(!source) {
		return;
	}

	const canvas::TimelineModel *timeline = d->canvas->timeline();
	int cameraId = timeline->getAvailableCameraId();
	if(cameraId == 0) {
		qWarning("Couldn't find a free ID for a duplicate camera");
		return;
	}

	d->nextCameraId = cameraId;
	emitCommand([&](uint8_t contextId) {
		return net::makeCameraCreateMessage(
			contextId, cameraId, source->id(),
			timeline->getAvailableCameraName(source->title()));
	});
}

void TimelineWidget::deleteCamera()
{
	int cameraId = d->currentCameraId;
	if(cameraId == 0) {
		return;
	}

	d->nextCameraId = 0;
	emitCommand([&](uint8_t contextId) {
		return net::makeCameraDeleteMessage(contextId, cameraId);
	});
}

void TimelineWidget::showAnimationProperties()
{
	if(d->editable && d->canvas) {
		QString objectName = QStringLiteral("animationpropertiesdialog");
		dialogs::AnimationPropertiesDialog *dlg =
			findChild<dialogs::AnimationPropertiesDialog *>(
				objectName, Qt::FindDirectChildrenOnly);
		if(dlg) {
			dlg->activateWindow();
			dlg->raise();
		} else {
			canvas::DocumentMetadata *metadata = d->canvas->metadata();
			dlg = new dialogs::AnimationPropertiesDialog(
				metadata->framerate(), metadata->frameRangeFirst(),
				metadata->frameRangeLast(), d->canvas->isCompatibilityMode(),
				this);
			dlg->setObjectName(objectName);
			dlg->setAttribute(Qt::WA_DeleteOnClose);
			utils::showWindow(dlg);
			connect(
				dlg, &dialogs::AnimationPropertiesDialog::propertiesChanged,
				this, &TimelineWidget::setAnimationProperties);
		}
	}
}

void TimelineWidget::setAnimationProperties(
	double framerate, int frameRangeFirst, int frameRangeLast)
{
	if(d->editable && d->canvas) {
		bool compatibilityMode = d->canvas->isCompatibilityMode();
		const canvas::DocumentMetadata *metadata = d->canvas->metadata();

		bool framerateChanged =
			framerate != -1 && metadata->framerate() != framerate;
		bool frameRangeFirstChanged =
			!compatibilityMode && frameRangeFirst != -1 &&
			frameRangeFirst != metadata->frameRangeFirst();
		bool frameRangeLastChanged =
			!compatibilityMode && frameRangeLast != -1 &&
			frameRangeLast != metadata->frameRangeLast();

		int frameCount;
		if(compatibilityMode) {
			frameCount = frameRangeLast + 1;
		} else if(frameRangeLastChanged) {
			// Set the hard frame count limit to be 100 away from the last
			// frame to give the user some room at the end to spill into.
			int actualLastFrame = frameRangeLast;
			for(const canvas::TimelineTrack &track : d->getTracks()) {
				for(const canvas::TimelineKeyFrame &keyFrame :
					track.keyFrames) {
					int keyFrameIndex = keyFrame.frameIndex;
					if(actualLastFrame < keyFrameIndex) {
						actualLastFrame = keyFrameIndex;
					}
				}
			}

			int maxCount = std::numeric_limits<int32_t>::max();
			frameCount = actualLastFrame < maxCount - EXTRA_FRAME_COUNT
							 ? actualLastFrame + EXTRA_FRAME_COUNT
							 : maxCount;
		} else {
			frameCount = -1;
		}

		bool frameCountChanged =
			frameCount > 0 && frameCount != metadata->frameCount();
		int changes = (framerateChanged ? (compatibilityMode ? 1 : 2) : 0) +
					  (frameRangeFirstChanged ? 1 : 0) +
					  (frameRangeLastChanged ? 1 : 0) +
					  (frameCountChanged ? 1 : 0);
		if(changes != 0) {
			net::MessageList msgs;
			msgs.reserve(changes + 1);

			uint8_t contextId = d->canvas->localUserId();
			msgs.append(net::makeUndoPointMessage(contextId));

			if(framerateChanged) {
				if(compatibilityMode) {
					msgs.append(net::makeSetMetadataIntMessage(
						contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE,
						qMax(1, qRound(framerate))));
				} else {
					int whole, fraction;
					drawdance::DocumentMetadata::splitEffectiveFramerate(
						framerate, whole, fraction);
					msgs.append(net::makeSetMetadataIntMessage(
						contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE,
						whole));
					msgs.append(net::makeSetMetadataIntMessage(
						contextId,
						DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE_FRACTION,
						fraction));
				}
			}

			if(frameCountChanged) {
				msgs.append(net::makeSetMetadataIntMessage(
					contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT,
					frameCount));
			}

			if(frameRangeFirstChanged) {
				msgs.append(net::makeSetMetadataIntMessage(
					contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAME_RANGE_FIRST,
					frameRangeFirst));
			}

			if(frameRangeLastChanged) {
				msgs.append(net::makeSetMetadataIntMessage(
					contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAME_RANGE_LAST,
					frameRangeLast));
			}

			emit timelineEditCommands(msgs.size(), msgs.constData());
		}
	}
}

void TimelineWidget::nextFrame()
{
	int targetFrame = d->currentFrame + 1;
	setCurrentFrame(targetFrame < d->visibleFrameCount() ? targetFrame : 0);
}

void TimelineWidget::prevFrame()
{
	int targetFrame = d->currentFrame - 1;
	setCurrentFrame(
		targetFrame >= 0 ? targetFrame : d->visibleFrameCount() - 1);
}

void TimelineWidget::nextKeyFrame()
{
	const canvas::TimelineTrack *track = d->trackById(d->currentTrackId);
	if(track) {
		const QVector<canvas::TimelineKeyFrame> &keyFrames = track->keyFrames;
		int keyFrameCount = keyFrames.size();
		if(keyFrameCount != 0) {
			int bestFrameIndex = -1;
			for(int i = 0; i < keyFrameCount; ++i) {
				const canvas::TimelineKeyFrame &keyFrame = keyFrames[i];
				int f = keyFrame.frameIndex;
				if(f > d->currentFrame &&
				   (bestFrameIndex == -1 || f < bestFrameIndex)) {
					bestFrameIndex = f;
				}
			}
			setCurrentFrame(
				bestFrameIndex == -1 ? keyFrames[0].frameIndex
									 : bestFrameIndex);
		}
	}
}

void TimelineWidget::prevKeyFrame()
{
	const canvas::TimelineTrack *track = d->trackById(d->currentTrackId);
	if(track) {
		const QVector<canvas::TimelineKeyFrame> &keyFrames = track->keyFrames;
		int keyFrameCount = keyFrames.size();
		if(keyFrameCount != 0) {
			int bestFrameIndex = -1;
			for(int i = 0; i < keyFrameCount; ++i) {
				const canvas::TimelineKeyFrame &keyFrame = keyFrames[i];
				int f = keyFrame.frameIndex;
				if(f < d->currentFrame && f > bestFrameIndex) {
					bestFrameIndex = f;
				}
			}
			setCurrentFrame(
				bestFrameIndex == -1 ? keyFrames[keyFrameCount - 1].frameIndex
									 : bestFrameIndex);
		}
	}
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

	int maxTitleAdvance = 0;
	int nextTrackId = 0;
	int currentTrackId = 0;
	int anyTrackId = 0;
	for(const canvas::TimelineTrack &track : d->getTracks()) {
		int titleAdvance = fm.horizontalAdvance(track.title);
		if(titleAdvance > maxTitleAdvance) {
			maxTitleAdvance = titleAdvance;
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
	int effectiveTrackId = nextTrackId != 0		 ? nextTrackId
						   : currentTrackId != 0 ? currentTrackId
												 : anyTrackId;

	int nextCameraId = 0;
	int currentCameraId = 0;
	for(const canvas::TimelineCamera &camera : d->getCameras()) {
		int cameraId = camera.id();
		if(d->nextCameraId != 0 && cameraId == d->nextCameraId) {
			nextCameraId = d->nextCameraId;
			d->nextCameraId = 0;
		}
		if(d->currentCameraId == cameraId) {
			currentCameraId = d->currentCameraId;
		}
	}
	int effectiveCameraId = nextCameraId != 0	   ? nextCameraId
							: currentCameraId != 0 ? currentCameraId
												   : 0;
	d->currentCameraId = effectiveCameraId;

	QString cameraTitle;
	if(const canvas::TimelineCamera *camera = d->cameraById(effectiveCameraId);
	   camera) {
		cameraTitle = camera->title();
	} else {
		cameraTitle = noCameraTitle();
	}

	int titleAdvance = fm.horizontalAdvance(cameraTitle);
	if(titleAdvance > maxTitleAdvance) {
		maxTitleAdvance = titleAdvance;
	}

	d->headerWidth = maxTitleAdvance + TRACK_PADDING * 4 + ICON_SIZE * 2;
	setMinimumHeight(
		d->rowHeight * 3 + d->horizontalScroll->sizeHint().height());
	setCurrent(effectiveTrackId, d->currentFrame, false, false);
	updateActions();
	updateScrollbars();
	update();
}

void TimelineWidget::updateFrameCount()
{
	setCurrent(d->currentTrackId, d->currentFrame, false, false);
	updateFrameRange();
}

void TimelineWidget::updateFrameRange()
{
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

void TimelineWidget::showCameraMenu(const QPoint &pos)
{
	QMenu *cameraMenu = new QMenu(this);

	const QVector<canvas::TimelineCamera> &cameras = d->getCameras();
	if(!cameras.isEmpty()) {
		QActionGroup *cameraActionGroup = new QActionGroup(cameraMenu);

		QAction *noCameraAction = cameraMenu->addAction(noCameraTitle());
		noCameraAction->setCheckable(true);
		noCameraAction->setChecked(d->currentCameraId == 0);
		cameraActionGroup->addAction(noCameraAction);
		connect(
			noCameraAction, &QAction::triggered, this,
			std::bind(&TimelineWidget::setActiveCamera, this, 0));

		for(const canvas::TimelineCamera &camera : cameras) {
			QAction *cameraAction = cameraMenu->addAction(camera.title());
			cameraAction->setCheckable(true);
			int id = camera.id();
			cameraAction->setChecked(d->currentCameraId == id);
			cameraActionGroup->addAction(cameraAction);
			connect(
				cameraAction, &QAction::triggered, this,
				std::bind(&TimelineWidget::setActiveCamera, this, id));
		}

		cameraMenu->addSeparator();
	}

	cameraMenu->addAction(d->actions.cameraAdd);
	cameraMenu->addAction(d->actions.cameraDuplicate);
	cameraMenu->addAction(d->actions.cameraProperties);
	cameraMenu->addAction(d->actions.cameraDelete);

	cameraMenu->popup(mapToGlobal(pos));
	cameraMenu->setAttribute(Qt::WA_DeleteOnClose);
}

void TimelineWidget::setActiveCamera(int cameraId)
{
	d->nextCameraId = 0;
	if(cameraId != d->currentCameraId) {
		d->currentCameraId = cameraId;
		updateTracks();
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

	int actualFrame = qBound(0, frame, qMax(0, d->visibleFrameCount() - 1));
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
			int layerIdToSelect;
			if(d->guessLayerIdToSelect(layerIdToSelect)) {
				if(layerIdToSelect == 0) {
					emit blankLayerSelected();
				} else {
					emit layerSelected(layerIdToSelect);
				}
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
			QVector<uint32_t> layers;
			layers.reserve(layerVisibility.size());
			using LayersIt = QHash<int, bool>::const_iterator;
			const LayersIt end = layerVisibility.constEnd();
			for(LayersIt it = layerVisibility.constBegin(); it != end; ++it) {
				uint32_t id = it.key();
				uint32_t flags = it.value() ? DP_KEY_FRAME_LAYER_REVEALED
											: DP_KEY_FRAME_LAYER_HIDDEN;
				layers.append(
					(id & uint32_t(0xffffffu)) | (flags << uint32_t(24)));
			}
			messages[fill++] = net::makeKeyFrameLayerAttributesMessage(
				contextId, trackId, frame, layers);
		}
		emit timelineEditCommands(fill, messages);
	}
}

void TimelineWidget::changeFrameExposure(int direction, bool visible)
{
	Q_ASSERT(direction == -1 || direction == 1);
	if(!d->editable) {
		return;
	}

	QVector<QPair<int, QVector<int>>> trackFrameIndexes;
	bool forward = direction == 1;
	if(visible) {
		for(const canvas::TimelineTrack &track : d->getTracks()) {
			if(!track.hidden &&
			   !collectFrameExposure(trackFrameIndexes, forward, track.id)) {
				return;
			}
		}
	} else if(!collectFrameExposure(
				  trackFrameIndexes, forward, d->currentTrackId)) {
		return;
	}

	if(trackFrameIndexes.isEmpty()) {
		return;
	}

	compat::sizetype reserve = 1;
	for(const QPair<int, QVector<int>> &p : trackFrameIndexes) {
		reserve += p.second.size();
	}

	uint8_t contextId = d->canvas->localUserId();
	QVector<net::Message> messages;
	messages.reserve(reserve);
	messages.append(net::makeUndoPointMessage(contextId));
	for(QPair<int, QVector<int>> &p : trackFrameIndexes) {
		// Order these correctly so we don't move frames over each other.
		QVector<int> &frameIndexes = p.second;
		if(forward) {
			std::sort(frameIndexes.rbegin(), frameIndexes.rend());
		} else {
			std::sort(frameIndexes.begin(), frameIndexes.end());
		}

		int trackId = p.first;
		for(int frameIndex : frameIndexes) {
			messages.append(net::makeKeyFrameDeleteMessage(
				contextId, trackId, frameIndex, trackId,
				frameIndex + direction));
		}
	}
	emit timelineEditCommands(messages.size(), messages.constData());
}

bool TimelineWidget::collectFrameExposure(
	QVector<QPair<int, QVector<int>>> &trackFrameIndexes, bool forward,
	int trackId)
{
	Exposure exposure = d->currentExposureByTrackId(trackId);
	if(!exposure.isValid()) {
		return true;
	}

	// Blocking case: user is trying to increase exposure, but we're at the end
	// of the timeline. This operation would mess things up potentially far out
	// of view, so we don't allow it on any tracks in the set.
	if(forward && exposure.hasFrameAtEnd) {
		return false;
	}

	if(!exposure.canChangeInDirection(forward)) {
		return true;
	}

	QPair<int, QVector<int>> p = {trackId, {}};
	p.second = d->gatherCurrentExposureFramesByTrackId(trackId, exposure.start);
	if(!p.second.isEmpty()) {
		trackFrameIndexes.append(std::move(p));
	}

	return true;
}

void TimelineWidget::updateActions()
{
	if(!d->haveActions || !d->canvas) {
		return;
	}

	bool timelineEditable = d->editable;
	d->actions.animationProperties->setEnabled(timelineEditable);

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

	const canvas::TimelineCamera *camera = d->currentCamera();
	bool cameraEditable = timelineEditable && camera;
	d->actions.cameraAdd->setEnabled(timelineEditable);
	d->actions.cameraDuplicate->setEnabled(cameraEditable);
	d->actions.cameraProperties->setEnabled(cameraEditable);
	d->actions.cameraDelete->setEnabled(cameraEditable);

	int visibleFrameCount = d->visibleFrameCount();
	bool haveMultipleFrames = visibleFrameCount > 1;
	d->actions.frameNext->setEnabled(haveMultipleFrames);
	d->actions.framePrev->setEnabled(haveMultipleFrames);
	int keyFrameCount = track ? track->keyFrames.size() : 0;
	bool trackHasAnyKeyFrames = keyFrameCount != 0;
	bool isOnOnlyKeyFrame =
		keyFrameCount == 1 && track->keyFrames[0].frameIndex == d->currentFrame;
	bool nextPrevKeyFrame =
		haveMultipleFrames && trackHasAnyKeyFrames && !isOnOnlyKeyFrame;
	d->actions.keyFrameNext->setEnabled(nextPrevKeyFrame);
	d->actions.keyFramePrev->setEnabled(nextPrevKeyFrame);

	bool haveMultipleTracks = d->trackCount() > 1;
	d->actions.trackAbove->setEnabled(haveMultipleTracks);
	d->actions.trackBelow->setEnabled(haveMultipleTracks);

	bool keyFrameSettable = trackEditable && visibleFrameCount > 0;

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
	d->actions.animationKeyFrameColorMenu->setEnabled(keyFrameEditable);
	d->actions.animationKeyFrameColorMenu->setProperty(
		"markercolor", currentKeyFrame ? currentKeyFrame->color : QColor());
	for(QAction *ca : d->actions.animationKeyFrameColorMenu->actions()) {
		ca->setEnabled(keyFrameEditable);
	}
	d->actions.keyFrameProperties->setEnabled(keyFrameEditable);
	d->actions.keyFrameDelete->setEnabled(keyFrameEditable);

	bool canIncreaseExposure = false;
	bool canDecreaseExposure = false;
	if(timelineEditable && track) {
		Exposure exposure = d->currentExposure();
		canIncreaseExposure = exposure.canIncrease();
		canDecreaseExposure = exposure.canDecrease();
	}
	d->actions.keyFrameExposureIncrease->setEnabled(canIncreaseExposure);
	d->actions.keyFrameExposureDecrease->setEnabled(canDecreaseExposure);

	bool canIncreaseExposureVisible = false;
	bool canDecreaseExposureVisible = false;
	if(timelineEditable) {
		bool blockIncrease = false;
		for(const canvas::TimelineTrack &exposureTrack : d->getTracks()) {
			if(!exposureTrack.hidden) {
				Exposure exposure =
					d->currentExposureByTrackId(exposureTrack.id);
				if(exposure.isValid()) {
					if(!blockIncrease) {
						// Blocking case: user is trying to increase exposure,
						// but we're at the end of the timeline. This operation
						// would mess things up potentially far out of view, so
						// we don't allow it on any tracks in the set.
						if(exposure.hasFrameAtEnd) {
							canIncreaseExposureVisible = false;
							blockIncrease = true;
						} else if(
							!canIncreaseExposureVisible &&
							exposure.canIncrease()) {
							canIncreaseExposureVisible = true;
						}
					}

					if(!canDecreaseExposureVisible && exposure.canDecrease()) {
						canDecreaseExposureVisible = true;
					}
				}
			}
		}
	}
	d->actions.keyFrameExposureIncreaseVisible->setEnabled(
		canIncreaseExposureVisible);
	d->actions.keyFrameExposureDecreaseVisible->setEnabled(
		canDecreaseExposureVisible);

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
		0, (d->headerWidth + vsh.width() +
			d->visibleFrameCount(false) * d->columnWidth) -
			   width()));
	d->verticalScroll->setMaximum(qMax(
		0, (d->rowHeight + hsh.height() + d->trackCount() * d->rowHeight) -
			   height()));
}

TimelineWidget::Target TimelineWidget::getMouseTarget(const QPoint &pos) const
{
	Target target{-1, 0, -1, false, TargetHeader::None, TargetAction::None};
	if(d->canvas) {
		int x = pos.x();
		int headerWidth = d->headerWidth;
		if(x > headerWidth) {
			int frameIndex = (x - headerWidth + d->xScroll) / d->columnWidth;
			if(frameIndex >= 0 && frameIndex < d->visibleFrameCount()) {
				target.frameIndex = frameIndex;
			}
		}

		int y = pos.y();
		int rowHeight = d->rowHeight;
		bool haveCamera = d->currentCameraId != 0;
		int trackOffsetY = haveCamera ? rowHeight * 2 : rowHeight;
		if(y > trackOffsetY) {
			target.uiTrackIndex = (y - trackOffsetY + d->yScroll) / rowHeight;
			target.trackId = d->trackIdByUiIndex(target.uiTrackIndex);
			int tp = TRACK_PADDING;
			int tph = TRACK_PADDING / 2;
			int mid = tph + ICON_SIZE + tp;
			if(x >= tph && x < mid) {
				target.action = TargetAction::ToggleVisible;
			} else if(x >= mid && x < tp * 3 + ICON_SIZE * 2) {
				target.action = TargetAction::ToggleOnionSkin;
			}
		} else if(haveCamera && y > rowHeight) {
			target.onCamera = true;
			if(x < TRACK_PADDING + ICON_SIZE + TRACK_PADDING / 2) {
				target.action = TargetAction::ToggleCamera;
			} else if(x <= headerWidth) {
				target.action = TargetAction::CameraMenu;
			}
		} else {
			target.header = TargetHeader::Header;
			if(d->editable && !d->canvas->isCompatibilityMode()) {
				if(!haveCamera && x <= headerWidth && y <= rowHeight) {
					target.action = TargetAction::CameraMenu;
				} else {
					qreal frameX = qreal(
						(x - headerWidth + d->xScroll) -
						(target.frameIndex * d->columnWidth));
					qreal handleInsetX = d->rangeHandleInsetX();
					if(frameX <= handleInsetX &&
					   target.frameIndex == d->currentFrameRangeFirst()) {
						target.header = TargetHeader::RangeFirst;
					} else if(
						frameX >= qreal(d->columnWidth - 1) - handleInsetX &&
						target.frameIndex == d->currentFrameRangeLast()) {
						target.header = TargetHeader::RangeLast;
					}
				}
			}
		}
	}
	return target;
}

void TimelineWidget::applyMouseTarget(
	QMouseEvent *event, const Target &target, bool press)
{
	bool action = press && target.action != TargetAction::None;
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

void TimelineWidget::executeTargetAction(
	const Target &target, const QPoint &pos)
{
	switch(target.action) {
	case TargetAction::None:
		break;
	case TargetAction::ToggleVisible: {
		const canvas::TimelineTrack *track = d->trackById(target.trackId);
		if(track) {
			emit trackHidden(track->id, !track->hidden);
		}
		return;
	}
	case TargetAction::ToggleOnionSkin: {
		const canvas::TimelineTrack *track = d->trackById(target.trackId);
		if(track) {
			emit trackOnionSkinEnabled(track->id, !track->onionSkin);
		}
		return;
	}
	case TargetAction::ToggleCamera: {
		const canvas::TimelineCamera *camera = d->currentCamera();
		if(camera) {
			emit cameraHidden(camera->id(), !camera->hidden());
		}
		return;
	}
	case TargetAction::CameraMenu: {
		showCameraMenu(pos);
		return;
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

QString TimelineWidget::noCameraTitle()
{
	return tr("No camera");
}

}
