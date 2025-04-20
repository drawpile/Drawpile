// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PAINTENGINE_H
#define PAINTENGINE_H
extern "C" {
#include <dpengine/draw_context.h>
}
#include "libclient/canvas/tilecache.h"
#include "libclient/drawdance/aclstate.h"
#include "libclient/drawdance/canvashistory.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/paintengine.h"
#include "libclient/drawdance/selectionset.h"
#include "libclient/drawdance/snapshotqueue.h"
#include <QColor>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <functional>

struct DP_Mutex;
struct DP_Semaphore;

namespace drawdance {
class LayerPropsList;
class ViewModeBuffer;
}

namespace server {
class BuiltinServer;
}

namespace canvas {

class PaintEngine final : public QObject {
	Q_OBJECT
public:
	PaintEngine(
		int canvasImplementation, const QColor &checkerColor1,
		const QColor &checkerColor2, const QColor &selectionColor, int fps,
		int snapshotMaxCount, long long snapshotMinDelayMs,
		bool wantCanvasHistoryDump, QObject *parent = nullptr);

	~PaintEngine() override;

	bool useTileCache() const { return m_useTileCache; }

	bool isTileCacheDirtyCheckOnTick() const
	{
		return m_tileCacheDirtyCheckOnTick;
	}

	void setFps(int fps);
	void setSnapshotMaxCount(int snapshotMaxCount);
	void setSnapshotMinDelayMs(long long snapshotMinDelayMs);
	void setWantCanvasHistoryDump(bool wantCanvasHistoryDump);

	/// Reset the paint engine to its default state
	void reset(
		uint8_t localUserId,
		const drawdance::CanvasState &canvasState =
			drawdance::CanvasState::null(),
		DP_Player *player = nullptr);

	// Do something with the currently rendered canvas pixmap. This is what the
	// user currently sees, but may have "unfinished" parts. If you need the
	// full, proper canvas at the current time, use renderPixmap instead.
	void withPixmap(const std::function<void(const QPixmap &)> &fn) const;

	// Renders the whole canvas and returns it as an image. A slow operation!
	QImage renderPixmap();

	void withTileCache(const std::function<void(TileCache &)> &fn);

	void setCanvasViewArea(const QRect &area);
	void setCanvasViewTileArea(const QRect &canvasViewTileArea);

	void setRenderOutsideView(bool renderOutsideView);

	//! Get the number of frames in an animated canvas
	int frameCount() const;

	DP_ViewModeFilter viewModeFilter(
		drawdance::ViewModeBuffer &vmb,
		const drawdance::CanvasState &canvasState) const;

	//! Get a flat image of the given canvas state using the current view mode.
	QImage getFlatImage(
		drawdance::ViewModeBuffer &vmb,
		const drawdance::CanvasState &canvasState, bool includeBackground,
		bool includeSublayers, const QRect *rect = nullptr) const;

	//! Get a layer as an image
	//! An id of 0 means to flatten the whole image including the background,
	//! -1 means flatten without the background, other ids are taken as actual
	//! layer ids. Returns a null image if the layer wasn't found, it was a
	//! group or the canvas size is empty.
	QImage getLayerImage(int id, const QRect &rect = QRect()) const;

	//! Receive and handle messages, returns how many messages were actually
	//! pushed to the paint engine.
	int receiveMessages(
		bool local, int count, const net::Message *msgs,
		bool overrideAcls = false);

	void enqueueReset();

	void enqueueLoadBlank(
		int undoDepthLimit, const QSize &size, const QColor &backgroundColor);

	//! Enqueue a "catchup progress" marker.
	//! Will trigger the emission of caughtUpTo signal once the marker
	//! has been processed by the paint engine.
	void enqueueCatchupProgress(int progress);

	void resetAcl(uint8_t localUserId);

	//! Clean up dangling state after disconnecting from a remote session
	void cleanup();

	//! Get the color of the background tile
	QColor historyBackgroundColor() const;
	QColor viewBackgroundColor() const;

	int undoDepthLimit() const;

	//! Get the color of the local background tile, returning if one exists
	bool localBackgroundColor(QColor &outColor) const;
	void setLocalBackgroundColor(const QColor &color);
	void clearLocalBackgroundColor();

	//! Get the annotation at the given point
	drawdance::Annotation getAnnotationById(int annotationId) const;

	//! Get the annotation at the given point
	drawdance::Annotation getAnnotationAt(int x, int y, int expand) const;

	//! Is OpenRaster file format needed to save the canvas losslessly?
	bool needsOpenRaster() const;

	/*
	 * @brief Set the "local user is currently drawing!" hint
	 *
	 * This affects the way the retcon local fork is handled: when
	 * local drawing is in progress, the local fork is not discarded
	 * on conflict (with other users) to avoid self-conflict feedback loop.
	 *
	 * Not setting this flag doesn't break anything, but may cause
	 * unnecessary rollbacks if a conflict occurs during local drawing.
	 */
	void setLocalDrawingInProgress(bool localDrawingInProgress);

