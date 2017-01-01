/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2016 Calle Laakkonen

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

#ifndef DP_SHARED_SERVER_SESSION_H
#define DP_SHARED_SERVER_SESSION_H

#include <QVector>
#include <QHash>
#include <QString>
#include <QObject>
#include <QDateTime>
#include <QUuid>
#include <QJsonObject>

#include "../util/logger.h"
#include "../util/announcementapi.h"
#include "../net/message.h"
#include "../net/messagestream.h"
#include "../net/protover.h"
#include "jsonapi.h"

namespace recording {
	class Writer;
}

namespace server {

class Client;
class ServerConfig;

/**
 * The serverside session state.
 */
class Session : public QObject {
	Q_OBJECT
public:
	enum State {
		Initialization,
		Running,
		Reset,
		Shutdown
	};

	Session(const QUuid &id, const QString &alias, const protocol::ProtocolVersion &protocolVersion, const QString &founder, ServerConfig *config, QObject *parent=0);

	const ServerConfig *config() const { return m_config; }

	/**
	 * \brief Get the ID of the session
	 */
	QUuid id() const { return m_id; }

	/**
	 * @brief Get the ID of the session as a properly formatted string
	 */
	QString idString() const {
		QString s = m_id.toString();
		return s.mid(1, s.length()-2);
	}

	/**
	 * @brief Get the custom alias for the session ID
	 *
	 * Session ID alias is optional. If set, it can be used in place of the ID when joining a session.
	 */
	QString idAlias() const { return m_idAlias; }

	/**
	 * @brief Get the name of the user who started this session
	 * @return founder username
	 */
	QString founder() const { return m_founder; }

	/**
	 * @brief Get the full protocol version of this session
	 *
	 * A server only needs match the server-protocol version, but the
	 * client must match the version exactly.
	 */
	protocol::ProtocolVersion protocolVersion() const { return m_protocolVersion; }

	/**
	 * @brief Is this an age-restricted session?
	 */
	bool isNsfm() const { return m_nsfm; }

	/**
	 * @brief Get the current history limit for this session
	 *
	 * This is the upper limit. If the size goes above this, the session is terminated.
	 * @return
	 */
	uint historyLimit() const { return m_historylimit; }

	/**
	 * @brief Set the name of the recording file to create
	 *
	 * The recording will be created after a snapshot point has been created.
	 * @param filename path to output file
	 */
	void setRecordingFile(const QString &filename) { m_recordingFile = filename; }

	/**
	 * @brief Stop any recording that might be in progress
	 */
	void stopRecording();

	/**
	 * @brief Is this session password protected?
	 */
	bool hasPassword() const { return !m_passwordhash.isEmpty(); }

	/**
	 * @brief Check if the password is OK
	 *
	 * If no session password is set, this will always return true.
	 *
	 * @param password
	 * @return true if password matches the session password
	 */
	bool checkPassword(const QString &password) const;

	/**
	 * @brief Get the title of the session
	 * @return
	 */
	const QString &title() const { return m_title; }

	/**
	 * @brief Is the session closed to new users?
	 *
	 * A session can be closed in two ways:
	 * - by setting the Closed flag
	 * - when user count reaches maxUsers()
	 *
	 * The session is also temporarily closed during the Reset state.
	 *
	 * @return true if new users will not be admitted
	 */
	bool isClosed() const { return m_closed || userCount() >= maxUsers() || m_state == Reset; }
	void setClosed(bool closed);

	/**
	 * @brief Get the maximum number of users allowed in the session
	 *
	 * This setting only affects new joins. Old users are not removed,
	 * even if the limit is lowered.
	 * @return user limit
	 */
	int maxUsers() const { return m_maxusers; }

	/**
	 * @brief Is this a persistent session
	 *
	 * A persistent session is not automatically deleted when the last user leaves.
	 */
	bool isPersistent() const { return m_persistent; }

	//! Set session attributes
	void setSessionConfig(const QJsonObject &conf);

	/**
	 * @brief Add a new client to the session
	 * @param user
	 */
	void joinUser(Client *user, bool host);

	/**
	 * @brief Assign an ID for this user
	 *
	 * This is used during the login phase to prepare
	 * the user for joining the session
	 * @param user
	 */
	void assignId(Client *user);

	/**
	 * @brief Get a client by ID
	 * @param id user ID
	 * @return user or 0 if not found
	 */
	Client *getClientById(int id);

	/**
	 * @brief Get a client by user name
	 *
	 * The name comparison is case insensitive.
	 * Note! In debug mode username uniqueness is not enforced!
	 * @param username
	 * @return
	 */
	Client *getClientByUsername(const QString &username);

	/**
	 * @brief Get the number of clients in the session
	 * @return user count
	 */
	int userCount() const { return m_clients.size(); }

	const QList<Client*> &clients() const { return m_clients; }

	/**
	 * @brief Get the ID of the user uploading initialization or reset data
	 * @return user ID or invalid ID if init not in progress
	 */
	int initUserId() const { return m_initUser; }

