#ifndef DRAWDANCE_PAINTENGINE_H
#define DRAWDANCE_PAINTENGINE_H

extern "C" {
#include <dpengine/paint_engine.h>
}

#include "canvasstate.h"
#include "drawcontextpool.h"
#include <QtGlobal>
#include <functional>

struct DP_PaintEngine;

namespace drawdance {

class AclState;
class SnapshotQueue;

enum RecordStartResult {
	RECORD_START_SUCCESS,
	RECORD_START_UNKNOWN_FORMAT,
	RECORD_START_OPEN_ERROR,
};

class PaintEngine {
public:
	using BuildIndexProgressFn = std::function<void (int)>;

	PaintEngine(
		AclState &acls, SnapshotQueue &sq, bool wantCanvasHistoryDump,
		DP_PaintEnginePlaybackFn playbackFn,
		DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
		const CanvasState &canvasState = CanvasState::null());

	~PaintEngine();

	PaintEngine(const PaintEngine &) = delete;
	PaintEngine(PaintEngine &&) = delete;
	PaintEngine &operator=(const PaintEngine &) = delete;
	PaintEngine &operator=(PaintEngine &&) = delete;

	DP_PaintEngine *get();

	void reset(AclState &acls, SnapshotQueue &sq, uint8_t localUserId,
		DP_PaintEnginePlaybackFn playbackFn,
		DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
		const CanvasState &canvasState = CanvasState::null(),
		DP_Player *player = nullptr);

	int renderThreadCount() const;

	LayerContent renderContent() const;

	void setLocalDrawingInProgress(bool localDrawingInProgress);

	void setWantCanvasHistoryDump(bool wantCanvasHistoryDump);

	void setActiveLayerId(int layerId);

	void setActiveFrameIndex(int frameIndex);

	void setViewMode(DP_ViewMode vm);

	void setOnionSkins(const DP_OnionSkins *oss);

	bool revealCensored() const;
	void setRevealCensored(bool revealCensored);

	void setInspectContextId(unsigned int contextId);

	void setLayerVisibility(int layerId, bool hidden);

	Tile localBackgroundTile() const;
	void setLocalBackgroundTile(const Tile &tile);

	RecordStartResult startRecorder(const QString &path);
	bool stopRecorder();
	bool recorderIsRecording() const;

	DP_PlayerResult stepPlayback(long long steps, MessageList &outMsgs);
	DP_PlayerResult skipPlaybackBy(long long steps, MessageList &outMsgs);
	DP_PlayerResult jumpPlaybackTo(long long position, MessageList &outMsgs);
	bool buildPlaybackIndex(BuildIndexProgressFn progressFn);
	bool loadPlaybackIndex();
	unsigned int playbackIndexMessageCount();
	size_t playbackIndexEntryCount();
	QImage playbackIndexThumbnailAt(size_t index);
	DP_PlayerResult stepDumpPlayback(MessageList &outMsgs);
	DP_PlayerResult jumpDumpPlaybackToPreviousReset(MessageList &outMsgs);
	DP_PlayerResult jumpDumpPlaybackToNextReset(MessageList &outMsgs);
	DP_PlayerResult jumpDumpPlayback(long long position, MessageList &outMsgs);
	bool closePlayback();

	void previewCut(int layerId, const QRect &bounds, const QImage &mask);
	void previewDabs(int layerId, int count, const Message *msgs);
	void clearPreview();

	CanvasState viewCanvasState() const;
	CanvasState historyCanvasState() const;
	CanvasState sampleCanvasState() const;

private:
	DrawContext m_paintDc;
	DrawContext m_previewDc;
	DP_PaintEngine *m_data;

	static QString getDumpDir();
	static long long getTimeMs(void *);
	static void pushMessage(void *user, DP_Message *msg);
	static bool shouldSnapshot(void *user);
	static void indexProgress(void *user, int percent);
};

}

#endif
