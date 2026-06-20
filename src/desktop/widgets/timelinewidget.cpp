// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/key_frame.h>
}
#include "desktop/dialogs/animationpropertiesdialog.h"
#include "desktop/dialogs/keyframepropertiesdialog.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/net/message.h"
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDrag>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPair>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QtMath>
#include <algorithm>

namespace widgets {

enum class TimelineTool { Normal, Select, Exposure };
enum class TrackAction { None, ToggleVisible, ToggleOnionSkin, ToggleMoveLock };
enum class TargetHeader { None, Header, RangeFirst, RangeLast };
enum class TargetSide { None, Left, Right };

namespace {

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

struct ExposureToolState {
	int fromUiTrackIndex = -1;
	int fromFrameIndex = -1;
	int toUiTrackIndex = -1;
	int toFrameIndex = -1;
	int offset = 0;
	int leftIndex = -1;
	int topIndex = -1;
	int rightIndex = -1;
	int bottomIndex = -1;
	bool active = false;
	bool valid = false;

	void update(const QVector<canvas::TimelineTrack> &tracks, int frameCount)
	{
		int trackCount = tracks.size();
		if(fromUiTrackIndex == -1) {
			topIndex = -1;
			bottomIndex = tracks.size() - 1;
		} else {
			if(fromUiTrackIndex <= toUiTrackIndex) {
				topIndex = fromUiTrackIndex;
				bottomIndex = toUiTrackIndex;
			} else {
				topIndex = toUiTrackIndex;
				bottomIndex = fromUiTrackIndex;
			}

			if(topIndex >= trackCount) {
				topIndex = -1;
				bottomIndex = -1;
			} else if(bottomIndex >= trackCount) {
				bottomIndex = trackCount - 1;
			}
		}

		offset = toFrameIndex - fromFrameIndex;
		for(int i = qMax(0, topIndex); offset != 0 && i <= bottomIndex; ++i) {
			const canvas::TimelineTrack &track = tracks[trackCount - i - 1];
			if(!track.moveLock) {
				if(offset < 0) {
					clampOffsetDecrease(track);
				} else {
					clampOffsetIncrease(track, frameCount);
				}
			}
		}

		if(offset >= 0) {
			leftIndex = fromFrameIndex;
			rightIndex = fromFrameIndex + offset;
		} else {
			rightIndex = fromFrameIndex;
			leftIndex = fromFrameIndex + offset;
		}
	}

	void clampOffsetIncrease(const canvas::TimelineTrack &track, int frameCount)
	{
		const QVector<canvas::TimelineKeyFrame> &kfs = track.keyFrames;
		if(!kfs.isEmpty()) {
			int frameIndex = kfs.constLast().frameIndex;
			if(frameIndex >= fromFrameIndex) {
				int maxOffset = frameCount - frameIndex - 1;
				if(offset > maxOffset) {
					offset = maxOffset;
				}
			}
		}
	}

	void clampOffsetDecrease(const canvas::TimelineTrack &track)
	{
		int frameIndexBefore = -1;
		for(const canvas::TimelineKeyFrame &kf : track.keyFrames) {
			int frameIndex = kf.frameIndex;
			if(frameIndex < fromFrameIndex) {
				frameIndexBefore = frameIndex;
			} else {
				break;
			}
		}
		int minOffset = frameIndexBefore - fromFrameIndex + 1;
		if(offset < minOffset) {
			offset = minOffset;
		}
	}
};

struct SelectionKeyFrame {
	int trackId;
	int trackIndex;
	int frameIndex;
	int layerId;
};

class SelectionState {
public:
	const QVector<SelectionKeyFrame> &keyFrames() const { return m_keyFrames; }

	int keyFrameCount() const { return m_keyFrames.size(); }

	QVector<SelectionKeyFrame>
	sortedKeyFrames(int deltaTrackIndex, int deltaFrameIndex) const
	{
		QVector<SelectionKeyFrame> skfs = m_keyFrames;
		std::sort(
			skfs.begin(), skfs.end(),
			[up = deltaTrackIndex < 0, left = deltaFrameIndex < 0](
				const SelectionKeyFrame &a, const SelectionKeyFrame &b) {
				if(a.trackIndex < b.trackIndex) {
					return up;
				} else if(a.trackIndex > b.trackIndex) {
					return !up;
				} else if(left) {
					return a.frameIndex < b.frameIndex;
				} else {
					return a.frameIndex > b.frameIndex;
				}
			});
		return skfs;
	}

	bool isAnyLayerIdsSelected() const
	{
		for(const SelectionKeyFrame &skf : m_keyFrames) {
			if(skf.layerId > 0) {
				return true;
			}
		}
		return false;
	}

	QSet<int> selectedLayerIds() const
	{
		QSet<int> layerIds;
		for(const SelectionKeyFrame &skf : m_keyFrames) {
			if(skf.layerId > 0) {
				layerIds.insert(skf.layerId);
			}
		}
		return layerIds;
	}

	QSet<QPair<int, int>> selectedTrackIdKeyFrameIndexPairs() const
	{
		QSet<QPair<int, int>> pairs;
		for(const SelectionKeyFrame &skf : m_keyFrames) {
			pairs.insert({skf.trackId, skf.frameIndex});
		}
		return pairs;
	}

	void addKeyFrame(
		int trackId, int trackIndex, int frameIndex, int layerId,
		bool trackMoveLock)
	{
		m_keyFrames.append({trackId, trackIndex, frameIndex, layerId});
		if(trackMoveLock) {
			m_anyKeyFrameLocked = true;
		} else {
			m_anyKeyFrameUnlocked = true;
		}
	}

	void clear()
	{
		m_keyFrames.clear();
		m_anyKeyFrameUnlocked = false;
		m_anyKeyFrameLocked = false;
	}

private:
	QVector<SelectionKeyFrame> m_keyFrames;
	bool m_anyKeyFrameUnlocked = false;
	bool m_anyKeyFrameLocked = false;
};

struct PasteKeyFrame {
	int trackId;
	int frameIndex;
	int layerId;
	int createLayerId;
	QString title;
	QString createLayerName;
	QHash<int, bool> layerVisibility;
};

}

struct TimelineWidget::Target {
	int uiTrackIndex = -1;
	int trackId = 0;
	int frameIndex = -1;
	int rawFrameIndex = -1;
	TargetHeader header = TargetHeader::None;
	TrackAction action = TrackAction::None;
	TargetSide side = TargetSide::None;
};

struct TimelineWidget::Private {
	TimelineWidget *const q;
	canvas::CanvasModel *canvas = nullptr;
	canvas::TimelineItemModel *itemModel;
	QItemSelectionModel *itemSelectionModel;
	SelectionState selectionState;

	bool selectionUpdatesBlocked = false;
	bool haveActions = false;
	bool selectionStateValid = false;
	Actions actions;
	QWidget *trackActionsWidget = nullptr;
	QMenu *trackMenu = nullptr;
	QMenu *frameMenu = nullptr;

	QIcon visibleIcon;
	QIcon hiddenIcon;
	QIcon onionSkinOnIcon;
	QIcon onionSkinOffIcon;
	QIcon moveLockedIcon;
	QIcon moveUnlockedIcon;
	QIcon uselessIcon;
	int headerWidth = 64;
	int rowHeight = 10;
	int baseColumnWidth = 10;
	int columnWidth = baseColumnWidth;
	int xScroll = 0;
	int yScroll = 0;
	int currentTrackId = 0;
	int currentFrame = 0;
	int nextTrackId = 0;
	int zoomAdjust = 0;
	TimelineTool currentTool = TimelineTool::Normal;
	Target hoverTarget;
	TargetHeader pressedHeader = TargetHeader::None;
	bool editable = false;
	Qt::KeyboardModifiers modifiersDown;
	bool lastPosValid = false;
	bool zoomInProgress = false;
	Drag drag = Drag::None;
	Drag dragHover = Drag::None;
	QVector<TimelineWidget::KeyFrameDrag> dragKeyFrames;
	QSet<QPair<int, int>> dragKeyFrameIndexes;
	Qt::DropAction dropAction;
	SelectionAction moveSelectionAction = SelectionAction::ReplaceMove;
	PendingSelectionAction pendingSelectionAction =
		PendingSelectionAction::None;
	QModelIndex selectionPressIndex;
	QModelIndex selectionRangeStartIndex;
	QPoint dragOrigin;
	QPoint dragPos;
	QPoint lastPos;
	ExposureToolState exposureTool;
	QDeadlineTimer frameViewRequestTimer;

	int selectedLayerId = 0;
	QHash<QPair<int, int>, int> layerIdByKeyFrame;

	QScrollBar *verticalScroll = nullptr;
	QScrollBar *horizontalScroll = nullptr;

	Private(TimelineWidget *widget)
		: q(widget)
		, itemModel(new canvas::TimelineItemModel(q))
		, itemSelectionModel(new QItemSelectionModel(itemModel, q))
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

