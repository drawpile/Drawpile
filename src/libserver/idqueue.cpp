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
#include "libserver/idqueue.h"

namespace server {

IdQueue::IdQueue()
{
	m_ids.reserve(LAST_ID-FIRST_ID+1);
	for(uint8_t i=FIRST_ID;i<=LAST_ID;++i)
		m_ids << i;
}

void IdQueue::setIdForName(uint8_t id, const QString &name)
{
	m_nameIds[name] = id;
	reserveId(id);
}

uint8_t IdQueue::getIdForName(const QString &name) const
{
	return m_nameIds.value(name, 0);
}

uint8_t IdQueue::nextId()
{
	uint8_t id = m_ids.takeFirst();
	m_ids.append(id);
	return id;
}

void IdQueue::reserveId(uint8_t id)
{
	bool found = m_ids.removeOne(id);
	Q_ASSERT(found);
	if(found)
		m_ids.append(id);
}

}

