// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/inspector.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

Inspector::Inspector(ToolController &owner)
	: Tool(owner, INSPECTOR, QCursor(Qt::WhatsThisCursor), false, false, false)
{
}

void Inspector::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	if(!right) {
		m_inspecting = true;
		m_owner.model()->inspectCanvas(point.x(), point.y(), true, m_showTiles);
	}
}

void Inspector::motion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(m_inspecting) {
		m_owner.model()->inspectCanvas(
			point.x(), point.y(), false, m_showTiles);
	}
}

void Inspector::end()
{
	m_inspecting = false;
	m_owner.model()->stopInspectingCanvas();
}

void Inspector::cancelMultipart()
{
	end();
}

}
