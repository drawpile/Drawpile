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
		  false, false, true, true)
	, m_brushEngine(std::bind(&Freehand::pollControl, this, _1))
{
	m_pollTimer.setSingleShot(false);
	m_pollTimer.setTimerType(Qt::PreciseTimer);
	m_pollTimer.setInterval(15);
	QObject::connect(&m_pollTimer, &QTimer::timeout, [this]() {
		poll();
	});
}

Freehand::~Freehand() {}

void Freehand::begin(const BeginParams &params)
{
	Q_ASSERT(!m_drawing);
	if(params.right) {
		return;
	}

	m_drawing = true;
	m_firstPoint = true;

	m_owner.setBrushEngineBrush(m_brushEngine, true);

	// The pressure value of the first point is unreliable
	// because it is (or was?) possible to get a synthetic MousePress event
	// before the StylusPress event.
	m_start = params.point;
	m_zoom = params.zoom;
	m_angle = params.angle;
	m_mirror = params.mirror;
	m_flip = params.flip;
}

void Freehand::motion(const MotionParams &params)
{
	if(!m_drawing)
		return;

	drawdance::CanvasState canvasState =
		m_owner.model()->paintEngine()->sampleCanvasState();

	if(m_firstPoint) {
		m_firstPoint = false;
		m_brushEngine.beginStroke(
			m_owner.client()->myId(), true, m_mirror, m_flip, m_zoom, m_angle);
		m_start.setPressure(qMin(m_start.pressure(), params.point.pressure()));
		m_brushEngine.strokeTo(m_start, canvasState);
	}

	m_brushEngine.strokeTo(params.point, canvasState);
	m_brushEngine.sendMessagesTo(m_owner.client());
}

void Freehand::end(const EndParams &)
{
	if(m_drawing) {
		m_drawing = false;
		drawdance::CanvasState canvasState =
			m_owner.model()->paintEngine()->sampleCanvasState();

		if(m_firstPoint) {
			m_firstPoint = false;
			m_brushEngine.beginStroke(
				m_owner.client()->myId(), true, m_mirror, m_flip, m_zoom,
				m_angle);
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
