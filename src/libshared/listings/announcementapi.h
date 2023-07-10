// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ANNOUNCEMENTAPI_V2_H
#define ANNOUNCEMENTAPI_V2_H

#include "libshared/net/protover.h"

#include <QObject>
#include <QDateTime>
#include <QUrl>
#include <QVariant>
#include <QStringList>
#include <QNetworkReply>

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
	int maxUsers;
	bool closed;
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

class AnnouncementApiResponse final : public QObject
{
	Q_OBJECT
public:
	AnnouncementApiResponse(const QUrl &url, QObject *parent=nullptr)
		: QObject(parent), m_apiUrl(url), m_networkError(QNetworkReply::NoError), m_gone(false), m_cancelled(false)
	{ }

	void setResult(const QVariant &result, const QString &message=QString());
	void setError(const QString &error, QNetworkReply::NetworkError networkError = QNetworkReply::NoError);
	void setUrl(const QUrl &url) { m_apiUrl = url; }

	QUrl apiUrl() const { return m_apiUrl; }
	QVariant result() const { return m_result; }
	QString message() const { return m_message; }
	QString errorMessage() const { return m_error; }
	QNetworkReply::NetworkError networkError() const { return m_networkError; }
	bool isGone() const { return m_gone; }
	bool isCancelled() const { return m_cancelled; }

	void updateRequestUrl(const QUrl &url);
	void cancel();

signals:
	void cancelRequested();
	void requestUrlChanged(const QUrl &url);
	void finished(const QVariant &result, const QString &message, const QString &error);
	void serverGone();

private:
	QUrl m_apiUrl;
	QVariant m_result;
	QString m_message;
	QString m_error;
	QNetworkReply::NetworkError m_networkError;
	bool m_gone;
	bool m_cancelled;
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
 * Returns QList<Session>
 */
AnnouncementApiResponse *getSessionList(const QUrl &apiUrl);

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

