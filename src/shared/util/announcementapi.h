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
	QString owner;
	QDateTime started;
};

/**
 * @brief Public session listing API client
 */
class AnnouncementApi : public QObject
{
	Q_OBJECT
public:
	explicit AnnouncementApi(QObject *parent = 0);

	void setApiUrl(const QUrl &url) { _apiUrl = url; }
	QUrl apiUrl() const { return _apiUrl; }

	/**
	 * @brief Query information about the API
	 */
	void getApiInfo();

	/**
	 * @brief Send a request for a session list
	 *
	 * The signal sessionListReceived is emitted when the query finishes successfully.
	 *
	 * @param protocol if empty, limit query to sessions with this protocol version
	 * @param title if empty, limit query to sessions whose title contains this string
	 */
	void getSessionList(const QString &protocol=QString(), const QString &title=QString());

	/**
	 * @brief Send session announcement
	 * @param host
	 * @param port
	 * @param session
	 */
	void announceSession(const Session &session);

	/**
	 * @brief Refresh the session previously announced with announceSession
	 */
	void refreshSession(const Session &session);

	/**
	 * @brief Unlist the session previously announced with announceSession
	 */
	void unlistSession();

	/**
	 * @brief Has an announcement been succesfully made?
	 * @return
	 */
	bool isAnnounced() const { return _listingId>0; }

signals:
	void serverInfo(ListServerInfo info);
	void sessionListReceived(QList<Session> sessions);
	void sessionAnnounced();
	void error(QString errorString);

private slots:
	void handleResponse(QNetworkReply *reply);

private:
	void handleAnnounceResponse(QNetworkReply *reply);
	void handleUnlistResponse(QNetworkReply *reply);
	void handleRefreshResponse(QNetworkReply *reply);
	void handleListingResponse(QNetworkReply *reply);
	void handleServerInfoResponse(QNetworkReply *reply);

	QUrl _apiUrl;
	QNetworkAccessManager *_net;

	int _listingId;
	QString _updateKey;
};

}

#endif // ANNOUNCEMENTAPI_H