	void setLayerVisibility(int layerId, bool hidden);
	void setLayerSketch(int layerId, uint16_t opacity, const QColor &tint);
	void setTrackVisibility(int trackId, bool hidden);
	void setTrackOnionSkin(int trackId, bool onionSkin);

	//! Set layerstack rendering mode (normal, solo, frame)
	void setViewMode(DP_ViewMode vm, bool censor);
	DP_ViewMode viewMode() const;

	//! Is the "censor" view mode flag set?
	bool revealCensored() const;

	//! Set the active view layer (for solo mode)
	void setViewLayer(int id);
	int viewLayer() const;

	//! Set the active view frame (for frame and onionskin modes)
	void setViewFrame(int frame);
	int viewFrame() const;

	void setOnionSkins(
		bool wrap, const QVector<QPair<float, QColor>> &skinsBelow,
		const QVector<QPair<float, QColor>> &skinsAbove);

	int pickLayer(int x, int y);
	unsigned int pickContextId(int x, int y);

	void setInspect(unsigned int contextId, bool showTiles);
	void setCheckerColor1(const QColor &color1);
	void setCheckerColor2(const QColor &color2);
	void setSelectionColor(const QColor &color);
	void setShowSelectionMask(bool showSelectionMask);
	void setSelectionEditActive(bool selectionEditActive);
	void setTransformActive(bool transformActive);
	bool checkersVisible() const;

	//! The current canvas state with the local view (hidden layers, local
	//! background) applied.
	drawdance::CanvasState viewCanvasState() const
	{
		return m_paintEngine.viewCanvasState();
	}
	//! The current canvas state as it came out of the canvas history.
	drawdance::CanvasState historyCanvasState() const
	{
		return m_paintEngine.historyCanvasState();
	}
	//! Grabs the very latest canvas state out of the canvas history for color
	//! sampling. This involves taking a lock, so it's slower than the above.
	//! However, it gives a more up-to-date result, relevant for brushes.
	//! It would be even more up to date if we ran brush strokes through the
	//! paint thread to ensure up-to-dateness, but not doing that for now.
	drawdance::CanvasState sampleCanvasState() const
	{
		return m_paintEngine.sampleCanvasState();
	}

	const drawdance::SnapshotQueue &snapshotQueue() const
	{
		return m_snapshotQueue;
	}

	const drawdance::AclState &aclState() const { return m_acls; }

	QColor sampleColor(int x, int y, int layerId, int diameter);

#ifdef DP_HAVE_BUILTIN_SERVER
	void setServer(server::BuiltinServer *server);
#endif

	drawdance::RecordStartResult startRecording(const QString &path);
	drawdance::RecordStartResult
	exportTemplate(const QString &path, const net::MessageList &snapshot);
	bool stopRecording();
	bool isRecording() const;

	DP_PlayerResult stepPlayback(long long steps);
	DP_PlayerResult skipPlaybackBy(long long steps, bool bySnapshots);
	DP_PlayerResult jumpPlaybackTo(long long position);
	DP_PlayerResult beginPlayback();
	DP_PlayerResult playPlayback(long long msecs);
	bool
	buildPlaybackIndex(drawdance::PaintEngine::BuildIndexProgressFn progressFn);
	bool loadPlaybackIndex();
	unsigned int playbackIndexMessageCount();
	size_t playbackIndexEntryCount();
	QImage playbackIndexThumbnailAt(size_t index);
	DP_PlayerResult stepDumpPlayback();
	DP_PlayerResult jumpDumpPlaybackToPreviousReset();
	DP_PlayerResult jumpDumpPlaybackToNextReset();
	DP_PlayerResult jumpDumpPlayback(long long position);
	bool flushPlayback();
	bool closePlayback();

	void previewCut(
		const QSet<int> &layerIds, const QRect &bounds, const QImage &mask);
	void clearCutPreview();
	void previewTransform(
		int id, int layerId, int blendMode, qreal opacity, int x, int y,
		const QImage &img, const QPolygon &dstPolygon, int interpolation);
	void clearTransformPreview(int id);
	void clearAllTransformPreviews();
	void previewDabs(int layerId, const net::MessageList &msgs);
	void clearDabsPreview();
	void previewFill(
		int layerId, int blendMode, qreal opacity, int x, int y,
		const QImage &img);
	void clearFillPreview();

signals:
	// Only emitted in tile cache mode.
	void tileCacheDirtyCheckNeeded();
	// Only emitted in tile cache mode.
	void tileCacheNavigatorDirtyCheckNeeded();

	// Only emitted in pixmap mode.
	void areaChanged(const QRect &area);
	// Only emitted in pixmap mode.
	void
	resized(const QSize &newSize, const QPoint &offset, const QSize &oldSize);

