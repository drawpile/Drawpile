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
#include "libclient/settings.h"
#include "libshared/util/qtcompat.h"
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

static DP_Rect toDpRect(const QRect &canvasViewTileArea)
{
	DP_Rect tileBounds = {
		canvasViewTileArea.left(), canvasViewTileArea.top(),
		canvasViewTileArea.right(), canvasViewTileArea.bottom()};
	return tileBounds;
}

namespace canvas {

PaintEngine::PaintEngine(
	int canvasImplementation, const QColor &checkerColor1,
	const QColor &checkerColor2, int fps, int snapshotMaxCount,
	long long snapshotMinDelayMs, bool wantCanvasHistoryDump, QObject *parent)
	: QObject(parent)
	, m_useTileCache(
		  canvasImplementation !=
		  int(libclient::settings::CanvasImplementation::GraphicsView))
	, m_tileCacheDirtyCheckOnTick(
		  canvasImplementation ==
		  int(libclient::settings::CanvasImplementation::Software))
	, m_acls{}
	, m_snapshotQueue{snapshotMaxCount, snapshotMinDelayMs}
	, m_paintEngine(
		  m_acls, m_snapshotQueue, wantCanvasHistoryDump, !m_useTileCache,
		  checkerColor1, checkerColor2,
		  m_useTileCache ? PaintEngine::onRenderTileToTileCache
						 : PaintEngine::onRenderTileToPixmap,
		  PaintEngine::onRenderUnlock,
		  m_useTileCache ? PaintEngine::onRenderResizeTileCache
						 : PaintEngine::onRenderResizePixmap,
		  this, ON_SOFT_RESET_FN, this, PaintEngine::onPlayback,
		  PaintEngine::onDumpPlayback, this, PaintEngine::onStreamResetStart,
		  this)
	, m_fps{fps}
	, m_timerId{0}
	, m_cache{}
	, m_painter{}
	, m_tileCache(canvasImplementation)
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
		m_acls, m_snapshotQueue, localUserId, !m_useTileCache,
		m_useTileCache ? PaintEngine::onRenderTileToTileCache
					   : PaintEngine::onRenderTileToPixmap,
		PaintEngine::onRenderUnlock,
		m_useTileCache ? PaintEngine::onRenderResizeTileCache
					   : PaintEngine::onRenderResizePixmap,
		this, ON_SOFT_RESET_FN, this, PaintEngine::onPlayback,
		PaintEngine::onDumpPlayback, this, PaintEngine::onStreamResetStart,
		this, canvasState, player);
	DP_mutex_lock(m_cacheMutex);
	if(m_useTileCache) {
		m_tileCache.clear();
	} else {
		m_cache = QPixmap{};
	}
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
	m_revealedLayers.clear();
	DP_paint_engine_tick(
		m_paintEngine.get(), toDpRect(m_canvasViewTileArea),
		m_renderOutsideView, &PaintEngine::onCatchup,
		&PaintEngine::onResetLockChanged, &PaintEngine::onRecorderStateChanged,
		&PaintEngine::onLayerPropsChanged, &PaintEngine::onAnnotationsChanged,
		&PaintEngine::onDocumentMetadataChanged,
		&PaintEngine::onTimelineChanged, &PaintEngine::onSelectionsChanged,
		&PaintEngine::onCursorMoved, &PaintEngine::onDefaultLayer,
		&PaintEngine::onUndoDepthLimitSet,
		&PaintEngine::onCensoredLayerRevealed, this);
	if(m_tileCacheDirtyCheckOnTick && m_tileCache.needsDirtyCheck()) {
		emit tileCacheDirtyCheckNeeded();
	}
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
		net::makeCanvasResizeMessage(
			0, 0, qBound(0, size.width(), int(UINT16_MAX)),
			qBound(0, size.height(), int(UINT16_MAX)), 0),
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

QColor PaintEngine::historyBackgroundColor() const
{
	return historyCanvasState().backgroundTile().singleColor(Qt::transparent);
}

