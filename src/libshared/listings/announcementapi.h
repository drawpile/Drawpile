/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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

#ifndef ANNOUNCEMENTAPI_V2_H
#define ANNOUNCEMENTAPI_V2_H

#include "libshared/net/protover.h"

#include <QObject>
#include <QDateTime>
#include <QUrl>
#include <QVariant>
#include <QStringList>

namespace sessionlisting {

struct ListServerInfo {
	QString version;      // Server API version
	QString name;         // Name of the server
	QString description;  // Short description of the server
	QString faviconUrl;   // URL of the server's favicon
	bool readOnly;        // If true, listings cannot be submitted
	bool publicListings;  // Does this server supports public listings
	bool privateListings; // Does this server supports private (room code only) listings
};

enum class PrivacyMode {
	Undefined, // not specified, defaults to public
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
	PrivacyMode isPrivate;
	QString owner;
	QDateTime started;
};

struct Announcement {
	QUrl apiUrl;
	QString id;
	QString updateKey;
	QString roomcode;
	int listingId;
	int refreshInterval;
	bool isPrivate;
};

}

Q_DECLARE_METATYPE(sessionlisting::ListServerInfo)
Q_DECLARE_METATYPE(sessionlisting::Session)
Q_DECLARE_METATYPE(sessionlisting::Announcement)

namespace sessionlisting {

class AnnouncementApiResponse : public QObject
{
	Q_OBJECT
public:
	AnnouncementApiResponse(const QUrl &url, QObject *parent=nullptr)
		: QObject(parent), m_apiUrl(url), m_gone(false)
	{ }

	void setResult(const QVariant &result, const QString &message=QString());
	void setError(const QString &error);
	void setUrl(const QUrl &url) { m_apiUrl = url; }

	QUrl apiUrl() const { return m_apiUrl; }
	QVariant result() const { return m_result; }
	QString message() const { return m_message; }
	QString errorMessage() const { return m_error; }
	bool isGone() const { return m_gone; }

signals:
	void finished(const QVariant &result, const QString &message, const QString &error);
	void serverGone();

private:
	QUrl m_apiUrl;
	QVariant m_result;
	QString m_message;
	QString m_error;
	bool m_gone;
};

/**
 * @brief Fetch information about a listing server
 *
 * Returns ListServerInfo
 */
AnnouncementApiResponse *getApiInfo(const QUrl &apiUrl);

/**
 * @brief Fetch the list of public sessions from a listing server
 *
 * @param url API url
 * @param protocol if specified, return only sessions using the given protocol
 * @param title if specified, return only sessions whose title contains this substring
 * @param nsfm if true, fetch sessions tagged as NSFM
 *
 * Returns QList<Session>
 */
AnnouncementApiResponse *getSessionList(const QUrl &apiUrl, const QString &protocol=QString(), const QString &title=QString(), bool nsfm=true);

/**
 * @brief Announce a session at the given listing server
 *
 * Returns Announcement
 */
AnnouncementApiResponse *announceSession(const QUrl &apiUrl, const Session &session);

/**
 * @brief Refresh a session announcement
 *
 * Returns the session ID and may set the result message.
 */
AnnouncementApiResponse *refreshSession(const Announcement &a, const Session &session);

/**
 * @brief Refresh multiple session announcement
 *
 * Note: All announcements *MUST* be to the same server.
 * Returns a QHash<int,bool>, which maps listing IDs to success statuses.
 */
AnnouncementApiResponse *refreshSessions(const QVector<QPair<Announcement, Session>> &listings);

/**
 * @brief Unlist a session announcement
 *
 * Returns the session ID
 */
AnnouncementApiResponse *unlistSession(const Announcement &a);

/**
 * @brief Query this server for a room code
 *
 * Returns a Session
 */
AnnouncementApiResponse *queryRoomcode(const QUrl &apiUrl, const QString &roomcode);

}

#endif

