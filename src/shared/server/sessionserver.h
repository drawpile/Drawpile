/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QObject>

namespace server {

class SessionState;
class Client;

/**
 * @brief Session manager
 *
 */
class SessionServer : public QObject {
Q_OBJECT
public:
	explicit SessionServer(QObject *parent=0);
	~SessionServer();

	/**
	 * @brief Set the maximum number of sessions
	 *
	 * This does not affect existing sessions.
	 *
	 * @param limit
	 */
	void setSessionLimit(int limit) { Q_ASSERT(limit>=0); _sessionLimit = limit; }
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
	void setHostPassword(const QString &password) { _hostPassword = password; }
	const QString &hostPassword() const { return _hostPassword; }

	/**
	 * @brief Set whether persistent sessions are allowed
	 * @param persistent
	 */
	void setAllowPersistentSessions(bool persistent) { _allowPersistentSessions = persistent; }
	bool allowPersistentSessions() const { return _allowPersistentSessions; }

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
	 * @param minorVersion client minor version
	 * @param password password for the new session
	 * @return the newly created session
	 */
	SessionState *createSession(int minorVersion);

	/**
	 * @brief Get all current sessions
	 * @return list of all sessions
	 */
	QList<SessionState*> sessions() const { return _sessions; }

	/**
	 * @brief Get the session with the specified ID
	 * @param id session ID
	 * @return session or null if not found
	 */
	SessionState *getSessionById(int id) const;

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
	 * @brief Stop all running sessions
	 */
	void stopAll();

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
	void sessionChanged(SessionState *session);

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
	void sessionEnded(int id);

private slots:
	void moveFromLobby(SessionState *session, Client *client);
	void lobbyDisconnectedEvent(Client *client);
	void userDisconnectedEvent(SessionState *session);

private:
	void destroySession(SessionState *session);

	QList<SessionState*> _sessions;
	QList<Client*> _lobby;
	int _nextId;

	int _sessionLimit;
	uint _historyLimit;
	QString _hostPassword;
	bool _allowPersistentSessions;
};

}