	void layersChanged(
		const drawdance::LayerPropsList &lpl, const QSet<int> &revealedLayers);
	void annotationsChanged(const drawdance::AnnotationList &al);
	void cursorMoved(unsigned int flags, int userId, int layerId, int x, int y);
	void playbackAt(long long pos);
	void
	dumpPlaybackAt(long long pos, const drawdance::CanvasHistorySnapshot &chs);
	void streamResetStarted(
		const drawdance::CanvasState &cs, const QString &correlator);
	void caughtUpTo(int progress);
	void resetLockSet(bool locked);
	void recorderStateChanged(bool started);
	void documentMetadataChanged(const drawdance::DocumentMetadata &dm);
	void timelineChanged(const drawdance::Timeline &tl);
	void selectionsChanged(const drawdance::SelectionSet &ss);
	void frameVisibilityChanged(const QSet<int> layers, int viewMode);
	void aclsChanged(
		const drawdance::AclState &acls, int aclChangeFlags, bool reset);
	void laserTrail(int userId, int persistence, uint32_t color);
	void defaultLayer(uint16_t layerId);
	void undoDepthLimitSet(int undoDepthLimit);

protected:
	void timerEvent(QTimerEvent *) override;

private slots:
#ifdef DP_HAVE_BUILTIN_SERVER
	void unsetServer();
#endif

private:
#ifdef DP_HAVE_BUILTIN_SERVER
	static void
	onSoftReset(void *user, unsigned int contextId, DP_CanvasState *cs);
#endif
	static void onPlayback(void *user, long long position);
	static void onDumpPlayback(
		void *user, long long position, DP_CanvasHistorySnapshot *chs);
	static void onStreamResetStart(
		void *user, DP_CanvasState *cs, size_t correlatorLength,
		const char *correlator);
	static void onAclsChanged(void *user, int aclChangeFlags);
	static void onLaserTrail(
		void *user, unsigned int contextId, int persistence, uint32_t color);
	static void onMovePointer(void *user, unsigned int contextId, int x, int y);
	static void onDefaultLayer(void *user, int layerId);
	static void onUndoDepthLimitSet(void *user, int undoDepthLimit);
	static void onCensoredLayerRevealed(void *user, int layerId);
	static void onCatchup(void *user, int progress);
	static void onResetLockChanged(void *user, bool locked);
	static void onRecorderStateChanged(void *user, bool started);
	static void onLayerPropsChanged(void *user, DP_LayerPropsList *lpl);
	static void onAnnotationsChanged(void *user, DP_AnnotationList *al);
	static void onDocumentMetadataChanged(void *user, DP_DocumentMetadata *dm);
	static void onTimelineChanged(void *user, DP_Timeline *tl);
	static void onSelectionsChanged(void *user, DP_SelectionSet *ssOrNull);
	static void onCursorMoved(
		void *user, unsigned int flags, unsigned int contextId, int layerId,
		int x, int y);

	static void
	onRenderTileToPixmap(void *user, int tileX, int tileY, DP_Pixel8 *pixels);

	static void onRenderTileToTileCache(
		void *user, int tileX, int tileY, DP_Pixel8 *pixels);

	static void onRenderUnlock(void *user);

	static void onRenderResizePixmap(
		void *user, int width, int height, int prevWidth, int prevHeight,
		int offsetX, int offsetY);

	static void onRenderResizeTileCache(
		void *user, int width, int height, int prevWidth, int prevHeight,
		int offsetX, int offsetY);

	void start();

	void updateLayersVisibleInFrame();

	void updateSelectionColor();

	uint16_t *getSampleColorStampBuffer(int diameter);

	const bool m_useTileCache;
	const bool m_tileCacheDirtyCheckOnTick;
	drawdance::AclState m_acls;
	drawdance::SnapshotQueue m_snapshotQueue;
	drawdance::PaintEngine m_paintEngine;
	int m_fps;
	int m_timerId;
	QSet<int> m_revealedLayers;
	QPixmap m_cache;
	QPainter m_painter;
	TileCache m_tileCache;
	DP_Mutex *m_cacheMutex;
	DP_Semaphore *m_viewSem;
	QRect m_canvasViewTileArea;
	bool m_renderOutsideView = false;
	uint16_t *m_sampleColorStampBuffer = nullptr;
	size_t m_sampleColorStampBufferCapacity = 0;
	int m_sampleColorLastDiameter = -1;
	int m_undoDepthLimit;
	QColor m_selectionColor;
	bool m_showSelectionMask = false;
	bool m_selectionEditActive = false;
	bool m_transformActive = false;
	bool m_updateLayersVisibleInFrame;
#ifdef DP_HAVE_BUILTIN_SERVER
	server::BuiltinServer *m_server = nullptr;
#endif
};

}

#endif
