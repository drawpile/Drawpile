/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "scene/canvasscene.h"
#include "docks/toolsettingsdock.h"

#include "tools/toolsettings.h"
#include "tools/colorpicker.h"

namespace tools {

void ColorPicker::begin(const paintcore::Point& point, bool right)
{
	_bg = right;
	motion(point, false, false);
}

void ColorPicker::motion(const paintcore::Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	int layer=0;
	if(settings().getColorPickerSettings()->pickFromLayer()) {
		layer = this->layer();
	}
	scene().pickColor(point.x(), point.y(), layer, _bg);
}

void ColorPicker::end()
{
	// nothing to do here
}

}

