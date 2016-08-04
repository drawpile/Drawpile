/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QObject>

#include "sessiondesc.h"

namespace sessionlisting {
	class AnnouncementApi;
	struct Session;
	struct Announcement;
}

namespace server {

class SessionState;
class Client;
class SessionStore;
class IdentityManager;

/**
 * @brief Session manager
 *
 */
class SessionServer : public QObject {
Q_OBJECT
public:
	explicit SessionServer(QObject *parent=0);

	/**
	 * @brief Set the title of the server
	 *
	 * The server title is shown by the client in the session selection dialog.
	 * @param title
	 */
	Q_INVOKABLE void setTitle(const QString& title) { _title = title; }
	const QString &title() const { return _title; }

	/**
	 * @brief Set the maximum number of sessions
	 *
	 * This does not affect existing sessions.
	 *
	 * @param limit
	 */
	Q_INVOKABLE void setSessionLimit(int limit) { _sessionLimit = qMax(0, limit); }
	int sessionLimit() const { return _sessionLimit; }

	/**
	 * @brief Set session history limit
	 *
	 * A limit of 0 means size is unlimited.
	 *
	 * @param limit max history size in bytes
	 */
	void setHistoryLimit(uint limit) { _historyLimit = limit; }
	uint historyLimit() const { return _historyLimit; }

	/**
	 * @brief Set the password needed to host a sessionCount()
	 *
	 * By default, an empty password is set, meaning no password is needed.
	 *
	 * @param password
	 */
	Q_INVOKABLE void setHostPassword(const QString &password) { _hostPassword = password; }
	const QString &hostPassword() const { return _hostPassword; }

	/**
	 * @brief Set whether persistent sessions are allowed
	 * @param persistent
	 */
	void setAllowPersistentSessions(bool persistent) { _allowPersistentSessions = persistent; }
	bool allowPersistentSessions() const { return _allowPersistentSessions; }

	/**
	 * @brief Set persistent session expiration time
	 *
	 * A session must be vacant for this many seconds before it is automatically deleted.
	 *
	 * @param seconds expiration time in seconds
	 */
	void setExpirationTime(uint seconds) { _expirationTime = qint64(seconds) * 1000; }

	//! Never include user lists in announcements
	void setPrivateUserList(bool p) { m_privateUserList = p; }

	/**
	 * @brief Set whether a secure connection is mandatory
	 * @param mustSecure
	 */
	void setMustSecure(bool mustSecure) { _mustSecure = mustSecure; }
	bool mustSecure() const { return _mustSecure; }

#ifndef NDEBUG
	void setRandomLag(uint lag) { _randomlag = lag; }
#endif

	/**
	 * @brief Set the server's welcome message
	 *
	 * This is sent as a chat message to new clients
	 * @param message message content
	 */
	void setWelcomeMessage(const QString &message);
	const QString &welcomeMessage() const { return _welcomeMessage; }

	/**
	 * @brief Set the session storage to use (if any)
	 *
	 * Setting this enables restoration of hibernated sessions
	 * This should be called either during the initialization phase or not at all.
	 * This object will be made the parent of the session store.
	 *
	 * @param store
	 */
	void setSessionStore(SessionStore *store);

	/**
	 * @brief Set the user identity manager to use (if any)
	 *
	 * Setting this enables authenticated user logins.
	 *
	 * @param identman
	 */
	void setIdentityManager(IdentityManager *identman) { _identman = identman; }
	IdentityManager *identityManager() const { return _identman; }

	/**
	 * @brief Set client connection idle timeout
	 * @param timeout timeout in milliseconds
	 */
	void setConnectionTimeout(int timeout) { _connectionTimeout = timeout; }
	int connectionTimeout() const { return _connectionTimeout; }

	/**
	 * @brief Get the session announcement server client
	 */
	sessionlisting::AnnouncementApi *announcementApiClient() const { return _publicListingApi; }

	/**
	 * @brief Add a new client
	 *
	 * This will start the login process during which the client will
	 * be assigned to a session.
	 *
	 * @param client newly created client
	 */
	void addClient(Client *client);

