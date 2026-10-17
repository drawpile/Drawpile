// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/crop.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

CropTool::CropTool(ToolController &owner)
	: Tool(owner, CROP, Qt::PointingHandCursor, Capability::HandlesRightClick)
{
}

void CropTool::begin(const BeginParams &params)
{
	if(params.right) {

	} else {
		m_drawing = true;
		m_start = floorPoint(params.point);
		m_end = m_start;
		m_active = true;
		Q_EMIT m_owner.cropAreaChanged(getCurrentRect());
	}
}

void CropTool::motion(const MotionParams &params)
{
	if(m_drawing) {
		m_end = floorPoint(params.point);
		Q_EMIT m_owner.cropAreaChanged(getCurrentRect());
	}
}

void CropTool::end(const EndParams &params)
{
	if(m_drawing) {
		m_drawing = false;
	}
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

void CropTool::setProperties(const Properties &props)
{
	bool hadGrow = m_props.grow;
	m_props = props;
	if(m_active && hadGrow && !props.grow) {
		QRect rect = getCurrentRect();
		m_start = rect.topLeft();
		m_end = rect.bottomRight();
		Q_EMIT m_owner.cropAreaChanged(rect);
	}
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
	int left = qMin(m_start.x(), m_end.x());
	int top = qMin(m_start.y(), m_end.y());
	int right = qMax(m_start.x(), m_end.x());
	int bottom = qMax(m_start.y(), m_end.y());
	if(!m_props.grow) {
		canvas::CanvasModel *canvas = m_owner.model();
		if(canvas) {
			QRect canvasRect(QPoint(0, 0), canvas->size());
			if(!canvasRect.isEmpty()) {
				return QRect(
					QPoint(
						qBound(canvasRect.left(), left, canvasRect.right()),
						qBound(canvasRect.top(), top, canvasRect.bottom())),
					QPoint(
						qBound(canvasRect.left(), right, canvasRect.right()),
						qBound(canvasRect.top(), bottom, canvasRect.bottom())));
			}
		}
	}
	return QRect(QPoint(left, top), QPoint(right, bottom));
}
}
