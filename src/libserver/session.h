/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#include "libserver/announcable.h"
#include "libshared/net/message.h"
#include "libshared/net/protover.h"
#include "libserver/sessionhistory.h"
#include "libserver/jsonapi.h"

#include <QHash>
#include <QString>
#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>

class QTimer;

namespace recording {
	class Writer;
}

namespace sessionlisting {
	class Announcements;
}

namespace server {

class Client;
class ServerConfig;
class Log;

/**
 * The serverside session state.
 *
 * This is an abstract base class. Concrete implementations are ThinSession and ThickSession,
 * for thin and thick servers respectively.
 */
class Session : public QObject, public sessionlisting::Announcable {
	Q_OBJECT
public:
	//! State of the session
	enum class State {
		Initialization,
		Running,
		Reset,
		Shutdown
	};

	//! Information about a user who has since logged out
	struct PastClient {
		int id;
		QString authId;
		QString username;
		QHostAddress peerAddress;
		bool isBannable;
	};

	//! Get the server configuration
	const ServerConfig *config() const { return m_config; }

	//! Get the unique ID of the session
	QString id() const override { return m_history->id(); }

	/**
	 * @brief Get the custom alias for the session ID
	 *
	 * Session ID alias is optional. If set, it can be used in place of the ID when joining a session.
	 */
	QString idAlias() const { return m_history->idAlias(); }

	//! Get the session alias if set, or the ID if not
	QString aliasOrId() const {
		return m_history->idAlias().isEmpty() ? id() : m_history->idAlias();
	}

	/**
	 * @brief Set the name of the recording file to create
	 *
	 * The recording will be created after a snapshot point has been created.
	 * @param filename path to output file
	 */
	void setRecordingFile(const QString &filename) { m_recordingFile = filename; }

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
	bool isClosed() const;
	void setClosed(bool closed);

	/**
	 *  Does this session support autoresetting?
	 */
	virtual bool supportsAutoReset() const = 0;

	//! Set session attributes
	void setSessionConfig(const QJsonObject &conf, Client *changedBy);

	/**
	 * @brief Add a new client to the session
	 * @param user the client to add
	 * @param host is this the hosting user
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
	 * @return user or nullptr if not found
	 */
	Client *getClientById(uint8_t id);

	//! Has a client with the given ID been logged in (not currently)?
	bool hasPastClientWithId(uint8_t id) const { return m_pastClients.contains(id); }

	//! Get information about a past client who used the given ID
	PastClient getPastClientById(uint8_t id) const {
		Q_ASSERT(hasPastClientWithId(id));
		return m_pastClients[id];
	}

	/**
	 * @brief Get a client by user name
	 *
	 * The name comparison is case insensitive.
	 * Note! In debug mode username uniqueness is not enforced!
	 * @param username
	 * @return client or nullptr if not found
	 */
	Client *getClientByUsername(const QString &username);

	/**
	 * @brief Add an in-session IP ban for the given client
	 */
	void addBan(const Client *client, const QString &bannedBy);
	void addBan(const PastClient &client, const QString &bannedBy);

	/**
	 * @brief Remove a session specific IP ban
	 * @param entryId ban entry ID
	 */
	void removeBan(int entryId, const QString &removedBy);

	//! Get the number of connected clients
	int userCount() const { return m_clients.size(); }

	//! Get the of clients currently in this session
	const QList<Client*> &clients() const { return m_clients; }

	/**
	 * @brief Get the ID of the user uploading initialization or reset data
	 * @return user ID or -1 if init not in progress
	 */
	int initUserId() const { return m_initUser; }

	//! Get the names of this session's users
	QStringList userNames() const;

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

	//! Get the session history
	const SessionHistory *history() const { return m_history; }
	SessionHistory *history() { return m_history; }

	/**
	 * @brief Process a message received from a client
	 * @param client
	 * @param message
	 */
	void handleClientMessage(Client &client, protocol::MessagePtr message);

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
	 * @brief Generate a request for session announcement
	 *
	 * @param url listing server API url
	 * @param privateListing make this a private listing?
	 */
	void makeAnnouncement(const QUrl &url, bool privateListing);

	/**
	 * @brief Generate a request for session announcement unlisting
	 * @param url API url
	 * @param terminate if false, the removal is not logged in the history journal
	 * @param removeOnly if true, an unlisting request is not sent (use in case of error)
	 */
	void unlistAnnouncement(const QUrl &url, bool terminate=true);

