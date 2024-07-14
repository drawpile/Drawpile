// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/pan.h"
#include "libclient/tools/toolcontroller.h"
#include <QCursor>
#include <QPixmap>

namespace tools {

PanTool::PanTool(ToolController &owner)
	: Tool(
		  owner, PAN, QCursor(Qt::OpenHandCursor), true, false, false, true,
		  false)
{
}

void PanTool::begin(const BeginParams &params)
{
	if(!params.right) {
		setCursor(Qt::ClosedHandCursor);
		m_lastViewPos = params.viewPos;
		m_panning = true;
	}
}

void PanTool::motion(const MotionParams &params)
{
	if(m_panning) {
		QPointF delta = m_lastViewPos - params.viewPos;
		emit m_owner.panRequested(qRound(delta.x()), qRound(delta.y()));
		m_lastViewPos = params.viewPos;
	}
}

void PanTool::end(const EndParams &)
{
	if(m_panning) {
		setCursor(Qt::OpenHandCursor);
		m_panning = false;
	}
}

}
