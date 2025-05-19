// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/brush.h>
#include <dpengine/brush_engine.h>
}
#include "libclient/canvas/point.h"
#include "libclient/drawdance/brushengine.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/net/client.h"

namespace drawdance {

namespace {
static DP_BrushPoint canvasPointToBrushPoint(const canvas::Point &point)
{
	DP_BrushPoint bp = {float(point.x()),		 float(point.y()),
						float(point.pressure()), float(point.xtilt()),
						float(point.ytilt()),	 float(point.rotation()),
						point.timeMsec()};
	return bp;
}
}

StrokeEngine::StrokeEngine(PushPointFn pushPoint, PollControlFn pollControl)
	: m_pushPoint(pushPoint)
	, m_pollControl(pollControl)
	, m_data(DP_stroke_engine_new(
		  &StrokeEngine::pushPoint,
		  pollControl ? &StrokeEngine::pollControl : nullptr, this))
{
}

StrokeEngine::~StrokeEngine()
{
	DP_stroke_engine_free(m_data);
}

void StrokeEngine::setParams(const DP_StrokeEngineStrokeParams &sesp)
{
	DP_stroke_engine_params_set(m_data, &sesp);
}

void StrokeEngine::beginStroke()
{
	DP_stroke_engine_stroke_begin(m_data);
}

void StrokeEngine::strokeTo(
	const canvas::Point &point, const drawdance::CanvasState &cs)
{
	DP_stroke_engine_stroke_to(
		m_data, canvasPointToBrushPoint(point), cs.get());
}

void StrokeEngine::poll(long long timeMsec, const drawdance::CanvasState &cs)
{
	DP_stroke_engine_poll(m_data, timeMsec, cs.get());
}

void StrokeEngine::endStroke(
	long long timeMsec, const drawdance::CanvasState &cs)
{
	DP_stroke_engine_stroke_end(m_data, timeMsec, cs.get());
}

void StrokeEngine::pushPoint(
	void *user, DP_BrushPoint bp, DP_CanvasState *cs_or_null)
{
	StrokeEngine *strokeEngine = static_cast<StrokeEngine *>(user);
	strokeEngine->m_pushPoint(bp, drawdance::CanvasState::inc(cs_or_null));
}

void StrokeEngine::pollControl(void *user, bool enable)
{
	StrokeEngine *strokeEngine = static_cast<StrokeEngine *>(user);
	strokeEngine->m_pollControl(enable);
}


BrushEngine::BrushEngine(PollControlFn pollControl)
	: m_pollControl(pollControl)
	, m_data(DP_brush_engine_new(
		  &BrushEngine::pushMessage,
		  pollControl ? &BrushEngine::pollControl : nullptr, this))
{
}

BrushEngine::~BrushEngine()
{
	DP_brush_engine_free(m_data);
}

void BrushEngine::setClassicBrush(
	const DP_ClassicBrush &brush, const DP_BrushEngineStrokeParams &besp,
	bool eraserOverride)
{
	DP_brush_engine_classic_brush_set(
		m_data, &brush, &besp, nullptr, eraserOverride);
}

void BrushEngine::setMyPaintBrush(
	const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
	const DP_BrushEngineStrokeParams &besp, bool eraserOverride)
{
	DP_brush_engine_mypaint_brush_set(
		m_data, &brush, &settings, &besp, nullptr, eraserOverride);
}

void BrushEngine::flushDabs()
{
	DP_brush_engine_dabs_flush(m_data);
}

void BrushEngine::beginStroke(
	unsigned int contextId, const drawdance::CanvasState &cs,
	bool compatibilityMode, bool pushUndoPoint, bool mirror, bool flip,
	float zoom, float angle)
{
	m_messages.clear();
	DP_brush_engine_stroke_begin(
		m_data, cs.get(), contextId, compatibilityMode, pushUndoPoint, mirror,
		flip, zoom, angle);
}

void BrushEngine::strokeTo(
	const canvas::Point &point, const drawdance::CanvasState &cs)
{
	DP_brush_engine_stroke_to(m_data, canvasPointToBrushPoint(point), cs.get());
}

void BrushEngine::poll(long long timeMsec, const drawdance::CanvasState &cs)
{
	DP_brush_engine_poll(m_data, timeMsec, cs.get());
}

void BrushEngine::endStroke(
	long long timeMsec, const drawdance::CanvasState &cs, bool pushPenUp)
{
	DP_brush_engine_stroke_end(m_data, timeMsec, cs.get(), pushPenUp);
}

void BrushEngine::addOffset(float x, float y)
{
	DP_brush_engine_offset_add(m_data, x, y);
}

void BrushEngine::setSizeLimit(int limit)
{
	DP_brush_engine_size_limit_set(m_data, limit);
}

void BrushEngine::sendMessagesTo(net::Client *client)
{
	Q_ASSERT(client);
	flushDabs();
	client->sendCommands(m_messages.count(), m_messages.constData());
	clearMessages();
}

void BrushEngine::pushMessage(void *user, DP_Message *msg)
{
	BrushEngine *brushEngine = static_cast<BrushEngine *>(user);
	brushEngine->m_messages.append(net::Message::noinc(msg));
}

void BrushEngine::pollControl(void *user, bool enable)
{
	BrushEngine *brushEngine = static_cast<BrushEngine *>(user);
	brushEngine->m_pollControl(enable);
}

}
