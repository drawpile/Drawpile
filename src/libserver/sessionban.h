// SPDX-License-Identifier: GPL-3.0-or-later

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
	QString authId;
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
	 * @param authId authenticated user's id (if any)
	 * @param bannedBy name of the user who did the banning
	 * @param id explicitly specified ID
	 * @return id of newly added ban entry or 0 if not added
	 */
	int addBan(const QString &username, const QHostAddress &ip, const QString &authId, const QString &bannedBy, int id=0);

	/**
	 * @brief Remove a ban entry
	 * @param id the ID number of the ban entry
	 * @return username of the removed ban entry or an empty string if not found
	 */
	QString removeBan(int id);

	/**
	 * @brief Check if the given IP address or authenticated ID is on the ban list
	 *
	 * @param address the IP address to check (if not null)
	 * @param authId the user ID to check (if not empty)
	 */
	bool isBanned(const QHostAddress &address, const QString &authId) const;

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

