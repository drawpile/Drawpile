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

#include "libclient/net/client.h"

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/laser.h"
#include "libclient/net/envelopebuilder.h"

#include "rustpile/rustpile.h"

#include <QPixmap>

namespace tools {

LaserPointer::LaserPointer(ToolController &owner)
	: Tool(owner, LASERPOINTER, QCursor(QPixmap(":cursors/arrow.png"), 0, 0)),
	m_persistence(1), m_drawing(false)
{}

void LaserPointer::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);
	Q_ASSERT(!m_drawing);

	m_drawing = true;

	net::EnvelopeBuilder msgs;

	rustpile::write_lasertrail(msgs, owner.client()->myId(), owner.activeBrush().qColor().rgb(), m_persistence);
	rustpile::write_movepointer(msgs, owner.client()->myId(), point.x(), point.y());
	owner.client()->sendEnvelope(msgs.toEnvelope());

}

void LaserPointer::motion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(m_drawing) {
		net::EnvelopeBuilder msgs;
		rustpile::write_movepointer(msgs, owner.client()->myId(), point.x(), point.y());
		owner.client()->sendEnvelope(msgs.toEnvelope());
	}
}

void LaserPointer::end()
{
	if(m_drawing) {
		m_drawing = false;

		net::EnvelopeBuilder msgs;
		rustpile::write_lasertrail(msgs, owner.client()->myId(), 0, 0);
		owner.client()->sendEnvelope(msgs.toEnvelope());
	}
}

}

