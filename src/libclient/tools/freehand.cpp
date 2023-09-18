// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/freehand.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include <QDateTime>

using std::placeholders::_1;

namespace tools {

Freehand::Freehand(ToolController &owner, bool isEraser)
	: Tool(
		  owner, isEraser ? ERASER : FREEHAND, Qt::CrossCursor, true, true,
		  false)
	, m_pollTimer{}
	, m_brushEngine{std::bind(&Freehand::pollControl, this, _1)}
	, m_drawing(false)
{
	m_pollTimer.setSingleShot(false);
	m_pollTimer.setTimerType(Qt::PreciseTimer);
	m_pollTimer.setInterval(15);
	QObject::connect(&m_pollTimer, &QTimer::timeout, [this]() {
		poll();
	});
}

Freehand::~Freehand() {}

void Freehand::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_ASSERT(!m_drawing);
	if(right) {
		return;
	}

	m_drawing = true;
	m_firstPoint = true;

	m_owner.setBrushEngineBrush(m_brushEngine);

	// The pressure value of the first point is unreliable
	// because it is (or was?) possible to get a synthetic MousePress event
	// before the StylusPress event.
	m_start = point;
	m_zoom = zoom;
}

void Freehand::motion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(!m_drawing)
		return;

	drawdance::CanvasState canvasState =
		m_owner.model()->paintEngine()->sampleCanvasState();

	if(m_firstPoint) {
		m_firstPoint = false;
		m_brushEngine.beginStroke(m_owner.client()->myId(), true, m_zoom);
		m_start.setPressure(qMin(m_start.pressure(), point.pressure()));
		m_brushEngine.strokeTo(m_start, canvasState);
	}

	m_brushEngine.strokeTo(point, canvasState);
	m_brushEngine.sendMessagesTo(m_owner.client());
}

void Freehand::end()
{
	if(m_drawing) {
		m_drawing = false;
		drawdance::CanvasState canvasState =
			m_owner.model()->paintEngine()->sampleCanvasState();

		if(m_firstPoint) {
			m_firstPoint = false;
			m_brushEngine.beginStroke(m_owner.client()->myId(), true, m_zoom);
			m_brushEngine.strokeTo(m_start, canvasState);
		}

		m_brushEngine.endStroke(
			QDateTime::currentMSecsSinceEpoch(), canvasState, true);
		m_brushEngine.sendMessagesTo(m_owner.client());
	}
}

void Freehand::offsetActiveTool(int x, int y)
{
	if(m_drawing) {
		m_brushEngine.addOffset(x, y);
	}
}

void Freehand::pollControl(bool enable)
{
	if(enable) {
		m_pollTimer.start();
	} else {
		m_pollTimer.stop();
	}
}

void Freehand::poll()
{
	drawdance::CanvasState canvasState =
		m_owner.model()->paintEngine()->sampleCanvasState();
	m_brushEngine.poll(QDateTime::currentMSecsSinceEpoch(), canvasState);
	m_brushEngine.sendMessagesTo(m_owner.client());
}

}
