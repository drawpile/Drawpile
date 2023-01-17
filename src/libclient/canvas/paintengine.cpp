/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

extern "C" {
#include <dpengine/layer_routes.h>
#include <dpengine/paint_engine.h>
#include <dpengine/recorder.h>
#include <dpengine/tile.h>
#include <dpmsg/msg_internal.h>
}

#include "paintengine.h"
#include "drawdance/layercontent.h"
#include "drawdance/layerpropslist.h"
#include "drawdance/message.h"
#include "drawdance/perf.h"

#include <QPainter>
#include <QSet>
#include <QTimer>
#include <QtEndian>

#define DP_PERF_CONTEXT "paint_engine"

namespace canvas {

PaintEngine::PaintEngine(QObject *parent)
	: QObject(parent)
	, m_acls{}
	, m_snapshotQueue{5, 10000} // TODO: make these configurable
	, m_paintEngine{m_acls, m_snapshotQueue, PaintEngine::onPlayback, this}
	, m_timerId{0}
	, m_changedTileBounds{}
	, m_lastRefreshAreaTileBounds{}
	, m_lastRefreshAreaTileBoundsTouched{false}
	, m_cache{}
	, m_painter{}
	, m_painterMutex{}
	, m_sampleColorLastDiameter(-1)
	, m_onionSkins{nullptr}
	, m_enableOnionSkins{false}
{
	start();
}

PaintEngine::~PaintEngine()
{
	DP_onion_skins_free(m_onionSkins);
}

void PaintEngine::start()
{
	// TODO make this configurable
	m_timerId = startTimer(1000 / 60, Qt::PreciseTimer);
}

void PaintEngine::reset(
	uint8_t localUserId, const drawdance::CanvasState &canvasState, DP_Player *player)
{
	if(m_timerId != 0) {
		killTimer(m_timerId);
	}
	m_paintEngine.reset(m_acls, m_snapshotQueue, localUserId,
		PaintEngine::onPlayback, this, canvasState, player);
	m_cache = QPixmap{};
	m_lastRefreshAreaTileBounds = QRect{};
	m_lastRefreshAreaTileBoundsTouched = false;
	start();
	emit aclsChanged(m_acls, DP_ACL_STATE_CHANGE_MASK);
	emit defaultLayer(0);
}

void PaintEngine::timerEvent(QTimerEvent *)
{
	DP_PERF_SCOPE("tick");
	m_changedTileBounds = QRect{};
	DP_paint_engine_tick(
		m_paintEngine.get(), &PaintEngine::onCatchup,
		&PaintEngine::onRecorderStateChanged, &PaintEngine::onResized,
		&PaintEngine::onTileChanged, &PaintEngine::onLayerPropsChanged,
		&PaintEngine::onAnnotationsChanged,
		&PaintEngine::onDocumentMetadataChanged,
		&PaintEngine::onTimelineChanged,
		&PaintEngine::onCursorMoved, &PaintEngine::onDefaultLayer, this);

	if(m_changedTileBounds.isValid()) {
		QRect changedArea{
			m_changedTileBounds.x() * DP_TILE_SIZE,
			m_changedTileBounds.y() * DP_TILE_SIZE,
			m_changedTileBounds.width() * DP_TILE_SIZE,
			m_changedTileBounds.height() * DP_TILE_SIZE};
		emit areaChanged(changedArea);
	}
}

int PaintEngine::receiveMessages(
	bool local, int count, const drawdance::Message *msgs, bool overrideAcls)
{
	return DP_paint_engine_handle_inc(m_paintEngine.get(), local, overrideAcls,
		count, drawdance::Message::asRawMessages(msgs),
		&PaintEngine::onAclsChanged, &PaintEngine::onLaserTrail,
		&PaintEngine::onMovePointer, this);
}

void PaintEngine::enqueueReset()
{
	drawdance::Message msg = drawdance::Message::makeInternalReset(0);
	receiveMessages(false, 1, &msg);
}

void PaintEngine::enqueueLoadBlank(const QSize &size, const QColor &backgroundColor)
{
	drawdance::Message messages[] = {
		drawdance::Message::makeInternalReset(0),
		drawdance::Message::makeCanvasBackground(0, backgroundColor),
		drawdance::Message::makeCanvasResize(0, 0, size.width(), size.height(), 0),
		drawdance::Message::makeLayerCreate(0, 0x100, 0, 0, 0, 0, tr("Layer %1").arg(1)),
		drawdance::Message::makeInternalSnapshot(0),
	};
	receiveMessages(false, DP_ARRAY_LENGTH(messages), messages);
}

void PaintEngine::enqueueCatchupProgress(int progress)
{
	drawdance::Message msg = drawdance::Message::makeInternalCatchup(0, progress);
	receiveMessages(false, 1, &msg);
}

void PaintEngine::resetAcl(uint8_t localUserId)
{
	m_acls.reset(localUserId);
	emit aclsChanged(m_acls, DP_ACL_STATE_CHANGE_MASK);
}

void PaintEngine::cleanup()
{
	drawdance::Message msg = drawdance::Message::makeInternalCleanup(0);
	receiveMessages(false, 1, &msg);
}

QColor PaintEngine::backgroundColor() const
{
	return historyCanvasState().backgroundTile().singleColor(Qt::transparent);
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
	m_paintEngine.setLocalBackgroundTile(drawdance::Tile::fromColor(color));
}

void PaintEngine::clearLocalBackgroundColor()
{
	m_paintEngine.setLocalBackgroundTile(drawdance::Tile::null());
}

uint16_t PaintEngine::findAvailableAnnotationId(uint8_t forUser) const
{
	QSet<int> usedIds;
	int idMask = forUser << 8;
	drawdance::AnnotationList annotations = canvasState().annotations();
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

drawdance::Annotation PaintEngine::getAnnotationAt(int x, int y, int expand) const
{
	QPoint point{x, y};
	QMargins margins{expand, expand, expand, expand};

	drawdance::AnnotationList annotations = canvasState().annotations();
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

	return closestIndex == -1 ? drawdance::Annotation::null() : annotations.at(closestIndex);
}

bool PaintEngine::needsOpenRaster() const
{
	drawdance::CanvasState cs = canvasState();
	return cs.backgroundTile().isNull() && cs.layers().count() > 1 && cs.annotations().count() != 0;
}

void PaintEngine::setLocalDrawingInProgress(bool localDrawingInProgress)
{
	m_paintEngine.setLocalDrawingInProgress(localDrawingInProgress);
}

void PaintEngine::setLayerVisibility(int layerId, bool hidden)
{
	m_paintEngine.setLayerVisibility(layerId, hidden);
}

void PaintEngine::setViewMode(DP_ViewMode vm, bool censor, bool enableOnionSkins)
{
	m_paintEngine.setViewMode(vm);
	m_paintEngine.setRevealCensored(!censor);
	m_enableOnionSkins = enableOnionSkins;
	m_paintEngine.setOnionSkins(m_enableOnionSkins ? m_onionSkins : nullptr);
}

bool PaintEngine::isCensored() const
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

	DP_OnionSkins *prev_oss = m_onionSkins;
	m_onionSkins = oss;
	if(m_enableOnionSkins) {
		m_paintEngine.setOnionSkins(m_onionSkins);
	}
	DP_onion_skins_free(prev_oss);
}

void PaintEngine::setViewLayer(int id)
{
	m_paintEngine.setActiveLayerId(id);
}

void PaintEngine::setViewFrame(int frame)
{
	m_paintEngine.setActiveFrameIndex(frame - 1); // 1-based to 0-based index.
}

void PaintEngine::setInspectContextId(unsigned int contextId)
{
	m_paintEngine.setInspectContextId(contextId);
}

QColor PaintEngine::sampleColor(int x, int y, int layerId, int diameter)
{
	drawdance::LayerContent lc = layerId == 0
		? m_paintEngine.renderContent()
		: canvasState().searchLayerContent(layerId);
	if(lc.isNull()) {
		return Qt::transparent;
	} else {
		return lc.sampleColorAt(m_sampleColorStampBuffer, x, y, diameter, m_sampleColorLastDiameter);
	}
}

drawdance::RecordStartResult PaintEngine::startRecording(const QString &path)
{
	return m_paintEngine.startRecorder(path);
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
	drawdance::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.stepPlayback(steps, msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

DP_PlayerResult PaintEngine::skipPlaybackBy(long long steps)
{
	drawdance::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.skipPlaybackBy(steps, msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

DP_PlayerResult PaintEngine::jumpPlaybackTo(long long position)
{
	drawdance::MessageList msgs;
	DP_PlayerResult result = m_paintEngine.jumpPlaybackTo(position, msgs);
	receiveMessages(false, msgs.count(), msgs.constData(), true);
	return result;
}

bool PaintEngine::buildPlaybackIndex(drawdance::PaintEngine::BuildIndexProgressFn progressFn)
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

bool PaintEngine::closePlayback()
{
	return m_paintEngine.closePlayback();
}

void PaintEngine::previewCut(int layerId, const QRect &bounds, const QImage &mask)
{
	m_paintEngine.previewCut(layerId, bounds, mask);
}

void PaintEngine::previewDabs(int layerId, const drawdance::MessageList &msgs)
{
	m_paintEngine.previewDabs(layerId, msgs.count(), msgs.constData());
}

void PaintEngine::clearPreview()
{
	m_paintEngine.clearPreview();
}

const QPixmap &PaintEngine::getPixmapView(const QRect &refreshArea)
{
	QRect refreshAreaTileBounds{
		QPoint{refreshArea.left() / DP_TILE_SIZE, refreshArea.top() / DP_TILE_SIZE},
		QPoint{refreshArea.right() / DP_TILE_SIZE, refreshArea.bottom() / DP_TILE_SIZE}};
	if(refreshAreaTileBounds == m_lastRefreshAreaTileBounds) {
		if(m_lastRefreshAreaTileBoundsTouched) {
			renderTileBounds(refreshAreaTileBounds);
			m_lastRefreshAreaTileBoundsTouched = false;
		}
	} else {
		renderTileBounds(refreshAreaTileBounds);
		m_lastRefreshAreaTileBounds = refreshAreaTileBounds;
		m_lastRefreshAreaTileBoundsTouched = false;
	}
	return m_cache;
}

const QPixmap &PaintEngine::getPixmap()
{
	renderEverything();
	m_lastRefreshAreaTileBoundsTouched = false;
	return m_cache;
}

void PaintEngine::renderTileBounds(const QRect &tileBounds)
{
	DP_PaintEngine *pe = m_paintEngine.get();
	DP_paint_engine_prepare_render(pe, &PaintEngine::onRenderSize, this);
	if(!m_cache.isNull() && m_painter.begin(&m_cache)) {
		m_painter.setCompositionMode(QPainter::CompositionMode_Source);
		DP_paint_engine_render_tile_bounds(
			pe, tileBounds.left(), tileBounds.top(), tileBounds.right(),
			tileBounds.bottom(), &PaintEngine::onRenderTile, this);
		m_painter.end();
	}
}

void PaintEngine::renderEverything()
{
	DP_PaintEngine *pe = m_paintEngine.get();
	DP_paint_engine_prepare_render(pe, &PaintEngine::onRenderSize, this);
	if(!m_cache.isNull() && m_painter.begin(&m_cache)) {
		m_painter.setCompositionMode(QPainter::CompositionMode_Source);
		DP_paint_engine_render_everything(pe, &PaintEngine::onRenderTile, this);
		m_painter.end();
	}
}

int PaintEngine::frameCount() const
{
	return canvasState().frameCount();
}

QImage PaintEngine::getLayerImage(int id, const QRect &rect) const
{
	drawdance::CanvasState cs = canvasState();
	QRect area = rect.isNull() ? QRect{0, 0, cs.width(), cs.height()} : rect;
	if (area.isEmpty()) {
		return QImage{};
	}

	if(id <= 0) {
		bool includeBackground = id == 0;
		return cs.toFlatImage(includeBackground, true, &area);
	} else {
		return cs.layerToFlatImage(id, area);
	}
}

QImage PaintEngine::getFrameImage(int index, const QRect &rect) const
{
	drawdance::CanvasState cs = canvasState();
	QRect area = rect.isNull() ? QRect{0, 0, cs.width(), cs.height()} : rect;
	if (area.isEmpty()) {
		return QImage{};
	}
	DP_ViewModeFilter vmf = DP_view_mode_filter_make_frame(cs.get(), index);
	return cs.toFlatImage(true, true, &area, &vmf);
}


void PaintEngine::onPlayback(void *user, long long position, int interval)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->playbackAt(position, interval);
}

void PaintEngine::onAclsChanged(void *user, int aclChangeFlags)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->aclsChanged(pe->m_acls, aclChangeFlags);
}

void PaintEngine::onLaserTrail(void *user, unsigned int contextId, int persistence, uint32_t color)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->laserTrail(contextId, persistence, color);
}

void PaintEngine::onMovePointer(void *user, unsigned int contextId, int x, int y)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->cursorMoved(contextId, 0, x, y);
}

void PaintEngine::onDefaultLayer(void *user, int layerId)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->defaultLayer(layerId >= 0 && layerId <= UINT16_MAX ? layerId : 0);
}

