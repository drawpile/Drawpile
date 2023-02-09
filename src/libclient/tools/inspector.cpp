/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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
#include "tools/inspector.h"

namespace tools {

Inspector::Inspector(ToolController &owner)
	: Tool(owner, INSPECTOR, QCursor(Qt::WhatsThisCursor), false, false)
{
}

void Inspector::begin(const canvas::Point& point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);
	owner.model()->inspectCanvas(point.x(), point.y());
}

void Inspector::motion(const canvas::Point& point, bool constrain, bool center)
{
	Q_UNUSED(point);
	Q_UNUSED(constrain);
	Q_UNUSED(center);
}

void Inspector::end()
{
	owner.model()->stopInspectingCanvas();
}

void Inspector::cancelMultipart()
{
	end();
}

}