QColor PaintEngine::viewBackgroundColor() const
{
	return viewCanvasState().backgroundTile().singleColor(Qt::transparent);
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
	return !cs.backgroundTile().isNull() || cs.layers().count() > 1 ||
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

DP_ViewMode PaintEngine::viewMode() const
{
	return m_paintEngine.viewMode();
}

bool PaintEngine::revealCensored() const
{
	return m_paintEngine.revealCensored();
}

void PaintEngine::setOnionSkins(
	bool wrap, const QVector<QPair<float, QColor>> &skinsBelow,
	const QVector<QPair<float, QColor>> &skinsAbove)
{
	int countBelow = skinsBelow.count();
	int countAbove = skinsAbove.count();
	DP_OnionSkins *oss = DP_onion_skins_new(wrap, countBelow, countAbove);

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

int PaintEngine::viewLayer() const
{
	return m_paintEngine.activeLayerId();
}

void PaintEngine::setViewFrame(int frame)
{
	net::Message msg = net::makeLocalChangeActiveFrameMessage(frame);
	receiveMessages(false, 1, &msg);
	updateLayersVisibleInFrame();
}

int PaintEngine::viewFrame() const
{
	return m_paintEngine.activeFrameIndex();
}

int PaintEngine::pickLayer(int x, int y)
{
	return m_paintEngine.pick(x, y).layer_id;
}

unsigned int PaintEngine::pickContextId(int x, int y)
{
	return m_paintEngine.pick(x, y).context_id;
}

void PaintEngine::setInspect(unsigned int contextId, bool showTiles)
{
	m_paintEngine.setInspect(contextId, showTiles);
}

void PaintEngine::setCheckerColor1(const QColor &color1)
{
	m_paintEngine.setCheckerColor1(color1);
}

void PaintEngine::setCheckerColor2(const QColor &color2)
{
	m_paintEngine.setCheckerColor2(color2);
}

bool PaintEngine::checkersVisible() const
{
	return m_paintEngine.checkersVisible();
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

		QImage img;
		DP_mutex_lock(m_cacheMutex);
		if(m_useTileCache) {
			img = m_tileCache.toSubImage(rect);
		} else if(rect.intersects(m_cache.rect())) {
			img = m_cache.copy(rect).toImage();
		}
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
		drawdance::LayerSearchResult lsr =
			viewCanvasState().searchLayer(layerId, false);
		if(drawdance::LayerContent *layerContent =
			   std::get_if<drawdance::LayerContent>(&lsr.data)) {
			return layerContent->sampleColorAt(
				m_sampleColorStampBuffer, x, y, diameter, true,
				m_sampleColorLastDiameter);
		} else {
			return Qt::transparent;
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
	const QSet<int> &layerIds, const QRect &bounds, const QImage &mask)
{
	m_paintEngine.previewCut(layerIds, bounds, mask);
}

void PaintEngine::clearCutPreview()
{
	m_paintEngine.clearCutPreview();
}

void PaintEngine::previewTransform(
	int id, int layerId, int blendMode, qreal opacity, int x, int y,
	const QImage &img, const QPolygon &dstPolygon, int interpolation)
{
	m_paintEngine.previewTransform(
		id, layerId, blendMode, opacity, x, y, img, dstPolygon, interpolation);
}

void PaintEngine::clearTransformPreview(int id)
{
	m_paintEngine.clearTransformPreview(id);
}

void PaintEngine::clearAllTransformPreviews()
{
	m_paintEngine.clearAllTransformPreviews();
}

void PaintEngine::previewDabs(int layerId, const net::MessageList &msgs)
{
	m_paintEngine.previewDabs(layerId, msgs.count(), msgs.constData());
}

void PaintEngine::clearDabsPreview()
{
	m_paintEngine.clearDabsPreview();
}

void PaintEngine::previewFill(
	int layerId, int blendMode, qreal opacity, int x, int y, const QImage &img)
{
	m_paintEngine.previewFill(layerId, blendMode, opacity, x, y, img);
}

void PaintEngine::clearFillPreview()
{
	m_paintEngine.clearFillPreview();
}

void PaintEngine::withPixmap(
	const std::function<void(const QPixmap &)> &fn) const
{
	Q_ASSERT(!m_useTileCache);
	DP_mutex_lock(m_cacheMutex);
	fn(m_cache);
	DP_mutex_unlock(m_cacheMutex);
}

QImage PaintEngine::renderPixmap()
{
	DP_paint_engine_render_everything(m_paintEngine.get());
	DP_SEMAPHORE_MUST_WAIT(m_viewSem);
	DP_mutex_lock(m_cacheMutex);
	QImage img = m_useTileCache ? m_tileCache.toImage() : m_cache.toImage();
	DP_mutex_unlock(m_cacheMutex);
	return img;
}

void PaintEngine::withTileCache(const std::function<void(TileCache &)> &fn)
{
	Q_ASSERT(m_useTileCache);
	DP_mutex_lock(m_cacheMutex);
	fn(m_tileCache);
	DP_mutex_unlock(m_cacheMutex);
}

void PaintEngine::setCanvasViewArea(const QRect &area)
{
	// We can't use QPoint::operator/ because that rounds instead of truncates.
	setCanvasViewTileArea(QRect(
		QPoint(area.left() / DP_TILE_SIZE, area.top() / DP_TILE_SIZE),
		QPoint(area.right() / DP_TILE_SIZE, area.bottom() / DP_TILE_SIZE)));
}

void PaintEngine::setCanvasViewTileArea(const QRect &canvasViewTileArea)
{
	m_canvasViewTileArea = canvasViewTileArea;
	DP_paint_engine_change_bounds(
		m_paintEngine.get(), toDpRect(m_canvasViewTileArea),
		m_renderOutsideView);
	DP_SEMAPHORE_MUST_WAIT(m_viewSem);
	if(m_tileCacheDirtyCheckOnTick) {
		emit tileCacheDirtyCheckNeeded();
	}
}

void PaintEngine::setRenderOutsideView(bool renderOutsideView)
{
	m_renderOutsideView = renderOutsideView;
	if(renderOutsideView) {
		DP_paint_engine_render_continuous(
			m_paintEngine.get(), toDpRect(m_canvasViewTileArea),
			m_renderOutsideView);
	}
}

int PaintEngine::frameCount() const
{
	return historyCanvasState().frameCount();
}

QImage PaintEngine::getFlatImage(
	drawdance::ViewModeBuffer &vmb, const drawdance::CanvasState &canvasState,
	bool includeBackground, bool includeSublayers, const QRect *rect) const
{
	DP_ViewModeFilter vmf = DP_paint_engine_view_mode_filter(
		m_paintEngine.get(), vmb.get(), canvasState.get());
	return canvasState.toFlatImage(
		includeBackground, includeSublayers, rect, &vmf);
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

void PaintEngine::onStreamResetStart(
	void *user, DP_CanvasState *cs, size_t correlatorLength,
	const char *correlator)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->streamResetStarted(
		drawdance::CanvasState::noinc(cs),
		correlator ? QString::fromUtf8(QByteArray::fromRawData(
						 correlator, compat::sizetype(correlatorLength)))
				   : QString());
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

void PaintEngine::onCensoredLayerRevealed(void *user, int layerId)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->m_revealedLayers.insert(layerId);
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
	emit pe->layersChanged(
		drawdance::LayerPropsList::inc(lpl), pe->m_revealedLayers);
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

void PaintEngine::onSelectionsChanged(void *user, DP_SelectionSet *ssOrNull)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->selectionsChanged(
		ssOrNull ? drawdance::SelectionSet::inc(ssOrNull)
				 : drawdance::SelectionSet::null());
}

void PaintEngine::onCursorMoved(
	void *user, unsigned int flags, unsigned int contextId, int layerId, int x,
	int y)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->cursorMoved(flags, contextId, layerId, x, y);
}

void PaintEngine::onRenderTileToPixmap(
	void *user, int tileX, int tileY, DP_Pixel8 *pixels)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	QRect area(
		tileX * DP_TILE_SIZE, tileY * DP_TILE_SIZE, DP_TILE_SIZE, DP_TILE_SIZE);
	QImage image(
		reinterpret_cast<unsigned char *>(pixels), DP_TILE_SIZE, DP_TILE_SIZE,
		QImage::Format_RGB32);
	DP_mutex_lock(pe->m_cacheMutex);
	QPainter &painter = pe->m_painter;
	painter.begin(&pe->m_cache);
	painter.drawImage(area.x(), area.y(), image);
	painter.end();
	DP_mutex_unlock(pe->m_cacheMutex);
	emit pe->areaChanged(area);
}

void PaintEngine::onRenderTileToTileCache(
	void *user, int tileX, int tileY, DP_Pixel8 *pixels)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	DP_mutex_lock(pe->m_cacheMutex);
	TileCache::RenderResult result =
		pe->m_tileCache.render(tileX, tileY, pixels);
	if(result.dirtyCheck && !pe->m_tileCacheDirtyCheckOnTick) {
		emit pe->tileCacheDirtyCheckNeeded();
	}
	if(result.navigatorDirtyCheck) {
		emit pe->tileCacheNavigatorDirtyCheckNeeded();
	}
	DP_mutex_unlock(pe->m_cacheMutex);
}

