// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/sessionhistory.h"
#include "libserver/client.h"

namespace server {

SessionHistory::SessionHistory(const QString &id, QObject *parent)
	: QObject(parent)
	, m_id(id)
	, m_startTime(QDateTime::currentDateTimeUtc())
	, m_sizeInBytes(0)
	, m_sizeLimit(0)
	, m_autoResetBaseSize(0)
	, m_firstIndex(0)
	, m_lastIndex(-1)
{
}

bool SessionHistory::hasSpaceFor(uint bytes, uint extra) const
{
	return m_sizeLimit <= 0 || m_sizeInBytes + bytes <= m_sizeLimit + extra;
}

bool SessionHistory::addBan(
	const QString &username, const QHostAddress &ip, const QString &extAuthId,
	const QString &sid, const QString &bannedBy, const Client *client)
{
	int id;
	if(client) {
		const SessionBanner banner = {
			client->username(),
			client->authId(),
			client->peerAddress(),
			client->sid(),
		};
		id = m_banlist.addBan(
			username, ip, extAuthId, sid, bannedBy, 0, &banner);
	} else {
		id = m_banlist.addBan(username, ip, extAuthId, sid, bannedBy);
	}

	if(id > 0) {
		historyAddBan(id, username, ip, extAuthId, sid, bannedBy);
		return true;
	}
	return false;
}

bool SessionHistory::importBans(
	const QJsonObject &data, int &outTotal, int &outImported,
	const Client *client)
{
	outTotal = 0;
	outImported = 0;
	return SessionBanList::importBans(data, [&](const SessionBan &b) {
		++outTotal;
		if(addBan(b.username, b.ip, b.authId, b.sid, b.bannedBy, client)) {
			++outImported;
		}
	});
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
	Q_ASSERT(m_lastIndex == -1);
	m_sizeInBytes = size;
	m_lastIndex = messageCount - 1;
	m_autoResetBaseSize = size;
}

bool SessionHistory::addMessage(const net::Message &msg)
{
	uint bytes = uint(msg.length());
	if(hasRegularSpaceFor(bytes)) {
		addMessageInternal(msg, bytes);
		return true;
	} else {
		return false;
	}
}

bool SessionHistory::addEmergencyMessage(const net::Message &msg)
{
	uint bytes = uint(msg.length());
	if(hasEmergencySpaceFor(bytes)) {
		addMessageInternal(msg, bytes);
		return true;
	} else {
		return false;
	}
}

void SessionHistory::addMessageInternal(const net::Message &msg, uint bytes)
{
	m_sizeInBytes += bytes;
	++m_lastIndex;
	historyAdd(msg);
	emit newMessagesAvailable();
}

bool SessionHistory::reset(const net::MessageList &newHistory)
{
	uint newSize = 0;
	for(const net::Message &msg : newHistory) {
		newSize += uint(msg.length());
	}
	if(m_sizeLimit > 0 && newSize > m_sizeLimit) {
		return false;
	}

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
	if(t > 0) {
		t += m_autoResetBaseSize;
		if(m_sizeLimit > 0) {
			t = qMin(t, uint(m_sizeLimit * 0.9));
		}
	}
	return t;
}

void SessionHistory::setAuthenticatedOperator(const QString &authId, bool op)
{
	if(op) {
		Q_ASSERT(!authId.isEmpty());
		m_authOps.insert(authId);
	} else {
		m_authOps.remove(authId);
	}
}

void SessionHistory::setAuthenticatedTrust(const QString &authId, bool trusted)
{
	if(trusted) {
		Q_ASSERT(!authId.isEmpty());
		m_authTrusted.insert(authId);
	} else {
		m_authTrusted.remove(authId);
	}
}

void SessionHistory::setAuthenticatedUsername(
	const QString &authId, const QString &username)
{
	Q_ASSERT(!authId.isEmpty());
	Q_ASSERT(!username.isEmpty());
	m_authUsernames.insert(authId, username);
}

const QString *SessionHistory::authenticatedUsernameFor(const QString &authId)
{
	QHash<QString, QString>::const_iterator it = m_authUsernames.find(authId);
	return it == m_authUsernames.constEnd() ? nullptr : &it.value();
}

int SessionHistory::incrementNextCatchupKey(int &nextCatchupKey)
{
	int result = nextCatchupKey;
	// Wrap around the catchup key at an arbitrary, but plenty large value.
	nextCatchupKey = result < MAX_CATCHUP_KEY ? result + 1 : MIN_CATCHUP_KEY;
	return result;
}

}
