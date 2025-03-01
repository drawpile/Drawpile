// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/zoom.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QPainterPath>

namespace tools {

ZoomTool::ZoomTool(ToolController &owner)
	: Tool(
		  owner, ZOOM, utils::Cursors::zoom(),
		  Capability::AllowColorPick | Capability::HandlesRightClick)
{
}

void ZoomTool::begin(const BeginParams &params)
{
	m_start = params.point.toPoint();
	m_end = m_start;
	m_reverse = params.right;
	m_zooming = true;
}

void ZoomTool::motion(const MotionParams &params)
{
	m_end = params.point.toPoint();
	updatePreview();
}

void ZoomTool::end(const EndParams &)
{
	removePreview();
	if(m_zooming) {
		constexpr int STEPS = 3;
		emit m_owner.zoomRequested(getRect(), m_reverse ? -STEPS : STEPS);
		m_zooming = false;
	}
}

void ZoomTool::updatePreview() const
{
	QRect rect = getRect();
	if(rect.isEmpty()) {
		removePreview();
	} else {
		QPainterPath path;
		path.addRect(rect);
		emit m_owner.pathPreviewRequested(path);
	}
}

void ZoomTool::removePreview() const
{
	emit m_owner.pathPreviewRequested(QPainterPath());
}

QRect ZoomTool::getRect() const
{
	if(m_start == m_end) {
		return QRect(m_start, m_end);
	} else {
		return QRect(
			QPoint(qMin(m_start.x(), m_end.x()), qMin(m_start.y(), m_end.y())),
			QPoint(qMax(m_start.x(), m_end.x()), qMax(m_start.y(), m_end.y())));
	}
}

}