	/**
	 * @brief Create a new session
	 * @param id session ID
	 * @param minorVersion client minor version
	 * @param founder session founder username
	 * @return the newly created session
	 */
	SessionState *createSession(const SessionId &id, int minorVersion, const QString &founder);

	/**
	 * @brief Get all current sessions
	 * @return list of all sessions
	 */
	Q_INVOKABLE QList<SessionDescription> sessions() const;

	/**
	 * @brief Get a session description by ID
	 *
	 * @param id the session ID
	 * @param getExtended get extended information
	 * @param getUsers get user list as well
	 * @return session description or a blank object (id=0) if not found
	 */
	Q_INVOKABLE SessionDescription getSessionDescriptionById(const QString &id, bool getExtended=false, bool getUsers=false) const;

	/**
	 * @brief Get the session with the specified ID
	 *
	 * This may trigger the de-hibernation of a stored session
	 *
	 * @param id session ID
	 * @return session or null if not found
	 */
	SessionState *getSessionById(const QString &id);

	/**
	 * @brief Get the total number of users in all sessions
	 * @return user count
	 */
	int totalUsers() const;

	/**
	 * @brief Get the number of active sessions
	 * @return session count
	 */
	int sessionCount() const { return _sessions.size(); }

	/**
	 * @brief Get a summarized server status
	 */
	Q_INVOKABLE ServerStatus getServerStatus() const;

	/**
	 * @brief Delete the session with the given ID
	 *
	 * The session will be deleted even if it is hibernating.
	 *
	 * @param id session ID
	 * @return true on success
	 */
	Q_INVOKABLE bool killSession(const QString &id);

	/**
	 * @brief kick a user user from a session
	 *
	 * @param sessionId
	 * @param userId
	 * @return true on success
	 */
	Q_INVOKABLE bool kickUser(const QString &sessionId, int userId);

	/**
	 * @brief Stop all running sessions
	 */
	void stopAll();

	/**
	 * @brief Write a message to all active users
	 *
	 * This sends a system chat message to all users of
	 * every active session.
	 *
	 * @param message the message
	 * @param sessionId if set, limit message to this session only
	 * @return false if session was not found
	 */
	Q_INVOKABLE bool wall(const QString &message, const QString &sessionId=QString());

signals:
	/**
	 * @brief A session was just created
	 *
	 * Note. sessionChanged is also emitted for this event.
	 * @param session
	 */
	void sessionCreated(SessionState *session);

	/**
	 * @brief Published session information has changed
	 *
	 * Changes that emit a signal are:
	 * - session created
	 * - title change
	 * - participant count change
	 * - closed/open status change
	 */
	void sessionChanged(const SessionDescription &session);

	/**
	 * @brief A user just logged in to a session
	 */
	void userLoggedIn();

	/**
	 * @brief A user just disconnected
	 */
	void userDisconnected();

	/**
	 * @brief Session with the given ID has just been destroyed
	 */
	void sessionEnded(QString id);

private slots:
	void moveFromLobby(SessionState *session, Client *client);
	void lobbyDisconnectedEvent(Client *client);
	void userDisconnectedEvent(SessionState *session);
	void cleanupSessions();
	void refreshSessionAnnouncements();
	void announceSession(const QUrl &url, const sessionlisting::Session &session);
	void unlistSession(const sessionlisting::Announcement &listing);
	void sessionAnnounced(const sessionlisting::Announcement &listing);

private:
	void initSession(SessionState *session);
	void destroySession(SessionState *session);

	QList<SessionState*> _sessions;
	QList<Client*> _lobby;
	SessionStore *_store;
	IdentityManager *_identman;
	sessionlisting::AnnouncementApi *_publicListingApi;

	QString _title;
	QString _welcomeMessage;
	int _sessionLimit;
	int _connectionTimeout;
	uint _historyLimit;
	qint64 _expirationTime;
	QString _hostPassword;
	bool _allowPersistentSessions;
	bool _mustSecure;
	bool m_privateUserList;

#ifndef NDEBUG
	uint _randomlag;
#endif
};

}

