// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selection.h"

#include "libclient/tools/zoom.h"
#include "libclient/tools/toolcontroller.h"

#include <QCursor>
#include <QPixmap>

namespace tools {

ZoomTool::ZoomTool(ToolController &owner)
	: Tool(owner, ZOOM, QCursor(QPixmap(":cursors/zoom.png"), 8, 8), true, false, true)
{ }

void ZoomTool::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	m_start = point.toPoint();
	m_end = m_start;
	m_reverse = right;

	// We use the selection item to preview the zoom rectangle
	// This is OK, since the zoom rectangle is visible only when
	// the zoom tool is active, and both tools can't be active at the same time
	auto sel = new canvas::Selection;
	sel->setShapeRect(QRect(m_start, m_start));
	m_owner.model()->setSelection(sel);
}

void ZoomTool::motion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	auto sel = m_owner.model()->selection();
	if(!sel) {
		qWarning("ZoomTool::motion(): no selection!");
		return;
	}

	m_end = point.toPoint();
	sel->setShapeRect(QRect(m_start, m_end).normalized());
}

void ZoomTool::end()
{
	auto sel = m_owner.model()->selection();
	if(!sel)
		return;

	const int steps = 3;

	emit m_owner.zoomRequested(sel->boundingRect(), m_reverse ? -steps : steps);

	m_owner.model()->setSelection(nullptr);
}

}

