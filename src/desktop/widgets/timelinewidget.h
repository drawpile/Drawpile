// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H
#include <QColor>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QVector>
#include <QWidget>
#include <functional>

class QAction;
class QActionGroup;
class QMenu;

namespace canvas {
class CanvasModel;
}

namespace net {
class Message;
}

namespace widgets {

class TimelineWidget final : public QWidget {
	Q_OBJECT
public:
	static constexpr int MIN_COLUMN_WIDTH = 6;
	static constexpr int MAX_COLUMN_WIDTH = 80;

	struct Actions {
		QActionGroup *timelineToolGroup = nullptr;
		QAction *timelineToolNormal = nullptr;
		QAction *timelineToolSelect = nullptr;
		QAction *timelineToolExposure = nullptr;
		QAction *timelineZoomIn = nullptr;
		QAction *timelineZoomOut = nullptr;
		QAction *timelineZoomReset = nullptr;
		QAction *keyFrameSetLayer = nullptr;
		QAction *keyFrameSetEmpty = nullptr;
		QAction *keyFrameCreateLayer = nullptr;
		QAction *keyFrameCreateLayerNext = nullptr;
		QAction *keyFrameCreateLayerPrev = nullptr;
		QAction *keyFrameCreateGroup = nullptr;
		QAction *keyFrameCreateGroupNext = nullptr;
		QAction *keyFrameCreateGroupPrev = nullptr;
		QAction *keyFrameDuplicateNext = nullptr;
		QAction *keyFrameDuplicatePrev = nullptr;
		QAction *keyFrameCut = nullptr;
		QAction *keyFrameCopy = nullptr;
		QAction *keyFramePaste = nullptr;
		QAction *keyFramePasteDeclone = nullptr;
		QAction *keyFrameProperties = nullptr;
		QAction *keyFrameDeleteLayer = nullptr;
		QAction *keyFrameUnassign = nullptr;
		QAction *keyFrameDeclone = nullptr;
		QAction *keyFrameExposureIncrease = nullptr;
		QAction *keyFrameExposureIncreaseAll = nullptr;
		QAction *keyFrameExposureDecrease = nullptr;
		QAction *keyFrameExposureDecreaseAll = nullptr;
		QAction *trackAdd = nullptr;
		QAction *trackVisible = nullptr;
		QAction *trackOnionSkin = nullptr;
		QAction *trackMoveLock = nullptr;
		QAction *trackDuplicate = nullptr;
		QAction *trackRetitle = nullptr;
		QAction *trackDelete = nullptr;
		QAction *animationProperties = nullptr;
		QAction *frameNext = nullptr;
		QAction *framePrev = nullptr;
		QAction *frameNextClamp = nullptr;
		QAction *framePrevClamp = nullptr;
		QAction *keyFrameNext = nullptr;
		QAction *keyFramePrev = nullptr;
		QAction *trackAbove = nullptr;
		QAction *trackBelow = nullptr;
		QMenu *animationKeyFrameColorMenu = nullptr;
		QMenu *animationLayerMenu = nullptr;
		QMenu *animationGroupMenu = nullptr;
		QMenu *animationDuplicateMenu = nullptr;
	};

	struct SelectedFrame {
		int trackId;
		int frameIndex;
	};

	explicit TimelineWidget(QWidget *parent = nullptr);
	~TimelineWidget() override;

	void setCanvas(canvas::CanvasModel *canvas);

	void setActions(const Actions &actions);

	void setCurrentFrame(int frame);
	void setCurrentTrack(int trackId);
	void setCurrentLayer(int layerId);

	void updateControlsEnabled(bool access, bool locked);
	void updateKeyFrameColorMenuIcon();

	canvas::CanvasModel *canvas() const;

	int frameCount() const;
	int currentTrackId() const;
	int currentFrame() const;

	// Used in the layer list dock to create layers.
	QVector<SelectedFrame> selectedNonKeyFrames() const;

	int columnWidth() const;
	void setColumnWidth(int columnWidth);

signals:
	void timelineEditCommands(int count, const net::Message *msgs);
	void trackSelected(int trackId);
	void frameSelected(int frame);
	void layerSelected(int layerId);
	void blankLayerSelected();
	void trackHidden(int trackId, bool hidden);
	void trackOnionSkinEnabled(int trackId, bool onionSkin);
	void trackMoveLockEnabled(int trackId, bool moveLock);
	void frameViewModeRequested();
	void columnWidthChanged(int columnWidth);

protected:
	bool event(QEvent *event) override;
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void leaveEvent(QEvent *event) override;

private:
	static constexpr char KEY_FRAMES_MIME_TYPE[] = "x-drawpile/keyframes";
	static constexpr int TRACK_PADDING = 4;
	static constexpr int ICON_SIZE = 16;
	static constexpr int EXTRA_FRAME_COUNT = 101;
	static constexpr int EXTRA_VISIBLE_FRAME_COUNT = 11;

