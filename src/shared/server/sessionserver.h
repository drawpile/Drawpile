/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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
#include "../net/protover.h"

namespace sessionlisting {
	class AnnouncementApi;
	struct Session;
	struct Announcement;
}

namespace server {

class Session;
class Client;
class SessionStore;
class IdentityManager;
class ServerConfig;

/**
 * @brief Session manager
 *
 */
class SessionServer : public QObject {
Q_OBJECT
public:
	explicit SessionServer(ServerConfig *config, QObject *parent=0);

	/**
	 * @brief Get the server configuration
	 * @return
	 */
	const ServerConfig *config() const { return m_config; }

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
	 * @brief Set the user identity manager to use (if any)
	 *
	 * Setting this enables authenticated user logins.
	 *
	 * @param identman
	 */
	void setIdentityManager(IdentityManager *identman) { _identman = identman; }
	IdentityManager *identityManager() const { return _identman; }

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
	 * @param idAlias session ID alias (empty if no alias set)
	 * @param protocolVersion client protocol version
	 * @param founder session founder username
	 * @return the newly created session
	 */
	Session *createSession(const QUuid &id, const QString &idAlias, const protocol::ProtocolVersion &protocolVersion, const QString &founder);

	/**
	 * @brief Get all current sessions
	 * @return list of all sessions
	 */
	QList<SessionDescription> sessions() const;

	/**
	 * @brief Get a session description by ID
	 *
	 * @param id the session ID
	 * @return session description or a blank object (id=0) if not found
	 */
	SessionDescription getSessionDescriptionById(const QString &id) const;

	/**
	 * @brief Get the session with the specified ID
	 *
	 * This may trigger the de-hibernation of a stored session
	 *
	 * @param id session ID
	 * @return session or null if not found
	 */
	Session *getSessionById(const QString &id) const;

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
	 * @brief Delete the session with the given ID
	 *
	 * The session will be deleted even if it is hibernating.
	 *
	 * @param id session ID
	 * @return true on success
	 */
	bool killSession(const QString &id);

	/**
	 * @brief kick a user user from a session
	 *
	 * @param sessionId
	 * @param userId
	 * @return true on success
	 */
	bool kickUser(const QString &sessionId, int userId);

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
	bool wall(const QString &message, const QString &sessionId=QString());

signals:
	/**
	 * @brief A session was just created
	 *
	 * Note. sessionChanged is also emitted for this event.
	 * @param session
	 */
	void sessionCreated(Session *session);

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
	void sessionEnded(const QString &id);

private slots:
	void moveFromLobby(Session *session, Client *client);
	void lobbyDisconnectedEvent(Client *client);
	void userDisconnectedEvent(Session *session);
	void cleanupSessions();
	void refreshSessionAnnouncements();
	void announceSession(const QUrl &url, const sessionlisting::Session &session);
	void unlistSession(const sessionlisting::Announcement &listing);
	void sessionAnnounced(const sessionlisting::Announcement &listing);

private:
	void initSession(Session *session);
	void destroySession(Session *session);

	ServerConfig *m_config;

	QList<Session*> _sessions;
	QList<Client*> _lobby;
	SessionStore *_store;
	IdentityManager *_identman;
	sessionlisting::AnnouncementApi *_publicListingApi;

	int _connectionTimeout;
	qint64 _expirationTime;
	QString _hostPassword;
	bool _mustSecure;

#ifndef NDEBUG
	uint _randomlag;
#endif
};

}

