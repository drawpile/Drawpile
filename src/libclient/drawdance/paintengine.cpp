// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/tile.h>
}

#include "libclient/drawdance/paintengine.h"
#include "libclient/drawdance/aclstate.h"
#include "libclient/drawdance/image.h"
#include "libclient/drawdance/snapshotqueue.h"
#include "libshared/util/paths.h"
#include <QDateTime>

namespace drawdance {

PaintEngine::PaintEngine(AclState &acls, SnapshotQueue &sq,
		bool wantCanvasHistoryDump, DP_PaintEnginePlaybackFn playbackFn,
		DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
		const CanvasState &canvasState)
	: m_paintDc{DrawContextPool::acquire()}
	, m_previewDc{DrawContextPool::acquire()}
	, m_data(DP_paint_engine_new_inc(m_paintDc.get(), m_previewDc.get(),
		acls.get(), canvasState.get(), DP_snapshot_queue_on_save_point, sq.get(),
		wantCanvasHistoryDump, getDumpDir().toUtf8().constData(),
		&PaintEngine::getTimeMs, nullptr, nullptr, playbackFn, dumpPlaybackFn,
		playbackUser))
{
}

PaintEngine::~PaintEngine()
{
	DP_paint_engine_free_join(m_data);
}

DP_PaintEngine *PaintEngine::get()
{
	return m_data;
}

void PaintEngine::reset(
	AclState &acls, SnapshotQueue &sq, uint8_t localUserId,
	DP_PaintEnginePlaybackFn playbackFn,
	DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
	const CanvasState &canvasState, DP_Player *player)
{
	bool wantCanvasHistoryDump = DP_paint_engine_want_canvas_history_dump(m_data);
	DP_paint_engine_free_join(m_data);
	acls.reset(localUserId);
	m_data = DP_paint_engine_new_inc(m_paintDc.get(), m_previewDc.get(),
		acls.get(), canvasState.get(), DP_snapshot_queue_on_save_point, sq.get(),
		wantCanvasHistoryDump, getDumpDir().toUtf8().constData(),
		&PaintEngine::getTimeMs, nullptr, player, playbackFn, dumpPlaybackFn,
		playbackUser);
}

int PaintEngine::renderThreadCount() const
{
	return DP_paint_engine_render_thread_count(m_data);
}

LayerContent PaintEngine::renderContent() const
{
	return LayerContent::inc(DP_paint_engine_render_content_noinc(m_data));
}

void PaintEngine::setLocalDrawingInProgress(bool localDrawingInProgress)
{
	DP_paint_engine_local_drawing_in_progress_set(m_data, localDrawingInProgress);
}

void PaintEngine::setWantCanvasHistoryDump(bool wantCanvasHistoryDump)
{
	DP_paint_engine_want_canvas_history_dump_set(m_data, wantCanvasHistoryDump);
}

void PaintEngine::setActiveLayerId(int layerId)
{
	DP_paint_engine_active_layer_id_set(m_data, layerId);
}

void PaintEngine::setActiveFrameIndex(int frameIndex)
{
	DP_paint_engine_active_frame_index_set(m_data, frameIndex);
}

void PaintEngine::setViewMode(DP_ViewMode vm)
{
	DP_paint_engine_view_mode_set(m_data, vm);
}

void PaintEngine::setOnionSkins(const DP_OnionSkins *oss)
{
	DP_paint_engine_onion_skins_set(m_data, oss);
}

bool PaintEngine::revealCensored() const
{
	return DP_paint_engine_reveal_censored(m_data);
}

void PaintEngine::setRevealCensored(bool revealCensored)
{
	DP_paint_engine_reveal_censored_set(m_data, revealCensored);
}

void PaintEngine::setInspectContextId(unsigned int contextId)
{
	DP_paint_engine_inspect_context_id_set(m_data, contextId);
}

void PaintEngine::setLayerVisibility(int layerId, bool hidden)
{
	DP_paint_engine_layer_visibility_set(m_data, layerId, hidden);
}

Tile PaintEngine::localBackgroundTile() const
{
	return Tile::inc(DP_paint_engine_local_background_tile_noinc(m_data));
}

void PaintEngine::setLocalBackgroundTile(const Tile &tile)
{

	DP_paint_engine_local_background_tile_set_noinc(
		m_data, DP_tile_incref_nullable(tile.get()));
}

RecordStartResult PaintEngine::startRecorder(const QString &path)
{
	DP_RecorderType type;
	if(path.endsWith(".dprec", Qt::CaseInsensitive)) {
		type = DP_RECORDER_TYPE_BINARY;
	} else if(path.endsWith(".dptxt", Qt::CaseInsensitive)) {
		type = DP_RECORDER_TYPE_TEXT;
	} else {
		return RECORD_START_UNKNOWN_FORMAT;
	}

	QByteArray pathBytes = path.toUtf8();
	if (DP_paint_engine_recorder_start(m_data, type, pathBytes.constData())) {
		return RECORD_START_SUCCESS;
	} else {
		return RECORD_START_OPEN_ERROR;
	}
}

bool PaintEngine::stopRecorder()
{
	return DP_paint_engine_recorder_stop(m_data);
}

bool PaintEngine::recorderIsRecording() const
{
	return DP_paint_engine_recorder_is_recording(m_data);
}

DP_PlayerResult PaintEngine::stepPlayback(long long steps, MessageList &outMsgs)
{
	return DP_paint_engine_playback_step(m_data, steps, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult PaintEngine::skipPlaybackBy(long long steps, MessageList &outMsgs)
{
	DrawContext drawContext = DrawContextPool::acquire();
	return DP_paint_engine_playback_skip_by(
		m_data, drawContext.get(), steps, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult PaintEngine::jumpPlaybackTo(long long position, MessageList &outMsgs)
{

	DrawContext drawContext = DrawContextPool::acquire();
	return DP_paint_engine_playback_jump_to(
		m_data, drawContext.get(), position, PaintEngine::pushMessage, &outMsgs);
}

namespace {

struct BuildIndexParams {
	PaintEngine::BuildIndexProgressFn progressFn;
	long long lastSnapshotTime;
	long long messagesSinceLastSnapshot;
};

}

bool PaintEngine::buildPlaybackIndex(BuildIndexProgressFn progressFn)
{
	DrawContext drawContext = DrawContextPool::acquire();
	BuildIndexParams params = {progressFn, -1, 0};
	return DP_paint_engine_playback_index_build(
		m_data, drawContext.get(), PaintEngine::shouldSnapshot,
		PaintEngine::indexProgress, &params);
}

bool PaintEngine::loadPlaybackIndex()
{
	return DP_paint_engine_playback_index_load(m_data);
}

unsigned int PaintEngine::playbackIndexMessageCount()
{
	return DP_paint_engine_playback_index_message_count(m_data);
}

size_t PaintEngine::playbackIndexEntryCount()
{
	return DP_paint_engine_playback_index_entry_count(m_data);
}

QImage PaintEngine::playbackIndexThumbnailAt(size_t index)
{
	bool error;
	QImage img = wrapImage(
		 DP_paint_engine_playback_index_thumbnail_at(m_data, index, &error));
	if (error) {
		qWarning("Error in thumbnail at index %zu: %s", index, DP_error());
	}
	return img;
}

DP_PlayerResult PaintEngine::stepDumpPlayback(MessageList &outMsgs)
{
	return DP_paint_engine_playback_dump_step(m_data, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult PaintEngine::jumpDumpPlaybackToPreviousReset(MessageList &outMsgs)
{
	return DP_paint_engine_playback_dump_jump_previous_reset(m_data, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult PaintEngine::jumpDumpPlaybackToNextReset(MessageList &outMsgs)
{
	return DP_paint_engine_playback_dump_jump_next_reset(m_data, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult PaintEngine::jumpDumpPlayback(long long position, MessageList &outMsgs)
{
	return DP_paint_engine_playback_dump_jump(m_data, position, PaintEngine::pushMessage, &outMsgs);
}

bool PaintEngine::closePlayback()
{
	return DP_paint_engine_playback_close(m_data);
}

void PaintEngine::previewCut(int layerId, const QRect &bounds, const QImage &mask)
{
	Q_ASSERT(mask.isNull() || mask.format() == QImage::Format_ARGB32_Premultiplied);
	Q_ASSERT(mask.isNull() || mask.size() == bounds.size());
	DP_paint_engine_preview_cut(
		m_data, layerId, bounds.x(), bounds.y(), bounds.width(), bounds.height(),
		mask.isNull() ? nullptr : reinterpret_cast<const DP_Pixel8 *>(mask.constBits()));
}

void PaintEngine::clearCutPreview()
{
	DP_paint_engine_preview_cut_clear(m_data);
}

void PaintEngine::previewTransform(
	int layerId, int x, int y, const QImage &img, const QPolygon &dstPolygon,
	int interpolation)
{
	if(dstPolygon.count() == 4) {
		QPoint p1 = dstPolygon.point(0);
		QPoint p2 = dstPolygon.point(1);
		QPoint p3 = dstPolygon.point(2);
		QPoint p4 = dstPolygon.point(3);
		DP_Quad dstQuad = DP_quad_make(
			p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y(), p4.x(), p4.y());
		DP_paint_engine_preview_transform(
			m_data, layerId, x, y, img.width(), img.height(),
			reinterpret_cast<const DP_Pixel8 *>(img.constBits()), &dstQuad,
			interpolation);
	} else {
		qWarning("Preview transform destination is not a quad");
	}
}

void PaintEngine::clearTransformPreview()
{
	DP_paint_engine_preview_transform_clear(m_data);
}

void PaintEngine::previewDabs(int layerId, int count, const drawdance::Message *msgs)
{
	DP_paint_engine_preview_dabs_inc(
		m_data, layerId, count, drawdance::Message::asRawMessages(msgs));
}

void PaintEngine::clearDabsPreview()
{
	DP_paint_engine_preview_dabs_clear(m_data);
}

CanvasState PaintEngine::viewCanvasState() const
{
	return drawdance::CanvasState::noinc(DP_paint_engine_view_canvas_state_inc(m_data));
}

CanvasState PaintEngine::historyCanvasState() const
{
	return drawdance::CanvasState::noinc(DP_paint_engine_history_canvas_state_inc(m_data));
}

CanvasState PaintEngine::sampleCanvasState() const
{
	return drawdance::CanvasState::noinc(DP_paint_engine_sample_canvas_state_inc(m_data));
}

QString PaintEngine::getDumpDir()
{
	return utils::paths::writablePath("dumps", ".");
}

long long PaintEngine::getTimeMs(void *)
{
	return QDateTime::currentMSecsSinceEpoch();
}

void PaintEngine::pushMessage(void *user, DP_Message *msg)
{
	MessageList *outMsgs = static_cast<MessageList *>(user);
	outMsgs->append(drawdance::Message::noinc(msg));
}

bool PaintEngine::shouldSnapshot(void *user)
{
	static constexpr long long MS_INTERVAL = 300;
	static constexpr long long MESSAGE_INDEX_INTERVAL = 1000;
	BuildIndexParams *params = static_cast<BuildIndexParams *>(user);
	long long messagesSinceLastSnapshot = params->messagesSinceLastSnapshot++;
	long long now = QDateTime::currentMSecsSinceEpoch();

	// We want to create snapshots that will load quickly when skipped to, so we
	// try to measure the time it took to process the messages since the last
	// snapshot. But creating the snapshot also takes time, which we don't want
	// to count. So the first message after a snapshot resets our timer.
	if (messagesSinceLastSnapshot == 0) {
		params->lastSnapshotTime = now;
	}

	bool wantSnapshot = now - params->lastSnapshotTime > MS_INTERVAL
		&& messagesSinceLastSnapshot > MESSAGE_INDEX_INTERVAL;
	if(wantSnapshot) {
		params->messagesSinceLastSnapshot = 0;
		return true;
	} else {
		return false;
	}
}

void PaintEngine::indexProgress(void *user, int percent)
{
	BuildIndexParams *params = static_cast<BuildIndexParams *>(user);
	params->progressFn(percent);
}

}
