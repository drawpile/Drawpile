/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
	if(_domain == EVERYTHING || other._domain == EVERYTHING)
		return false;

	if(_domain == USERATTRS || _domain != other._domain || _layer != other._layer)
		return true;

	// for pixel changes, the effect bounding rectangles must not intersect
	if(_domain == PIXELS && !_bounds.intersects(other._bounds))
		return true;

	return false;
}

void LocalFork::setOffset(int offset)
{
	_offset = offset;
}

void LocalFork::addLocalMessage(MessagePtr msg, const AffectedArea &area)
{
	_messages.append(msg);
	_areas.append(area);
}

void LocalFork::clear()
{
	_messages.clear();
	_areas.clear();
}

LocalFork::MessageAction LocalFork::handleReceivedMessage(MessagePtr msg, const AffectedArea &area)
{
	// No local fork: nothing to do. It is possible that we get a message from ourselves
	// that is not in the local fork, but this is not an error. It could happen when
	// playing back recordings, for example.
	if(_messages.isEmpty())
		return CONCURRENT;

	// Check if this is our own message that has finished its roundtrip
	if(msg->contextId() == _messages.first()->contextId()) {
		if(msg.equals(_messages.first())) {
			_messages.removeFirst();
			_areas.removeFirst();
			return ALREADYDONE;

		} else {
			// Unusual, but not (necessarily) an error. This can happen when the layer is locked
			// while drawing or when an operator performs some function on behalf the user.
			qWarning("Local fork out of sync, discarding %d messages!", _messages.size());
			qDebug("     Got: %s", qPrintable(msg->toString()));
			qDebug("Expected: %s", qPrintable(_messages.first()->toString()));
			clear();
			return ROLLBACK;
		}
	}

	// OK, so this is another user's message. Check if it is concurrent
	for(const AffectedArea &a : _areas) {
		if(!area.isConcurrentWith(a)) {
			return ROLLBACK;
		}
	}

	return CONCURRENT;
}

}
