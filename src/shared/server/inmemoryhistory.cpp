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
#include "inmemoryhistory.h"

namespace server {

InMemoryHistory::InMemoryHistory(QObject *parent)
	: SessionHistory(parent)
{
}

std::tuple<QList<protocol::MessagePtr>, int> InMemoryHistory::getBatch(int after) const
{
	if(after >= lastIndex())
		return std::make_tuple(QList<protocol::MessagePtr>(), lastIndex());

	const int offset = qMax(0, after - firstIndex() + 1);
	Q_ASSERT(offset<m_history.size());

	return std::make_tuple(m_history.mid(offset), lastIndex());
}

void InMemoryHistory::historyAdd(const protocol::MessagePtr &msg)
{
	m_history << msg;
}

void InMemoryHistory::historyReset(const QList<protocol::MessagePtr> &newHistory)
{
	m_history = newHistory;
}

}

