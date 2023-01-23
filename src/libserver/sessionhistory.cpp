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
#include "libserver/sessionhistory.h"

namespace server {

SessionHistory::SessionHistory(const QString &id, QObject *parent)
	: QObject(parent), m_id(id), m_startTime(QDateTime::currentDateTimeUtc()),
	  m_sizeInBytes(0), m_sizeLimit(0), m_autoResetBaseSize(0),
	  m_firstIndex(0), m_lastIndex(-1)
{
}

bool SessionHistory::addBan(const QString &username, const QHostAddress &ip, const QString &extAuthId, const QString &bannedBy)
{
	const int id = m_banlist.addBan(username, ip, extAuthId, bannedBy);
	if(id>0) {
		historyAddBan(id, username, ip, extAuthId, bannedBy);
		return true;
	}
	return false;
}

QString SessionHistory::removeBan(int id)
{
	QString unbanned = m_banlist.removeBan(id);
	if(!unbanned.isEmpty())
		historyRemoveBan(id);
	return unbanned;
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
	m_autoResetBaseSize = size;
}

bool SessionHistory::addMessage(const protocol::MessagePtr &msg)
{
	if(isOutOfSpace())
		return false;

	m_sizeInBytes += msg->length();
	++m_lastIndex;
	historyAdd(msg);
	emit newMessagesAvailable();
	return true;
}

bool SessionHistory::reset(const protocol::MessageList &newHistory)
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
	m_autoResetBaseSize = newSize;
	historyReset(newHistory);
	emit newMessagesAvailable();
	return true;
}

uint SessionHistory::effectiveAutoResetThreshold() const
{
	uint t = autoResetThreshold();
	// Zero means autoreset is not enabled
	if(t>0) {
		t += m_autoResetBaseSize;
		if(m_sizeLimit>0)
			t = qMin(t, uint(m_sizeLimit * 0.9));
	}
	return t;
}

}

