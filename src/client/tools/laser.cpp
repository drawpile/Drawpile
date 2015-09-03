/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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

#include "docks/toolsettingsdock.h"
#include "core/brush.h"
#include "net/client.h"

#include "tools/toolcontroller.h"
#include "tools/toolsettings.h"
#include "tools/laser.h"

namespace tools {

LaserPointer::LaserPointer(ToolController &owner)
	: Tool(owner, LASERPOINTER, QCursor(QPixmap(":cursors/arrow.png"), 0, 0))
{}

void LaserPointer::begin(const paintcore::Point &point, float zoom)
{
	Q_UNUSED(zoom);
	owner.client()->sendLaserTrail(
		owner.activeBrush().color(),
		owner.toolSettings()->getLaserPointerSettings()->trailPersistence()
	);
	owner.client()->sendMovePointer(point);
}

void LaserPointer::motion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	owner.client()->sendMovePointer(point);
}

void LaserPointer::end()
{
	owner.client()->sendLaserTrail(Qt::black, 0);
}

}

