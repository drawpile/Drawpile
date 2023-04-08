// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpcommon/queue.h>
#include <dpmsg/message.h>
}

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/freehand.h"

#include "libshared/net/undo.h"

namespace tools {

Freehand::Freehand(ToolController &owner, bool isEraser)
	: Tool(owner, isEraser ? ERASER : FREEHAND, Qt::CrossCursor, true, true, false)
	, m_brushEngine{}
	, m_drawing(false)
{
}

Freehand::~Freehand()
{
}

void Freehand::begin(const canvas::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_ASSERT(!m_drawing);
	if(right) {
		return;
	}

	m_drawing = true;
	m_firstPoint = true;
	m_lastTimestamp = QDateTime::currentMSecsSinceEpoch();

	m_owner.setBrushEngineBrush(m_brushEngine);

	// The pressure value of the first point is unreliable
	// because it is (or was?) possible to get a synthetic MousePress event
	// before the StylusPress event.
	m_start = point;
}

void Freehand::motion(const canvas::Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(!m_drawing)
		return;

	drawdance::CanvasState canvasState = m_owner.model()->paintEngine()->sampleCanvasState();

	if(m_firstPoint) {
		m_firstPoint = false;
		m_brushEngine.beginStroke(m_owner.client()->myId());
		m_brushEngine.strokeTo(
			m_start.x(),
			m_start.y(),
			qMin(m_start.pressure(), point.pressure()),
			m_start.xtilt(),
			m_start.ytilt(),
			m_start.rotation(),
			0,
			canvasState);
	}

	qint64 now = QDateTime::currentMSecsSinceEpoch();
	qint64 deltaMsec = now - m_lastTimestamp;
	m_lastTimestamp = now;

	m_brushEngine.strokeTo(
		point.x(),
		point.y(),
		point.pressure(),
		point.xtilt(),
		point.ytilt(),
		point.rotation(),
		deltaMsec,
		canvasState);

	m_brushEngine.sendMessagesTo(m_owner.client());
}

void Freehand::end()
{
	if(m_drawing) {
		m_drawing = false;

		if(m_firstPoint) {
			m_firstPoint = false;
			m_brushEngine.beginStroke(m_owner.client()->myId());
			m_brushEngine.strokeTo(
				m_start.x(),
				m_start.y(),
				m_start.pressure(),
				m_start.xtilt(),
				m_start.ytilt(),
				m_start.rotation(),
				QDateTime::currentMSecsSinceEpoch(),
				drawdance::CanvasState::null());
		}

		m_brushEngine.endStroke();
		m_brushEngine.sendMessagesTo(m_owner.client());
	}
}

void Freehand::offsetActiveTool(int x, int y)
{
	if(m_drawing) {
		m_brushEngine.addOffset(x, y);
	}
}

}

