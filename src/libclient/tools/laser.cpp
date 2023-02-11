/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2016 Calle Laakkonen

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

#include "drawdance/message.h"
#include "net/client.h"
#include "tools/toolcontroller.h"
#include "tools/laser.h"

#include <QPixmap>

namespace tools {

LaserPointer::LaserPointer(ToolController &owner)
	: Tool(owner, LASERPOINTER, QCursor(QPixmap(":cursors/arrow.png"), 0, 0), false, true, false),
	m_persistence(1), m_drawing(false)
{}

void LaserPointer::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_ASSERT(!m_drawing);
	if(right) {
		return;
	}

	m_drawing = true;

	net::Client *client = owner.client();
	uint8_t contextId = client->myId();
	uint32_t color = owner.activeBrush().qColor().rgb();
	drawdance::Message messages[] = {
		drawdance::Message::noinc(DP_msg_laser_trail_new(contextId, color, m_persistence)),
		drawdance::Message::noinc(DP_msg_move_pointer_new(contextId, point.x(), point.y())),
	};
	client->sendMessages(DP_ARRAY_LENGTH(messages), messages);
}

void LaserPointer::motion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(m_drawing) {
		owner.client()->sendMessage(drawdance::Message::noinc(
			DP_msg_move_pointer_new(owner.client()->myId(), point.x(), point.y())));
	}
}

void LaserPointer::end()
{
	if(m_drawing) {
		m_drawing = false;
		owner.client()->sendMessage(drawdance::Message::noinc(
			DP_msg_laser_trail_new(owner.client()->myId(), 0, 0)));
	}
}

}

