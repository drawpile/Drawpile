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

BrushEngine::BrushEngine(PollControlFn pollControl)
	: m_messages{}
	, m_pollControl{pollControl}
	, m_data{DP_brush_engine_new(
		  &BrushEngine::pushMessage,
		  pollControl ? &BrushEngine::pollControl : nullptr, this)}
{
}

BrushEngine::~BrushEngine()
{
	DP_brush_engine_free(m_data);
}

void BrushEngine::setClassicBrush(
	const DP_ClassicBrush &brush, const DP_StrokeParams &stroke,
	bool eraserOverride)
{
	DP_brush_engine_classic_brush_set(
		m_data, &brush, &stroke, nullptr, eraserOverride);
}

void BrushEngine::setMyPaintBrush(
	const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
	const DP_StrokeParams &stroke, bool eraserOverride)
{
	DP_brush_engine_mypaint_brush_set(
		m_data, &brush, &settings, &stroke, nullptr, eraserOverride);
}

void BrushEngine::flushDabs()
{
	DP_brush_engine_dabs_flush(m_data);
}

void BrushEngine::beginStroke(
	unsigned int contextId, const drawdance::CanvasState &cs,
	bool pushUndoPoint, bool mirror, bool flip, float zoom, float angle)
{
	m_messages.clear();
	DP_brush_engine_stroke_begin(
		m_data, cs.get(), contextId, pushUndoPoint, mirror, flip, zoom, angle);
}

void BrushEngine::strokeTo(
	const canvas::Point &point, const drawdance::CanvasState &cs)
{
	DP_BrushPoint bp = {float(point.x()),		 float(point.y()),
						float(point.pressure()), float(point.xtilt()),
						float(point.ytilt()),	 float(point.rotation()),
						point.timeMsec()};
	DP_brush_engine_stroke_to(m_data, bp, cs.get());
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
