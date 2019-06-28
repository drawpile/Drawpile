/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#include "retcon.h"

using protocol::MessagePtr;

namespace canvas {

bool AffectedArea::isConcurrentWith(const AffectedArea &other) const
{
	if(m_domain == EVERYTHING || other.m_domain == EVERYTHING)
		return false;

	if(m_domain == USERATTRS || m_domain != other.m_domain || m_layer != other.m_layer)
		return true;

	// for pixel changes, the effect bounding rectangles must not intersect
	if(m_domain == PIXELS && !m_bounds.intersects(other.m_bounds))
		return true;

	return false;
}

void LocalFork::setOffset(int offset)
{
	m_offset = offset;
}

void LocalFork::addLocalMessage(MessagePtr msg, const AffectedArea &area)
{
	m_messages.append(msg);
	m_areas.append(area);
}

void LocalFork::clear()
{
	m_fallenBehind = 0;
	m_messages.clear();
	m_areas.clear();
}

LocalFork::MessageAction LocalFork::handleReceivedMessage(MessagePtr msg, const AffectedArea &area)
{
	// No local fork: nothing to do. It is possible that we get a message from ourselves
	// that is not in the local fork, but this is not an error. It could happen when
	// playing back recordings, for example.
	if(m_messages.isEmpty())
		return CONCURRENT;

	// Check if this is our own message that has finished its roundtrip
	if(msg->contextId() == m_messages.first()->contextId()) {
		if(msg.equals(m_messages.first())) {
			m_messages.removeFirst();
			m_areas.removeFirst();
			if(m_messages.isEmpty())
				m_fallenBehind = 0;
			return ALREADYDONE;

		} else {
			// Unusual, but not (necessarily) an error. This can happen when the layer is locked
			// while drawing or when an operator performs some function on behalf the user.
			qWarning("Local fork out of sync, discarding %d messages!", m_messages.size());
			qDebug("     Got: %s", qPrintable(msg->toString()));
			qDebug("Expected: %s", qPrintable(m_messages.first()->toString()));
			clear();
			return ROLLBACK;
		}
	}

	// OK, so this is another user's message. Check if it is concurrent

	// But first, check if the local fork has fallen too much behind
	if(m_maxFallBehind>0) {
		if(++m_fallenBehind >= m_maxFallBehind) {
			qWarning("Fell behind %d >= %d", m_fallenBehind, m_maxFallBehind);
			clear();
			return ROLLBACK;
		}
	}

	for(const AffectedArea &a : m_areas) {
		if(!area.isConcurrentWith(a)) {
			return ROLLBACK;
		}
	}

	return CONCURRENT;
}

}
