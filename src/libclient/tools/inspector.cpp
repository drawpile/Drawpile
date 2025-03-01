// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/inspector.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

Inspector::Inspector(ToolController &owner)
	: Tool(
		  owner, INSPECTOR, QCursor(Qt::WhatsThisCursor),
		  Capability::Fractional)
{
}

void Inspector::begin(const BeginParams &params)
{
	if(!params.right) {
		m_inspecting = true;
		inspect(params.point, true);
	}
}

void Inspector::motion(const MotionParams &params)
{
	if(m_inspecting) {
		inspect(params.point, false);
	}
}

void Inspector::end(const EndParams &)
{
	endInspecting();
}

void Inspector::cancelMultipart()
{
	endInspecting();
}

void Inspector::inspect(const QPointF &point, bool clobber) const
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		canvas->inspectCanvas(point.x(), point.y(), clobber, m_showTiles);
	}
}

void Inspector::endInspecting()
{
	m_inspecting = false;
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		canvas->stopInspectingCanvas();
	}
}

}