	//! Get the names of this session's users
	QStringList userNames() const;

	/**
	 * @brief Get the name of the session owner
	 *
	 * The operator who has been in the session the longest
	 * is considered the owner.
	 */
	QString ownerName() const;

	/**
	 * @brief Get the time the session was started
	 * @return timestamp
	 */
	const QDateTime &sessionStartTime() const { return m_startTime; }

	/**
	 * @brief Get session uptime in nice human readable format
	 * @return
	 */
	QString uptime() const;

	/**
	 * @brief Get the time of the last join/logout event
	 * @return timestamp
	 */
	const QDateTime &lastEventTime() const { return m_lastEventTime; }

	/**
	 * @brief get the main command stream
	 * @return reference to main command stream
	 */
	const protocol::MessageStream &mainstream() const { return m_mainstream; }

	/**
	 * @brief Add a command to the message stream.
	 *
	 * Emits newCommandsAvailable
	 * @param msg
	 */
	void addToCommandStream(protocol::MessagePtr msg);

	/**
	 * @brief Add a message to the initialization stream
	 *
	 * During init state, this goes to the normal command stream.
	 * During reset state, this goes to a reset buffer which will
	 * replace the old session history once completed.
	 * @param msg
	 */
	void addToInitStream(protocol::MessagePtr msg);

	/**
	 * @brief Disconnect all users
	 */
	void kickAllUsers();

	/**
	 * @brief Initiate the shutdown of this session
	 *
	 * The session will not persist after the users have been disconnected,
	 * nor will it be hibernated.
	 */
	void killSession();

	/**
	 * @brief Send a message to every user of this session
	 * @param message
	 */
	void wall(const QString &message);

	/**
	 * @brief Start resetting this session
	 * @param resetter ID of the user who started the reset
	 */
	void resetSession(int resetter);

	QString toLogString() const;

	sessionlisting::Announcement publicListing() const { return m_publicListing; }
	void setPublicListing(const sessionlisting::Announcement &a) { m_publicListing = a; }

	/**
	 * @brief Generate a request for session announcement
	 *
	 * @param url listing server API url
	 */
	void makeAnnouncement(const QUrl &url);

	/**
	 * @brief Generate a request for session announcement unlisting
	 */
	void unlistAnnouncement();

	//! Get the session state
	State state() const { return m_state; }

	void handleInitComplete(int ctxId);

	/**
	 * @brief Update session operator bits
	 * @param ids lisf of new session operators
	 * @return sanitized list of actual session operators
	 */
	QList<uint8_t> updateOwnership(QList<uint8_t> ids);

	/**
	 * @brief Get a JSON object describing the session
	 *
	 * This is used in the login phase session list
	 * and the JSON api.
	 *
	 * @param full - include detailed information (for admin use)
	 * @return
	 */
	QJsonObject getDescription(bool full=false) const;

	/**
	 * @brief Call the server's JSON administration API
	 *
	 * This is used by the HTTP admin API.
	 *
	 * @param method query method
	 * @param path path components
	 * @param request request body content
	 * @return JSON API response content
	 */
	JsonApiResult callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

signals:
	//! A user just connected to the session
	void userConnected(Session *thisSession, Client *client);

	//! A user disconnected from the session
	void userDisconnected(Session *thisSession);

	/**
	 * @brief A publishable session attribute just changed.
	 *
	 * This signal is emitted when any of the following attributes are changed:
	 *
	 * - title
	 * - open/closed status
	 * - maximum user count
	 * - password
	 * - persistent mode
	 *
	 * @param thisSession
	 */
	void sessionAttributeChanged(Session *thisSession);

	//! New commands have been added to the main stream
	void newCommandsAvailable();

	//! Request session announcement
	void requestAnnouncement(const QUrl &url, const sessionlisting::Session &session);

	//! Request this session to be unlisted
	void requestUnlisting(const sessionlisting::Announcement &listing);

private slots:
	void removeUser(Client *user);


private:
	void cleanupCommandStream();
	void startRecording(const QList<protocol::MessagePtr> &snapshot);

	void sendUpdatedSessionProperties();
	void ensureOperatorExists();

	void switchState(State newstate);

	ServerConfig *m_config;

	State m_state;
	int m_initUser; // the user who is currently uploading init/reset data

	recording::Writer *m_recorder;
	QString m_recordingFile;

	QList<Client*> m_clients;

	protocol::MessageStream m_mainstream;
	QList<protocol::MessagePtr> m_resetstream;

	sessionlisting::Announcement m_publicListing;

	int m_lastUserId;

	const QDateTime m_startTime;
	QDateTime m_lastEventTime;

	const QUuid m_id;
	const QString m_idAlias;
	protocol::ProtocolVersion m_protocolVersion;
	int m_maxusers;
	uint m_historylimit;
	uint m_historyLimitWarning;

	QByteArray m_passwordhash;
	QString m_title;
	QString m_founder;

	bool m_closed;
	bool m_persistent;
	bool m_preserveChat;
	bool m_nsfm;
	bool m_historytLimitWarningSent;
};

}

#endif

