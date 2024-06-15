// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/output.h>
#include <dpengine/tile.h>
}
#include "libclient/drawdance/aclstate.h"
#include "libclient/drawdance/image.h"
#include "libclient/drawdance/paintengine.h"
#include "libclient/drawdance/snapshotqueue.h"
#include "libshared/util/paths.h"
#include <QColor>
#include <QDateTime>

namespace drawdance {

PaintEngine::PaintEngine(
	AclState &acls, SnapshotQueue &sq, bool wantCanvasHistoryDump,
	bool rendererChecker, const QColor &checkerColor1,
	const QColor &checkerColor2, DP_RendererTileFn rendererTileFn,
	DP_RendererUnlockFn rendererUnlockFn, DP_RendererResizeFn rendererResizeFn,
	void *rendererUser, DP_CanvasHistorySoftResetFn softResetFn,
	void *softResetUser, DP_PaintEnginePlaybackFn playbackFn,
	DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
	const CanvasState &canvasState)
	: m_paintDc{DrawContextPool::acquire()}
	, m_mainDc{DrawContextPool::acquire()}
	, m_previewDc{DrawContextPool::acquire()}
	, m_data(DP_paint_engine_new_inc(
		  m_paintDc.get(), m_mainDc.get(), m_previewDc.get(), acls.get(),
		  canvasState.get(), rendererChecker, checkerColor1.rgba(),
		  checkerColor2.rgba(), rendererTileFn, rendererUnlockFn,
		  rendererResizeFn, rendererUser, DP_snapshot_queue_on_save_point,
		  sq.get(), softResetFn, softResetUser, wantCanvasHistoryDump,
		  getDumpDir().toUtf8().constData(), &PaintEngine::getTimeMs, nullptr,
		  nullptr, playbackFn, dumpPlaybackFn, playbackUser))
{
}

PaintEngine::~PaintEngine()
{
	DP_paint_engine_free_join(m_data);
}

DP_PaintEngine *PaintEngine::get() const
{
	return m_data;
}

net::MessageList PaintEngine::reset(
	AclState &acls, SnapshotQueue &sq, uint8_t localUserId,
	bool rendererChecker, DP_RendererTileFn rendererTileFn,
	DP_RendererUnlockFn rendererUnlockFn, DP_RendererResizeFn rendererResizeFn,
	void *rendererUser, DP_CanvasHistorySoftResetFn softResetFn,
	void *softResetUser, DP_PaintEnginePlaybackFn playbackFn,
	DP_PaintEngineDumpPlaybackFn dumpPlaybackFn, void *playbackUser,
	const CanvasState &canvasState, DP_Player *player)
{
	net::MessageList localResetImage;
	DP_paint_engine_local_state_reset_image_build(
		m_data, pushResetMessage, &localResetImage);
	bool wantCanvasHistoryDump =
		DP_paint_engine_want_canvas_history_dump(m_data);
	QColor checkerColor1(DP_paint_engine_checker_color1(m_data));
	QColor checkerColor2(DP_paint_engine_checker_color2(m_data));
	DP_paint_engine_free_join(m_data);
	acls.reset(localUserId);
	m_data = DP_paint_engine_new_inc(
		m_paintDc.get(), m_mainDc.get(), m_previewDc.get(), acls.get(),
		canvasState.get(), rendererChecker, checkerColor1.rgba(),
		checkerColor2.rgba(), rendererTileFn, rendererUnlockFn,
		rendererResizeFn, rendererUser, DP_snapshot_queue_on_save_point,
		sq.get(), softResetFn, softResetUser, wantCanvasHistoryDump,
		getDumpDir().toUtf8().constData(), &PaintEngine::getTimeMs, nullptr,
		player, playbackFn, dumpPlaybackFn, playbackUser);
	return localResetImage;
}

int PaintEngine::renderThreadCount() const
{
	return DP_paint_engine_render_thread_count(m_data);
}

void PaintEngine::setLocalDrawingInProgress(bool localDrawingInProgress)
{
	DP_paint_engine_local_drawing_in_progress_set(
		m_data, localDrawingInProgress);
}

void PaintEngine::setWantCanvasHistoryDump(bool wantCanvasHistoryDump)
{
	DP_paint_engine_want_canvas_history_dump_set(m_data, wantCanvasHistoryDump);
}

QSet<int> PaintEngine::getLayersVisibleInFrame()
{
	QSet<int> layersVisibleInFrame;
	DP_paint_engine_get_layers_visible_in_frame(
		m_data, addLayerVisibleInFrame, &layersVisibleInFrame);
	return layersVisibleInFrame;
}

int PaintEngine::activeLayerId() const
{
	return DP_paint_engine_active_layer_id(m_data);
}

int PaintEngine::activeFrameIndex() const
{
	return DP_paint_engine_active_frame_index(m_data);
}

DP_ViewMode PaintEngine::viewMode() const
{
	return DP_paint_engine_view_mode(m_data);
}

bool PaintEngine::revealCensored() const
{
	return DP_paint_engine_reveal_censored(m_data);
}

void PaintEngine::setRevealCensored(bool revealCensored)
{
	DP_paint_engine_reveal_censored_set(m_data, revealCensored);
}

DP_ViewModePick PaintEngine::pick(int x, int y)
{
	return DP_paint_engine_pick(m_data, x, y);
}

void PaintEngine::setInspect(unsigned int contextId, bool showTiles)
{
	DP_paint_engine_inspect_set(m_data, contextId, showTiles);
}

void PaintEngine::setCheckerColor1(const QColor &color1)
{
	DP_paint_engine_checker_color1_set(m_data, color1.rgba());
}

void PaintEngine::setCheckerColor2(const QColor &color2)
{
	DP_paint_engine_checker_color2_set(m_data, color2.rgba());
}

bool PaintEngine::checkersVisible() const
{
	return DP_paint_engine_checkers_visible(m_data);
}

Tile PaintEngine::localBackgroundTile() const
{
	return Tile::inc(DP_paint_engine_local_background_tile_noinc(m_data));
}

RecordStartResult PaintEngine::makeRecorderParameters(
	const QString &path, const QString &writer, const QString &writerVersion,
	const QString &type, DP_RecorderType &outRecorderType,
	JSON_Value *&outHeader)
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
		"writer", qUtf8Printable(writer), "writerversion",
		qUtf8Printable(writerVersion), "type", qUtf8Printable(type), NULL);
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
	if(DP_paint_engine_recorder_start(
		   m_data, recorderType, header, pathBytes.constData())) {
		return RECORD_START_SUCCESS;
	} else {
		return RECORD_START_OPEN_ERROR;
	}
}

