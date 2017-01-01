/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2017 Calle Laakkonen

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

#include "history.h"
#include "../shared/net/undo.h"

namespace canvas {

using namespace protocol;

History::History()
	: m_offset(0), m_bytes(0)
{
}

void History::append(MessagePtr msg)
{
	m_messages.append(msg);
	m_bytes += msg->length();
}

void History::cleanup(int indexlimit)
{
	Q_ASSERT(indexlimit <= end());

	// First, find the index of the last protected undo point
	int undo_point = m_offset;
	int undo_points = 0;
	for(int i=end()-1;i>=offset() && undo_points<UNDO_HISTORY_LIMIT;--i) {
		if(at(i)->type() == MSG_UNDOPOINT) {
			undo_point = i;
			++undo_points;
		}
	}

	if(undo_point < indexlimit)
		indexlimit = undo_point;

	// Remove oldest messages
	while(m_offset < indexlimit) {
		m_bytes -= m_messages.takeFirst()->length();
		++m_offset;
	}
}

void History::resetTo(int newoffset)
{
	m_offset = newoffset;
	m_messages.clear();
	m_bytes = 0;
}

}
