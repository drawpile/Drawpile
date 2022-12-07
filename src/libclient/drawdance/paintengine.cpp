#include "paintengine.h"
#include "aclstate.h"
#include "snapshotqueue.h"
#include <QDateTime>

namespace drawdance {

PaintEngine::PaintEngine(AclState &acls, SnapshotQueue &sq, const CanvasState &canvasState)
    : m_paintDc{DrawContextPool::acquire()}
    , m_previewDc{DrawContextPool::acquire()}
    , m_data(DP_paint_engine_new_inc(m_paintDc.get(), m_previewDc.get(),
        acls.get(), canvasState.get(), DP_snapshot_queue_on_save_point, sq.get(),
        &PaintEngine::getTimeMs, nullptr))
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

void PaintEngine::reset(AclState &acls, SnapshotQueue &sq, const CanvasState &canvasState)
{
	DP_paint_engine_free_join(m_data);
	acls.reset(0);
	m_data = DP_paint_engine_new_inc(m_paintDc.get(), m_previewDc.get(),
        acls.get(), canvasState.get(), DP_snapshot_queue_on_save_point, sq.get(),
        &PaintEngine::getTimeMs, nullptr);
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

void PaintEngine::previewCut(int layerId, const QRect &bounds, const QImage &mask)
{
    Q_ASSERT(mask.isNull() || mask.format() == QImage::Format_ARGB32_Premultiplied);
    Q_ASSERT(mask.isNull() || mask.size() == bounds.size());
    DP_paint_engine_preview_cut(
        m_data, layerId, bounds.x(), bounds.y(), bounds.width(), bounds.height(),
        mask.isNull() ? nullptr : reinterpret_cast<const DP_Pixel8 *>(mask.constBits()));
}

void PaintEngine::previewDabs(int layerId, int count, const drawdance::Message *msgs)
{
    DP_paint_engine_preview_dabs_inc(
        m_data, layerId, count, drawdance::Message::asRawMessages(msgs));
}

void PaintEngine::clearPreview()
{
    DP_paint_engine_preview_clear(m_data);
}

CanvasState PaintEngine::canvasState() const
{
	return drawdance::CanvasState::noinc(DP_paint_engine_canvas_state_inc(m_data));
}

long long PaintEngine::getTimeMs(void *)
{
    return QDateTime::currentMSecsSinceEpoch();
}

}
