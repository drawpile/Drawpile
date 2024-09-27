// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/colorpicker.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/tools/toolcontroller.h"
#include <QPixmap>

namespace tools {

ColorPicker::ColorPicker(ToolController &owner)
	: Tool(
		  owner, PICKER, QCursor(QPixmap(":/cursors/colorpicker.png"), 2, 29),
		  false, true, false, true, false)
{
}

void ColorPicker::begin(const BeginParams &params)
{
	if(!params.right) {
		m_picking = true;
		pick(params.point, params.viewPos);
	}
}

void ColorPicker::motion(const MotionParams &params)
{
	if(m_picking) {
		pick(params.point, params.viewPos);
	}
}

void ColorPicker::end(const EndParams &)
{
	m_picking = false;
	m_owner.colorPickRequested(QPointF(), QColor());
}

void ColorPicker::pick(const QPointF &point, const QPointF &viewPos) const
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		int layer = m_pickFromCurrentLayer ? m_owner.activeLayer() : 0;
		QColor color = canvas->pickColor(point.x(), point.y(), layer, m_size);
		m_owner.colorPickRequested(viewPos, color);
	}
}

}
