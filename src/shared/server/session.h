/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#include <QElapsedTimer>
#include <QUuid>
#include <QJsonObject>

#include "../util/announcementapi.h"
#include "../util/passwordhash.h"
#include "../net/message.h"
#include "../net/protover.h"
#include "sessionhistory.h"
#include "jsonapi.h"

namespace recording {
	class Writer;
}

class QTimer;

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

	Session(SessionHistory *history, ServerConfig *config, QObject *parent=0);

	const ServerConfig *config() const { return m_config; }

	/**
	 * \brief Get the ID of the session
	 */
	QUuid id() const { return m_history->id(); }

	/**
	 * @brief Get the ID of the session as a properly formatted string
	 */
	QString idString() const {
		QString s = id().toString();
		return s.mid(1, s.length()-2);
	}

	/**
	 * @brief Get the custom alias for the session ID
	 *
	 * Session ID alias is optional. If set, it can be used in place of the ID when joining a session.
	 */
	QString idAlias() const { return m_history->idAlias(); }

	/**
	 * @brief Return the ID alias if set, or else the unique ID
	 */
	QString aliasOrId() const {
		return m_history->idAlias().isEmpty() ? idString() : m_history->idAlias();
	}

	/**
	 * @brief Get the name of the user who started this session
	 * @return founder username
	 */
	QString founder() const { return m_history->founderName(); }

	/**
	 * @brief Get the full protocol version of this session
	 *
	 * A server only needs match the server-protocol version, but the
	 * client must match the version exactly.
	 */
	protocol::ProtocolVersion protocolVersion() const { return m_history->protocolVersion(); }

	/**
	 * @brief Is this an age-restricted session?
	 */
	bool isNsfm() const { return m_history->flags().testFlag(SessionHistory::Nsfm); }

	/**
	 * @brief Set the name of the recording file to create
	 *
	 * The recording will be created after a snapshot point has been created.
	 * @param filename path to output file
	 */
	void setRecordingFile(const QString &filename) { m_recordingFile = filename; }

	/**
	 * @brief Is this session password protected?
	 */
	bool hasPassword() const { return !m_history->passwordHash().isEmpty(); }

	/**
	 * @brief Does this session have a password for gaining operator status?
	 */
	bool hasOpword() const { return !m_history->opwordHash().isEmpty(); }

	/**
	 * @brief Set the session password.
	 * @param password
	 */
	void setPassword(const QString &password) { m_history->setPasswordHash(passwordhash::hash(password)); }

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
	QString title() const { return m_history->title(); }

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
	bool isClosed() const { return m_closed || userCount() >= maxUsers() || (m_state != Initialization && m_state != Running); }
	void setClosed(bool closed);

	/**
	 * @brief Get the maximum number of users allowed in the session
	 *
	 * This setting only affects new joins. Old users are not removed,
	 * even if the limit is lowered.
	 * @return user limit
	 */
	int maxUsers() const { return m_history->maxUsers(); }

	/**
	 * @brief Is this a persistent session
	 *
	 * A persistent session is not automatically deleted when the last user leaves.
	 */
	bool isPersistent() const { return m_history->flags().testFlag(SessionHistory::Persistent); }

	//! Set session attributes
	void setSessionConfig(const QJsonObject &conf, Client *changedBy);

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
	 * @brief Get the session internal ban list
	 *
	 * Don't manipulate the banlist directly, instead use tha addBan and
	 * removeBan functions so the events are logged properly.
	 */
	const SessionBanList &banlist() const { return m_history->banlist(); }

	/**
	 * @brief Add an in-session IP ban for the given client
	 */
	void addBan(const Client *client, const QString &bannedBy);

	/**
	 * @brief Remove a session specific IP ban
	 * @param entryId ban entry ID
	 */
	void removeBan(int entryId, const QString &removedBy);

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
	QDateTime sessionStartTime() const { return m_history->startTime(); }

	/**
	 * @brief Get session uptime in nice human readable format
	 * @return
	 */
	QString uptime() const;

	/**
	 * @brief Get the time of the last join/logout event
	 * @return milliseconds since last event
	 */
	qint64 lastEventTime() const { return m_lastEventTime.elapsed(); }

	/**
	 * @brief Get the session history
	 */
	const SessionHistory *history() const { return m_history; }

	/**
	 * @brief Add a message to the session history
	 * @param msg
	 */
	void addToHistory(const protocol::MessagePtr &msg);

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
	 * @brief Initiate the shutdown of this session
	 *
	 * If the terminate parameter is false, the session history
	 * will not be terminated. This allows the session to survive
	 * server restarts.
	 */
	void killSession(bool terminate=true);

	/**
	 * @brief Send a direct message to all session participants
	 *
	 * This bypasses the session history.
	 * @param msg
	 */
	void directToAll(protocol::MessagePtr msg);

	/**
	 * @brief Send a message to every user of this session
	 * @param message
	 * @param alert is this an alert type message?
	 */
	void messageAll(const QString &message, bool alert);

	/**
	 * @brief Start resetting this session
	 * @param resetter ID of the user who started the reset
	 */
	void resetSession(int resetter);

	/**
	 * @brief Get all active announcements for this session
	 */
	QList<sessionlisting::Announcement> announcements() const { return m_publicListings; }

	/**
	 * @brief Generate a request for session announcement
	 *
	 * @param url listing server API url
	 */
	void makeAnnouncement(const QUrl &url);

	/**
	 * @brief Generate a request for session announcement unlisting
	 * @param url API url
	 * @param terminate if false, the removal is not logged in the history journal
	 * @param removeOnly if true, an unlisting request is not sent (use in case of error)
	 */
	void unlistAnnouncement(const QString &url, bool terminate=true, bool removeOnly=false);

	//! Get the session state
	State state() const { return m_state; }

	void handleInitComplete(int ctxId);

	/**
	 * @brief Update session operator bits
	 *
	 * Generates log entries for each change
	 *
	 * @param ids lisf of new session operators
	 * @param changedBy name of the user who issued the change command
	 * @return sanitized list of actual session operators
	 */
	QList<uint8_t> updateOwnership(QList<uint8_t> ids, const QString &chanedBy);

	/**
	 * @brief Grant or revoke OP status of a user
	 * @param id user ID
	 * @param op new status
	 * @param changedBy name of the user who issued the command
	 */
	void changeOpStatus(int id, bool op, const QString &changedBy);

	//! Send refreshed ban list to all logged in users
	void sendUpdatedBanlist();

	//! Send refreshed session announcement list to all logged in users
	void sendUpdatedAnnouncementList();

	//! Send a refreshed list of muted users
	void sendUpdatedMuteList();

	//! Release caches that can be released
	void historyCacheCleanup();

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

	/**
	 * @brief Write a session related log entry.
	 * The abridged version is also sent to all active memeers of the session.
	 */
	Q_SLOT void log(const Log &entry);

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

private slots:
	void removeUser(Client *user);

	void refreshAnnouncements();
	void sessionAnnounced(const sessionlisting::Announcement &announcement);
	void sessionAnnouncementError(const QString &apiUrl, const QString &message);

private:
	sessionlisting::AnnouncementApi *publicListingClient();

	void cleanupCommandStream();

	void restartRecording();
	void stopRecording();


	void sendUpdatedSessionProperties();
	void sendStatusUpdate();
	void ensureOperatorExists();

	void switchState(State newstate);

	ServerConfig *m_config;

	State m_state;
	int m_initUser; // the user who is currently uploading init/reset data

	recording::Writer *m_recorder;
	QString m_recordingFile;

	QList<Client*> m_clients;

	SessionHistory *m_history;
	QList<protocol::MessagePtr> m_resetstream;
	uint m_resetstreamsize;
	uint m_historyLimitWarning;

	QList<sessionlisting::Announcement> m_publicListings;
	sessionlisting::AnnouncementApi *m_publicListingClient;
	QTimer *m_refreshTimer;

	QElapsedTimer m_lastEventTime;
	QElapsedTimer m_lastStatusUpdate;

	bool m_closed;
	bool m_historyLimitWarningSent;
};

}

#endif