void PaintEngine::onRenderUnlock(void *user)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	DP_SEMAPHORE_MUST_POST(pe->m_viewSem);
}

void PaintEngine::onRenderResizePixmap(
	void *user, int width, int height, int prevWidth, int prevHeight,
	int offsetX, int offsetY)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	QSize size(width, height);
	DP_mutex_lock(pe->m_cacheMutex);
	QSize cacheSize = pe->m_cache.size();
	if(cacheSize != size) {
		QPixmap pixmap(size);
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
	emit pe->resized(
		size, QPoint(offsetX, offsetY), QSize(prevWidth, prevHeight));
}

void PaintEngine::onRenderResizeTileCache(
	void *user, int width, int height, int prevWidth, int prevHeight,
	int offsetX, int offsetY)
{
	Q_UNUSED(prevWidth);
	Q_UNUSED(prevHeight);
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	QSize size(width, height);
	DP_mutex_lock(pe->m_cacheMutex);
	pe->m_tileCache.resize(width, height, offsetX, offsetY);
	if(!pe->m_tileCacheDirtyCheckOnTick) {
		emit pe->tileCacheDirtyCheckNeeded();
	}
	emit pe->tileCacheNavigatorDirtyCheckNeeded();
	DP_mutex_unlock(pe->m_cacheMutex);
}

}
