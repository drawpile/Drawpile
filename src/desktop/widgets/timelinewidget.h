// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_TIMELINEWIDGET_H
#define DESKTOP_WIDGETS_TIMELINEWIDGET_H
#include <QColor>
#include <QTableView>
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

class TimelineWidget final : public QTableView {
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

	canvas::CanvasModel *canvas() const;
	void setCanvas(canvas::CanvasModel *canvas);

	void setActions(const Actions &actions);

	void setCurrentFrameIndex(int frame);
	void setCurrentTrackId(int trackId);
	void setCurrentTrackIndex(int trackIndex);
	void setCurrentLayer(int layerId);

	void updateControlsEnabled(bool access, bool locked);
	void updateKeyFrameColorMenuIcon();

	int frameCount() const;
	int currentTrackId() const;
	int currentFrame() const;

	void changeFramerate(int framerate);
	void changeFrameCount(int frameCount);

signals:
	void timelineEditCommands(int count, const net::Message *msgs);
	void trackSelected(int trackId);
	void frameSelected(int frame);
	void layerSelected(int layerId);
	void trackHidden(int trackId, bool hidden);
	void trackOnionSkinEnabled(int trackId, bool onionSkin);

private:
	static constexpr char KEY_FRAME_MIME_TYPE[] = "x-drawpile/keyframe";
	static constexpr int TRACK_PADDING = 4;
	static constexpr int ICON_SIZE = 16;

	void addTrack();

	void emitCommand(std::function<net::Message(uint8_t)> getMessage);

	struct Private;
	Private *d;
};

}

#endif // TIMELINEWIDGET_H
