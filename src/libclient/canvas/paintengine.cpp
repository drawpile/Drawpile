// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/threading.h>
#include <dpengine/layer_routes.h>
#include <dpengine/paint_engine.h>
#include <dpengine/recorder.h>
#include <dpengine/tile.h>
#include <dpmsg/msg_internal.h>
}
#include "cmake-config/config.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/image.h"
#include "libclient/drawdance/layercontent.h"
#include "libclient/drawdance/layerpropslist.h"
#include "libclient/drawdance/perf.h"
#include "libclient/drawdance/viewmode.h"
#include "libclient/net/message.h"
#include <QPainter>
#include <QSet>
#include <QTimer>
#include <QtEndian>
#ifdef DP_HAVE_BUILTIN_SERVER
#	include "libclient/server/builtinserver.h"
#	define ON_SOFT_RESET_FN PaintEngine::onSoftReset
#else
#	define ON_SOFT_RESET_FN nullptr
#endif

#define DP_PERF_CONTEXT "paint_engine"

namespace canvas {

PaintEngine::PaintEngine(
	int fps, int snapshotMaxCount, long long snapshotMinDelayMs,
	bool wantCanvasHistoryDump, QObject *parent)
	: QObject(parent)
	, m_acls{}
	, m_snapshotQueue{snapshotMaxCount, snapshotMinDelayMs}
	, m_paintEngine(
		  m_acls, m_snapshotQueue, wantCanvasHistoryDump,
		  PaintEngine::onRenderTile, PaintEngine::onRenderUnlock,
		  PaintEngine::onRenderResize, this, ON_SOFT_RESET_FN, this,
		  PaintEngine::onPlayback, PaintEngine::onDumpPlayback, this)
	, m_fps{fps}
	, m_timerId{0}
	, m_cache{}
	, m_painter{}
	, m_cacheMutex{nullptr}
	, m_viewSem{nullptr}
	, m_sampleColorLastDiameter(-1)
	, m_undoDepthLimit{DP_UNDO_DEPTH_DEFAULT}
	, m_updateLayersVisibleInFrame{false}
{
	m_cacheMutex = DP_mutex_new();
	m_viewSem = DP_semaphore_new(0);
	start();
}

PaintEngine::~PaintEngine()
{
	DP_semaphore_free(m_viewSem);
	DP_mutex_free(m_cacheMutex);
}

void PaintEngine::setFps(int fps)
{
	if(fps != m_fps) {
		m_fps = fps;
		start();
	}
}

void PaintEngine::setSnapshotMaxCount(int snapshotMaxCount)
{
	m_snapshotQueue.setMaxCount(snapshotMaxCount);
}

void PaintEngine::setSnapshotMinDelayMs(long long snapshotMinDelayMs)
{
	m_snapshotQueue.setMinDelayMs(snapshotMinDelayMs);
}

void PaintEngine::setWantCanvasHistoryDump(bool wantCanvasHistoryDump)
{
	m_paintEngine.setWantCanvasHistoryDump(wantCanvasHistoryDump);
}

void PaintEngine::start()
{
	if(m_timerId != 0) {
		killTimer(m_timerId);
	}
	int fps = qBound(1, m_fps, 120);
	m_timerId = startTimer(1000 / fps, Qt::PreciseTimer);
}

void PaintEngine::reset(
	uint8_t localUserId, const drawdance::CanvasState &canvasState,
	DP_Player *player)
{
	net::MessageList localResetImage = m_paintEngine.reset(
		m_acls, m_snapshotQueue, localUserId, PaintEngine::onRenderTile,
		PaintEngine::onRenderUnlock, PaintEngine::onRenderResize, this,
		ON_SOFT_RESET_FN, this, PaintEngine::onPlayback,
		PaintEngine::onDumpPlayback, this, canvasState, player);
	DP_mutex_lock(m_cacheMutex);
	m_cache = QPixmap{};
	DP_mutex_unlock(m_cacheMutex);
	m_undoDepthLimit = DP_UNDO_DEPTH_DEFAULT;
	start();
	emit aclsChanged(m_acls, DP_ACL_STATE_CHANGE_MASK, true);
	emit defaultLayer(0);
	receiveMessages(
		false, localResetImage.size(), localResetImage.constData(), false);
}

void PaintEngine::timerEvent(QTimerEvent *)
{
	DP_PERF_SCOPE("tick");
	DP_Rect tileBounds = {
		m_canvasViewTileArea.left(), m_canvasViewTileArea.top(),
		m_canvasViewTileArea.right(), m_canvasViewTileArea.bottom()};
	DP_paint_engine_tick(
		m_paintEngine.get(), tileBounds, m_renderOutsideView,
		&PaintEngine::onCatchup, &PaintEngine::onResetLockChanged,
		&PaintEngine::onRecorderStateChanged, &PaintEngine::onLayerPropsChanged,
		&PaintEngine::onAnnotationsChanged,
		&PaintEngine::onDocumentMetadataChanged,
		&PaintEngine::onTimelineChanged, &PaintEngine::onCursorMoved,
		&PaintEngine::onDefaultLayer, &PaintEngine::onUndoDepthLimitSet, this);

	if(m_updateLayersVisibleInFrame) {
		updateLayersVisibleInFrame();
	}
}

void PaintEngine::updateLayersVisibleInFrame()
{
	m_updateLayersVisibleInFrame = false;
	QSet<int> layersVisibleInFrame = m_paintEngine.getLayersVisibleInFrame();
	bool frameMode = m_paintEngine.viewMode() == DP_VIEW_MODE_FRAME;
	emit frameVisibilityChanged(layersVisibleInFrame, frameMode);
}

int PaintEngine::receiveMessages(
	bool local, int count, const net::Message *msgs, bool overrideAcls)
{
	return DP_paint_engine_handle_inc(
		m_paintEngine.get(), local, overrideAcls, count,
		net::Message::asRawMessages(msgs), &PaintEngine::onAclsChanged,
		&PaintEngine::onLaserTrail, &PaintEngine::onMovePointer, this);
}

void PaintEngine::enqueueReset()
{
	net::Message msg = net::makeInternalResetMessage(0);
	receiveMessages(false, 1, &msg);
}

void PaintEngine::enqueueLoadBlank(
	int undoDepthLimit, const QSize &size, const QColor &backgroundColor)
{
	net::Message messages[] = {
		net::makeInternalResetMessage(0),
		net::makeUndoDepthMessage(0, undoDepthLimit),
		net::makeCanvasBackgroundMessage(0, backgroundColor),
		net::makeCanvasResizeMessage(0, 0, size.width(), size.height(), 0),
		net::makeLayerTreeCreateMessage(
			0, 0x100, 0, 0, 0, 0, tr("Layer %1").arg(1)),
		net::makeInternalSnapshotMessage(0),
	};
	receiveMessages(false, DP_ARRAY_LENGTH(messages), messages);
}

void PaintEngine::enqueueCatchupProgress(int progress)
{
	net::Message msg = net::makeInternalCatchupMessage(0, progress);
	receiveMessages(false, 1, &msg);
}

void PaintEngine::resetAcl(uint8_t localUserId)
{
	m_acls.reset(localUserId);
	emit aclsChanged(m_acls, DP_ACL_STATE_CHANGE_MASK, true);
}

void PaintEngine::cleanup()
{
	net::Message msg = net::makeInternalCleanupMessage(0);
	receiveMessages(false, 1, &msg);
}

QColor PaintEngine::backgroundColor() const
{
	return historyCanvasState().backgroundTile().singleColor(Qt::transparent);
}

int PaintEngine::undoDepthLimit() const
{
	return m_undoDepthLimit;
}

bool PaintEngine::localBackgroundColor(QColor &outColor) const
{
	drawdance::Tile tile = m_paintEngine.localBackgroundTile();
	if(tile.isNull()) {
		return false;
	} else {
		outColor = tile.singleColor(Qt::transparent);
		return true;
	}
}

void PaintEngine::setLocalBackgroundColor(const QColor &color)
{
	net::Message msg = net::makeLocalChangeBackgroundColorMessage(color);
	if(msg.isNull()) {
		qWarning("Error setting local background color: %s", DP_error());
	} else {
		receiveMessages(false, 1, &msg);
	}
}

void PaintEngine::clearLocalBackgroundColor()
{
	net::Message msg = net::makeLocalChangeBackgroundClearMessage();
	receiveMessages(false, 1, &msg);
}

uint16_t PaintEngine::findAvailableAnnotationId(uint8_t forUser) const
{
	QSet<int> usedIds;
	int idMask = forUser << 8;
	drawdance::AnnotationList annotations = historyCanvasState().annotations();
	int count = annotations.count();
	for(int i = 0; i < count; ++i) {
		int id = annotations.at(i).id();
		if((id & 0xff00) == idMask) {
			usedIds.insert(id & 0xff);
		}
	}

	for(int i = 0; i < 256; ++i) {
		if(!usedIds.contains(i)) {
			return idMask | i;
		}
	}

	qWarning("No available annotation id for user %d", forUser);
	return 0;
}

drawdance::Annotation
PaintEngine::getAnnotationAt(int x, int y, int expand) const
{
	QPoint point{x, y};
	QMargins margins{expand, expand, expand, expand};

	drawdance::AnnotationList annotations = viewCanvasState().annotations();
	int count = annotations.count();
	int closestIndex = -1;
	int closestDistance = INT_MAX;

	for(int i = 0; i < count; ++i) {
		drawdance::Annotation annotation = annotations.at(i);
		QRect bounds = annotation.bounds().marginsAdded(margins);
		if(bounds.contains(point)) {
			int distance = (point - bounds.center()).manhattanLength();
			if(closestIndex == -1 || distance < closestDistance) {
				closestIndex = i;
				closestDistance = distance;
			}
		}
	}

	return closestIndex == -1 ? drawdance::Annotation::null()
							  : annotations.at(closestIndex);
}

bool PaintEngine::needsOpenRaster() const
{
	drawdance::CanvasState cs = viewCanvasState();
	return cs.backgroundTile().isNull() && cs.layers().count() > 1 &&
		   cs.annotations().count() != 0;
}

void PaintEngine::setLocalDrawingInProgress(bool localDrawingInProgress)
{
	m_paintEngine.setLocalDrawingInProgress(localDrawingInProgress);
}

void PaintEngine::setLayerVisibility(int layerId, bool hidden)
{
	net::Message msg =
		net::makeLocalChangeLayerVisibilityMessage(layerId, hidden);
	receiveMessages(false, 1, &msg);
}

void PaintEngine::setTrackVisibility(int trackId, bool hidden)
{
	net::Message msg =
		net::makeLocalChangeTrackVisibilityMessage(trackId, hidden);
	receiveMessages(false, 1, &msg);
}

void PaintEngine::setTrackOnionSkin(int trackId, bool onionSkin)
{
	net::Message msg =
		net::makeLocalChangeTrackOnionSkinMessage(trackId, onionSkin);
	receiveMessages(false, 1, &msg);
}

void PaintEngine::setViewMode(DP_ViewMode vm, bool censor)
{
	net::Message msg = net::makeLocalChangeViewModeMessage(vm);
	receiveMessages(false, 1, &msg);
	m_paintEngine.setRevealCensored(!censor);
	updateLayersVisibleInFrame();
}

bool PaintEngine::revealCensored() const
{
	return m_paintEngine.revealCensored();
}

void PaintEngine::setOnionSkins(
	const QVector<QPair<float, QColor>> &skinsBelow,
	const QVector<QPair<float, QColor>> &skinsAbove)
{
	int countBelow = skinsBelow.count();
	int countAbove = skinsAbove.count();
	DP_OnionSkins *oss = DP_onion_skins_new(countBelow, countAbove);

	for(int i = 0; i < countBelow; ++i) {
		const QPair<float, QColor> &skin = skinsBelow[i];
		DP_onion_skins_skin_below_at_set(
			oss, i, DP_channel_float_to_15(skin.first),
			DP_upixel15_from_color(skin.second.rgba()));
	}

	for(int i = 0; i < countAbove; ++i) {
		const QPair<float, QColor> &skin = skinsAbove[i];
		DP_onion_skins_skin_above_at_set(
			oss, i, DP_channel_float_to_15(skin.first),
			DP_upixel15_from_color(skin.second.rgba()));
	}

	net::Message msg = net::makeLocalChangeOnionSkinsMessage(oss);
	receiveMessages(false, 1, &msg);
	DP_onion_skins_free(oss);
}

void PaintEngine::setViewLayer(int id)
{
	net::Message msg = net::makeLocalChangeActiveLayerMessage(id);
	receiveMessages(false, 1, &msg);
}

void PaintEngine::setViewFrame(int frame)
{
	net::Message msg = net::makeLocalChangeActiveFrameMessage(frame);
	receiveMessages(false, 1, &msg);
	updateLayersVisibleInFrame();
}

int PaintEngine::pickLayer(int x, int y)
{
	return m_paintEngine.pick(x, y).layer_id;
}

unsigned int PaintEngine::pickContextId(int x, int y)
{
	return m_paintEngine.pick(x, y).context_id;
}

void PaintEngine::setInspectContextId(unsigned int contextId)
{
	m_paintEngine.setInspectContextId(contextId);
}

QColor PaintEngine::sampleColor(int x, int y, int layerId, int diameter)
{
	if(layerId == 0) {
		// Only extract the part of the pixmap that we want to sample from,
		// since grabbing the whole thing is pretty slow.
		QPoint pos;
		QRect rect;
		if(diameter < 2) {
			pos = QPoint{0, 0};
			rect = QRect{x, y, 1, 1};
		} else {
			int radius = diameter / 2;
			int left = x - radius;
			int top = y - radius;
			pos = QPoint{x - left, y - top};
			rect = QRect{left, top, diameter, diameter};
		}

		DP_mutex_lock(m_cacheMutex);
		QImage img = rect.intersects(m_cache.rect())
						 ? m_cache.copy(rect).toImage()
						 : QImage{};
		DP_mutex_unlock(m_cacheMutex);

		if(img.isNull()) {
			return Qt::transparent;
		} else {
			return drawdance::sampleColorAt(
				img.convertToFormat(QImage::Format_ARGB32_Premultiplied),
				m_sampleColorStampBuffer, pos.x(), pos.y(), diameter, true,
				m_sampleColorLastDiameter);
		}
	} else {
		drawdance::LayerContent lc =
			viewCanvasState().searchLayerContent(layerId);
		if(lc.isNull()) {
			return Qt::transparent;
		} else {
			return lc.sampleColorAt(
				m_sampleColorStampBuffer, x, y, diameter, true,
				m_sampleColorLastDiameter);
		}
	}
}

#ifdef DP_HAVE_BUILTIN_SERVER
void PaintEngine::setServer(server::BuiltinServer *server)
{
	unsetServer();
	if(server) {
		m_server = server;
		connect(m_server, &QObject::destroyed, this, &PaintEngine::unsetServer);
	}
}

void PaintEngine::unsetServer()
{
	if(m_server) {
		disconnect(
			m_server, &QObject::destroyed, this, &PaintEngine::unsetServer);
		m_server = nullptr;
	}
}
#endif

drawdance::RecordStartResult PaintEngine::startRecording(const QString &path)
{
	return m_paintEngine.startRecorder(
		path, "drawpile", cmake_config::version(), "recording");
}

drawdance::RecordStartResult PaintEngine::exportTemplate(
	const QString &path, const net::MessageList &snapshot)
{
	return m_paintEngine.exportTemplate(
		path, snapshot, "drawpile", cmake_config::version(), "template");
}

bool PaintEngine::stopRecording()
{
	return m_paintEngine.stopRecorder();
}

bool PaintEngine::isRecording() const
{
	return m_paintEngine.recorderIsRecording();
}

DP_PlayerResult PaintEngine::stepPlayback(long long steps)
{
	net::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.stepPlayback(steps, msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

DP_PlayerResult PaintEngine::skipPlaybackBy(long long steps, bool bySnapshots)
{
	net::MessageList msgs;
	DP_PlayerResult result =
		m_paintEngine.skipPlaybackBy(steps, bySnapshots, msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

DP_PlayerResult PaintEngine::jumpPlaybackTo(long long position)
{
	net::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.jumpPlaybackTo(position, msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

DP_PlayerResult PaintEngine::beginPlayback()
{
	return m_paintEngine.beginPlayback();
}

DP_PlayerResult PaintEngine::playPlayback(long long msecs)
{
	net::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.playPlayback(msecs, msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

bool PaintEngine::buildPlaybackIndex(
	drawdance::PaintEngine::BuildIndexProgressFn progressFn)
{
	return m_paintEngine.buildPlaybackIndex(progressFn);
}

bool PaintEngine::loadPlaybackIndex()
{
	return m_paintEngine.loadPlaybackIndex();
}

unsigned int PaintEngine::playbackIndexMessageCount()
{
	return m_paintEngine.playbackIndexMessageCount();
}

size_t PaintEngine::playbackIndexEntryCount()
{
	return m_paintEngine.playbackIndexEntryCount();
}

QImage PaintEngine::playbackIndexThumbnailAt(size_t index)
{
	return m_paintEngine.playbackIndexThumbnailAt(index);
}

DP_PlayerResult PaintEngine::stepDumpPlayback()
{
	net::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.stepDumpPlayback(msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

DP_PlayerResult PaintEngine::jumpDumpPlaybackToPreviousReset()
{
	net::MessageList msgs;
	DP_PlayerResult result =
		m_paintEngine.jumpDumpPlaybackToPreviousReset(msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

DP_PlayerResult PaintEngine::jumpDumpPlaybackToNextReset()
{
	net::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.jumpDumpPlaybackToNextReset(msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

DP_PlayerResult PaintEngine::jumpDumpPlayback(long long position)
{
	net::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.jumpDumpPlayback(position, msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

bool PaintEngine::flushPlayback()
{
	net::MessageList msgs;
	bool ok = m_paintEngine.flushPlayback(msgs);
	receiveMessages(false, msgs.count(), msgs.constData());
	return ok;
}

bool PaintEngine::closePlayback()
{
	return m_paintEngine.closePlayback();
}

void PaintEngine::previewCut(
	int layerId, const QRect &bounds, const QImage &mask)
{
	m_paintEngine.previewCut(layerId, bounds, mask);
}

void PaintEngine::clearCutPreview()
{
	m_paintEngine.clearCutPreview();
}

void PaintEngine::previewTransform(
	int layerId, int x, int y, const QImage &img, const QPolygon &dstPolygon,
	int interpolation)
{
	m_paintEngine.previewTransform(
		layerId, x, y, img, dstPolygon, interpolation);
}

void PaintEngine::clearTransformPreview()
{
	m_paintEngine.clearTransformPreview();
}

void PaintEngine::previewDabs(int layerId, const net::MessageList &msgs)
{
	m_paintEngine.previewDabs(layerId, msgs.count(), msgs.constData());
}

void PaintEngine::clearDabsPreview()
{
	m_paintEngine.clearDabsPreview();
}

void PaintEngine::withPixmap(std::function<void(const QPixmap &)> fn) const
{
	DP_mutex_lock(m_cacheMutex);
	fn(m_cache);
	DP_mutex_unlock(m_cacheMutex);
}

QImage PaintEngine::renderPixmap()
{
	DP_paint_engine_render_everything(m_paintEngine.get());
	DP_SEMAPHORE_MUST_WAIT(m_viewSem);
	DP_mutex_lock(m_cacheMutex);
	QImage img = m_cache.toImage();
	DP_mutex_unlock(m_cacheMutex);
	return img;
}

void PaintEngine::setCanvasViewArea(const QRect &area)
{
	// We can't use QPoint::operator/ because that rounds instead of truncates.
	m_canvasViewTileArea = QRect{
		QPoint{area.left() / DP_TILE_SIZE, area.top() / DP_TILE_SIZE},
		QPoint{area.right() / DP_TILE_SIZE, area.bottom() / DP_TILE_SIZE}};
	DP_Rect tileBounds = {
		m_canvasViewTileArea.left(), m_canvasViewTileArea.top(),
		m_canvasViewTileArea.right(), m_canvasViewTileArea.bottom()};
	DP_paint_engine_change_bounds(
		m_paintEngine.get(), tileBounds, m_renderOutsideView);
	DP_SEMAPHORE_MUST_WAIT(m_viewSem);
}

int PaintEngine::frameCount() const
{
	return historyCanvasState().frameCount();
}

QImage PaintEngine::getLayerImage(int id, const QRect &rect) const
{
	drawdance::CanvasState cs = viewCanvasState();
	QRect area = rect.isNull() ? QRect{0, 0, cs.width(), cs.height()} : rect;
	if(area.isEmpty()) {
		return QImage{};
	}

	if(id <= 0) {
		bool includeBackground = id == 0;
		return cs.toFlatImage(includeBackground, true, &area);
	} else {
		return cs.layerToFlatImage(id, area);
	}
}

QImage PaintEngine::getFrameImage(
	drawdance::ViewModeBuffer &vmb, int index, const QRect &rect) const
{
	drawdance::CanvasState cs = viewCanvasState();
	QRect area = rect.isNull() ? QRect{0, 0, cs.width(), cs.height()} : rect;
	if(area.isEmpty()) {
		return QImage{};
	}
	DP_ViewModeFilter vmf =
		DP_view_mode_filter_make_frame(vmb.get(), cs.get(), index, nullptr);
	return cs.toFlatImage(true, true, &area, &vmf);
}


#ifdef DP_HAVE_BUILTIN_SERVER
void PaintEngine::onSoftReset(
	void *user, unsigned int contextId, DP_CanvasState *cs)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	server::BuiltinServer *server = pe->m_server;
	if(server && contextId == pe->m_acls.localUserId()) {
		QMetaObject::invokeMethod(
			server, "doInternalReset", Qt::BlockingQueuedConnection,
			Q_ARG(drawdance::CanvasState, drawdance::CanvasState::inc(cs)));
	}
}
#endif

void PaintEngine::onPlayback(void *user, long long position)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->playbackAt(position);
}

void PaintEngine::onDumpPlayback(
	void *user, long long position, DP_CanvasHistorySnapshot *chs)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->dumpPlaybackAt(
		position, drawdance::CanvasHistorySnapshot::inc(chs));
}

void PaintEngine::onAclsChanged(void *user, int aclChangeFlags)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->aclsChanged(pe->m_acls, aclChangeFlags, false);
}

void PaintEngine::onLaserTrail(
	void *user, unsigned int contextId, int persistence, uint32_t color)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->laserTrail(contextId, persistence, color);
}

void PaintEngine::onMovePointer(
	void *user, unsigned int contextId, int x, int y)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->cursorMoved(DP_USER_CURSOR_FLAG_VALID, contextId, 0, x / 4, y / 4);
}

void PaintEngine::onDefaultLayer(void *user, int layerId)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->defaultLayer(layerId >= 0 && layerId <= UINT16_MAX ? layerId : 0);
}

void PaintEngine::onUndoDepthLimitSet(void *user, int undoDepthLimit)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	pe->m_undoDepthLimit = undoDepthLimit;
	emit pe->undoDepthLimitSet(undoDepthLimit);
}

void PaintEngine::onCatchup(void *user, int progress)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->caughtUpTo(progress);
}

void PaintEngine::onResetLockChanged(void *user, bool locked)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->resetLockSet(locked);
}

void PaintEngine::onRecorderStateChanged(void *user, bool started)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->recorderStateChanged(started);
}

void PaintEngine::onLayerPropsChanged(void *user, DP_LayerPropsList *lpl)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	pe->m_updateLayersVisibleInFrame = true;
	emit pe->layersChanged(drawdance::LayerPropsList::inc(lpl));
}

void PaintEngine::onAnnotationsChanged(void *user, DP_AnnotationList *al)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->annotationsChanged(drawdance::AnnotationList::inc(al));
}

void PaintEngine::onDocumentMetadataChanged(void *user, DP_DocumentMetadata *dm)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	pe->m_updateLayersVisibleInFrame = true;
	emit pe->documentMetadataChanged(drawdance::DocumentMetadata::inc(dm));
}

void PaintEngine::onTimelineChanged(void *user, DP_Timeline *tl)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	pe->m_updateLayersVisibleInFrame = true;
	emit pe->timelineChanged(drawdance::Timeline::inc(tl));
}

void PaintEngine::onCursorMoved(
	void *user, unsigned int flags, unsigned int contextId, int layerId, int x,
	int y)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->cursorMoved(flags, contextId, layerId, x, y);
}

void PaintEngine::onRenderTile(
	void *user, int tileX, int tileY, DP_Pixel8 *pixels)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	QRect area{
		tileX * DP_TILE_SIZE, tileY * DP_TILE_SIZE, DP_TILE_SIZE, DP_TILE_SIZE};
	QImage image{
		reinterpret_cast<unsigned char *>(pixels), DP_TILE_SIZE, DP_TILE_SIZE,
		QImage::Format_RGB32};
	DP_mutex_lock(pe->m_cacheMutex);
	QPainter &painter = pe->m_painter;
	painter.begin(&pe->m_cache);
	painter.drawImage(area.x(), area.y(), image);
	painter.end();
	DP_mutex_unlock(pe->m_cacheMutex);
	emit pe->areaChanged(area);
}

void PaintEngine::onRenderUnlock(void *user)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	DP_SEMAPHORE_MUST_POST(pe->m_viewSem);
}

void PaintEngine::onRenderResize(
	void *user, int width, int height, int prevWidth, int prevHeight,
	int offsetX, int offsetY)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	QSize size{width, height};
	DP_mutex_lock(pe->m_cacheMutex);
	QSize cacheSize = pe->m_cache.size();
	if(cacheSize != size) {
		QPixmap pixmap{size};
		pixmap.fill(QColor(100, 100, 100));
		QPainter &painter = pe->m_painter;
		painter.begin(&pixmap);
		painter.drawPixmap(
			QRect{QPoint{offsetX, offsetY}, cacheSize}, pe->m_cache,
			QRect{QPoint{0, 0}, cacheSize});
		painter.end();
		pe->m_cache = std::move(pixmap);
	}
	DP_mutex_unlock(pe->m_cacheMutex);
	emit pe->resized(offsetX, offsetY, QSize{prevWidth, prevHeight});
}

}
