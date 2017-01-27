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
#include "sessionhistory.h"

namespace server {

SessionHistory::SessionHistory(const QUuid &id, QObject *parent)
	: QObject(parent), m_id(id), m_sizeInBytes(0), m_sizeLimit(0), m_firstIndex(0), m_lastIndex(-1)
{
}

bool SessionHistory::addBan(const QString &username, const QHostAddress &ip, const QString &bannedBy)
{
	const int id = m_banlist.addBan(username, ip, bannedBy);
	if(id>0) {
		historyAddBan(id, username, ip, bannedBy);
		return true;
	}
	return false;
}

bool SessionHistory::removeBan(int id)
{
	if(m_banlist.removeBan(id)) {
		historyRemoveBan(id);
		return true;
	}
	return false;
}

void SessionHistory::joinUser(uint8_t id, const QString &name)
{
	idQueue().setIdForName(id, name);
}

void SessionHistory::historyLoaded(uint size, int messageCount)
{
	Q_ASSERT(m_lastIndex==-1);
	m_sizeInBytes = size;
	m_lastIndex = messageCount - 1;
}

bool SessionHistory::addMessage(const protocol::MessagePtr &msg)
{
	m_sizeInBytes += msg->length();
	if(m_sizeLimit>0 && m_sizeInBytes > m_sizeLimit) {
		m_sizeInBytes -= msg->length();
		return false;
	}

	++m_lastIndex;
	historyAdd(msg);
	emit newMessagesAvailable();
	return true;
}

bool SessionHistory::reset(const QList<protocol::MessagePtr> &newHistory)
{
	uint newSize = 0;
	for(const protocol::MessagePtr &msg : newHistory) {
		newSize += msg->length();
	}
	if(m_sizeLimit>0 && newSize > m_sizeLimit)
		return false;

	m_sizeInBytes = newSize;
	m_firstIndex = m_lastIndex + 1;
	m_lastIndex += newHistory.size();
	historyReset(newHistory);
	emit newMessagesAvailable();
	return true;
}

}