	int frameCount() const
	{
		return canvas ? canvas->metadata()->frameCount() : 0;
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

	int frameRangeFirst() const
	{
		if(canvas) {
			return canvas->metadata()->frameRangeFirst();
		} else {
			return 0;
		}
	}

	int frameRangeLast() const
	{
		if(canvas) {
			return canvas->metadata()->frameRangeLast();
		} else {
			return 0;
		}
	}

	bool isInRange(int frameIndex)
	{
		return frameIndex >= frameRangeFirst() &&
			   frameIndex <= frameRangeLast();
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

	bool trackMoveLockedById(int trackId) const
	{
		const canvas::TimelineTrack *track = trackById(trackId);
		return track && track->moveLock;
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

	QVector<int> unlockedTrackIds()
	{
		const QVector<canvas::TimelineTrack> &tracks = getTracks();
		QVector<int> trackIds;
		trackIds.reserve(tracks.size());
		for(const canvas::TimelineTrack &track : tracks) {
			if(!track.moveLock) {
				trackIds.append(track.id);
			}
		}
		return trackIds;
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

	bool updateColumnWidth()
	{
		zoomAdjust = qBound(
			-baseColumnWidth + TimelineWidget::MIN_COLUMN_WIDTH, zoomAdjust,
			TimelineWidget::MAX_COLUMN_WIDTH - baseColumnWidth);
		int newColumnWidth = baseColumnWidth + zoomAdjust;

		if(haveActions) {
			actions.timelineZoomIn->setEnabled(
				newColumnWidth < TimelineWidget::MAX_COLUMN_WIDTH);
			actions.timelineZoomOut->setEnabled(
				newColumnWidth > TimelineWidget::MIN_COLUMN_WIDTH);
		}

		if(newColumnWidth == columnWidth) {
			return false;
		} else {
			columnWidth = newColumnWidth;
			return true;
		}
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

	QRect trackSidebarRect() const
	{
		return QRect{0, rowHeight, headerWidth, bodyHeight() + 1};
	}

	qreal rangeHandleInsetX() const { return rowHeight / 4.0; }

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

	const QIcon &getMoveLockIcon(const canvas::TimelineTrack &track)
	{
		return track.moveLock ? moveLockedIcon : moveUnlockedIcon;
	}

	void setModifiers(Qt::KeyboardModifiers modifiers)
	{
		bool wasExposureTool = isCurrentExposureTool();
		modifiersDown = modifiers;
		if(isCurrentExposureTool() != wasExposureTool) {
			q->updateCursor();
			q->update();
		}
	}

	bool updateModifierState(const QKeyEvent *event, bool press)
	{
		// QKeyEvent::modifiers is unreliable on some platforms, so we use use
		// the keys straight and hope that's close enough. AltGr is not included
		// because it doesn't seem to behave consistently.
		Qt::KeyboardModifier modifier;
		switch(event->key()) {
		case Qt::Key_Shift:
			modifier = Qt::ShiftModifier;
			break;
		case Qt::Key_Control:
			modifier = Qt::ControlModifier;
			break;
		case Qt::Key_Meta:
			modifier = Qt::MetaModifier;
			break;
		case Qt::Key_Alt:
			modifier = Qt::AltModifier;
			break;
		default:
			return false;
		}
		Qt::KeyboardModifiers modifiers = modifiersDown;
		modifiers.setFlag(modifier, press);
		setModifiers(modifiers);
		return true;
	}

	void updateMouseState(const QMouseEvent *event)
	{
		lastPosValid = true;
		lastPos = compat::mousePos(*event);
		setModifiers(event->modifiers());
	}

	void updateWheelState(const QWheelEvent *event)
	{
		lastPosValid = true;
		lastPos = compat::wheelPos(*event);
		setModifiers(event->modifiers());
	}

	bool isCurrentExposureTool() const
	{
		if(currentTool == TimelineTool::Exposure) {
			return modifiersDown != Qt::AltModifier;
		} else {
			return modifiersDown == Qt::AltModifier;
		}
	}

	bool isCurrentSelectTool() const
	{
		return currentTool == TimelineTool::Select;
	}

	void updateExposureToolState(const QVector<canvas::TimelineTrack> &tracks)
	{
		if(exposureTool.active && !exposureTool.valid) {
			exposureTool.update(tracks, frameCount());
		}
	}

	const SelectionState &getSelectionState()
	{
		if(!selectionStateValid) {
			selectionStateValid = true;
			selectionState.clear();
			if(canvas) {
				const QVector<canvas::TimelineTrack> &tracks = getTracks();
				int trackCount = tracks.size();
				for(int i = 0; i < trackCount; ++i) {
					const canvas::TimelineTrack &track = tracks[i];
					for(const canvas::TimelineKeyFrame &kf : track.keyFrames) {
						QModelIndex idx = itemModel->index(i, kf.frameIndex);
						if(itemSelectionModel->isSelected(idx)) {
							selectionState.addKeyFrame(
								track.id, i, kf.frameIndex, kf.layerId,
								track.moveLock);
						}
					}
				}
			}
		}
		return selectionState;
	}

	bool isCurrentKeyFrameSelected()
	{
		for(const SelectionKeyFrame &skf : getSelectionState().keyFrames()) {
			if(skf.trackId == currentTrackId &&
			   skf.frameIndex == currentFrame) {
				return true;
			}
		}
		return false;
	}

	QHash<int, int> collectLayerIdReferenceCounts(
		const QSet<QPair<int, int>> *clobbered = nullptr) const
	{
		QHash<int, int> layerIdReferenceCounts;
		for(const canvas::TimelineTrack &track : getTracks()) {
			int trackId = track.id;
			for(const canvas::TimelineKeyFrame &keyFrame : track.keyFrames) {
				int layerId = keyFrame.layerId;
				if(layerId > 0 &&
				   (!clobbered ||
					!clobbered->contains({trackId, keyFrame.frameIndex}))) {
					++layerIdReferenceCounts[layerId];
				}
			}
		}
		return layerIdReferenceCounts;
	}

	net::Message makeKeyFrameLayerAttributesMessage(
		uint8_t contextId, int trackId, int frameIndex,
		const QHash<int, bool> layerVisibility,
		const QHash<int, int> *layerIdMap = nullptr)
	{
		QVector<uint32_t> layers;
		layers.reserve(layerVisibility.size());

		for(QHash<int, bool>::const_iterator it = layerVisibility.constBegin(),
											 end = layerVisibility.constEnd();
			it != end; ++it) {

			int layerId = it.key();
			if(layerIdMap) {
				QHash<int, int>::const_iterator layerIdIt =
					layerIdMap->constFind(layerId);
				if(layerIdIt != layerIdMap->constEnd()) {
					layerId = layerIdIt.value();
				}
			}

			uint32_t id = layerId;
			uint32_t flags = it.value() ? DP_KEY_FRAME_LAYER_REVEALED
										: DP_KEY_FRAME_LAYER_HIDDEN;
			layers.append((id & uint32_t(0xffffffu)) | (flags << uint32_t(24)));
		}

		return net::makeKeyFrameLayerAttributesMessage(
			contextId, trackId, frameIndex, layers);
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
	d->moveLockedIcon =
		QIcon::fromTheme(QStringLiteral("drawpile_keyframe_movelocked"));
	d->moveUnlockedIcon =
		QIcon::fromTheme(QStringLiteral("drawpile_keyframe_moveunlocked"));
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
	connect(
		d->itemSelectionModel, &QItemSelectionModel::selectionChanged, this,
		&TimelineWidget::onSelectionChanged, Qt::DirectConnection);
	updateScrollbars();
}

TimelineWidget::~TimelineWidget()
{
	delete d;
}

void TimelineWidget::setCanvas(canvas::CanvasModel *canvas)
{
	d->canvas = canvas;
	d->selectionStateValid = false;
	connect(
		canvas->timeline(), &canvas::TimelineModel::tracksChanged, this,
		&TimelineWidget::updateTracks);
	connect(
		canvas->timeline(), &canvas::TimelineModel::restoreSelectedTrackId,
		this, &TimelineWidget::setCurrentTrack);
	connect(
		canvas->timeline(), &canvas::TimelineModel::restoreSelectedFrameIndex,
		this, &TimelineWidget::setCurrentFrame);
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
	actions.keyFramePaste->setEnabled(false);
	actions.keyFrameDeclone->setEnabled(false);

	d->trackActionsWidget = new QWidget(this);
	d->trackActionsWidget->setContentsMargins(0, 0, 0, 0);
	d->trackActionsWidget->setCursor(Qt::ArrowCursor);

	QHBoxLayout *trackActionsLayout = new QHBoxLayout(d->trackActionsWidget);
	trackActionsLayout->setContentsMargins(0, 0, 0, 0);
	trackActionsLayout->setSpacing(0);

	widgets::GroupedToolButton *trackAddButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	trackAddButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	trackAddButton->setAutoRaise(true);
	trackAddButton->setDefaultAction(actions.trackAdd);
	trackActionsLayout->addWidget(trackAddButton);

	widgets::GroupedToolButton *trackDuplicateButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	trackDuplicateButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	trackDuplicateButton->setAutoRaise(true);
	trackDuplicateButton->setDefaultAction(actions.trackDuplicate);
	trackActionsLayout->addWidget(trackDuplicateButton);

	widgets::GroupedToolButton *trackRetitleButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	trackRetitleButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	trackRetitleButton->setAutoRaise(true);
	trackRetitleButton->setDefaultAction(actions.trackRetitle);
	trackActionsLayout->addWidget(trackRetitleButton);

	widgets::GroupedToolButton *trackDeleteButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	trackDeleteButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	trackDeleteButton->setAutoRaise(true);
	trackDeleteButton->setDefaultAction(actions.trackDelete);
	trackActionsLayout->addWidget(trackDeleteButton);

	d->trackActionsWidget->setGeometry(0, 0, d->headerWidth, d->rowHeight);

	d->trackMenu = new QMenu{this};
	d->frameMenu = new QMenu{this};
	d->frameMenu->addAction(actions.keyFrameSetLayer);
	d->frameMenu->addAction(actions.keyFrameSetEmpty);
	d->frameMenu->addAction(actions.keyFrameCut);
	d->frameMenu->addAction(actions.keyFrameCopy);
	d->frameMenu->addAction(actions.keyFramePaste);
	d->frameMenu->addAction(actions.keyFramePasteDeclone);
	d->frameMenu->addMenu(actions.animationKeyFrameColorMenu);
	d->frameMenu->addAction(actions.keyFrameProperties);
	d->frameMenu->addAction(actions.keyFrameDeleteLayer);
	d->frameMenu->addAction(actions.keyFrameUnassign);
	d->frameMenu->addAction(actions.keyFrameDeclone);
	d->frameMenu->addSeparator();
	d->frameMenu->addAction(actions.keyFrameExposureIncrease);
	d->frameMenu->addAction(actions.keyFrameExposureIncreaseAll);
	d->frameMenu->addAction(actions.keyFrameExposureDecrease);
	d->frameMenu->addAction(actions.keyFrameExposureDecreaseAll);
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
		menu->addAction(actions.trackMoveLock);
		menu->addSeparator();
		menu->addAction(actions.frameNext);
		menu->addAction(actions.framePrev);
		menu->addAction(actions.frameNextClamp);
		menu->addAction(actions.framePrevClamp);
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
		&TimelineWidget::setKeyFramesLayer);
	connect(
		actions.keyFrameSetEmpty, &QAction::triggered, this,
		&TimelineWidget::setKeyFramesEmpty);
	connect(
		actions.keyFrameCut, &QAction::triggered, this,
		&TimelineWidget::cutKeyFrames);
	connect(
		actions.keyFrameCopy, &QAction::triggered, this,
		&TimelineWidget::copyKeyFrames);
	connect(
		actions.keyFramePaste, &QAction::triggered, this,
		&TimelineWidget::pasteKeyFrames);
	connect(
		actions.keyFramePasteDeclone, &QAction::triggered, this,
		&TimelineWidget::pasteKeyFramesDeclone);
	connect(
		actions.keyFrameProperties, &QAction::triggered, this,
		&TimelineWidget::showKeyFrameProperties);
	connect(
		actions.animationKeyFrameColorMenu, &QMenu::triggered, this,
		&TimelineWidget::setKeyFramesColor);
	connect(
		actions.keyFrameDeleteLayer, &QAction::triggered, this,
		&TimelineWidget::deleteKeyFramesLayers);
	connect(
		actions.keyFrameUnassign, &QAction::triggered, this,
		&TimelineWidget::unassignKeyFrames);
	connect(
		actions.keyFrameDeclone, &QAction::triggered, this,
		&TimelineWidget::decloneKeyFrames);
	connect(
		actions.keyFrameExposureIncrease, &QAction::triggered, this,
		&TimelineWidget::increaseKeyFrameExposure);
	connect(
		actions.keyFrameExposureIncreaseAll, &QAction::triggered, this,
		&TimelineWidget::increaseKeyFrameExposureAll);
	connect(
		actions.keyFrameExposureDecrease, &QAction::triggered, this,
		&TimelineWidget::decreaseKeyFrameExposure);
	connect(
		actions.keyFrameExposureDecreaseAll, &QAction::triggered, this,
		&TimelineWidget::decreaseKeyFrameExposureAll);
	connect(
		actions.trackAdd, &QAction::triggered, this, &TimelineWidget::addTrack);
	connect(
		actions.trackVisible, &QAction::triggered, this,
		&TimelineWidget::toggleTrackVisible);
	connect(
		actions.trackOnionSkin, &QAction::triggered, this,
		&TimelineWidget::toggleTrackOnionSkin);
	connect(
		actions.trackMoveLock, &QAction::triggered, this,
		&TimelineWidget::toggleTrackMoveLock);
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
		actions.animationProperties, &QAction::triggered, this,
		&TimelineWidget::showAnimationProperties);
	connect(
		actions.frameNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.framePrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);
	connect(
		actions.frameNextClamp, &QAction::triggered, this,
		&TimelineWidget::nextFrameClamp);
	connect(
		actions.framePrevClamp, &QAction::triggered, this,
		&TimelineWidget::prevFrameClamp);
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
		actions.keyFrameCreateLayer, &QAction::triggered, this,
		&TimelineWidget::startFrameViewRequestTimer);
	connect(
		actions.keyFrameCreateLayerNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.keyFrameCreateLayerNext, &QAction::triggered, this,
		&TimelineWidget::startFrameViewRequestTimer);
	connect(
		actions.keyFrameCreateLayerPrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);
	connect(
		actions.keyFrameCreateLayerPrev, &QAction::triggered, this,
		&TimelineWidget::startFrameViewRequestTimer);
	connect(
		actions.keyFrameCreateGroupNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.keyFrameCreateGroupNext, &QAction::triggered, this,
		&TimelineWidget::startFrameViewRequestTimer);
	connect(
		actions.keyFrameCreateGroupPrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);
	connect(
		actions.keyFrameCreateGroupPrev, &QAction::triggered, this,
		&TimelineWidget::startFrameViewRequestTimer);
	connect(
		actions.keyFrameDuplicateNext, &QAction::triggered, this,
		&TimelineWidget::nextFrame);
	connect(
		actions.keyFrameDuplicateNext, &QAction::triggered, this,
		&TimelineWidget::startFrameViewRequestTimer);
	connect(
		actions.keyFrameDuplicatePrev, &QAction::triggered, this,
		&TimelineWidget::prevFrame);
	connect(
		actions.keyFrameDuplicatePrev, &QAction::triggered, this,
		&TimelineWidget::startFrameViewRequestTimer);
	connect(
		actions.timelineToolGroup, &QActionGroup::triggered, this,
		&TimelineWidget::switchTool);
	connect(
		actions.timelineZoomIn, &QAction::triggered, this,
		&TimelineWidget::zoomIn);
	connect(
		actions.timelineZoomOut, &QAction::triggered, this,
		&TimelineWidget::zoomOut);
	connect(
		actions.timelineZoomReset, &QAction::triggered, this,
		&TimelineWidget::resetZoom);

	updateActions();
}

void TimelineWidget::setCurrentFrame(int frame)
{
	setCurrent(d->currentTrackId, frame, true, true, SelectionAction::Replace);
}

void TimelineWidget::setCurrentTrack(int trackId)
{
	setCurrent(trackId, d->currentFrame, true, true, SelectionAction::Replace);
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

QVector<TimelineWidget::SelectedFrame>
TimelineWidget::selectedNonKeyFrames() const
{
	const SelectionState selectionState = d->getSelectionState();
	QModelIndexList selectedIndexes = d->itemSelectionModel->selectedIndexes();
	int selectedIndexCount = selectedIndexes.size();
	int selectedNonKeyFrameCount =
		selectedIndexCount - selectionState.keyFrameCount();

	QVector<SelectedFrame> sfs;
	if(selectedNonKeyFrameCount > 0) {
		sfs.reserve(selectedNonKeyFrameCount);

		int visibleFrameCount = d->visibleFrameCount();
		QSet<QPair<int, int>> selectedKeyFrameSet =
			d->getSelectionState().selectedTrackIdKeyFrameIndexPairs();

		for(const QModelIndex &idx : selectedIndexes) {
			int trackId = d->trackIdByIndex(idx.row());
			int frameIndex = idx.column();
			if(trackId != 0 && frameIndex >= 0 &&
			   frameIndex < visibleFrameCount &&
			   !selectedKeyFrameSet.contains({trackId, frameIndex})) {
				sfs.append({trackId, frameIndex});
			}
		}
	}

	return sfs;
}

int TimelineWidget::columnWidth() const
{
	return d->columnWidth;
}

void TimelineWidget::setColumnWidth(int columnWidth)
{
	if(!d->zoomInProgress && columnWidth != d->columnWidth) {
		d->zoomAdjust = columnWidth - d->baseColumnWidth;
		if(d->updateColumnWidth()) {
			updateScrollbars();
			update();
			QScopedValueRollback<bool> rollback(d->zoomInProgress, true);
			Q_EMIT columnWidthChanged(d->columnWidth);
		}
	}
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
			tip = tr("Toggle visibility for you");
		} else if(d->hoverTarget.action == TrackAction::ToggleOnionSkin) {
			tip = tr("Toggle onion skin for you");
		} else if(d->hoverTarget.action == TrackAction::ToggleMoveLock) {
			tip = tr("Toggle frame move lock for you");
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
	d->updateExposureToolState(tracks);
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

	int frameRangeFirst = d->frameRangeFirst();
	int frameRangeLast = d->frameRangeLast();
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

	// Selected rows and columns.
	int bodyVisibleRight = qMin(
		bodyRect.right(),
		headerWidth + visibleFrameCount * columnWidth - xScroll);
	if(trackCount != 0 && !d->isCurrentExposureTool()) {
		painter.setBrush(pal.highlight());
		painter.setOpacity(0.5);

		if(currentFrame != -1) {
			int x = headerWidth + currentFrame * columnWidth - xScroll;
			painter.drawRect(x, bodyRect.top(), columnWidth, bodyRect.height());
		}

		if(currentTrackIndex != -1) {
			int i = trackCount - currentTrackIndex - 1;
			int y = rowHeight + i * rowHeight - yScroll;
			painter.drawRect(
				bodyRect.left(), y, bodyVisibleRight - bodyRect.left() + 1,
				rowHeight);
		}

		for(const QModelIndex &idx : d->itemSelectionModel->selectedIndexes()) {
			int x = headerWidth + idx.column() * columnWidth - xScroll;
			int y =
				rowHeight + (trackCount - idx.row() - 1) * rowHeight - yScroll;
			painter.drawRect(x, y, columnWidth, rowHeight);
		}

		painter.setOpacity(1.0);
	}

	// Key frames.
	const canvas::TimelineKeyFrame *currentVisibleKeyFrame =
		d->currentVisibleKeyFrame();
	for(int i = 0; i < trackCount; ++i) {
		int y = rowHeight + i * rowHeight - yScroll;
		int trackIndex = trackCount - i - 1;
		const canvas::TimelineTrack &track = tracks[trackIndex];
		bool exposureTrackActive =
			d->exposureTool.active && i >= d->exposureTool.topIndex &&
			i <= d->exposureTool.bottomIndex && !track.moveLock;
		painter.setOpacity(track.hidden ? 0.5 : 1.0);
		const QVector<canvas::TimelineKeyFrame> &keyFrames = track.keyFrames;
		int keyFrameCount = keyFrames.size();
		for(int j = 0; j < keyFrameCount; ++j) {
			const canvas::TimelineKeyFrame &keyFrame = keyFrames[j];
			int frame = keyFrame.frameIndex;
			int offsetFrame;
			if(exposureTrackActive && frame >= d->exposureTool.leftIndex) {
				offsetFrame = frame + d->exposureTool.offset;
			} else {
				offsetFrame = frame;
			}

			int x = headerWidth + offsetFrame * columnWidth - xScroll;
			bool isSelected = d->itemSelectionModel->isSelected(
				d->itemModel->index(trackIndex, frame));
			QBrush brush;
			if(keyFrame.color.isValid()) {
				brush = isSelected ? keyFrame.color.lighter() : keyFrame.color;
			} else {
				brush = isSelected ? pal.highlightedText() : pal.windowText();
			}

			if(d->dragHover == Drag::KeyFrame &&
			   d->dragKeyFrameIndexes.contains({trackIndex, frame})) {
				brush.setStyle(Qt::Dense6Pattern);
			} else if(keyFrame.layerId == 0) {
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
		int firstX = headerWidth + columnWidth - (xScroll % columnWidth);
		for(int x = firstX; x <= bodyVisibleRight; x += columnWidth) {
			painter.drawLine(x, bodyRect.top(), x, bodyRect.bottom());
		}
		int firstY = rowHeight - (yScroll % rowHeight);
		for(int y = firstY; y < bodyRect.height(); y += rowHeight) {
			int yh = y + rowHeight;
			painter.drawLine(bodyRect.left(), yh, bodyVisibleRight, yh);
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

	int selectedFrameX = headerWidth + d->currentFrame * columnWidth - xScroll;
	painter.setPen(Qt::NoPen);
	painter.setBrush(pal.highlight());
	painter.drawRect(selectedFrameX + 1, 1, columnWidth - 1, rowHeight - 1);

	int frameNumberSkip =
		qMax(1, qCeil(qreal(d->baseColumnWidth) / qreal(d->columnWidth)));
	bool nextInRange = 0 >= frameRangeFirst && 0 <= frameRangeLast;
	for(int i = 0; i < visibleFrameCount; i += frameNumberSkip) {
		bool currentInRange = nextInRange;
		nextInRange = (i + 1) >= frameRangeFirst && (i + 1) <= frameRangeLast;

		int x = headerWidth + i * columnWidth - xScroll;
		int frameColumnWidth = columnWidth * frameNumberSkip;
		bool isSelected = d->currentFrame == i;
		painter.setPen(isSelected ? highlightedTextColor : textColor);
		painter.drawText(
			x, 0, d->baseColumnWidth, rowHeight,
			Qt::AlignCenter | Qt::AlignVCenter, QString::number(i + 1));

		if(currentInRange || nextInRange) {
			int frameLineX = x + frameColumnWidth;
			painter.setPen(outlineColor);
			painter.drawLine(frameLineX, 0, frameLineX, rowHeight);
		}
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
		qreal moveLockOpacity = opacity * (track.moveLock ? 1.0 : 0.3);
		painter.setPen(isSelected ? highlightedTextColor : textColor);
		painter.setOpacity(opacity);
		d->getVisibilityIcon(track).paint(
			&painter, x, yIcon, ICON_SIZE, ICON_SIZE);

		x += TRACK_PADDING + ICON_SIZE;
		painter.setOpacity(onionSkinOpacity);
		d->getOnionSkinIcon(track).paint(
			&painter, x, yIcon, ICON_SIZE, ICON_SIZE);

		x += TRACK_PADDING + ICON_SIZE;
		painter.setOpacity(moveLockOpacity);
		d->getMoveLockIcon(track).paint(
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

	if(d->dragHover == Drag::Track) {
		painter.setClipRect(QRect{}, Qt::NoClip);
		painter.setPen(textColor);
		int y = d->trackDropIndex(d->dragPos.y()) * rowHeight + rowHeight -
				d->yScroll;
		painter.drawLine(0, y, headerWidth, y);

	} else if(d->dragHover == Drag::KeyFrame) {
		if(!d->dragKeyFrames.isEmpty()) {
			painter.setClipRect(bodyRect);
			QBrush brush = pal.highlightedText();
			painter.setPen(QPen(brush, 1.0));
			brush.setStyle(Qt::Dense7Pattern);
			painter.setBrush(brush);
			for(const KeyFrameDrag &kfd : d->dragKeyFrames) {
				int x = headerWidth + kfd.toFrameIndex * columnWidth - xScroll;
				int y = rowHeight +
						(trackCount - kfd.toTrackIndex - 1) * rowHeight -
						yScroll;
				painter.drawRect(x, y, columnWidth, rowHeight);
			}
		}

	} else if(const ExposureToolState &e = d->exposureTool; e.active) {
		if(e.topIndex <= e.bottomIndex) {
			int exposureLeft =
				headerWidth + e.leftIndex * columnWidth - xScroll;
			int exposureRight =
				headerWidth + e.rightIndex * columnWidth - xScroll;
			int exposureTop = rowHeight + e.topIndex * rowHeight - yScroll;
			int exposureBottom =
				e.topIndex < 0
					? bodyRect.bottom()
					: 2 * rowHeight + e.bottomIndex * rowHeight - yScroll;

			painter.setClipRect(QRect{}, Qt::NoClip);
			painter.setBrush(Qt::NoBrush);
			painter.setPen(QPen(pal.highlightedText(), 1));
			painter.drawLine(
				exposureLeft, exposureTop, exposureLeft, exposureBottom);
			if(e.offset != 0) {
				painter.drawLine(
					exposureRight, exposureTop, exposureRight, exposureBottom);
				painter.setPen(Qt::NoPen);
				painter.setBrush(
					e.offset > 0 ? pal.highlight() : pal.alternateBase());
				painter.setOpacity(0.5);
				painter.drawRect(QRect(
					QPoint(exposureLeft, 0),
					QPoint(exposureRight, rowHeight - 1)));
				painter.setOpacity(1.0);
			}
		}

	} else if(
		d->isCurrentExposureTool() && d->hoverTarget.frameIndex >= 0 &&
		d->hoverTarget.side != TargetSide::None) {

		int xIndex = d->hoverTarget.frameIndex;
		if(d->hoverTarget.side == TargetSide::Right) {
			++xIndex;
		}
		int x = headerWidth + xIndex * columnWidth - xScroll;

		painter.setClipRect(QRect{}, Qt::NoClip);
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen(pal.highlight(), 5));
		painter.drawLine(x, 0, x, h);
		painter.setPen(QPen(pal.highlightedText(), 1));
		if(d->hoverTarget.uiTrackIndex < 0) {
			painter.drawLine(x, 0, x, h);
		} else {
			int y =
				rowHeight + d->hoverTarget.uiTrackIndex * rowHeight - yScroll;
			painter.drawLine(x, y, x, y + rowHeight);
		}
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

	if(d->dragHover != Drag::KeyFrame && !d->isCurrentExposureTool()) {
		painter.setClipRect(bodyRect);
		painter.setOpacity(1.0);
		painter.setBrush(Qt::NoBrush);
		QPen outerPen(pal.highlightedText(), 1.0);
		QPen innerPen(pal.highlight(), 1.0);
		for(const QModelIndex &idx : d->itemSelectionModel->selectedIndexes()) {
			int x = headerWidth + idx.column() * columnWidth - xScroll;
			int y =
				rowHeight + (trackCount - idx.row() - 1) * rowHeight - yScroll;
			painter.setPen(outerPen);
			painter.drawRect(x, y, columnWidth, rowHeight);
			painter.setPen(innerPen);
			painter.drawRect(x + 1, y + 1, columnWidth - 2, rowHeight - 2);
		}
	}

	// For debugging. Shows red rectangles on selected indexes and a yellow
	// rectangle on the current index.
	if constexpr(false) {
		painter.setClipRect(bodyRect);
		painter.setPen(QPen(Qt::black, 1.0));
		int debugWidth = qMax(1, columnWidth / 4);
		int debugHeight = qMax(1, rowHeight / 4);

		for(const QModelIndex &idx : d->itemSelectionModel->selectedIndexes()) {
			int x = headerWidth + idx.column() * columnWidth - xScroll;
			int y =
				rowHeight + (trackCount - idx.row() - 1) * rowHeight - yScroll;
			painter.setBrush(Qt::red);
			painter.drawRect(x, y, debugWidth, debugHeight);
		}

		QModelIndex currentIdx = d->itemSelectionModel->currentIndex();
		if(currentIdx.isValid()) {
			int x = headerWidth + currentIdx.column() * columnWidth - xScroll;
			int y = rowHeight +
					(trackCount - currentIdx.row() - 1) * rowHeight - yScroll;
			painter.setBrush(Qt::yellow);
			painter.drawRect(x, y + debugHeight, debugWidth, debugHeight);
		}
	}

	if(trackCount == 0) {
		painter.setOpacity(1.0);
		painter.setClipRect(QRect(), Qt::NoClip);
		painter.setPen(textColor);
		painter.setBrush(Qt::NoBrush);
		QString noTracksText =
			tr("There's no tracks yet.\n"
			   "Add one using the ＋ button above\n"
			   "or via Animation ▸ New Track.");
#ifdef Q_OS_ANDROID
		// The Android font can't deal with these characters.
		noTracksText.replace(QStringLiteral("＋"), QStringLiteral("+"));
		noTracksText.replace(QStringLiteral("▸"), QStringLiteral(">"));
#endif
		painter.drawText(
			bodyRect, noTracksText, Qt::AlignHCenter | Qt::AlignVCenter);
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
		prevFrameWith(
			event->modifiers().testFlag(Qt::ControlModifier),
			event->modifiers().testFlag(Qt::ShiftModifier));
		break;
	case Qt::Key_Up:
		event->accept();
		trackAboveWith(event->modifiers().testFlag(Qt::ShiftModifier));
		break;
	case Qt::Key_Right:
		event->accept();
		nextFrameWith(
			event->modifiers().testFlag(Qt::ControlModifier),
			event->modifiers().testFlag(Qt::ShiftModifier));
		break;
	case Qt::Key_Down:
		event->accept();
		trackBelowWith(event->modifiers().testFlag(Qt::ShiftModifier));
		break;
	default:
		if(d->updateModifierState(event, true)) {
			event->accept();
		}
		QWidget::keyPressEvent(event);
		break;
	}
}

void TimelineWidget::keyReleaseEvent(QKeyEvent *event)
{
	if(d->updateModifierState(event, false)) {
		event->accept();
	}
	QWidget::keyReleaseEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
	d->updateMouseState(event);
	QPoint mousePos = compat::mousePos(*event);
	Target target = getMouseTarget(mousePos);
	bool needsUpdate = d->isCurrentExposureTool() &&
					   (target.frameIndex != d->hoverTarget.frameIndex ||
						target.uiTrackIndex != d->hoverTarget.uiTrackIndex ||
						target.side != d->hoverTarget.side);
	d->hoverTarget = target;
	updateCursor();

	Drag dragType = d->drag;
	bool shouldDrag =
		d->editable && !d->exposureTool.active &&
		((dragType == Drag::Track && d->currentTrack()) ||
		 (dragType == Drag::KeyFrame && d->isCurrentKeyFrameSelected() &&
		  !d->trackMoveLockedById(d->currentTrackId))) &&
		(mousePos - d->dragOrigin).manhattanLength() >=
			QApplication::startDragDistance();
	if(shouldDrag) {
		d->drag = Drag::None;
		d->dragKeyFrames.clear();
		d->dragKeyFrameIndexes.clear();

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
			mimeData->setProperty(
				"trackIndex", d->trackIndexById(d->currentTrackId));
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
	} else if(d->exposureTool.active) {
		if(target.rawFrameIndex != -1) {
			int exposureFrameIndex = target.rawFrameIndex;
			if(target.side == TargetSide::Right) {
				++exposureFrameIndex;
			}

			if(exposureFrameIndex != d->exposureTool.toFrameIndex) {
				d->exposureTool.toFrameIndex = exposureFrameIndex;
				d->exposureTool.valid = false;
			}
		}
		if(d->exposureTool.fromUiTrackIndex != -1 && target.uiTrackIndex >= 0 &&
		   target.uiTrackIndex != d->exposureTool.toUiTrackIndex) {
			d->exposureTool.toUiTrackIndex = target.uiTrackIndex;
			d->exposureTool.valid = false;
		}
	} else if(
		d->pressedHeader != TargetHeader::None && target.frameIndex != -1) {
		setCurrent(
			d->isCurrentSelectTool() ? target.trackId : 0, target.frameIndex,
			true, true, d->moveSelectionAction);
	}

	if(needsUpdate) {
		update();
	}
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
	d->updateMouseState(event);
	Target target = getMouseTarget(compat::mousePos(*event));
	applyMouseTarget(event, target, true);

	Qt::MouseButton button = event->button();
	Drag drag = Drag::None;
	Qt::DropAction dropAction = Qt::MoveAction;
	if(button == Qt::LeftButton) {
		if(target.action != TrackAction::None) {
			executeTargetAction(target);
			event->accept();
		} else if(target.trackId != 0 && target.frameIndex == -1) {
			drag = Drag::Track;
		} else if(d->isCurrentExposureTool()) {
			d->exposureTool = ExposureToolState();
			bool onHeader = target.header == TargetHeader::Header ||
							target.header == TargetHeader::RangeFirst ||
							target.header == TargetHeader::RangeLast;
			if((onHeader || (target.trackId > 0 && target.uiTrackIndex >= 0)) &&
			   target.frameIndex >= 0 &&
			   !d->trackMoveLockedById(target.trackId)) {
				int exposureTrackIndex = onHeader ? -1 : target.uiTrackIndex;
				int exposureFrameIndex = target.frameIndex;
				if(target.side == TargetSide::Right) {
					++exposureFrameIndex;
				}
				d->exposureTool.fromUiTrackIndex = exposureTrackIndex;
				d->exposureTool.fromFrameIndex = exposureFrameIndex;
				d->exposureTool.toUiTrackIndex = exposureTrackIndex;
				d->exposureTool.toFrameIndex = exposureFrameIndex;
				d->exposureTool.active = true;
			}
			updateCursor();
			update();
		} else if(
			!d->isCurrentSelectTool() &&
			d->keyFrameBy(target.trackId, target.frameIndex) &&
			!d->trackMoveLockedById(target.trackId)) {
			drag = Drag::KeyFrame;
		} else {
			// If the user pressed nowhere useful, let them drag around to
			// change the current frame. That's done via the "Header" target.
			if(target.header == TargetHeader::None) {
				d->pressedHeader = TargetHeader::Header;
			} else {
				d->pressedHeader = target.header;
			}
			updateCursor();
		}
	} else if(button == Qt::MiddleButton) {
		if(d->keyFrameBy(target.trackId, target.frameIndex) &&
		   !d->trackMoveLockedById(target.trackId)) {
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
	d->updateMouseState(event);
	Target target = getMouseTarget(compat::mousePos(*event));
	applyMouseTarget(event, target, true);

	if(event->button() == Qt::LeftButton) {
		if(target.action != TrackAction::None) {
			executeTargetAction(target);
		} else if(target.frameIndex == -1 && target.trackId != 0) {
			retitleTrack();
		} else if(
			target.frameIndex != -1 && target.trackId != 0 &&
			!d->isCurrentExposureTool()) {
			if(d->isCurrentSelectTool()) {
				// Use the Header target to allow dragging.
				if(target.header == TargetHeader::None) {
					d->pressedHeader = TargetHeader::Header;
				} else {
					d->pressedHeader = target.header;
				}
			} else if(d->currentKeyFrame()) {
				showKeyFrameProperties();
			} else if(d->editable) {
				d->actions.keyFrameCreateLayer->trigger();
			}
		} else if(
			target.frameIndex != -1 && target.header == TargetHeader::Header) {
			showAnimationProperties();
		}
	}
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *event)
{
	d->updateMouseState(event);
	if(event->button() == Qt::LeftButton) {
		if(d->exposureTool.active) {
			finishExposureTool();
		} else if(d->pressedHeader == TargetHeader::RangeFirst) {
			int frameRangeFirst =
				qBound(0, d->currentFrame, d->frameRangeLast());
			setAnimationProperties(-1.0, frameRangeFirst, -1);
		} else if(d->pressedHeader == TargetHeader::RangeLast) {
			int frameRangeLast =
				qBound(d->frameRangeFirst(), d->currentFrame, d->frameCount());
			setAnimationProperties(-1.0, -1, frameRangeLast);
		}
		d->exposureTool = ExposureToolState();
		d->pressedHeader = TargetHeader::None;
		d->drag = Drag::None;
	}

	executePendingSelectionAction();
	d->moveSelectionAction = SelectionAction::ReplaceMove;
}

void TimelineWidget::wheelEvent(QWheelEvent *event)
{
	d->updateWheelState(event);
	Qt::KeyboardModifiers modifiers = event->modifiers();
	QPoint pd = event->pixelDelta() * (event->inverted() ? -1 : 1);
	if(modifiers.testFlag(Qt::ControlModifier)) {
		if(pd.y() != 0) {
			zoomBy(pd.y() < 0 ? -1 : 1);
		}

	} else {
		if(modifiers.testFlag(Qt::ShiftModifier)) {
			pd = QPoint(pd.y(), pd.x());
		}

		int x = qMax(0, d->horizontalScroll->value() - pd.x());
		int y = qMax(0, d->verticalScroll->value() - pd.y());
		d->horizontalScroll->setValue(x - (x % d->columnWidth));
		d->verticalScroll->setValue(y - (y % d->rowHeight));
	}
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
		bool isValid = false;

		QRect rect{
			d->headerWidth, d->rowHeight,
			qMin(width() - d->headerWidth, d->fullBodyWidth()),
			qMin(height() - d->rowHeight, d->bodyHeight())};
		if(event->source() == this && event->answerRect().intersects(rect)) {
			isValid = checkKeyFrameDrag(event, false);
		}

		if(isValid) {
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
		if(!checkKeyFrameDrag(event, true)) {
			return;
		}

		net::MessageList msgs;
		msgs.reserve(1 + int(d->dragKeyFrames.size()));

		uint8_t contextId = d->canvas->localUserId();
		msgs.append(net::makeUndoPointMessage(contextId));

		bool copy = event->dropAction() == Qt::CopyAction;
		for(const KeyFrameDrag &kfd : d->dragKeyFrames) {
			if(copy) {
				msgs.append(
					net::makeKeyFrameSetMessage(
						contextId, kfd.toTrackId, kfd.toFrameIndex,
						kfd.fromTrackId, kfd.fromFrameIndex,
						DP_MSG_KEY_FRAME_SET_SOURCE_KEY_FRAME));
			} else {
				msgs.append(
					net::makeKeyFrameDeleteMessage(
						contextId, kfd.fromTrackId, kfd.fromFrameIndex,
						kfd.toTrackId, kfd.toFrameIndex));
			}
		}

		Q_EMIT timelineEditCommands(msgs.size(), msgs.constData());
	}

	d->dragKeyFrames.clear();
	d->dragKeyFrameIndexes.clear();
}

void TimelineWidget::leaveEvent(QEvent *event)
{
	QWidget::leaveEvent(event);
	d->hoverTarget = Target();
	d->modifiersDown = Qt::NoModifier;
	d->lastPosValid = false;
	update();
	updateCursor();
}

void TimelineWidget::setKeyFramesLayer()
{
	if(!d->editable) {
		return;
	}
	if(d->selectedLayerId > 0) {
		setKeyFrames(d->selectedLayerId);
	}
}

void TimelineWidget::setKeyFramesEmpty()
{
	if(!d->editable) {
		return;
	}
	setKeyFrames(0);
	emit blankLayerSelected();
}

void TimelineWidget::cutKeyFrames()
{
	copyKeyFrames();
	unassignKeyFrames();
}

void TimelineWidget::copyKeyFrames()
{
	if(!d->editable) {
		return;
	}

	int currentTrackIndex = d->trackIndexById(d->currentTrackId);
	int currentFrameIndex = d->currentFrame;
	const QVector<SelectionKeyFrame> &skfs = d->getSelectionState().keyFrames();
	if(currentTrackIndex < 0 || currentFrameIndex < 0 || skfs.isEmpty()) {
		return;
	}

	QJsonArray keyFramesJson;
	for(const SelectionKeyFrame &skf : skfs) {
		const canvas::TimelineKeyFrame *keyFrame =
			d->keyFrameBy(skf.trackId, skf.frameIndex);
		if(keyFrame) {

			QJsonArray layerVisibilityJson;
			for(QHash<int, bool>::const_iterator
					it = keyFrame->layerVisibility.constBegin(),
					end = keyFrame->layerVisibility.constEnd();
				it != end; ++it) {

				layerVisibilityJson.append(QJsonObject({
					{QStringLiteral("layerId"), it.key()},
					{QStringLiteral("visible"), it.value()},
				}));
			}

			keyFramesJson.append(QJsonObject({
				{QStringLiteral("layerId"), keyFrame->layerId},
				{QStringLiteral("title"), keyFrame->titleWithColor()},
				{QStringLiteral("layerVisibility"), layerVisibilityJson},
				{QStringLiteral("trackOffset"),
				 skf.trackIndex - currentTrackIndex},
				{QStringLiteral("frameOffset"),
				 skf.frameIndex - currentFrameIndex},
			}));
		}
	}

	QJsonDocument doc(QJsonObject({
		{QStringLiteral("keyFrames"), keyFramesJson},
	}));

	QMimeData *mimeData = new QMimeData;
	mimeData->setData(KEY_FRAMES_MIME_TYPE, doc.toJson(QJsonDocument::Compact));
	QApplication::clipboard()->setMimeData(mimeData);
}

void TimelineWidget::pasteKeyFrames()
{
	pasteKeyFramesWith(false);
}

void TimelineWidget::pasteKeyFramesDeclone()
{
	pasteKeyFramesWith(true);
}

void TimelineWidget::pasteKeyFramesWith(bool declone)
{
	if(!d->editable || !d->canvas) {
		return;
	}

	int currentTrackIndex = d->trackIndexById(d->currentTrackId);
	int currentFrameIndex = d->currentFrame;
	int trackCount = d->trackCount();
	int frameCount = d->frameCount();
	if(currentTrackIndex < 0 || currentFrameIndex < 0) {
		return;
	}

	const QMimeData *mimeData = QApplication::clipboard()->mimeData();
	if(!mimeData->hasFormat(KEY_FRAMES_MIME_TYPE)) {
		return;
	}

	int messageCapacityToReserve = 1;
	QVector<PasteKeyFrame> pkfs;
	{
		QJsonParseError err;
		QJsonDocument doc =
			QJsonDocument::fromJson(mimeData->data(KEY_FRAMES_MIME_TYPE), &err);
		if(!doc.isObject()) {
			qWarning(
				"Error parsing key frames on clipboard: %s",
				qUtf8Printable(err.errorString()));
			return;
		}

		QJsonArray keyFramesArray =
			doc.object().value(QStringLiteral("keyFrames")).toArray();
		for(const QJsonValue keyFrameValue : keyFramesArray) {
			if(!keyFrameValue.isObject()) {
				continue;
			}

			QJsonObject keyFrameObject = keyFrameValue.toObject();

			int trackIndex =
				currentTrackIndex +
				keyFrameObject.value(QStringLiteral("trackOffset")).toInt();
			if(trackIndex < 0 || trackIndex >= trackCount) {
				continue;
			}

			int frameIndex =
				currentFrameIndex +
				keyFrameObject.value(QStringLiteral("frameOffset")).toInt();
			if(frameIndex < 0 || frameIndex >= frameCount) {
				continue;
			}

			int trackId = d->trackIdByIndex(trackIndex);
			if(trackId <= 0) {
				continue;
			}

			++messageCapacityToReserve;

			int layerId =
				keyFrameObject.value(QStringLiteral("layerId")).toInt();

			QString title =
				keyFrameObject.value(QStringLiteral("title")).toString();
			if(!title.isEmpty()) {
				++messageCapacityToReserve;
			}

			QJsonArray layerVisibilityArray =
				keyFrameObject.value(QStringLiteral("layerVisibility"))
					.toArray();
			int layerVisibilityCount = layerVisibilityArray.size();

			QHash<int, bool> layerVisibility;
			if(layerVisibilityCount != 0) {
				++messageCapacityToReserve;
				layerVisibility.reserve(layerVisibilityCount);
				for(const QJsonValue &value : layerVisibilityArray) {
					QJsonObject obj = value.toObject();
					layerVisibility.insert(
						obj.value(QStringLiteral("layerId")).toInt(),
						obj.value(QStringLiteral("visible")).toBool());
				}
			}

			pkfs.append({
				trackId,
				frameIndex,
				layerId,
				0,
				title,
				QString(),
				layerVisibility,
			});
		}
	}
	if(pkfs.isEmpty()) {
		return;
	}

	QHash<int, int> declonedLayerIds;
	if(declone) {
		QHash<int, int> layerIdReferenceCounts = [this, &pkfs] {
			QSet<QPair<int, int>> clobbered;
			for(const PasteKeyFrame &pkf : pkfs) {
				clobbered.insert({pkf.trackId, pkf.frameIndex});
			}
			return d->collectLayerIdReferenceCounts(&clobbered);
		}();

		canvas::LayerListModel *layers = d->canvas->layerlist();
		QHash<int, QPair<int, QString>> declonePending;
		int totalRequiredLayerIds = 0;
		for(const PasteKeyFrame &pkf : pkfs) {
			int layerId = pkf.layerId;
			if(layerId > 0 && layerIdReferenceCounts.value(layerId) > 0) {
				if(!declonePending.contains(layerId)) {
					QModelIndex idx = layers->layerIndex(layerId);
					if(idx.isValid()) {
						int requiredLayerIds = layers->countRequiredIds(idx);
						declonePending.insert(
							layerId,
							{requiredLayerIds,
							 idx.data(canvas::LayerListModel::TitleRole)
								 .toString()});
						totalRequiredLayerIds += requiredLayerIds;
					} else {
						qWarning(
							"Layer %d to paste declone not found", layerId);
					}
				}
			}
		}

		if(!declonePending.isEmpty()) {
			QVector<int> layerIds =
				layers->getAvailableLayerIds(totalRequiredLayerIds);
			if(totalRequiredLayerIds == 0 ||
			   compat::cast_6<int>(layerIds.size()) < totalRequiredLayerIds) {
				qWarning(
					"Could not find %d layer ids to paste declone",
					totalRequiredLayerIds);
				return;
			}

			QSet<QString> additionalTakenLayerNames;
			for(PasteKeyFrame &pkf : pkfs) {
				int layerId = pkf.layerId;
				QPair<int, QString> p;
				if(layerId > 0 &&
				   (p = declonePending.value(layerId)).first > 0) {
					int &newLayerId = declonedLayerIds[layerId];
					if(newLayerId == 0) {
						++messageCapacityToReserve;
						newLayerId = layerIds.takeFirst();
						pkf.createLayerId = newLayerId;
						pkf.createLayerName = layers->getAvailableLayerName(
							p.second, &additionalTakenLayerNames);
						if(!pkf.createLayerName.isEmpty()) {
							additionalTakenLayerNames.insert(
								pkf.createLayerName);
						}
					} else {
						pkf.layerId = newLayerId;
					}

					// TODO: implement this properly. Needs a proper mapping
					// between layer ids within a group, rather than duplicating
					// the group in one message and not really knowing the
					// effective layer ids.
					if(!pkf.layerVisibility.isEmpty()) {
						pkf.layerVisibility.clear();
					}
				}
			}
		}
	}

	net::MessageList msgs;
	msgs.reserve(messageCapacityToReserve);

	uint8_t contextId = d->canvas->localUserId();
	msgs.append(net::makeUndoPointMessage(contextId));

	for(const PasteKeyFrame &pkf : pkfs) {
		int layerId;
		if(pkf.createLayerId == 0) {
			layerId = pkf.layerId;
		} else {
			layerId = pkf.createLayerId;
			msgs.append(
				net::makeLayerTreeCreateMessage(
					contextId, pkf.createLayerId, pkf.layerId, pkf.layerId, 0,
					0, pkf.createLayerName));
		}

		msgs.append(
			net::makeKeyFrameSetMessage(
				contextId, pkf.trackId, pkf.frameIndex, layerId, 0,
				DP_MSG_KEY_FRAME_SET_SOURCE_LAYER));

		if(!pkf.title.isEmpty()) {
			msgs.append(
				net::makeKeyFrameRetitleMessage(
					contextId, pkf.trackId, pkf.frameIndex, pkf.title));
		}

		if(!pkf.layerVisibility.isEmpty()) {
			msgs.append(d->makeKeyFrameLayerAttributesMessage(
				contextId, pkf.trackId, pkf.frameIndex, pkf.layerVisibility,
				declone ? &declonedLayerIds : nullptr));
		}
	}

	if(msgs.size() > 1) {
		Q_EMIT timelineEditCommands(msgs.size(), msgs.constData());
	}
}

void TimelineWidget::setKeyFramesColor(QAction *action)
{
	if(!d->editable) {
		return;
	}

	const QVector<SelectionKeyFrame> &skfs = d->getSelectionState().keyFrames();
	int skfCount = skfs.size();
	if(skfCount != 0) {
		net::MessageList msgs;
		msgs.reserve(1 + skfCount);

		uint8_t contextId = d->canvas->localUserId();
		msgs.append(net::makeUndoPointMessage(contextId));

		QColor color = action->property("markercolor").value<QColor>();
		for(const SelectionKeyFrame &skf : skfs) {
			const canvas::TimelineKeyFrame *keyFrame =
				d->keyFrameBy(skf.trackId, skf.frameIndex);
			if(keyFrame && color != keyFrame->color) {
				msgs.append(
					net::makeKeyFrameRetitleMessage(
						contextId, skf.trackId, skf.frameIndex,
						canvas::TimelineKeyFrame::makeTitleWithColor(
							keyFrame->title, color)));
			}
		}

		if(msgs.size() > 1) {
			Q_EMIT timelineEditCommands(msgs.size(), msgs.constData());
		}
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

void TimelineWidget::deleteKeyFramesWith(bool deleteUnusedLayers)
{
	if(!d->editable) {
		return;
	}

	const SelectionState &selectionState = d->getSelectionState();
	const QVector<SelectionKeyFrame> &keyFrames = selectionState.keyFrames();
	int keyFrameCount = keyFrames.size();
	if(keyFrameCount == 0) {
		return;
	}

	QSet<int> layerIdsToDelete;
	if(deleteUnusedLayers) {
		layerIdsToDelete = selectionState.selectedLayerIds();

		QSet<QPair<int, int>> selected =
			selectionState.selectedTrackIdKeyFrameIndexPairs();

		for(const canvas::TimelineTrack &t : d->getTracks()) {
			for(const canvas::TimelineKeyFrame &kf : t.keyFrames) {
				if(!selected.contains({t.id, kf.frameIndex})) {
					layerIdsToDelete.remove(kf.layerId);
				}
			}
		}
	}

	net::MessageList msgs;
	msgs.reserve(1 + keyFrameCount + int(layerIdsToDelete.size()));

	uint8_t contextId = d->canvas->localUserId();
	msgs.append(net::makeUndoPointMessage(contextId));

	for(const SelectionKeyFrame &skf : keyFrames) {
		msgs.append(
			net::makeKeyFrameDeleteMessage(
				contextId, skf.trackId, skf.frameIndex, 0, 0));
	}

	for(int layerId : layerIdsToDelete) {
		msgs.append(net::makeLayerTreeDeleteMessage(contextId, layerId, 0));
	}

	emit timelineEditCommands(msgs.size(), msgs.constData());
}

void TimelineWidget::deleteKeyFramesLayers()
{
	deleteKeyFramesWith(true);
}

void TimelineWidget::unassignKeyFrames()
{
	deleteKeyFramesWith(false);
}

void TimelineWidget::decloneKeyFrames()
{
	if(!d->editable || !d->canvas) {
		return;
	}

	QHash<int, int> layerIdReferenceCounts = d->collectLayerIdReferenceCounts();
	canvas::LayerListModel *layers = d->canvas->layerlist();
	QVector<QPair<SelectionKeyFrame, QString>> declone;
	int requiredLayerIds = 0;

	// Reversed ordering so that we duplicate the left-most key frames last,
	// otherwise it's weird that the "first" key frame gets a new layer and the
	// "last" one still references the original.
	for(const SelectionKeyFrame &skf :
		d->getSelectionState().sortedKeyFrames(1, 1)) {
		int layerId = skf.layerId;
		if(layerId > 0) {
			int &refcount = layerIdReferenceCounts[layerId];
			if(refcount > 1) {
				QModelIndex idx = layers->layerIndex(layerId);
				if(idx.isValid()) {
					requiredLayerIds += layers->countRequiredIds(idx);
					declone.append(
						{skf, idx.data(canvas::LayerListModel::TitleRole)
								  .toString()});
					--refcount;
				} else {
					qWarning("Layer %d to declone not found", layerId);
				}
			}
		}
	}

	int count = declone.size();
	if(count == 0) {
		return;
	}

	QVector<int> layerIds = layers->getAvailableLayerIds(requiredLayerIds);
	if(layerIds.isEmpty() ||
	   compat::cast_6<int>(layerIds.size()) < requiredLayerIds) {
		qWarning("Could not find enough layer ids to declone");
		return;
	}

	net::MessageList msgs;
	msgs.reserve(count * 2 + 1);

	uint8_t contextId = d->canvas->localUserId();
	// We will prepend the messages so that the layers get proper titles.

	QSet<QString> additionalTakenLayerNames;
	additionalTakenLayerNames.reserve(count);
	for(int i = 0; i < count; ++i) {
		// Reversed order, since the list is inverted from before.
		const QPair<SelectionKeyFrame, QString> &p = declone[count - i - 1];
		int oldLayerId = p.first.layerId;
		int newLayerId = layerIds[i];
		QString newLayerName =
			layers->getAvailableLayerName(p.second, &additionalTakenLayerNames);
		additionalTakenLayerNames.insert(newLayerName);
		msgs.prepend(
			net::makeKeyFrameSetMessage(
				contextId, p.first.trackId, p.first.frameIndex, newLayerId, 0,
				DP_MSG_KEY_FRAME_SET_SOURCE_LAYER));
		msgs.prepend(
			net::makeLayerTreeCreateMessage(
				contextId, newLayerId, oldLayerId, oldLayerId, 0, 0,
				newLayerName));
	}

	msgs.prepend(net::makeUndoPointMessage(contextId));

	Q_EMIT timelineEditCommands(msgs.size(), msgs.constData());
}

void TimelineWidget::increaseKeyFrameExposure()
{
	changeFrameExposure(-1, 1, {d->currentTrackId});
}

void TimelineWidget::increaseKeyFrameExposureAll()
{
	changeFrameExposure(-1, 1, d->unlockedTrackIds());
}

void TimelineWidget::decreaseKeyFrameExposure()
{
	changeFrameExposure(-1, -1, {d->currentTrackId});
}

void TimelineWidget::decreaseKeyFrameExposureAll()
{
	changeFrameExposure(-1, -1, d->unlockedTrackIds());
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

void TimelineWidget::toggleTrackMoveLock(bool moveLock)
{
	if(d->currentTrackId != 0) {
		emit trackMoveLockEnabled(d->currentTrackId, moveLock);
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

		int actualFrameRangeFirst = metadata->actualFrameRangeFirst();
		int actualFrameRangeLast = metadata->actualFrameRangeLast();

		bool framerateChanged =
			framerate != -1 && metadata->framerate() != framerate;
		bool frameRangeFirstChanged = !compatibilityMode &&
									  frameRangeFirst != -1 &&
									  frameRangeFirst != actualFrameRangeFirst;
		bool frameRangeLastChanged = !compatibilityMode &&
									 frameRangeLast != -1 &&
									 frameRangeLast != actualFrameRangeLast;

		// If the frame range is unset and we would only be setting one of the
		// sides, force the other one as well so that we get a valid range.
		bool haveInvalidFrameRange =
			!compatibilityMode &&
			(actualFrameRangeFirst < 0 || actualFrameRangeLast < 0) &&
			frameRangeFirstChanged != frameRangeLastChanged;
		if(haveInvalidFrameRange) {
			int frameCount = metadata->frameCount();

			// Fix up whichever side we're not updating.
			if(frameRangeFirstChanged && !frameRangeLastChanged) {
				frameRangeLastChanged = true;
				if(actualFrameRangeLast < 0) {
					frameRangeLast = frameCount - 1;
				} else {
					frameRangeLast = frameCount;
				}
			} else if(!frameRangeFirstChanged && frameRangeLastChanged) {
				frameRangeFirstChanged = true;
				if(actualFrameRangeFirst < 0) {
					frameRangeFirst = 0;
				} else {
					frameRangeFirst = actualFrameRangeFirst;
				}
			} else {
				// Should not be reached, since haveInvalidFrameRange checks
				// whether the first and last range are different.
			}

			// Some extra bounds checks just in case.
			if(frameRangeLast >= frameCount) {
				frameRangeLast = frameCount - 1;
			}
			if(frameRangeFirst > frameRangeLast) {
				frameRangeFirst = frameRangeLast;
			}
		}

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
					msgs.append(
						net::makeSetMetadataIntMessage(
							contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE,
							qMax(1, qRound(framerate))));
				} else {
					int whole, fraction;
					drawdance::DocumentMetadata::splitEffectiveFramerate(
						framerate, whole, fraction);
					msgs.append(
						net::makeSetMetadataIntMessage(
							contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE,
							whole));
					msgs.append(
						net::makeSetMetadataIntMessage(
							contextId,
							DP_MSG_SET_METADATA_INT_FIELD_FRAMERATE_FRACTION,
							fraction));
				}
			}

			if(frameCountChanged) {
				msgs.append(
					net::makeSetMetadataIntMessage(
						contextId, DP_MSG_SET_METADATA_INT_FIELD_FRAME_COUNT,
						frameCount));
			}

			if(frameRangeFirstChanged) {
				msgs.append(
					net::makeSetMetadataIntMessage(
						contextId,
						DP_MSG_SET_METADATA_INT_FIELD_FRAME_RANGE_FIRST,
						frameRangeFirst));
			}

			if(frameRangeLastChanged) {
				msgs.append(
					net::makeSetMetadataIntMessage(
						contextId,
						DP_MSG_SET_METADATA_INT_FIELD_FRAME_RANGE_LAST,
						frameRangeLast));
			}

			emit timelineEditCommands(msgs.size(), msgs.constData());
		}
	}
}

void TimelineWidget::nextFrame()
{
	nextFrameWith(false, false);
}

void TimelineWidget::prevFrame()
{
	prevFrameWith(false, false);
}

void TimelineWidget::nextFrameClamp()
{
	nextFrameWith(true, false);
}

void TimelineWidget::prevFrameClamp()
{
	prevFrameWith(true, false);
}

void TimelineWidget::nextFrameWith(bool clamp, bool select)
{
	int targetFrame = d->currentFrame + 1;
	if(clamp) {
		if(!d->isInRange(targetFrame)) {
			targetFrame = d->frameRangeFirst();
		}
	} else if(targetFrame >= d->visibleFrameCount()) {
		targetFrame = 0;
	}
	setCurrent(
		d->currentTrackId, targetFrame, true, true,
		select ? SelectionAction::SelectCurrentRange
			   : SelectionAction::Replace);
}

void TimelineWidget::prevFrameWith(bool clamp, bool select)
{
	int targetFrame = d->currentFrame - 1;
	if(clamp) {
		if(!d->isInRange(targetFrame)) {
			targetFrame = d->frameRangeLast();
		}
	} else if(targetFrame < 0) {
		targetFrame = d->visibleFrameCount() - 1;
	}
	setCurrent(
		d->currentTrackId, targetFrame, true, true,
		select ? SelectionAction::SelectCurrentRange
			   : SelectionAction::Replace);
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
	trackAboveWith(false);
}

void TimelineWidget::trackBelow()
{
	trackBelowWith(false);
}

void TimelineWidget::trackAboveWith(bool select)
{
	int targetTrack = d->trackIndexById(d->currentTrackId) + 1;
	if(targetTrack >= d->trackCount()) {
		targetTrack = 0;
	}
	setCurrent(
		d->trackIdByIndex(targetTrack), d->currentFrame, true, true,
		select ? SelectionAction::SelectCurrentRange
			   : SelectionAction::Replace);
}

void TimelineWidget::trackBelowWith(bool select)
{
	int targetTrack = d->trackIndexById(d->currentTrackId) - 1;
	if(targetTrack < 0) {
		targetTrack = d->trackCount() - 1;
	}
	setCurrent(
		d->trackIdByIndex(targetTrack), d->currentFrame, true, true,
		select ? SelectionAction::SelectCurrentRange
			   : SelectionAction::Replace);
}

void TimelineWidget::switchTool(QAction *action)
{
	TimelineTool tool;
	if(action == d->actions.timelineToolExposure) {
		tool = TimelineTool::Exposure;
	} else if(action == d->actions.timelineToolSelect) {
		tool = TimelineTool::Select;
	} else {
		tool = TimelineTool::Normal;
	}

	if(tool != d->currentTool) {
		d->currentTool = tool;
		updateCursor();
		update();
	}
}

void TimelineWidget::updateTracks()
{
	d->selectionStateValid = false;
	if(!d->canvas) {
		d->itemModel->setTrackCount(0);
		return;
	}

	d->itemModel->setTrackCount(d->trackCount());

	const QFontMetrics fm = fontMetrics();
	d->rowHeight = qMax(fm.height() * 3 / 2, ICON_SIZE);
	d->baseColumnWidth = fm.horizontalAdvance(QStringLiteral("999")) + 8;
	bool updatedColumnWidth = d->updateColumnWidth();

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
	d->headerWidth =
		qMax(64, maxTrackAdvance) + TRACK_PADDING * 6 + ICON_SIZE * 3;
	d->exposureTool.valid = false;
	int effectiveTrackId = nextTrackId != 0		 ? nextTrackId
						   : currentTrackId != 0 ? currentTrackId
												 : anyTrackId;
	if(d->trackActionsWidget) {
		d->trackActionsWidget->setGeometry(0, 0, d->headerWidth, d->rowHeight);
	}
	setMinimumHeight(
		d->rowHeight * 3 + d->horizontalScroll->sizeHint().height());
	setCurrent(
		effectiveTrackId, d->currentFrame, false, false,
		SelectionAction::Retain);
	updateActions();
	updateScrollbars();
	update();

	if(updatedColumnWidth) {
		QScopedValueRollback<bool> rollback(d->zoomInProgress, true);
		Q_EMIT columnWidthChanged(d->columnWidth);
	}

	if(!d->frameViewRequestTimer.hasExpired()) {
		d->frameViewRequestTimer.setRemainingTime(0, Qt::VeryCoarseTimer);
		emit frameViewModeRequested();
	}
}

void TimelineWidget::updateFrameCount()
{
	d->selectionStateValid = false;
	d->exposureTool.valid = false;
	setCurrent(
		d->currentTrackId, d->currentFrame, false, false,
		SelectionAction::Retain);
	updateFrameRange();
}

void TimelineWidget::updateFrameRange()
{
	d->itemModel->setFrameCount(d->frameCount());
	d->exposureTool.valid = false;
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

void TimelineWidget::finishExposureTool()
{
	d->updateExposureToolState(d->getTracks());
	// Only call update if we do nothing. Otherwise wait for a change to come in
	// so that there's no flicker.
	bool needsUpdate = true;
	if(d->exposureTool.offset != 0) {
		int topIndex = qMax(d->exposureTool.topIndex, 0);
		int bottomIndex =
			qMin(d->exposureTool.bottomIndex, d->trackCount() - 1);
		int trackCount = bottomIndex - topIndex + 1;
		if(trackCount > 0) {
			QVector<int> trackIds;
			trackIds.reserve(trackCount);
			for(int i = topIndex; i <= bottomIndex; ++i) {
				int trackId = d->trackIdByUiIndex(i);
				if(trackId > 0 && !d->trackMoveLockedById(trackId)) {
					trackIds.append(trackId);
				}
			}
			if(!trackIds.isEmpty()) {
				needsUpdate = !changeFrameExposure(
					d->exposureTool.fromFrameIndex, d->exposureTool.offset,
					trackIds);
			}
		}
	}

	if(needsUpdate) {
		update();
	}
}

void TimelineWidget::updatePasteAction()
{
	bool enabled = false;
	if(d->canvas && d->haveActions && d->editable &&
	   d->visibleFrameCount() > 0 && d->currentTrack() &&
	   d->currentFrame >= 0) {
		const QMimeData *mimeData = QApplication::clipboard()->mimeData();
		enabled = mimeData && mimeData->hasFormat(KEY_FRAMES_MIME_TYPE);
	}
	d->actions.keyFramePaste->setEnabled(enabled);
	d->actions.keyFramePasteDeclone->setEnabled(enabled);
}

void TimelineWidget::updateCursor()
{
	if(d->pressedHeader == TargetHeader::RangeFirst ||
	   d->pressedHeader == TargetHeader::RangeLast ||
	   (d->currentTool != TimelineTool::Exposure &&
		(d->hoverTarget.header == TargetHeader::RangeFirst ||
		 d->hoverTarget.header == TargetHeader::RangeLast) &&
		!d->isCurrentExposureTool())) {
		setCursor(Qt::SizeHorCursor);
	} else if(d->isCurrentExposureTool() && d->hoverTarget.frameIndex >= 0) {
		if(!d->exposureTool.active &&
		   d->trackMoveLockedById(d->hoverTarget.trackId)) {
			setCursor(Qt::ForbiddenCursor);
		} else {
			setCursor(Qt::SplitHCursor);
		}
	} else {
		setCursor(Qt::ArrowCursor);
	}
}

void TimelineWidget::zoomIn()
{
	zoomBy(1);
}

void TimelineWidget::zoomOut()
{
	zoomBy(-1);
}

void TimelineWidget::resetZoom()
{
	setZoomAdjust(0);
}

void TimelineWidget::zoomBy(int delta)
{
	setZoomAdjust(d->zoomAdjust + delta);
}

void TimelineWidget::setZoomAdjust(int zoomAdjust)
{
	int focusX =
		d->lastPosValid ? d->lastPos.x() : d->headerWidth + d->bodyWidth() / 2;
	qreal prevOffset = d->xScroll == 0
						   ? 0.0
						   : qreal(d->xScroll + focusX - d->headerWidth) /
								 qreal(d->columnWidth);
	d->zoomAdjust = zoomAdjust;
	if(d->updateColumnWidth()) {
		updateScrollbars();
		if(prevOffset > 0.0) {
			qreal currentOffset = qreal(d->xScroll + focusX - d->headerWidth) /
								  qreal(d->columnWidth);
			int scrollDelta =
				qRound((prevOffset - currentOffset) * qreal(d->columnWidth));
			d->horizontalScroll->setValue(d->xScroll + scrollDelta);
		}
		update();
		QScopedValueRollback<bool> rollback(d->zoomInProgress, true);
		Q_EMIT columnWidthChanged(d->columnWidth);
	}
}

void TimelineWidget::onSelectionChanged()
{
	d->selectionStateValid = false;
	if(!d->selectionUpdatesBlocked) {
		updateActions();
		update();
	}
}

TimelineWidget::SetCurrentResult TimelineWidget::setCurrent(
	int trackId, int frame, bool triggerUpdate, bool selectLayer,
	SelectionAction selectionAction)
{
	bool needsUpdate = false;

	int trackIndex = d->trackIndexById(trackId);
	if(trackIndex != -1) {
		d->currentTrackId = trackId;
		emit trackSelected(trackId);
		needsUpdate = true;
	} else {
		trackIndex = d->trackIndexById(d->currentTrackId);
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

	// Only update based on selection changes if we want updates from this
	// operation and there isn't another update that's going to happen anyway.
	d->selectionUpdatesBlocked = !triggerUpdate || needsUpdate;

	QModelIndex idx = d->itemModel->index(trackIndex, d->currentFrame);
	bool isSelected = d->itemSelectionModel->isSelected(idx);

	QItemSelectionModel::SelectionFlags selectionFlags;
	switch(selectionAction) {
	case SelectionAction::Retain:
		d->selectionRangeStartIndex = idx;
		if(isSelected || d->itemSelectionModel->hasSelection()) {
			selectionFlags = QItemSelectionModel::NoUpdate;
			int deltaTrackIndex =
				idx.row() - d->itemSelectionModel->currentIndex().row();
			if(deltaTrackIndex != 0) {
				shiftSelectionBy(deltaTrackIndex, 0);
			}
		} else {
			selectionFlags = QItemSelectionModel::ClearAndSelect;
		}
		break;
	case SelectionAction::ReplaceMove:
		if(setCurrentCheckPending(idx)) {
			selectionFlags = QItemSelectionModel::NoUpdate;
			break;
		}
		Q_FALLTHROUGH();
	case SelectionAction::Replace:
		d->selectionRangeStartIndex = idx;
		selectionFlags = QItemSelectionModel::ClearAndSelect;
		break;
	case SelectionAction::ReplaceIfNotSelected:
		d->selectionRangeStartIndex = idx;
		if(isSelected) {
			selectionFlags = QItemSelectionModel::NoUpdate;
		} else {
			selectionFlags = QItemSelectionModel::ClearAndSelect;
		}
		break;
	case SelectionAction::Toggle:
		d->selectionRangeStartIndex = idx;
		selectionFlags = QItemSelectionModel::Toggle;
		break;
	case SelectionAction::ToggleIfNotSelected:
		d->selectionRangeStartIndex = idx;
		if(isSelected) {
			selectionFlags = QItemSelectionModel::NoUpdate;
		} else {
			selectionFlags = QItemSelectionModel::Select;
		}
		break;
	case SelectionAction::SelectCurrentRange:
		selectionFlags = setCurrentSelectRange(
			idx, QItemSelectionModel::Select | QItemSelectionModel::Current);
		break;
	case SelectionAction::DeselectCurrentRange:
		selectionFlags = setCurrentSelectRange(
			idx, QItemSelectionModel::Deselect | QItemSelectionModel::Current);
		break;
	case SelectionAction::SelectRange:
		selectionFlags =
			setCurrentSelectRange(idx, QItemSelectionModel::Select);
		d->selectionRangeStartIndex = idx;
		break;
	case SelectionAction::DeselectRange:
		selectionFlags =
			setCurrentSelectRange(idx, QItemSelectionModel::Deselect);
		d->selectionRangeStartIndex = idx;
		break;
	default:
		qWarning("Unknown selection action %d", int(selectionAction));
		selectionFlags = QItemSelectionModel::ClearAndSelect;
		break;
	}

	d->itemSelectionModel->setCurrentIndex(idx, selectionFlags);

	if(needsUpdate && triggerUpdate) {
		if(selectLayer) {
			int layerIdToSelect;
			if(d->guessLayerIdToSelect(layerIdToSelect)) {
				if(layerIdToSelect == 0) {
					emit blankLayerSelected();
				} else {
					emit layerSelected(layerIdToSelect);
				}
				emit frameViewModeRequested();
			}
		}
		updateActions();
		update();
	}

	d->selectionUpdatesBlocked = false;
	return {idx, isSelected};
}

QItemSelectionModel::SelectionFlags TimelineWidget::setCurrentSelectRange(
	const QModelIndex &idx,
	QItemSelectionModel::SelectionFlags rangeSelectionFlags)
{
	if(setCurrentCheckPending(idx)) {
		return QItemSelectionModel::NoUpdate | QItemSelectionModel::Current;
	} else if(idx.isValid() && d->selectionRangeStartIndex.isValid()) {
		int idxRow = idx.row();
		int idxCol = idx.column();
		int selRow = d->selectionRangeStartIndex.row();
		int selCol = d->selectionRangeStartIndex.column();
		d->itemSelectionModel->select(
			QItemSelection(
				d->itemModel->index(qMin(idxRow, selRow), qMin(idxCol, selCol)),
				d->itemModel->index(
					qMax(idxRow, selRow), qMax(idxCol, selCol))),
			rangeSelectionFlags);
		return QItemSelectionModel::NoUpdate | QItemSelectionModel::Current;
	} else {
		return QItemSelectionModel::ClearAndSelect;
	}
}

bool TimelineWidget::setCurrentCheckPending(const QModelIndex &idx)
{
	if(d->pendingSelectionAction != PendingSelectionAction::None) {
		if(idx == d->selectionPressIndex) {
			return true;
		} else {
			executePendingSelectionAction();
		}
	}
	return false;
}

void TimelineWidget::executePendingSelectionAction()
{
	switch(d->pendingSelectionAction) {
	case PendingSelectionAction::None:
		return;
	case PendingSelectionAction::Replace:
		d->itemSelectionModel->select(
			d->selectionPressIndex, QItemSelectionModel::ClearAndSelect);
		d->pendingSelectionAction = PendingSelectionAction::None;
		return;
	case PendingSelectionAction::Deselect:
		d->itemSelectionModel->select(
			d->selectionPressIndex, QItemSelectionModel::Deselect);
		d->pendingSelectionAction = PendingSelectionAction::None;
		return;
	}
	qWarning(
		"Unhandled pending selection action %d",
		int(d->pendingSelectionAction));
	d->pendingSelectionAction = PendingSelectionAction::None;
}

void TimelineWidget::setKeyFrames(int layerId)
{
	if(layerId > 0) {
		startFrameViewRequestTimer();
	}

	QModelIndexList idxs = d->itemSelectionModel->selectedIndexes();
	int idxCount = idxs.size();
	if(idxCount != 0) {
		net::MessageList msgs;
		msgs.reserve(1 + idxCount);

		uint8_t contextId = d->canvas->localUserId();
		msgs.append(net::makeUndoPointMessage(contextId));

		for(const QModelIndex &idx : idxs) {
			int trackId = d->trackIdByIndex(idx.row());
			if(trackId > 0) {
				msgs.append(
					net::makeKeyFrameSetMessage(
						contextId, trackId, idx.column(), layerId, 0,
						DP_MSG_KEY_FRAME_SET_SOURCE_LAYER));
			}
		}

		if(msgs.size() > 1) {
			Q_EMIT timelineEditCommands(msgs.size(), msgs.constData());
		}
	}
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
			messages[fill++] = d->makeKeyFrameLayerAttributesMessage(
				contextId, trackId, frame, layerVisibility);
		}
		emit timelineEditCommands(fill, messages);
	}
}

bool TimelineWidget::changeFrameExposure(
	int start, int offset, const QVector<int> &trackIds)
{
	if(!d->editable || offset == 0) {
		return false;
	}

	QVector<QPair<int, QVector<int>>> trackFrameIndexes;
	bool forward = offset > 0;
	if(start < 0) {
		for(int trackId : trackIds) {
			if(!checkAndCollectFrameExposure(
				   trackFrameIndexes, forward, trackId)) {
				return false;
			}
		}
	} else {
		for(int trackId : trackIds) {
			collectFrameExposure(trackFrameIndexes, start - 1, trackId);
		}
	}

	if(trackFrameIndexes.isEmpty()) {
		return false;
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
			messages.append(
				net::makeKeyFrameDeleteMessage(
					contextId, trackId, frameIndex, trackId,
					frameIndex + offset));
		}
	}
	emit timelineEditCommands(messages.size(), messages.constData());
	return true;
}

bool TimelineWidget::checkAndCollectFrameExposure(
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

	collectFrameExposure(trackFrameIndexes, exposure.start, trackId);
	return true;
}

void TimelineWidget::collectFrameExposure(
	QVector<QPair<int, QVector<int>>> &trackFrameIndexes, int start,
	int trackId)
{
	QPair<int, QVector<int>> p = {trackId, {}};
	p.second = d->gatherCurrentExposureFramesByTrackId(trackId, start);
	if(!p.second.isEmpty()) {
		trackFrameIndexes.append(std::move(p));
	}
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
	d->actions.trackMoveLock->setEnabled(track);
	setCheckedSignalBlocked(d->actions.trackMoveLock, track && track->moveLock);
	d->actions.trackDuplicate->setEnabled(trackEditable);
	d->actions.trackRetitle->setEnabled(trackEditable);
	d->actions.trackDelete->setEnabled(trackEditable);

	int visibleFrameCount = d->visibleFrameCount();
	bool haveMultipleFrames = visibleFrameCount > 1;
	d->actions.frameNext->setEnabled(haveMultipleFrames);
	d->actions.framePrev->setEnabled(haveMultipleFrames);
	d->actions.frameNextClamp->setEnabled(haveMultipleFrames);
	d->actions.framePrevClamp->setEnabled(haveMultipleFrames);
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

	bool haveKeyFrameLayer = false;
	bool isSameKeyFrameLayer = false;
	const SelectionState selectionState = d->getSelectionState();
	QModelIndexList selectedIndexes = d->itemSelectionModel->selectedIndexes();
	int selectedIndexCount = selectedIndexes.size();
	int selectedNonKeyFrameCount =
		qMax(0, selectedIndexCount - selectionState.keyFrameCount());
	QString keyFrameSetLayerText;
	if(d->selectedLayerId > 0) {
		QModelIndex layerIndex = d->layerIndexById(d->selectedLayerId);
		if(layerIndex.isValid()) {
			int layerId =
				layerIndex.data(canvas::LayerListModel::IdRole).toInt();
			QString layerTitle =
				layerIndex.data(canvas::LayerListModel::TitleRole).toString();
			//: %1 is the name of a layer.
			keyFrameSetLayerText = QCoreApplication::translate(
									   "MainWindow", "Set Key Frame(s) to %1",
									   nullptr, selectedIndexCount)
									   .arg(layerTitle);
			haveKeyFrameLayer = true;

			if(selectedNonKeyFrameCount == 0) {
				isSameKeyFrameLayer = true;
				for(const SelectionKeyFrame &skf : selectionState.keyFrames()) {
					if(skf.layerId != layerId) {
						isSameKeyFrameLayer = false;
						break;
					}
				}
			}
		}
	}

	d->actions.keyFrameSetLayer->setEnabled(
		keyFrameSettable && haveKeyFrameLayer && !isSameKeyFrameLayer &&
		selectedIndexCount != 0);
	if(haveKeyFrameLayer) {
		utils::setActionText(d->actions.keyFrameSetLayer, keyFrameSetLayerText);
	} else {
		utils::setActionText(
			d->actions.keyFrameSetLayer,
			QCoreApplication::translate(
				"MainWindow", "Set Key Frame(s) to Current Layer", nullptr,
				selectedIndexCount));
	}

	d->actions.keyFrameSetEmpty->setEnabled(
		keyFrameSettable &&
		(!currentKeyFrame || currentKeyFrame->layerId != 0) &&
		selectedIndexCount != 0);
	utils::setActionText(
		d->actions.keyFrameSetEmpty, QCoreApplication::translate(
										 "MainWindow", "Set Blank Key Frame(s)",
										 nullptr, selectedIndexCount));

	bool keyFrameCreatable = keyFrameSettable && selectedNonKeyFrameCount != 0;
	d->actions.keyFrameCreateLayer->setEnabled(keyFrameCreatable);
	d->actions.keyFrameCreateGroup->setEnabled(keyFrameCreatable);
	utils::setActionText(
		d->actions.keyFrameCreateLayer,
		QCoreApplication::translate(
			"MainWindow", "Create Layers on Current Key Frame(s)", nullptr,
			selectedNonKeyFrameCount));
	utils::setActionText(
		d->actions.keyFrameCreateGroup,
		QCoreApplication::translate(
			"MainWindow", "Create Layer Group(s) on Current Key Frame(s)",
			nullptr, selectedNonKeyFrameCount));

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
	d->actions.keyFrameProperties->setEnabled(keyFrameEditable);

	int selectedKeyFrameCount = selectionState.keyFrameCount();
	bool anySelectedKeyFramesEditable =
		timelineEditable && visibleFrameCount > 0 && selectedKeyFrameCount > 0;

	d->actions.animationKeyFrameColorMenu->setEnabled(
		anySelectedKeyFramesEditable);
	// This isn't correct if multiple frames are selected, but whatever.
	d->actions.animationKeyFrameColorMenu->setProperty(
		"markercolor", currentKeyFrame ? currentKeyFrame->color : QColor());
	for(QAction *ca : d->actions.animationKeyFrameColorMenu->actions()) {
		ca->setEnabled(anySelectedKeyFramesEditable);
	}

	d->actions.keyFrameCut->setEnabled(anySelectedKeyFramesEditable);
	utils::setActionText(
		d->actions.keyFrameCut,
		QCoreApplication::translate(
			"MainWindow", "Cut Key Frame(s)", nullptr, selectedKeyFrameCount));

	d->actions.keyFrameCopy->setEnabled(anySelectedKeyFramesEditable);
	utils::setActionText(
		d->actions.keyFrameCopy,
		QCoreApplication::translate(
			"MainWindow", "Copy Key Frame(s)", nullptr, selectedKeyFrameCount));

	d->actions.keyFrameDeleteLayer->setEnabled(anySelectedKeyFramesEditable);
	utils::setActionText(
		d->actions.keyFrameDeleteLayer, QCoreApplication::translate(
											"MainWindow", "Delete Key Frame(s)",
											nullptr, selectedKeyFrameCount));

	d->actions.keyFrameUnassign->setEnabled(anySelectedKeyFramesEditable);
	utils::setActionText(
		d->actions.keyFrameUnassign, QCoreApplication::translate(
										 "MainWindow", "Unassign Key Frame(s)",
										 nullptr, selectedKeyFrameCount));

	d->actions.keyFrameDeclone->setEnabled(
		anySelectedKeyFramesEditable && selectionState.isAnyLayerIdsSelected());

	bool canIncreaseExposure = false;
	bool canDecreaseExposure = false;
	if(timelineEditable && track && !track->moveLock) {
		Exposure exposure = d->currentExposure();
		canIncreaseExposure = exposure.canIncrease();
		canDecreaseExposure = exposure.canDecrease();
	}
	d->actions.keyFrameExposureIncrease->setEnabled(canIncreaseExposure);
	d->actions.keyFrameExposureDecrease->setEnabled(canDecreaseExposure);

	bool canIncreaseExposureAll = false;
	bool canDecreaseExposureAll = false;
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
							canIncreaseExposureAll = false;
							blockIncrease = true;
						} else if(
							!canIncreaseExposureAll && exposure.canIncrease()) {
							canIncreaseExposureAll = true;
						}
					}

					if(!canDecreaseExposureAll && exposure.canDecrease()) {
						canDecreaseExposureAll = true;
					}
				}
			}
		}
	}
	d->actions.keyFrameExposureIncreaseAll->setEnabled(canIncreaseExposureAll);
	d->actions.keyFrameExposureDecreaseAll->setEnabled(canDecreaseExposureAll);

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

void TimelineWidget::startFrameViewRequestTimer()
{
	if(d->editable) {
		d->frameViewRequestTimer.setRemainingTime(10000, Qt::VeryCoarseTimer);
	}
}

bool TimelineWidget::checkKeyFrameDrag(
	const QDropEvent *event, bool updateSelection)
{
	const QMimeData *mimeData = event->mimeData();
	int sourceTrackIndex = mimeData->property("trackIndex").toInt();
	int sourceFrameIndex = mimeData->property("frameIndex").toInt();

	Target target = getMouseTarget(compat::dropPos(*event));
	int targetTrackIndex = d->trackCount() - target.uiTrackIndex - 1;
	int targetFrameIndex = target.frameIndex;

	int deltaTrackIndex = targetTrackIndex - sourceTrackIndex;
	int deltaFrameIndex = targetFrameIndex - sourceFrameIndex;
	if(deltaTrackIndex == 0 && deltaFrameIndex == 0) {
		return false;
	}

	QVector<SelectionKeyFrame> skfs =
		d->selectionState.sortedKeyFrames(deltaTrackIndex, deltaFrameIndex);

	const QVector<canvas::TimelineTrack> &tracks = d->getTracks();
	int trackCount = tracks.size();
	int frameCount = d->frameCount();
	int moveCount = 0;
	for(SelectionKeyFrame &skf : skfs) {
		if(skf.trackIndex >= 0 && skf.trackIndex < trackCount &&
		   !tracks[skf.trackIndex].moveLock) {
			int toTrackIndex = skf.trackIndex + deltaTrackIndex;
			if(toTrackIndex < 0 || toTrackIndex >= trackCount ||
			   tracks[toTrackIndex].moveLock) {
				return false;
			}

			int toFrameIndex = skf.frameIndex + deltaFrameIndex;
			if(toFrameIndex < 0 || toFrameIndex >= frameCount) {
				return false;
			}

			++moveCount;
		} else {
			skf.trackIndex = -1;
		}
	}

	if(moveCount == 0) {
		return false;
	}

	d->dragKeyFrames.clear();
	d->dragKeyFrameIndexes.clear();
	d->dragKeyFrames.reserve(moveCount);
	d->dragKeyFrameIndexes.reserve(moveCount);
	for(const SelectionKeyFrame &skf : skfs) {
		if(skf.trackIndex != -1) {
			int toTrackIndex = skf.trackIndex + deltaTrackIndex;
			d->dragKeyFrames.append({
				skf.trackId,
				skf.frameIndex,
				tracks[toTrackIndex].id,
				toTrackIndex,
				skf.frameIndex + deltaFrameIndex,
			});
			d->dragKeyFrameIndexes.insert({skf.trackIndex, skf.frameIndex});
		}
	}

	if(updateSelection) {
		d->selectionUpdatesBlocked = true;

		int newTrackIndex = qBound(
			0, d->trackIndexById(d->currentTrackId) + deltaTrackIndex,
			trackCount - 1);
		int newTrackId = tracks[newTrackIndex].id;
		if(newTrackId != d->currentTrackId) {
			d->currentTrackId = newTrackId;
			Q_EMIT trackSelected(newTrackId);
		}

		int newFrameIndex =
			qBound(0, d->currentFrame + deltaFrameIndex, frameCount - 1);
		if(newFrameIndex != d->currentFrame) {
			d->currentFrame = newFrameIndex;
			Q_EMIT frameSelected(newFrameIndex);
		}

		d->itemSelectionModel->setCurrentIndex(
			d->itemModel->index(newTrackIndex, newFrameIndex),
			QItemSelectionModel::NoUpdate);

		shiftSelectionBy(deltaTrackIndex, deltaFrameIndex);

		d->selectionUpdatesBlocked = false;
		updateActions();
		update();
	}

	return true;
}

void TimelineWidget::shiftSelectionBy(int deltaTrackIndex, int deltaFrameIndex)
{
	QItemSelection selection;
	for(QModelIndex &idx : d->itemSelectionModel->selectedIndexes()) {
		QModelIndex siblingIdx = idx.sibling(
			idx.row() + deltaTrackIndex, idx.column() + deltaFrameIndex);
		if(siblingIdx.isValid()) {
			selection.select(siblingIdx, siblingIdx);
		}
	}
	d->itemSelectionModel->select(
		selection, QItemSelectionModel::ClearAndSelect);

	if(d->selectionRangeStartIndex.isValid()) {
		d->selectionRangeStartIndex = d->selectionRangeStartIndex.sibling(
			d->selectionRangeStartIndex.row() + deltaTrackIndex,
			d->selectionRangeStartIndex.column() + deltaFrameIndex);
	}
}

TimelineWidget::Target TimelineWidget::getMouseTarget(const QPoint &pos) const
{
	Target target;
	if(d->canvas) {
		int x = pos.x();
		int headerWidth = d->headerWidth;
		int frameX = (x - headerWidth + d->xScroll);
		int frameIndex = frameX / d->columnWidth;
		int visibleFrameCount = d->visibleFrameCount();
		target.rawFrameIndex = qBound(0, frameIndex, visibleFrameCount - 1);
		if(x > headerWidth) {
			if(frameIndex >= visibleFrameCount) {
				return target;
			} else if(frameIndex >= 0) {
				target.frameIndex = frameIndex;
				target.side = (frameX % d->columnWidth) < d->columnWidth / 2
								  ? TargetSide::Left
								  : TargetSide::Right;
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
			int rgt = mid + ICON_SIZE + tp;
			if(x >= tph && x < mid) {
				target.action = TrackAction::ToggleVisible;
			} else if(x >= mid && x < rgt) {
				target.action = TrackAction::ToggleOnionSkin;
			} else if(x >= rgt && x < tp * 4 + ICON_SIZE * 3) {
				target.action = TrackAction::ToggleMoveLock;
			}
		} else {
			target.header = TargetHeader::Header;
			if(d->editable && !d->canvas->isCompatibilityMode()) {
				qreal xInFrame =
					qreal(frameX - (target.frameIndex * d->columnWidth));
				qreal handleInsetX = d->rangeHandleInsetX();
				if(xInFrame <= handleInsetX &&
				   target.frameIndex == d->frameRangeFirst()) {
					target.header = TargetHeader::RangeFirst;
				} else if(
					xInFrame >= qreal(d->columnWidth - 1) - handleInsetX &&
					target.frameIndex == d->frameRangeLast()) {
					target.header = TargetHeader::RangeLast;
				} else {
					target.header = TargetHeader::Header;
				}
			}
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
	bool onTrack = target.frameIndex == -1;
	d->pendingSelectionAction = PendingSelectionAction::None;

	if((!action && (onTrack || !d->isCurrentExposureTool())) || right) {
		// These conditions should reflect how Qt widgets usually behave with
		// regards to selections between using right-click or left-click with
		// modifiers. There's a slight difference here because our frames can be
		// key frames or not and only the former is draggable, so this adds a
		// few more cases where we have to handle the user clicking and sliding.
		SelectionAction selectionAction;
		if(target.frameIndex == -1) {
			selectionAction = SelectionAction::Retain;
		} else if(right) {
			selectionAction = SelectionAction::ReplaceIfNotSelected;
		} else if(d->isCurrentSelectTool()) {
			if(event && event->type() == QEvent::MouseButtonDblClick) {
				selectionAction = SelectionAction::Replace;
			} else {
				selectionAction = SelectionAction::Toggle;
				d->pendingSelectionAction = PendingSelectionAction::None;
			}
		} else if(event) {
			Qt::KeyboardModifiers mods = event->modifiers();
			if(mods.testFlag(Qt::ShiftModifier)) {
				selectionAction = SelectionAction::SelectCurrentRange;
			} else if(mods.testFlag(Qt::ControlModifier)) {
				selectionAction = SelectionAction::ToggleIfNotSelected;
				d->pendingSelectionAction = PendingSelectionAction::Deselect;
			} else {
				selectionAction = SelectionAction::ReplaceIfNotSelected;
				d->pendingSelectionAction = PendingSelectionAction::Replace;
			}
		} else {
			selectionAction = SelectionAction::ReplaceIfNotSelected;
		}

		int trackId = target.trackId == 0 ? d->currentTrackId : target.trackId;
		int frame = onTrack ? d->currentFrame : target.frameIndex;
		SetCurrentResult result =
			setCurrent(trackId, frame, true, event && !right, selectionAction);
		d->selectionPressIndex = result.idx;

		switch(selectionAction) {
		case SelectionAction::Replace:
			d->moveSelectionAction = SelectionAction::SelectCurrentRange;
			break;
		case SelectionAction::ReplaceIfNotSelected:
			d->moveSelectionAction = SelectionAction::ReplaceMove;
			break;
		case SelectionAction::Toggle:
			if(result.wasSelected) {
				d->moveSelectionAction = SelectionAction::DeselectCurrentRange;
			} else {
				d->moveSelectionAction = SelectionAction::SelectCurrentRange;
			}
			break;
		case SelectionAction::ToggleIfNotSelected:
			if(result.wasSelected) {
				d->moveSelectionAction = SelectionAction::DeselectRange;
			} else {
				d->moveSelectionAction = SelectionAction::SelectRange;
			}
			break;
		default:
			d->moveSelectionAction = selectionAction;
			break;
		}

		// A pending selection action is set by *IfNotSelected above. If nothing
		// was selected, there's nothing pending, so reset it again.
		if(!result.wasSelected || onTrack) {
			d->pendingSelectionAction = PendingSelectionAction::None;
		}

	} else {
		d->moveSelectionAction = SelectionAction::ReplaceMove;
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
		} else if(target.action == TrackAction::ToggleMoveLock) {
			emit trackMoveLockEnabled(track->id, !track->moveLock);
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
