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

#include "sessionban.h"

#include <QJsonArray>
#include <QJsonObject>

namespace server {

SessionBanList::SessionBanList()
	: m_idautoinc(0)
{
}

bool SessionBanList::addBan(const QString &username, const QHostAddress &ip, const QString &bannedBy)
{
	if(ip.isNull() || ip.isLoopback() || isBanned(ip))
		return false;

	m_banlist << SessionBan {
		++m_idautoinc,
		username,
		ip,
		bannedBy
	};
	return true;
}

bool SessionBanList::removeBan(int id)
{
	QMutableListIterator<SessionBan> i(m_banlist);
	while(i.hasNext()) {
		if(i.next().id == id) {
			i.remove();
			return true;
		}
	}
	return false;
}

bool SessionBanList::isBanned(const QHostAddress &address) const
{
	for(const SessionBan &b : m_banlist) {
		if(b.ip == address)
			return true;
	}
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
		if(showIp)
			o["ip"] = b.ip.toString();
		list.append(o);
	}
	return list;
}

}

