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
#include "docks/toolsettingsdock.h"

#include "tools/toolcontroller.h"
#include "tools/toolsettings.h"
#include "tools/colorpicker.h"

namespace tools {

ColorPicker::ColorPicker(ToolController &owner)
	: Tool(owner, PICKER, QCursor(QPixmap(":/cursors/colorpicker.png"), 2, 29))
{
}

void ColorPicker::begin(const paintcore::Point& point, float zoom)
{
	Q_UNUSED(zoom);

	motion(point, false, false);
}

void ColorPicker::motion(const paintcore::Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	int layer=0;
	if(owner.toolSettings()->getColorPickerSettings()->pickFromLayer()) {
		layer = owner.activeLayer();
	}

	owner.model()->pickColor(point.x(), point.y(), layer);
}

void ColorPicker::end()
{
	// nothing to do here
}

}

