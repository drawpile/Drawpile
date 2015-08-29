/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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

#include "messagestream.h"
#include "undo.h"

namespace protocol {

MessageStream::MessageStream()
	: m_offset(0), m_bytes(0)
{
}

void MessageStream::append(MessagePtr msg)
{
	m_messages.append(msg);
	m_bytes += msg->length();
}

void MessageStream::hardCleanup(uint sizelimit, int indexlimit)
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

	// Remove messages until size limit or protected undo point is reached
	while(m_bytes > sizelimit && m_offset < indexlimit) {
		m_bytes -= m_messages.takeFirst()->length();
		++m_offset;
	}
}

void MessageStream::resetTo(int newoffset)
{
	m_offset = newoffset;
	m_messages.clear();
	m_bytes = 0;
}

QList<MessagePtr> MessageStream::toCommandList() const
{
	QList<MessagePtr> lst;
	for(MessagePtr m : m_messages)
		if(m->isCommand())
			lst.append(m);
	return lst;
}

}