	sessionlisting::Session getSessionAnnouncement() const override;

	void sendListserverMessage(const QString &message) override { messageAll(message, false); }

	//! Get the session state
	State state() const { return m_state; }

	// Resetting related functions, called via opcommands
	void resetSession(int resetter);
	virtual void readyToAutoReset(int ctxId) = 0;
	void handleInitBegin(int ctxId);
	void handleInitComplete(int ctxId);
	void handleInitCancel(int ctxId);

	/**
	 * @brief Grant or revoke OP status of a user
	 * @param id user ID
	 * @param op new status
	 * @param changedBy name of the user who issued the command
	 */
	void changeOpStatus(uint8_t id, bool op, const QString &changedBy);

	/**
	 * @brief Grant or revoke trusted status of a user
	 * @param id user ID
	 * @param trusted new status
	 * @param changedBy name of the user who issued the command
	 */
	void changeTrustedStatus(uint8_t id, bool trusted, const QString &changedBy);

	//! Send refreshed ban list to all logged in users
	void sendUpdatedBanlist();

	//! Send refreshed session announcement list to all logged in users
	void sendUpdatedAnnouncementList();

	//! Send a refreshed list of muted users
	void sendUpdatedMuteList();

	/**
	 * @brief Send an abuse report
	 *
	 * A reporting backend server must have been configured.
	 *
	 * @param reporter the user who is making the report
	 * @param aboutUser the ID of the user this report is about (if 0, the report is about the session in general)
	 * @param message freeform message entered by the reporter
	 */
	void sendAbuseReport(const Client *reporter, int aboutUser, const QString &message);

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
	 * - number of users
	 *
	 * @param thisSession
	 */
	void sessionAttributeChanged(Session *thisSession);

private slots:
	void removeUser(Client *user);
	void onAnnouncementsChanged(const Announcable *session);

protected:
	Session(SessionHistory *history, ServerConfig *config, sessionlisting::Announcements *announcements, QObject *parent);

	//! Add a message to the session history
	virtual void addToHistory(protocol::MessagePtr msg) = 0;

	//! Session history was just reset
	virtual void onSessionReset() = 0;

	//! A regular (non-hosting) client just joined
	virtual void onClientJoin(Client *client, bool host) = 0;

	//! This message was just added to session history
	void addedToHistory(protocol::MessagePtr msg);

	void switchState(State newstate);

	//! Get the user join, SessionOwner, etc. messages that should be prepended to a reset image
	protocol::MessageList serverSideStateMessages() const;

private:
	/**
	 * Add a message to the initialization stream
	 *
	 * During init state, this goes to the normal command stream.
	 * During reset state, this goes to a reset buffer which will
	 * replace the old session history once completed.
	 */
	void addToInitStream(protocol::MessagePtr msg);

	/**
	 * @brief Update session operator bits
	 *
	 * Generates log entries for each change
	 *
	 * @param ids new list of session operators
	 * @param changedBy name of the user who issued the change command
	 * @return sanitized list of actual session operators
	 */
	QList<uint8_t> updateOwnership(QList<uint8_t> ids, const QString &changedBy);

	/**
	 * @brief Update the list of trusted users
	 *
	 * Generates log entries for each change
	 * @param ids new list of trusted users
	 * @param changedBy name of the user who issued the change command
	 * @return sanitized list of actual trusted users
	 */
	QList<uint8_t> updateTrustedUsers(QList<uint8_t> ids, const QString &changedBy);

	void restartRecording();
	void stopRecording();
	void abortReset();

	void sendUpdatedSessionProperties();
	void sendStatusUpdate();
	void ensureOperatorExists();

	JsonApiResult callListingsJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

	SessionHistory *m_history;
	ServerConfig *m_config;
	sessionlisting::Announcements *m_announcements;

	State m_state = State::Initialization;
	int m_initUser = -1; // the user who is currently uploading init/reset data

	recording::Writer *m_recorder = nullptr;
	QString m_recordingFile;

	QList<Client*> m_clients;
	QHash<int, PastClient> m_pastClients;

	protocol::MessageList m_resetstream;
	uint m_resetstreamsize = 0;

	QElapsedTimer m_lastEventTime;

	bool m_closed = false;
};

}

#endif

