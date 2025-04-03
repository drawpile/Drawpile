// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H
#include <QColor>
#include <QWidget>
#include <functional>

class QAction;
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
	struct Actions {
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
		QAction *keyFrameProperties = nullptr;
		QAction *keyFrameDelete = nullptr;
		QAction *keyFrameExposureIncrease = nullptr;
		QAction *keyFrameExposureDecrease = nullptr;
		QAction *trackAdd = nullptr;
		QAction *trackVisible = nullptr;
		QAction *trackOnionSkin = nullptr;
		QAction *trackDuplicate = nullptr;
		QAction *trackRetitle = nullptr;
		QAction *trackDelete = nullptr;
		QAction *frameCountSet = nullptr;
		QAction *framerateSet = nullptr;
		QAction *frameNext = nullptr;
		QAction *framePrev = nullptr;
		QAction *keyFrameNext = nullptr;
		QAction *keyFramePrev = nullptr;
		QAction *trackAbove = nullptr;
		QAction *trackBelow = nullptr;
		QMenu *animationKeyFrameColorMenu = nullptr;
		QMenu *animationLayerMenu = nullptr;
		QMenu *animationGroupMenu = nullptr;
		QMenu *animationDuplicateMenu = nullptr;
	};

	explicit TimelineWidget(QWidget *parent = nullptr);
	~TimelineWidget() override;

	void setCanvas(canvas::CanvasModel *canvas);

	void setActions(const Actions &actions);

	void setCurrentFrame(int frame);
	void setCurrentTrack(int trackId);
	void setCurrentLayer(int layerId);

	void
	updateControlsEnabled(bool access, bool locked, bool compatibilityMode);
	void updateKeyFrameColorMenuIcon();

	canvas::CanvasModel *canvas() const;

	int frameCount() const;
	int currentTrackId() const;
	int currentFrame() const;

public slots:
	void changeFramerate(int framerate);
	void changeFrameCount(int frameCount);

signals:
	void timelineEditCommands(int count, const net::Message *msgs);
	void trackSelected(int trackId);
	void frameSelected(int frame);
	void layerSelected(int layerId);
	void trackHidden(int trackId, bool hidden);
	void trackOnionSkinEnabled(int trackId, bool onionSkin);

protected:
	bool event(QEvent *event) override;
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private slots:
	void setKeyFrameLayer();
	void setKeyFrameEmpty();
	void cutKeyFrame();
	void copyKeyFrame();
	void pasteKeyFrame();
	void setKeyFrameColor(QAction *action);
	void showKeyFrameProperties();
	void keyFramePropertiesChanged(
		int trackId, int frame, const QColor &color, const QString &title,
		const QHash<int, bool> &layerVisibility);
	void deleteKeyFrame();
	void increaseKeyFrameExposure();
	void decreaseKeyFrameExposure();
	void addTrack();
	void toggleTrackVisible(bool visible);
	void toggleTrackOnionSkin(bool onionSkin);
	void duplicateTrack();
	void retitleTrack();
	void deleteTrack();
	void setFrameCount();
	void setFramerate();
	void nextFrame();
	void prevFrame();
	void nextKeyFrame();
	void prevKeyFrame();
	void trackAbove();
	void trackBelow();
	void updateTracks();
	void updateFrameCount();
	void setHorizontalScroll(int pos);
	void setVerticalScroll(int pos);
	void updatePasteAction();

private:
	static constexpr char KEY_FRAME_MIME_TYPE[] = "x-drawpile/keyframe";
	static constexpr int TRACK_PADDING = 4;
	static constexpr int ICON_SIZE = 16;

	enum class Drag { None, Track, KeyFrame };

	void
	setCurrent(int trackId, int frame, bool triggerUpdate, bool selectLayer);

	void setKeyFrame(int layerId);
	void setKeyFrameProperties(
		int trackId, int frame, const QString &prevTitle,
		const QHash<int, bool> prevLayerVisibility, const QString &title,
		const QHash<int, bool> layerVisibility);
	void changeFrameExposure(int direction);
	void updateActions();
	void updateScrollbars();

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
