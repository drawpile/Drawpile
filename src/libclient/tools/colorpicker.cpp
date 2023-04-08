// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/canvas/canvasmodel.h"

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/colorpicker.h"

#include <QPixmap>

namespace tools {

ColorPicker::ColorPicker(ToolController &owner)
	: Tool(owner, PICKER, QCursor(QPixmap(":/cursors/colorpicker.png"), 2, 29), false, true, false),
	m_pickFromCurrentLayer(false),
	m_size(0)
{
}

void ColorPicker::begin(const canvas::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	if(right) {
		return;
	}

	motion(point, false, false);
}

void ColorPicker::motion(const canvas::Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	int layer=0;
	if(m_pickFromCurrentLayer)
		layer = m_owner.activeLayer();

	m_owner.model()->pickColor(point.x(), point.y(), layer, m_size);
}

void ColorPicker::end()
{
	// nothing to do here
}

}

