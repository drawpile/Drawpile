/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef ANNOUNCEMENTAPI_H
#define ANNOUNCEMENTAPI_H

#include <QObject>
#include <QDateTime>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace sessionlisting {

struct ListServerInfo {
	QString version;
	QString name;
	QString description;
	QString faviconUrl;
};

struct Session {
	QString host;
	int port;
	QString id;
	QString protocol;
	QString title;
	int users;
	bool password;
	bool nsfm;
	QString owner;
	QDateTime started;
};

struct Announcement {
	Announcement() : listingId(0) { }

	QUrl apiUrl;
	QString id;
	QString updateKey;
	int listingId;
};

typedef std::function<bool(const QUrl &)> WhitelistFunction;

/**
 * @brief Public session listing API client
 */
class AnnouncementApi : public QObject
{
	Q_OBJECT
public:
	explicit AnnouncementApi(QObject *parent = 0);

	/**
	 * @brief Set the API URL whitelist function to use
	 *
	 * @param whitelist
	 */
	void setWhitelist(WhitelistFunction whitelist) { _whitelist = whitelist; }

	/**
	 * @brief Set the address of this server to announce
	 *
	 * This can be used to set the canonical server address to use
	 * when announcing a session. If not set, the listing server will use
	 * the peer address of the announcing machine.
	 *
	 * @param addr canonical address of this server
	 */
	void setLocalAddress(const QString &addr) { _localAddress = addr; }

	/**
	 * @brief Query information about the API
	 */
	void getApiInfo(const QUrl &apiUrl);

	/**
	 * @brief Send a request for a session list
	 *
	 * The signal sessionListReceived is emitted when the query finishes successfully.
	 *
	 * @param protocol if empty, limit query to sessions with this protocol version
	 * @param title if empty, limit query to sessions whose title contains this string
	 * @param nsfm if set to false, sessions tagged as "Not Suitable For Minors" will not be fetched
	 */
	void getSessionList(const QUrl &apiUrl, const QString &protocol=QString(), const QString &title=QString(), bool nsfm=false);

	/**
	 * @brief Send session announcement
	 * @param apiUrl
	 * @param session
	 */
	void announceSession(const QUrl &apiUrl, const Session &session);

	/**
	 * @brief Refresh the session previously announced with announceSession
	 */
	void refreshSession(const Announcement &a, const Session &session);

	/**
	 * @brief Unlist the session previously announced with announceSession
	 */
	void unlistSession(const Announcement &a);

signals:
	void serverInfo(const ListServerInfo &info);
	void sessionListReceived(const QList<Session> &sessions);
	void sessionAnnounced(const Announcement &session);
	void unlisted(const QString &sessionId);
	void error(const QString &errorString);

private slots:
	void handleResponse(QNetworkReply *reply);

private:
	bool isWhitelisted(const QUrl &url) const;

	void handleAnnounceResponse(QNetworkReply *reply);
	void handleUnlistResponse(QNetworkReply *reply);
	void handleRefreshResponse(QNetworkReply *reply);
	void handleListingResponse(QNetworkReply *reply);
	void handleServerInfoResponse(QNetworkReply *reply);

	QNetworkAccessManager *_net;
	WhitelistFunction _whitelist;
	QString _localAddress;
};

}

#endif // ANNOUNCEMENTAPI_H
