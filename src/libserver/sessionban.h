// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_SERVER_SESSIONBAN_H
#define DP_SERVER_SESSIONBAN_H
#include <QHostAddress>
#include <QList>
#include <QString>
#include <functional>

class QJsonArray;

namespace server {

struct SessionBan {
	int id;
	QString username;
	QString authId;
	QHostAddress ip;
	QString sid;
	QString bannedBy;
};

struct SessionBanner {
	const QString &username;
	const QString &authId;
	const QHostAddress &address;
	const QString &sid;
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
	 * @param address IP address to be banned
	 * @param authId authenticated user's id (if any)
	 * @param sid system id (if any)
	 * @param bannedBy name of the user who did the banning
	 * @param id explicitly specified ID
	 * @param banner originator of the ban, to avoid self-bans
	 * @return id of newly added ban entry or 0 if not added
	 */
	int addBan(
		const QString &username, const QHostAddress &address,
		const QString &authId, const QString &sid, const QString &bannedBy,
		int id = 0, const SessionBanner *banner = nullptr);

	/**
	 * @brief Remove a ban entry
	 * @param id the ID number of the ban entry
	 * @return username of the removed ban entry or an empty string if not found
	 */
	QString removeBan(int id);

	/**
	 * @brief Check if the given parameters are on the ban list
	 *
	 * @param username the username to check (if not empty)
	 * @param address the IP address to check (if not null)
	 * @param authId the user ID to check (if not empty)
	 * @param sid system ID check (if not empty)
	 * @return the id of the ban, 0 for not banned
	 */
	int isBanned(
		const QString &username, const QHostAddress &address,
		const QString &authId, const QString &sid) const;

	/**
	 * @brief Get a JSON representation of the ban list
	 *
	 * This is used when sending the updated list to clients, as well
	 * as the JSON admin api.
	 */
	QJsonArray toJson(bool showSensitive) const;

	QJsonObject toExportJson() const;

	static bool importBans(
		const QJsonObject &data, std::function<void(const SessionBan &)> fn);

private:
	bool canAddBan(const SessionBan &ban);

	static QHostAddress toIpv6(const QHostAddress &address);

	QList<SessionBan> m_banlist;
	int m_idautoinc;
};

}

#endif
