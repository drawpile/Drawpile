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

#include "../net/protover.h"

namespace sessionlisting {
	class AnnouncementApi;
	struct Session;
	struct Announcement;
}

namespace server {

class Session;
class Client;
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
	void setMustSecure(bool mustSecure) { m_mustSecure = mustSecure; }
	bool mustSecure() const { return m_mustSecure; }

#ifndef NDEBUG
	void setRandomLag(uint lag) { m_randomlag = lag; }
#endif

	/**
	 * @brief Get the session announcement server client
	 */
	sessionlisting::AnnouncementApi *announcementApiClient() const { return m_publicListingApi; }

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
	 * @brief Get descriptions of all sessions
	 */
	QJsonArray sessionDescriptions() const;

	/**
	 * @brief Get a session description by ID
	 *
	 * @param id the session ID
	 * @param full return full session info
	 * @return session description or a blank object if not found
	 */
	QJsonObject getSessionDescriptionById(const QString &id, bool full=false) const;

	/**
	 * @brief Get the session with the specified ID
	 *
	 * @param id session ID
	 * @return session or null if not found
	 */
	Session *getSessionById(const QString &id) const;

	/**
	 * @brief Get the total number of connected users
	 */
	int totalUsers() const;

	/**
	 * @brief Get the number of active sessions
	 */
	int sessionCount() const { return m_sessions.size(); }

	/**
	 * @brief Delete the session with the given ID
	 *
	 * The session will be deleted even if it is hibernating.
	 * TODO replace with json api
	 *
	 * @param id session ID
	 * @return true on success
	 */
	bool killSession(const QString &id);

	/**
	 * @brief Stop all running sessions
	 */
	void stopAll();

	/**
	 * @brief Write a message to all active users
	 *
	 * TODO replace with json api
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
	void sessionChanged(const QJsonObject &session);

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

	QList<Session*> m_sessions;
	QList<Client*> m_lobby;
	sessionlisting::AnnouncementApi *m_publicListingApi;

	bool m_mustSecure;

#ifndef NDEBUG
	uint m_randomlag;
#endif
};

}

