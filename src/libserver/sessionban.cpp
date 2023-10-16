// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/sessionban.h"
#include <QJsonArray>
#include <QJsonObject>

namespace server {

SessionBanList::SessionBanList()
	: m_idautoinc(0)
{
}

int SessionBanList::addBan(
	const QString &username, const QHostAddress &address, const QString &authId,
	const QString &sid, const QString &bannedBy, int id,
	const SessionBanner *banner)
{
	SessionBan ban = {
		id,
		username,
		authId,
		// Always use IPv6 for consistency. Localhost can't be banned by IP.
		address.isLoopback() ? QHostAddress() : toIpv6(address),
		sid,
		bannedBy,
	};

	// Don't allow the originator of the ban to ban themselves.
	if(banner) {
		if(ban.username.compare(banner->username, Qt::CaseInsensitive) == 0) {
			ban.username.clear();
		}
		if(ban.ip == toIpv6(banner->address)) {
			ban.ip.clear();
		}
		if(ban.authId == banner->authId) {
			ban.authId.clear();
		}
		if(ban.sid == banner->sid) {
			ban.sid.clear();
		}
	}

	if(!canAddBan(ban)) {
		return 0;
	}

	if(id > 0) {
		// Make sure this ID doesn't exist already
		for(const SessionBan &b : m_banlist) {
			if(b.id == ban.id) {
				return 0;
			}
		}
		m_idautoinc = qMax(m_idautoinc, ban.id);
	} else if(m_idautoinc < INT_MAX) {
		ban.id = ++m_idautoinc;
	} else {
		qWarning("Maximum ban id reached");
		return 0;
	}

	m_banlist.append(ban);
	return ban.id;
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

int SessionBanList::isBanned(
	const QString &username, const QHostAddress &address, const QString &authId,
	const QString &sid) const
{
	bool haveUsername = !username.isEmpty();
	bool haveAuthId = !authId.isEmpty();
	bool haveSid = !sid.isEmpty();
	bool haveAddress = !address.isNull();
	if(haveUsername || haveAuthId || haveSid || haveAddress) {
		QHostAddress ip = haveAddress ? toIpv6(address) : QHostAddress();
		for(const SessionBan &b : m_banlist) {
			bool isBanned =
				(haveUsername && !b.username.isEmpty() &&
				 username.compare(b.username, Qt::CaseInsensitive) == 0) ||
				(haveAuthId && !b.authId.isEmpty() && authId == b.authId) ||
				(haveSid && !b.sid.isEmpty() && sid == b.sid) ||
				(haveAddress && !b.ip.isNull() && ip == b.ip);
			if(isBanned) {
				// Zero ids shouldn't happen, but guard against it anyway.
				return b.id == 0 ? INT_MAX : b.id;
			}
		}
	} else {
		qWarning("isBanned() called without a valid parameters");
	}
	return 0;
}

QJsonArray SessionBanList::toJson(bool showSensitive) const
{
	QJsonArray list;
	for(const SessionBan &b : m_banlist) {
		QJsonObject o = {
			{QStringLiteral("id"), b.id},
			{QStringLiteral("username"), b.username},
			{QStringLiteral("bannedBy"), b.bannedBy},
		};
		if(showSensitive) {
			o[QStringLiteral("ip")] = b.ip.toString();
			// TODO: Should  be called "authid", it's not necessarily external.
			o[QStringLiteral("extauthid")] = b.authId;
			o[QStringLiteral("s")] = b.sid;
		}
		list.append(o);
	}
	return list;
}

QJsonObject SessionBanList::toExportJson() const
{
	QJsonArray list;
	for(const SessionBan &b : m_banlist) {
		QJsonObject o;
		if(!b.username.isEmpty()) {
			o[QStringLiteral("u")] = b.username;
		}
		if(!b.ip.isNull()) {
			o[QStringLiteral("i")] = b.ip.toString();
		}
		if(!b.authId.isEmpty()) {
			o[QStringLiteral("a")] = b.authId;
		}
		if(!b.sid.isEmpty()) {
			o[QStringLiteral("s")] = b.sid;
		}
		if(!b.bannedBy.isEmpty()) {
			o[QStringLiteral("b")] = b.bannedBy;
		}
		list.append(o);
	}
	return {
		{QStringLiteral("v"), 1},
		{QStringLiteral("l"), list},
	};
}

bool SessionBanList::importBans(
	const QJsonObject &data, std::function<void(const SessionBan &)> fn)
{
	if(data[QStringLiteral("v")].toInt() != 1) {
		return false;
	}

	QJsonValue list = data[QStringLiteral("l")];
	if(!list.isArray()) {
		return false;
	}

	for(const QJsonValue &value : list.toArray()) {
		if(value.isObject()) {
			QJsonObject o = value.toObject();
			SessionBan b = {
				-1,
				o[QStringLiteral("u")].toString(),
				o[QStringLiteral("a")].toString(),
				QHostAddress(o[QStringLiteral("i")].toString()),
				o[QStringLiteral("s")].toString(),
				o[QStringLiteral("b")].toString(),
			};
			bool isNull = b.username.isEmpty() && b.ip.isNull() &&
						  b.authId.isEmpty() && b.sid.isEmpty();
			if(!isNull) {
				fn(b);
			}
		}
	}
	return true;
}

bool SessionBanList::canAddBan(const SessionBan &ban)
{
	// Arbitrary limits to avoid denial of service.
	if(m_banlist.size() >= 1000) {
		return false;
	}

	bool isNull = ban.username.isEmpty() && ban.ip.isNull() &&
				  ban.authId.isEmpty() && ban.sid.isEmpty();
	if(isNull) {
		return false;
	}

	for(const SessionBan &b : m_banlist) {
		bool isSubsumed =
			ban.id == b.id ||
			((ban.username.isEmpty() ||
			  ban.username.compare(b.username, Qt::CaseInsensitive) == 0) &&
			 (ban.ip.isNull() || ban.ip == b.ip) &&
			 (ban.authId.isEmpty() || ban.authId == b.authId) &&
			 (ban.sid.isEmpty() || ban.sid == b.sid));
		if(isSubsumed) {
			return false;
		}
	}

	return true;
}

QHostAddress SessionBanList::toIpv6(const QHostAddress &address)
{
	return address.protocol() == QAbstractSocket::IPv4Protocol
			   ? QHostAddress(address.toIPv6Address())
			   : address;
}

}
