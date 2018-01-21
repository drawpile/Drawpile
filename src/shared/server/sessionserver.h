/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#include "../net/protover.h"
#include "jsonapi.h"

#include <QObject>
#include <QDir>

namespace sessionlisting {
	class AnnouncementApi;
	struct Session;
	struct Announcement;
}

namespace server {

class Session;
class SessionHistory;
class Client;
class ServerConfig;
class TemplateLoader;

/**
 * @brief Session manager
 *
 */
class SessionServer : public QObject {
Q_OBJECT
public:
	SessionServer(ServerConfig *config, QObject *parent=nullptr);

	/**
	 * @brief Enable file backed sessions
	 * @param dir session directory
	 */
	void setSessionDir(const QDir &dir);

	/**
	 * @brief Set the template loader to use
	 */
	void setTemplateLoader(TemplateLoader *loader) { m_tpls = loader; }
	const TemplateLoader *templateLoader() const { return m_tpls; }

	/**
	 * @brief Load new sessions from the directory
	 *
	 * If no session directory is set, this does nothing.
	 */
	void loadNewSessions();

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
	 * @brief Create a new session by instantiating a template
	 * @param idAlias template alis
	 * @return nullptr if instantiation failed
	 */
	Session *createFromTemplate(const QString &idAlias);

	/**
	 * @brief Get descriptions of all sessions
	 */
	QJsonArray sessionDescriptions() const;

	/**
	 * @brief Get the session with the specified ID
	 *
	 * @param id session ID
	 * @return session or null if not found
	 */
	Session *getSessionById(const QString &id) const;

	/**
	 * @brief Check if a session or template exists with this ID or alias
	 */
	bool isIdInUse(const QString &id) const;

	/**
	 * @brief Get the total number of connected users
	 */
	int totalUsers() const;

	/**
	 * @brief Get the number of active sessions
	 */
	int sessionCount() const { return m_sessions.size(); }

	/**
	 * @brief Stop all running sessions
	 */
	void stopAll();

	/**
	 * @brief Send a global message to all active sessions
	 * @param message the message
	 * @param alert is this an alert type message?
	 */
	void messageAll(const QString &message, bool alert);

	/**
	 * @brief Call the server's JSON administration API (session list)
	 *
	 * This is used by the HTTP admin API.
	 *
	 * @param method query method
	 * @param path path components
	 * @param request request body content
	 * @return JSON API response content
	 */
	JsonApiResult callSessionJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

	//! Like callSessionJsonApi, but for the user list
	JsonApiResult callUserJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

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
	 * @param count total number of logged in users (including the new one)
	 */
	void userLoggedIn(int count);

	/**
	 * @brief A user just disconnected
	 * @param count total number of logged in users
	 */
	void userDisconnected(int count);

	/**
	 * @brief Session with the given ID has just been destroyed
	 */
	void sessionEnded(const QString &id);

private slots:
	void moveFromLobby(Session *session, Client *client);
	void lobbyDisconnectedEvent(Client *client);
	void userDisconnectedEvent(Session *session);
	void cleanupSessions();

private:
	SessionHistory *initHistory(const QUuid &id, const QString alias, const protocol::ProtocolVersion &protocolVersion, const QString &founder);
	void initSession(Session *session);

	ServerConfig *m_config;
	TemplateLoader *m_tpls;
	QDir m_sessiondir;
	bool m_useFiledSessions;

	QList<Session*> m_sessions;
	QList<Client*> m_lobby;

	bool m_mustSecure;

#ifndef NDEBUG
	uint m_randomlag;
#endif
};

}

