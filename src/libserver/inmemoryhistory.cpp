/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "libserver/inmemoryhistory.h"
#include "libshared/util/passwordhash.h"

namespace server {

InMemoryHistory::InMemoryHistory(const QString &id, const QString &alias, const protocol::ProtocolVersion &version, const QString &founder, QObject *parent)
	: SessionHistory(id, parent),
	  m_alias(alias),
	  m_founder(founder),
	  m_version(version),
	  m_maxUsers(254),
	  m_autoReset(0),
	  m_flags()
{
}

std::tuple<protocol::MessageList, int> InMemoryHistory::getBatch(int after) const
{
	if(after >= lastIndex())
		return std::make_tuple(protocol::MessageList(), lastIndex());

	const int offset = qMax(0, after - firstIndex() + 1);
	Q_ASSERT(offset<m_history.size());

	return std::make_tuple(m_history.mid(offset), lastIndex());
}

void InMemoryHistory::historyAdd(const protocol::MessagePtr &msg)
{
	m_history << msg;
}

void InMemoryHistory::historyReset(const protocol::MessageList &newHistory)
{
	m_history = newHistory;
}

}