void PaintEngine::onCatchup(void *user, int progress)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->caughtUpTo(progress);
}

void PaintEngine::onRecorderStateChanged(void *user, bool started)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->recorderStateChanged(started);
}

void PaintEngine::onResized(void *user, int offsetX, int offsetY, int prevWidth, int prevHeight)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	pe->resized(offsetX, offsetY, QSize{prevWidth, prevHeight});
}

void PaintEngine::onTileChanged(void *user, int x, int y)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	pe->m_changedTileBounds |= QRect{x, y, 1, 1};
	if(!pe->m_lastRefreshAreaTileBoundsTouched && pe->m_lastRefreshAreaTileBounds.contains(x, y)) {
		pe->m_lastRefreshAreaTileBoundsTouched = true;
	}
}

void PaintEngine::onLayerPropsChanged(void *user, DP_LayerPropsList *lpl)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
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
	emit pe->documentMetadataChanged(drawdance::DocumentMetadata::inc(dm));
}

void PaintEngine::onTimelineChanged(void *user, DP_Timeline *tl)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->timelineChanged(drawdance::Timeline::inc(tl));
}

void PaintEngine::onCursorMoved(void *user, unsigned int contextId, int layerId, int x, int y)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	emit pe->cursorMoved(contextId, layerId, x, y);
}

void PaintEngine::onRenderSize(void *user, int width, int height)
{
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	QSize size{width, height};
	if(pe->m_cache.size() != size) {
		pe->m_cache = QPixmap{size};
	}
}

void PaintEngine::onRenderTile(void *user, int x, int y, DP_Pixel8 *pixels, int threadIndex)
{
	// My initial idea was to use an array of QPainters to spew pixels into the
	// pixmap in parallel, but Qt doesn't support multiple painters on a single
	// pixmap. So we have to use a single painter and lock its usage instead.
	Q_UNUSED(threadIndex);
	QImage image{
		reinterpret_cast<unsigned char *>(pixels), DP_TILE_SIZE, DP_TILE_SIZE,
		QImage::Format_ARGB32_Premultiplied};
	PaintEngine *pe = static_cast<PaintEngine *>(user);
	QMutexLocker lock{&pe->m_painterMutex};
	pe->m_painter.drawImage(x * DP_TILE_SIZE, y * DP_TILE_SIZE, image);
}


}
