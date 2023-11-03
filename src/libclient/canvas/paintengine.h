// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PAINTENGINE_H
#define PAINTENGINE_H
extern "C" {
#include <dpengine/draw_context.h>
}
#include "libclient/drawdance/aclstate.h"
#include "libclient/drawdance/canvashistory.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/paintengine.h"
#include "libclient/drawdance/snapshotqueue.h"
#include <QObject>
#include <QPainter>
#include <QPixmap>
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
		int fps, int snapshotMaxCount, long long snapshotMinDelayMs,
		bool wantCanvasHistoryDump, QObject *parent = nullptr);

	~PaintEngine() override;

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
	void withPixmap(std::function<void(const QPixmap &)> fn) const;

	// Renders the whole canvas and returns it as an image. A slow operation!
	QImage renderPixmap();

	void setCanvasViewArea(const QRect &area);

	void setRenderOutsideView(bool renderOutsideView)
	{
		m_renderOutsideView = renderOutsideView;
	}

	//! Get the number of frames in an animated canvas
	int frameCount() const;

	//! Get a layer as an image
	//! An id of 0 means to flatten the whole image including the background,
	//! -1 means flatten without the background, other ids are taken as actual
	//! layer ids. Returns a null image if the layer wasn't found, it was a
	//! group or the canvas size is empty.
	QImage getLayerImage(int id, const QRect &rect = QRect()) const;

	//! Render a frame
	QImage getFrameImage(
		drawdance::ViewModeBuffer &vmb, int index,
		const QRect &rect = QRect()) const;

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
	QColor backgroundColor() const;

	int undoDepthLimit() const;

	//! Get the color of the local background tile, returning if one exists
	bool localBackgroundColor(QColor &outColor) const;
	void setLocalBackgroundColor(const QColor &color);
	void clearLocalBackgroundColor();

	//! Find an unused ID for a new annotation
	uint16_t findAvailableAnnotationId(uint8_t forUser) const;

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
	void setTrackVisibility(int trackId, bool hidden);
	void setTrackOnionSkin(int trackId, bool onionSkin);

	//! Set layerstack rendering mode (normal, solo, frame)
	void setViewMode(DP_ViewMode vm, bool censor);

	//! Is the "censor" view mode flag set?
	bool revealCensored() const;

	//! Set the active view layer (for solo mode)
	void setViewLayer(int id);

	//! Set the active view frame (for frame and onionskin modes)
	void setViewFrame(int frame);

	void setOnionSkins(
		bool wrap, const QVector<QPair<float, QColor>> &skinsBelow,
		const QVector<QPair<float, QColor>> &skinsAbove);

	int pickLayer(int x, int y);
	unsigned int pickContextId(int x, int y);

	void setInspect(unsigned int contextId, bool showTiles);

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

	void previewCut(int layerId, const QRect &bounds, const QImage &mask);
	void clearCutPreview();
	void previewTransform(
		int layerId, int x, int y, const QImage &img,
		const QPolygon &dstPolygon, int interpolation);
	void clearTransformPreview();
	void previewDabs(int layerId, const net::MessageList &msgs);
	void clearDabsPreview();

signals:
	void areaChanged(const QRect &area);
	void resized(int xoffset, int yoffset, const QSize &oldSize);
	void layersChanged(const drawdance::LayerPropsList &lpl);
	void annotationsChanged(const drawdance::AnnotationList &al);
	void
	cursorMoved(unsigned int flags, uint8_t user, uint16_t layer, int x, int y);
	void playbackAt(long long pos);
	void
	dumpPlaybackAt(long long pos, const drawdance::CanvasHistorySnapshot &chs);
	void caughtUpTo(int progress);
	void resetLockSet(bool locked);
	void recorderStateChanged(bool started);
	void documentMetadataChanged(const drawdance::DocumentMetadata &dm);
	void timelineChanged(const drawdance::Timeline &tl);
	void frameVisibilityChanged(const QSet<int> layers, bool frameMode);
	void aclsChanged(
		const drawdance::AclState &acls, int aclChangeFlags, bool reset);
	void laserTrail(uint8_t userId, int persistence, uint32_t color);
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
	static void onAclsChanged(void *user, int aclChangeFlags);
	static void onLaserTrail(
		void *user, unsigned int contextId, int persistence, uint32_t color);
	static void onMovePointer(void *user, unsigned int contextId, int x, int y);
	static void onDefaultLayer(void *user, int layerId);
	static void onUndoDepthLimitSet(void *user, int undoDepthLimit);
	static void onCatchup(void *user, int progress);
	static void onResetLockChanged(void *user, bool locked);
	static void onRecorderStateChanged(void *user, bool started);
	static void onLayerPropsChanged(void *user, DP_LayerPropsList *lpl);
	static void onAnnotationsChanged(void *user, DP_AnnotationList *al);
	static void onDocumentMetadataChanged(void *user, DP_DocumentMetadata *dm);
	static void onTimelineChanged(void *user, DP_Timeline *tl);
	static void onCursorMoved(
		void *user, unsigned int flags, unsigned int contextId, int layerId,
		int x, int y);

	static void
	onRenderTile(void *user, int tileX, int tileY, DP_Pixel8 *pixels);

	static void onRenderUnlock(void *user);

	static void onRenderResize(
		void *user, int width, int height, int prevWidth, int prevHeight,
		int offsetX, int offsetY);

	void start();

	void updateLayersVisibleInFrame();

	drawdance::AclState m_acls;
	drawdance::SnapshotQueue m_snapshotQueue;
	drawdance::PaintEngine m_paintEngine;
	int m_fps;
	int m_timerId;
	QPixmap m_cache;
	QPainter m_painter;
	DP_Mutex *m_cacheMutex;
	DP_Semaphore *m_viewSem;
	QRect m_canvasViewTileArea;
	bool m_renderOutsideView = false;
	uint16_t m_sampleColorStampBuffer[DP_DRAW_CONTEXT_STAMP_BUFFER_SIZE];
	int m_sampleColorLastDiameter;
	int m_undoDepthLimit;
	bool m_updateLayersVisibleInFrame;
#ifdef DP_HAVE_BUILTIN_SERVER
	server::BuiltinServer *m_server = nullptr;
#endif
};

}

#endif