RecordStartResult PaintEngine::exportTemplate(
	const QString &path, const net::MessageList &snapshot,
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
	DP_Output *output =
		DP_file_output_save_new_from_path(pathBytes.constData());
	if(!output) {
		return RECORD_START_OPEN_ERROR;
	}

	DP_Recorder *r = DP_recorder_new_inc(
		recorderType, header, nullptr, nullptr, nullptr, output);
	if(!r) {
		return RECORD_START_RECORDER_ERROR;
	}

	for(const net::Message &msg : snapshot) {
		if(!DP_recorder_message_push_inc(r, msg.get())) {
			break;
		}
	}

	char *error;
	DP_recorder_free_join(r, &error);
	if(error) {
		DP_error_set("%s", error);
		DP_free(error);
		return RECORD_START_RECORDER_ERROR;
	} else {
		return RECORD_START_SUCCESS;
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

DP_PlayerResult
PaintEngine::stepPlayback(long long steps, net::MessageList &outMsgs)
{
	return DP_paint_engine_playback_step(
		m_data, steps, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult PaintEngine::skipPlaybackBy(
	long long steps, bool bySnapshots, net::MessageList &outMsgs)
{
	DrawContext drawContext = DrawContextPool::acquire();
	return DP_paint_engine_playback_skip_by(
		m_data, drawContext.get(), steps, bySnapshots, PaintEngine::pushMessage,
		&outMsgs);
}

DP_PlayerResult
PaintEngine::jumpPlaybackTo(long long position, net::MessageList &outMsgs)
{

	DrawContext drawContext = DrawContextPool::acquire();
	return DP_paint_engine_playback_jump_to(
		m_data, drawContext.get(), position, PaintEngine::pushMessage,
		&outMsgs);
}

DP_PlayerResult PaintEngine::beginPlayback()
{
	return DP_paint_engine_playback_begin(m_data);
}

DP_PlayerResult
PaintEngine::playPlayback(long long msecs, net::MessageList &outMsgs)
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
	if(error) {
		qWarning("Error in thumbnail at index %zu: %s", index, DP_error());
	}
	return img;
}

DP_PlayerResult PaintEngine::stepDumpPlayback(net::MessageList &outMsgs)
{
	return DP_paint_engine_playback_dump_step(
		m_data, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult
PaintEngine::jumpDumpPlaybackToPreviousReset(net::MessageList &outMsgs)
{
	return DP_paint_engine_playback_dump_jump_previous_reset(
		m_data, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult
PaintEngine::jumpDumpPlaybackToNextReset(net::MessageList &outMsgs)
{
	return DP_paint_engine_playback_dump_jump_next_reset(
		m_data, PaintEngine::pushMessage, &outMsgs);
}

DP_PlayerResult
PaintEngine::jumpDumpPlayback(long long position, net::MessageList &outMsgs)
{
	return DP_paint_engine_playback_dump_jump(
		m_data, position, PaintEngine::pushMessage, &outMsgs);
}

bool PaintEngine::flushPlayback(net::MessageList &outMsgs)
{
	return DP_paint_engine_playback_flush(
		m_data, PaintEngine::pushMessage, &outMsgs);
}

bool PaintEngine::closePlayback()
{
	return DP_paint_engine_playback_close(m_data);
}

void PaintEngine::previewCut(
	const QSet<int> &layerIds, const QRect &bounds, const QImage &mask)
{
	Q_ASSERT(
		mask.isNull() || mask.format() == QImage::Format_ARGB32_Premultiplied);
	Q_ASSERT(mask.isNull() || mask.size() == bounds.size());
	QVector<int> layerIdsVec(layerIds.begin(), layerIds.end());
	DP_paint_engine_preview_cut(
		m_data, bounds.x(), bounds.y(), bounds.width(), bounds.height(),
		mask.isNull() ? nullptr
					  : reinterpret_cast<const DP_Pixel8 *>(mask.constBits()),
		layerIdsVec.size(), layerIdsVec.constData());
}

void PaintEngine::clearCutPreview()
{
	DP_paint_engine_preview_clear(m_data, DP_PREVIEW_CUT);
}

void PaintEngine::previewTransform(
	int id, int layerId, int x, int y, const QImage &img,
	const QPolygon &dstPolygon, int interpolation)
{
	if(id >= 0 && id < DP_PREVIEW_TRANSFORM_COUNT) {
		QPoint p1 = dstPolygon.point(0);
		QPoint p2 = dstPolygon.point(1);
		QPoint p3 = dstPolygon.point(2);
		QPoint p4 = dstPolygon.point(3);
		DP_Quad dstQuad = DP_quad_make(
			p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y(), p4.x(), p4.y());
		DP_paint_engine_preview_transform(
			m_data, id, layerId, x, y, img.width(), img.height(), &dstQuad,
			interpolation, getTransformPreviewPixels,
			disposeTransformPreviewPixels, new QImage{img});
	} else {
		qWarning("Invalid preview transform id %d", id);
	}
}

void PaintEngine::clearTransformPreview(int id)
{
	if(id >= 0 && id < DP_PREVIEW_TRANSFORM_COUNT) {
		DP_paint_engine_preview_clear(m_data, DP_PREVIEW_TRANSFORM_FIRST + id);
	} else {
		qWarning("Invalid preview transform id %d", id);
	}
}

void PaintEngine::clearAllTransformPreviews()
{
	DP_paint_engine_preview_clear_all_transforms(m_data);
}

void PaintEngine::previewDabs(int layerId, int count, const net::Message *msgs)
{
	DP_paint_engine_preview_dabs_inc(
		m_data, layerId, count, net::Message::asRawMessages(msgs));
}

void PaintEngine::clearDabsPreview()
{
	DP_paint_engine_preview_clear(m_data, DP_PREVIEW_DABS);
}

void PaintEngine::previewFill(
	int layerId, int blendMode, int x, int y, const QImage &img)
{
	DP_paint_engine_preview_fill(
		m_data, layerId, blendMode, x, y, img.width(), img.height(),
		reinterpret_cast<const DP_Pixel8 *>(img.constBits()));
}

void PaintEngine::clearFillPreview()
{
	DP_paint_engine_preview_clear(m_data, DP_PREVIEW_FILL);
}

CanvasState PaintEngine::viewCanvasState() const
{
	return drawdance::CanvasState::noinc(
		DP_paint_engine_view_canvas_state_inc(m_data));
}

CanvasState PaintEngine::historyCanvasState() const
{
	return drawdance::CanvasState::noinc(
		DP_paint_engine_history_canvas_state_inc(m_data));
}

CanvasState PaintEngine::sampleCanvasState() const
{
	return drawdance::CanvasState::noinc(
		DP_paint_engine_sample_canvas_state_inc(m_data));
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
	net::MessageList *outMsgs = static_cast<net::MessageList *>(user);
	outMsgs->append(net::Message::noinc(msg));
}

bool PaintEngine::pushResetMessage(void *user, DP_Message *msg)
{
	pushMessage(user, msg);
	return true;
}

bool PaintEngine::shouldSnapshot(void *user)
{
	static constexpr long long MESSAGE_INDEX_INTERVAL = 10000;
	BuildIndexParams *params = static_cast<BuildIndexParams *>(user);
	if(params->messagesSinceLastSnapshot++ > MESSAGE_INDEX_INTERVAL) {
		params->messagesSinceLastSnapshot = 0;
		return true;
	} else {
		return false;
	}
}

void PaintEngine::addLayerVisibleInFrame(void *user, int layerId, bool visible)
{
	if(visible) {
		QSet<int> *layersVisibleInFrame = static_cast<QSet<int> *>(user);
		layersVisibleInFrame->insert(layerId);
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
