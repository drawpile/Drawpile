/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#include "../net/protover.h"

#include <QObject>
#include <QDateTime>
#include <QUrl>
#include <QStringList>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace server {
	class Log;
}

namespace sessionlisting {

struct ListServerInfo {
	QString version;
	QString name;
	QString description;
	QString faviconUrl;
};

enum class PrivateMode {
	Undefined, // undefined, defaults to public
	Public,
	Private
};

struct Session {
	QString host;
	int port;
	QString id;
	protocol::ProtocolVersion protocol;
	QString title;
	int users;
	QStringList usernames;
	bool password;
	bool nsfm;
	PrivateMode isPrivate;
	QString owner;
	QDateTime started;
};

struct Announcement {
	Announcement() : listingId(0) { }

	QUrl apiUrl;
	QString id;
	QString updateKey;
	QString roomcode;
	int listingId;
	int refreshInterval;
	bool isPrivate;
};

/**
 * @brief Public session listing API client
 */
class AnnouncementApi : public QObject
{
	Q_OBJECT
public:
	explicit AnnouncementApi(QObject *parent=nullptr);

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

	/**
	 * @brief Query session info for a room code
	 *
	 * If the room code is found, sessionFound signal is emitted.
	 * Otherwise, the error signal is emitted.
	 *
	 * @param apiUrl
	 * @param roomcode
	 */
	void queryRoomcode(const QUrl &apiUrl, const QString &roomcode);

signals:
	//! Server info reply received
	void serverInfo(const ListServerInfo &info);

	//! Session list received
	void sessionListReceived(const QList<Session> &sessions);

	//! A message was received in response to a succesfull announcement
	void messageReceived(const QString &message);

	//! Session was announced succesfully
	void sessionAnnounced(const Announcement &session);

	//! Session was unlisted succesfully
	void unlisted(const QString &apiUrl, const QString &sessionId);

	//! Session for a roomcode was found (only host, port and id fields are filled)
	void sessionFound(const Session &session);

	//! An error occurred
	void error(const QString &apiUrl, const QString &errorString);

	//! A log message
	void logMessage(const server::Log &message);

private:
	typedef void (AnnouncementApi::*HandlerFunc)(QNetworkReply*);
	void handleResponse(QNetworkReply *reply, HandlerFunc);

	void handleAnnounceResponse(QNetworkReply *reply);
	void handleUnlistResponse(QNetworkReply *reply);
	void handleRefreshResponse(QNetworkReply *reply);
	void handleListingResponse(QNetworkReply *reply);
	void handleServerInfoResponse(QNetworkReply *reply);
	void handleRoomcodeResponse(QNetworkReply *reply);
};

}

#endif // ANNOUNCEMENTAPI_H
