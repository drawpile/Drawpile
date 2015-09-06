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

#include "../shared/net/meta2.h"

namespace tools {

LaserPointer::LaserPointer(ToolController &owner)
	: Tool(owner, LASERPOINTER, QCursor(QPixmap(":cursors/arrow.png"), 0, 0))
{}

void LaserPointer::begin(const paintcore::Point &point, float zoom)
{
	Q_UNUSED(zoom);
	QList<protocol::MessagePtr> msgs;
	msgs << protocol::MessagePtr(new protocol::LaserTrail(0,
		owner.activeBrush().color().rgb(),
		owner.toolSettings()->getLaserPointerSettings()->trailPersistence()
	));
	msgs << protocol::MessagePtr(new protocol::MovePointer(0, point.x() * 4, point.y() * 4));
	owner.client()->sendMessages(msgs);

}

void LaserPointer::motion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	owner.client()->sendMessage(protocol::MessagePtr(new protocol::MovePointer(0, point.x() * 4, point.y() * 4)));
}

void LaserPointer::end()
{
	owner.client()->sendMessage(protocol::MessagePtr(new protocol::LaserTrail(0, 0, 0)));
}

}

