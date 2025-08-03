// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/brush_engine.h>
#include <dpengine/stroke_worker.h>
}
#include "libclient/canvas/point.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/drawdance/strokeworker.h"

namespace drawdance {

StrokeWorker::StrokeWorker(
	DP_MaskSync *msOrNull, const PushMessageFn &pushMessage,
	const PollControlFn &pollControl, const SyncFn &sync)
	: m_pushMessage(pushMessage)
	, m_pollControl(pollControl)
	, m_sync(sync)
	, m_data(DP_stroke_worker_new(DP_brush_engine_new(
		  msOrNull, &StrokeWorker::pushMessage, &StrokeWorker::pollControl,
		  &StrokeWorker::sync, this)))
{
}

StrokeWorker::~StrokeWorker()
{
	DP_stroke_worker_free(m_data);
}

bool StrokeWorker::isThreadActive() const
{
	return DP_stroke_worker_thread_active(m_data);
}

bool StrokeWorker::startThread()
{
	return DP_stroke_worker_thread_start(m_data);
}

bool StrokeWorker::finishThread()
{
	return DP_stroke_worker_thread_finish(m_data);
}

void StrokeWorker::setSizeLimit(int sizeLimit)
{
	DP_stroke_worker_size_limit_set(m_data, sizeLimit);
}

void StrokeWorker::setClassicBrush(
	const DP_ClassicBrush &brush, const DP_BrushEngineStrokeParams &besp,
	bool eraserOverride)
{
	DP_stroke_worker_classic_brush_set(
		m_data, &brush, &besp, nullptr, eraserOverride);
}

void StrokeWorker::setMyPaintBrush(
	const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
	const DP_BrushEngineStrokeParams &besp, bool eraserOverride)
{
	DP_stroke_worker_mypaint_brush_set(
		m_data, &brush, &settings, &besp, nullptr, eraserOverride);
}

void StrokeWorker::flushDabs()
{
	DP_stroke_worker_dabs_flush(m_data);
}

void StrokeWorker::pushMessageNoinc(DP_Message *msg)
{
	DP_stroke_worker_message_push_noinc(m_data, msg);
}

void StrokeWorker::beginStroke(
	unsigned int contextId, const drawdance::CanvasState &cs,
	bool compatibilityMode, bool pushUndoPoint, bool mirror, bool flip,
	float zoom, float angle)
{
	DP_stroke_worker_stroke_begin(
		m_data, cs.get(), contextId, compatibilityMode, pushUndoPoint, mirror,
		flip, zoom, angle);
}

void StrokeWorker::strokeTo(
	const canvas::Point &point, const drawdance::CanvasState &cs)
{
	DP_BrushPoint bp = {float(point.x()),		 float(point.y()),
						float(point.pressure()), float(point.xtilt()),
						float(point.ytilt()),	 float(point.rotation()),
						point.timeMsec()};
	DP_stroke_worker_stroke_to(m_data, &bp, cs.get());
}

void StrokeWorker::poll(long long timeMsec, const drawdance::CanvasState &cs)
{
	DP_stroke_worker_poll(m_data, timeMsec, cs.get());
}

void StrokeWorker::endStroke(
	long long timeMsec, const drawdance::CanvasState &cs, bool pushPenUp)
{
	DP_stroke_worker_stroke_end(m_data, timeMsec, cs.get(), pushPenUp);
}

void StrokeWorker::cancelStroke(long long timeMsec, bool pushPenUp)
{
	DP_stroke_worker_stroke_cancel(m_data, timeMsec, pushPenUp);
}

void StrokeWorker::addOffset(float x, float y)
{
	DP_stroke_worker_offset_add(m_data, x, y);
}

void StrokeWorker::pushMessage(void *user, DP_Message *msg)
{
	static_cast<StrokeWorker *>(user)->m_pushMessage(msg);
}

void StrokeWorker::pollControl(void *user, bool enable)
{
	static_cast<StrokeWorker *>(user)->m_pollControl(enable);
}

DP_CanvasState *StrokeWorker::sync(void *user)
{
	return static_cast<StrokeWorker *>(user)->m_sync();
}

}
