/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#include <QDebug>

#include "messagestream.h"
#include "snapshot.h"

namespace protocol {

MessageStream::MessageStream()
	: _offset(0)
{
}

void MessageStream::addSnapshotPoint()
{
	// Sanity checking
	if(hasSnapshot()) {
		const SnapshotPoint &sp = snapshotPoint().cast<protocol::SnapshotPoint>();
		if(!sp.isComplete()) {
			qWarning() << "Tried to add a new snapshot point even though the old one isn't finished!";
			return;
		}
	}

	// Create the new point
	append(MessagePtr(new SnapshotPoint()));
	_snapshotpointer = end()-1;
}

void MessageStream::cleanup()
{
	if(hasSnapshot()) {
		const SnapshotPoint &sp = snapshotPoint().cast<protocol::SnapshotPoint>();
		if(sp.isComplete()) {
			_messages = _messages.mid(_snapshotpointer - _offset);
			_snapshotpointer = 0;
		}
	}
}

void MessageStream::clear()
{
	_offset = end();
	_snapshotpointer = -1;
	_messages.clear();
}

}

