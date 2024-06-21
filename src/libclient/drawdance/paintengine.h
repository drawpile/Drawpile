// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWDANCE_PAINTENGINE_H
#define DRAWDANCE_PAINTENGINE_H
extern "C" {
#include <dpengine/paint_engine.h>
}
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/global.h"
#include <QtGlobal>
#include <functional>

class QColor;
struct DP_PaintEngine;

namespace drawdance {

class AclState;
class SnapshotQueue;

enum RecordStartResult {
	RECORD_START_SUCCESS,
	RECORD_START_UNKNOWN_FORMAT,
	RECORD_START_HEADER_ERROR,
	RECORD_START_OPEN_ERROR,
	RECORD_START_RECORDER_ERROR,
};

class PaintEngine {
public:
	using BuildIndexProgressFn = std::function<void(int)>;

	PaintEngine(
		AclState &acls, SnapshotQueue &sq, bool wantCanvasHistoryDump,
		bool rendererChecker, const QColor &checkerColor1,
		const QColor &checkerColor2, DP_RendererTileFn rendererTileFn,
		DP_RendererUnlockFn rendererUnlockFn,
		DP_RendererResizeFn rendererResizeFn, void *rendererUser,
		DP_CanvasHistorySoftResetFn softResetFn, void *softResetUser,
		DP_PaintEnginePlaybackFn playbackFn,
		DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
		const CanvasState &canvasState = CanvasState::null());

	~PaintEngine();

	PaintEngine(const PaintEngine &) = delete;
	PaintEngine(PaintEngine &&) = delete;
	PaintEngine &operator=(const PaintEngine &) = delete;
	PaintEngine &operator=(PaintEngine &&) = delete;

	DP_PaintEngine *get() const;

	net::MessageList reset(
		AclState &acls, SnapshotQueue &sq, uint8_t localUserId,
		bool rendererChecker, DP_RendererTileFn rendererTileFn,
		DP_RendererUnlockFn rendererUnlockFn,
		DP_RendererResizeFn rendererResizeFn, void *rendererUser,
		DP_CanvasHistorySoftResetFn softResetFn, void *softResetUser,
		DP_PaintEnginePlaybackFn playbackFn,
		DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
		const CanvasState &canvasState = CanvasState::null(),
		DP_Player *player = nullptr);

	int renderThreadCount() const;

	void setLocalDrawingInProgress(bool localDrawingInProgress);

	void setWantCanvasHistoryDump(bool wantCanvasHistoryDump);

	QSet<int> getLayersVisibleInFrame();

	int activeLayerId() const;

	int activeFrameIndex() const;

	DP_ViewMode viewMode() const;

	bool revealCensored() const;
	void setRevealCensored(bool revealCensored);

	DP_ViewModePick pick(int x, int y);

	void setInspect(unsigned int contextId, bool showTiles);
	void setCheckerColor1(const QColor &color1);
	void setCheckerColor2(const QColor &color2);
	bool checkersVisible() const;

	Tile localBackgroundTile() const;

	static RecordStartResult makeRecorderParameters(
		const QString &path, const QString &writer,
		const QString &writerVersion, const QString &type,
		DP_RecorderType &outRecorderType, JSON_Value *&outHeader);

	RecordStartResult startRecorder(
		const QString &path, const QString &writer,
		const QString &writerVersion, const QString &type);

	RecordStartResult exportTemplate(
		const QString &path, const net::MessageList &snapshot,
		const QString &writer, const QString &writerVersion,
		const QString &type);

	bool stopRecorder();
	bool recorderIsRecording() const;

	DP_PlayerResult stepPlayback(long long steps, net::MessageList &outMsgs);
	DP_PlayerResult skipPlaybackBy(
		long long steps, bool bySnapshots, net::MessageList &outMsgs);
	DP_PlayerResult
	jumpPlaybackTo(long long position, net::MessageList &outMsgs);
	DP_PlayerResult beginPlayback();
	DP_PlayerResult playPlayback(long long msecs, net::MessageList &outMsgs);
	bool buildPlaybackIndex(BuildIndexProgressFn progressFn);
	bool loadPlaybackIndex();
	unsigned int playbackIndexMessageCount();
	size_t playbackIndexEntryCount();
	QImage playbackIndexThumbnailAt(size_t index);
	DP_PlayerResult stepDumpPlayback(net::MessageList &outMsgs);
	DP_PlayerResult jumpDumpPlaybackToPreviousReset(net::MessageList &outMsgs);
	DP_PlayerResult jumpDumpPlaybackToNextReset(net::MessageList &outMsgs);
	DP_PlayerResult
	jumpDumpPlayback(long long position, net::MessageList &outMsgs);
	bool flushPlayback(net::MessageList &outMsgs);
	bool closePlayback();

	void previewCut(
		const QSet<int> &layerIds, const QRect &bounds, const QImage &mask);
	void clearCutPreview();
	void previewTransform(
		int id, int layerId, int blendMode, qreal opacity, int x, int y,
		const QImage &img, const QPolygon &dstPolygon, int interpolation);
	void clearTransformPreview(int id);
	void clearAllTransformPreviews();
	void previewDabs(int layerId, int count, const net::Message *msgs);
	void clearDabsPreview();
	void previewFill(
		int layerId, int blendMode, qreal opacity, int x, int y,
		const QImage &img);
	void clearFillPreview();

	CanvasState viewCanvasState() const;
	CanvasState historyCanvasState() const;
	CanvasState sampleCanvasState() const;

private:
	DrawContext m_paintDc;
	DrawContext m_mainDc;
	DrawContext m_previewDc;
	DP_PaintEngine *m_data;

	static QString getDumpDir();
	static long long getTimeMs(void *);
	static void pushMessage(void *user, DP_Message *msg);
	static bool pushResetMessage(void *user, DP_Message *msg);
	static bool shouldSnapshot(void *user);
	static void indexProgress(void *user, int percent);
	static void addLayerVisibleInFrame(void *user, int layerId, bool visible);

	static const DP_Pixel8 *getTransformPreviewPixels(void *user);
	static void disposeTransformPreviewPixels(void *user);
};

}

#endif
