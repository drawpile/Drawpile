// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/sessionban.h"

#include <QJsonArray>
#include <QJsonObject>

namespace server {

SessionBanList::SessionBanList()
	: m_idautoinc(0)
{
}

static QHostAddress toIpv6(const QHostAddress &ip) {
	if(ip.protocol() == QAbstractSocket::IPv4Protocol)
		return QHostAddress(ip.toIPv6Address());
	return ip;
}

int SessionBanList::addBan(const QString &username, const QHostAddress &ip, const QString &extAuthId, const QString &bannedBy, int id)
{
	if(ip.isNull() || ip.isLoopback() || isBanned(ip, extAuthId))
		return 0;

	if(id>0) {
		// Make sure this ID doesn't exist already
		for(const SessionBan &b : m_banlist) {
			if(b.id == id)
				return 0;
		}
		m_idautoinc = qMax(m_idautoinc, id);

	} else {
		id = ++m_idautoinc;
	}

	m_banlist << SessionBan {
		id,
		username.isEmpty() ? "anon" : username,
		extAuthId,
		toIpv6(ip), // Always use IPv6 notation for consistency
		bannedBy
	};
	return id;
}

QString SessionBanList::removeBan(int id)
{
	QMutableListIterator<SessionBan> i(m_banlist);
	while(i.hasNext()) {
		SessionBan entry = i.next();
		if(entry.id == id) {
			i.remove();
			return entry.username;
		}
	}
	return QString();
}

bool SessionBanList::isBanned(const QHostAddress &address, const QString &authId) const
{
	// Registered user accounts are banned by auth ID.
	// Due to widespread use of NAT, lots of unrelated users can
	// share the same IP, so we want to avoid IP bans when possible.
	if(!authId.isEmpty()) {
		for(const SessionBan &b : m_banlist) {
			if(b.authId == authId)
				return true;
		}

		return false;
	}

	// Guest users are banned by IP, because that's the best we can do.
	if(!address.isNull()) {
		const QHostAddress ip = toIpv6(address);
		for(const SessionBan &b : m_banlist) {
			if(b.authId.isEmpty() && b.ip == ip)
				return true;
		}

		return false;
	}

	qWarning("isBanned() called without a valid address or extAuthId");
	return false;
}

QJsonArray SessionBanList::toJson(bool showIp) const
{
	QJsonArray list;
	for(const SessionBan &b : m_banlist) {
		QJsonObject o;
		o["id"] = b.id;
		o["username"] = b.username;
		o["bannedBy"] = b.bannedBy;
		if(showIp) {
			o["ip"] = b.ip.toString();
			o["extauthid"] = b.authId; // TODO for version 3.0: rename to "authId"
		}
		list.append(o);
	}
	return list;
}

}

