// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/colorpicker.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/tools/enums.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QPixmap>

namespace tools {

ColorPicker::ColorPicker(ToolController &owner)
	: Tool(
		  owner, PICKER, utils::Cursors::colorPick(),
		  Capability::Fractional | Capability::SendsNoMessages |
			  Capability::AllowToolAdjust1)
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
	m_owner.hideColorPickRequested();
}

void ColorPicker::pick(const QPointF &point, const QPointF &viewPos) const
{
	canvas::CanvasModel *canvas = m_owner.model();
	if(canvas) {
		int layer = m_pickFromCurrentLayer ? m_owner.activeLayer() : 0;
		canvas->pickColor(point.x(), point.y(), layer, m_size);
		m_owner.showColorPickRequested(int(ColorPickSource::Tool), viewPos);
	}
}

}
