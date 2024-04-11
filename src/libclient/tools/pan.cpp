// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/pan.h"
#include "libclient/tools/toolcontroller.h"
#include <QCursor>
#include <QPixmap>

namespace tools {

PanTool::PanTool(ToolController &owner)
	: Tool(owner, PAN, QCursor(Qt::OpenHandCursor), true, false, true)
{
}

void PanTool::begin(
	const canvas::Point &point, bool right, float zoom, const QPointF &viewPos)
{
	Q_UNUSED(point);
	Q_UNUSED(zoom);
	if(!right) {
		setCursor(Qt::ClosedHandCursor);
		m_lastViewPos = viewPos;
		m_panning = true;
	}
}

void PanTool::motion(
	const canvas::Point &point, bool constrain, bool center,
	const QPointF &viewPos)
{
	Q_UNUSED(point);
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(m_panning) {
		QPointF delta = m_lastViewPos - viewPos;
		emit m_owner.panRequested(qRound(delta.x()), qRound(delta.y()));
		m_lastViewPos = viewPos;
	}
}

void PanTool::end()
{
	if(m_panning) {
		setCursor(Qt::OpenHandCursor);
		m_panning = false;
	}
}

}
