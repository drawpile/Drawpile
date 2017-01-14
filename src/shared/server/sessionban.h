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
#ifndef DP_SERVER_SESSIONBAN_H
#define DP_SERVER_SESSIONBAN_H

#include <QString>
#include <QHostAddress>
#include <QList>

class QJsonArray;

namespace server {

struct SessionBan {
	int id;
	QString username;
	QHostAddress ip;
	QString bannedBy;
};

/**
 * @brief Session internal banlist
 *
 * This holds the session specific bans that can be enacted (and retracted)
 * by session operators.
 *
 * Unlike the serverwide banlist, this is designed to be used together with
 * the kick function, and for privacy reasons does not normally reveal the
 * actual IP addresses (except to moderators)
 */
class SessionBanList {
public:
	SessionBanList();

	/**
	 * @brief Add a new ban
	 *
	 * If the address already exists in the ban list, this does nothing.
	 * @param username the username of the user being banned
	 * @param ip IP address to be banned
	 * @param bannedBy name of the user who did the banning
	 * @param id explicitly specified ID
	 * @return id of newly added ban entry or 0 if not added
	 */
	int addBan(const QString &username, const QHostAddress &ip, const QString &bannedBy, int id=0);

	/**
	 * @brief Remove a ban entry
	 * @param id the ID number of the ban entry
	 * @return true if the entry was found and removed
	 */
	bool removeBan(int id);

	/**
	 * @brief Check if the given IP address is on the ban list
	 */
	bool isBanned(const QHostAddress &address) const;

	/**
	 * @brief Get a JSON representation of the ban list
	 *
	 * This is used when sending the updated list to clients, as well
	 * as the JSON admin api.
	 */
	QJsonArray toJson(bool showIp) const;

private:
	QList<SessionBan> m_banlist;
	int m_idautoinc;
};

}

#endif

