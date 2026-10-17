// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/crop.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

CropTool::CropTool(ToolController &owner)
	: Tool(owner, CROP, Qt::PointingHandCursor, Capability::HandlesRightClick)
{
}

void CropTool::begin(const BeginParams &params)
{
	m_drawing = true;
	m_start = floorPoint(params.point);
	m_end = m_start;
	m_active = true;
	Q_EMIT m_owner.cropAreaChanged(getCurrentRect());
}

void CropTool::motion(const MotionParams &params)
{
	m_end = floorPoint(params.point);
	Q_EMIT m_owner.cropAreaChanged(getCurrentRect());
}

void CropTool::end(const EndParams &params)
{
	m_drawing = false;
}

void CropTool::finishMultipart()
{
	cancelMultipart();
}

void CropTool::cancelMultipart()
{
	if(m_active) {
		m_active = false;
		Q_EMIT m_owner.cropAreaChanged(QRect());
	}
}

void CropTool::undoMultipart() {}

void CropTool::redoMultipart() {}

bool CropTool::isMultipart() const
{
	return m_active;
}

void CropTool::finish()
{
	cancelMultipart();
}

void CropTool::setCropArea(QRect cropArea)
{
	if(!m_drawing && !cropArea.isEmpty() && cropArea != getCurrentRect()) {
		m_active = true;
		m_start = cropArea.topLeft();
		m_end = cropArea.bottomRight();
		Q_EMIT m_owner.cropAreaChanged(getCurrentRect());
	}
}

QPoint CropTool::floorPoint(QPointF point)
{
	return QPoint(qFloor(point.x()), qFloor(point.y()));
}

QRect CropTool::getCurrentRect() const
{
	return QRect(
		QPoint(qMin(m_start.x(), m_end.x()), qMin(m_start.y(), m_end.y())),
		QPoint(qMax(m_start.x(), m_end.x()), qMax(m_start.y(), m_end.y())));
}

}
