/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "canvas/canvasmodel.h"

#include "tools/toolcontroller.h"
#include "tools/colorpicker.h"

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
		layer = owner.activeLayer();

	owner.model()->pickColor(point.x(), point.y(), layer, m_size);
}

void ColorPicker::end()
{
	// nothing to do here
}

}

