// SPDX-License-Identifier: GPL-3.0-or-later

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