	enum class Drag { None, Track, KeyFrame };

	enum class SelectionAction {
		Retain,
		Replace,
		ReplaceIfNotSelected,
		ReplaceMove,
		Toggle,
		ToggleIfNotSelected,
		SelectCurrentRange,
		DeselectCurrentRange,
		SelectRange,
		DeselectRange,
	};

	enum class PendingSelectionAction {
		None,
		Replace,
		Deselect,
	};

	struct SetCurrentResult {
		QModelIndex idx;
		bool wasSelected;
	};

	struct KeyFrameDrag {
		int fromTrackId;
		int fromFrameIndex;
		int toTrackId;
		int toTrackIndex;
		int toFrameIndex;
	};

	void setKeyFramesLayer();
	void setKeyFramesEmpty();
	void cutKeyFrames();
	void copyKeyFrames();
	void pasteKeyFrames();
	void pasteKeyFramesDeclone();
	void pasteKeyFramesWith(bool declone);
	void setKeyFramesColor(QAction *action);
	void showKeyFrameProperties();
	void keyFramePropertiesChanged(
		int trackId, int frame, const QColor &color, const QString &title,
		const QHash<int, bool> &layerVisibility);
	void deleteKeyFramesWith(bool deleteUnusedLayers);
	void deleteKeyFramesLayers();
	void unassignKeyFrames();
	void decloneKeyFrames();
	void increaseKeyFrameExposure();
	void increaseKeyFrameExposureAll();
	void decreaseKeyFrameExposure();
	void decreaseKeyFrameExposureAll();
	void addTrack();
	void toggleTrackVisible(bool visible);
	void toggleTrackOnionSkin(bool onionSkin);
	void toggleTrackMoveLock(bool moveLock);
	void duplicateTrack();
	void retitleTrack();
	void deleteTrack();
	void showAnimationProperties();
	void setAnimationProperties(
		double framerate, int frameRangeFirst, int frameRangeLast);
	void nextFrame();
	void prevFrame();
	void nextFrameClamp();
	void prevFrameClamp();
	void nextFrameWith(bool clamp, bool select);
	void prevFrameWith(bool clamp, bool select);
	void nextKeyFrame();
	void prevKeyFrame();
	void trackAbove();
	void trackBelow();
	void trackAboveWith(bool select);
	void trackBelowWith(bool select);
	void switchTool(QAction *action);
	void updateTracks();
	void updateFrameCount();
	void updateFrameRange();
	void setHorizontalScroll(int pos);
	void setVerticalScroll(int pos);
	void finishExposureTool();
	void updatePasteAction();
	void updateCursor();
	void zoomIn();
	void zoomOut();
	void resetZoom();
	void zoomBy(int delta);
	void setZoomAdjust(int zoomAdjust);
	void onSelectionChanged();

	SetCurrentResult setCurrent(
		int trackId, int frame, bool triggerUpdate, bool selectLayer,
		SelectionAction selectionAction);

	QItemSelectionModel::SelectionFlags setCurrentSelectRange(
		const QModelIndex &idx,
		QItemSelectionModel::SelectionFlags rangeSelectionFlags);

	bool setCurrentCheckPending(const QModelIndex &idx);

	void executePendingSelectionAction();

	void setKeyFrames(int layerId);
	void setKeyFrameProperties(
		int trackId, int frame, const QString &prevTitle,
		const QHash<int, bool> prevLayerVisibility, const QString &title,
		const QHash<int, bool> layerVisibility);
	bool
	changeFrameExposure(int start, int offset, const QVector<int> &trackIds);
	bool checkAndCollectFrameExposure(
		QVector<QPair<int, QVector<int>>> &trackFrameIndexes, bool forward,
		int trackId);
	void collectFrameExposure(
		QVector<QPair<int, QVector<int>>> &trackFrameIndexes, int start,
		int trackId);
	void updateActions();
	void updateScrollbars();
	void startFrameViewRequestTimer();

	bool checkKeyFrameDrag(const QDropEvent *event, bool updateSelection);
	void shiftSelectionBy(int deltaTrackIndex, int deltaFrameIndex);

	struct Target;
	Target getMouseTarget(const QPoint &pos) const;
	void applyMouseTarget(QMouseEvent *event, const Target &target, bool press);
	void executeTargetAction(const Target &target);

	void emitCommand(std::function<net::Message(uint8_t)> getMessage);

	static void setCheckedSignalBlocked(QAction *action, bool checked);

	struct Private;
	Private *d;
};

}

#endif // TIMELINEWIDGET_H
