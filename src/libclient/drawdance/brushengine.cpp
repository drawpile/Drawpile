// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/brush.h>
#include <dpengine/brush_engine.h>
}

#include "libclient/drawdance/brushengine.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/net/client.h"

namespace drawdance {

BrushEngine::BrushEngine()
    : m_messages{}
    , m_data{DP_brush_engine_new(&BrushEngine::pushMessage, &m_messages)}
{
}

BrushEngine::~BrushEngine()
{
    DP_brush_engine_free(m_data);
}

void BrushEngine::setClassicBrush(const DP_ClassicBrush &brush, int layerId, bool freehand)
{
    DP_brush_engine_classic_brush_set(m_data, &brush, layerId, freehand, brush.color);
}

void BrushEngine::setMyPaintBrush(
    const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
    int layerId, bool freehand)
{
    DP_brush_engine_mypaint_brush_set(m_data, &brush, &settings, layerId, freehand, brush.color);
}

void BrushEngine::flushDabs()
{
    DP_brush_engine_dabs_flush(m_data);
}

void BrushEngine::beginStroke(unsigned int contextId, bool pushUndoPoint)
{
    m_messages.clear();
    DP_brush_engine_stroke_begin(m_data, contextId, pushUndoPoint);
}

void BrushEngine::strokeTo(
    float x, float y, float pressure, float xtilt, float ytilt,
    float rotation, long long deltaMsec,
    const drawdance::CanvasState &cs)
{
    DP_brush_engine_stroke_to(m_data, x, y, pressure, xtilt, ytilt, rotation, deltaMsec, cs.get());
}

void BrushEngine::endStroke(bool pushPenUp)
{
    DP_brush_engine_stroke_end(m_data, pushPenUp);
}

void BrushEngine::addOffset(float x, float y)
{
    DP_brush_engine_offset_add(m_data, x, y);
}

void BrushEngine::sendMessagesTo(net::Client *client)
{
    Q_ASSERT(client);
    flushDabs();
    client->sendMessages(m_messages.count(), m_messages.constData());
    clearMessages();
}

void BrushEngine::pushMessage(void *user, DP_Message *msg)
{
    static_cast<drawdance::MessageList *>(user)
        ->append(drawdance::Message::noinc(msg));
}

}
