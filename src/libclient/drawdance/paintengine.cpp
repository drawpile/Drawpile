// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpcommon/output.h>
#include <dpengine/tile.h>
}

#include "libclient/drawdance/paintengine.h"
#include "libclient/drawdance/aclstate.h"
#include "libclient/drawdance/image.h"
#include "libclient/drawdance/snapshotqueue.h"
#include "libshared/util/paths.h"
#include <QDateTime>

namespace drawdance {

PaintEngine::PaintEngine(AclState &acls, SnapshotQueue &sq, int undoDepthLimit,
		bool wantCanvasHistoryDump, DP_PaintEnginePlaybackFn playbackFn,
		DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
		const CanvasState &canvasState)
	: m_paintDc{DrawContextPool::acquire()}
	, m_previewDc{DrawContextPool::acquire()}
	, m_data(DP_paint_engine_new_inc(m_paintDc.get(), m_previewDc.get(),
		acls.get(), canvasState.get(), DP_snapshot_queue_on_save_point, sq.get(),
		undoDepthLimit, wantCanvasHistoryDump, getDumpDir().toUtf8().constData(),
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
	AclState &acls, SnapshotQueue &sq, int undoDepthLimit, uint8_t localUserId,
	DP_PaintEnginePlaybackFn playbackFn,
	DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
	const CanvasState &canvasState, DP_Player *player)
{
	bool wantCanvasHistoryDump = DP_paint_engine_want_canvas_history_dump(m_data);
	DP_paint_engine_free_join(m_data);
	acls.reset(localUserId);
	m_data = DP_paint_engine_new_inc(m_paintDc.get(), m_previewDc.get(),
		acls.get(), canvasState.get(), DP_snapshot_queue_on_save_point, sq.get(),
		undoDepthLimit, wantCanvasHistoryDump, getDumpDir().toUtf8().constData(),
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

RecordStartResult PaintEngine::makeRecorderParameters(
	const QString &path, const QString &writer, const QString &writerVersion,
	const QString &type, DP_RecorderType &outRecorderType, JSON_Value *&outHeader)
{
	DP_RecorderType recorderType;
	if(path.endsWith(".dprec", Qt::CaseInsensitive)) {
		recorderType = DP_RECORDER_TYPE_BINARY;
	} else if(path.endsWith(".dptxt", Qt::CaseInsensitive)) {
		recorderType = DP_RECORDER_TYPE_TEXT;
	} else {
		return RECORD_START_UNKNOWN_FORMAT;
	}

	JSON_Value *header = DP_recorder_header_new(
		"writer", qUtf8Printable(writer),
		"writerversion", qUtf8Printable(writerVersion),
		"type", qUtf8Printable(type), NULL);
	if(!header) {
		return RECORD_START_HEADER_ERROR;
	}

	outRecorderType = recorderType;
	outHeader = header;
	return RECORD_START_SUCCESS;
}

RecordStartResult PaintEngine::startRecorder(
	const QString &path, const QString &writer, const QString &writerVersion,
	const QString &type)
{
	DP_RecorderType recorderType;
	JSON_Value *header;
	RecordStartResult result = makeRecorderParameters(
		path, writer, writerVersion, type, recorderType, header);
	if(result != RECORD_START_SUCCESS) {
		return result;
	}

	QByteArray pathBytes = path.toUtf8();
	if (DP_paint_engine_recorder_start(m_data, recorderType, header, pathBytes.constData())) {
		return RECORD_START_SUCCESS;
	} else {
		return RECORD_START_OPEN_ERROR;
	}
}

RecordStartResult PaintEngine::exportTemplate(
	const QString &path, const drawdance::MessageList &snapshot,
	const QString &writer, const QString &writerVersion, const QString &type)
{
	DP_RecorderType recorderType;
	JSON_Value *header;
	RecordStartResult result = PaintEngine::makeRecorderParameters(
		path, writer, writerVersion, type, recorderType, header);
	if(result != drawdance::RECORD_START_SUCCESS) {
		return result;
	}

	QByteArray pathBytes = path.toUtf8();
	DP_Output *output = DP_file_output_save_new_from_path(pathBytes.constData());
	if(!output) {
		return RECORD_START_OPEN_ERROR;
	}

	DP_Recorder *r = DP_recorder_new_inc(recorderType, header, nullptr, nullptr, nullptr, output);
	if(!r) {
		return RECORD_START_RECORDER_ERROR;
	}

	for(const drawdance::Message &msg : snapshot) {
		DP_recorder_message_push_inc(r, msg.get());
	}
	DP_recorder_free_join(r);
	return RECORD_START_SUCCESS;
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

DP_PlayerResult PaintEngine::skipPlaybackBy(long long steps, bool bySnapshots, MessageList &outMsgs)
{
	DrawContext drawContext = DrawContextPool::acquire();
	return DP_paint_engine_playback_skip_by(
		m_data, drawContext.get(), steps, bySnapshots, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult PaintEngine::jumpPlaybackTo(long long position, MessageList &outMsgs)
{

	DrawContext drawContext = DrawContextPool::acquire();
	return DP_paint_engine_playback_jump_to(
		m_data, drawContext.get(), position, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult PaintEngine::beginPlayback()
{
	return DP_paint_engine_playback_begin(m_data);
}

DP_PlayerResult PaintEngine::playPlayback(long long msecs, MessageList &outMsgs)
{
	return DP_paint_engine_playback_play(
		m_data, msecs, PaintEngine::pushMessage, &outMsgs);
}

namespace {

struct BuildIndexParams {
	PaintEngine::BuildIndexProgressFn progressFn;
	long long messagesSinceLastSnapshot;
};

}

bool PaintEngine::buildPlaybackIndex(BuildIndexProgressFn progressFn)
{
	DrawContext drawContext = DrawContextPool::acquire();
	BuildIndexParams params = {progressFn, 0};
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

bool PaintEngine::flushPlayback(MessageList &outMsgs)
{
	return DP_paint_engine_playback_flush(m_data, PaintEngine::pushMessage, &outMsgs);
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
			m_data, layerId, x, y, img.width(), img.height(), &dstQuad,
			interpolation, getTransformPreviewPixels,
			disposeTransformPreviewPixels, new QImage{img});
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
	static constexpr long long MESSAGE_INDEX_INTERVAL = 10000;
	BuildIndexParams *params = static_cast<BuildIndexParams *>(user);
	if( params->messagesSinceLastSnapshot++ > MESSAGE_INDEX_INTERVAL) {
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

const DP_Pixel8 *PaintEngine::getTransformPreviewPixels(void *user)
{
	QImage *img = static_cast<QImage *>(user);
	return reinterpret_cast<const DP_Pixel8 *>(img->constBits());
}

void PaintEngine::disposeTransformPreviewPixels(void *user)
{
	QImage *img = static_cast<QImage *>(user);
	delete img;
}

}
