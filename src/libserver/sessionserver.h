/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "libshared/net/protover.h"
#include "libserver/jsonapi.h"
#include "libserver/sessions.h"

#include <QObject>
#include <QDir>

namespace sessionlisting {
	class Announcements;
}

namespace server {

class Session;
class SessionHistory;
class ThinServerClient;
class ServerConfig;
class TemplateLoader;

/**
 * @brief Session manager
 *
 */
class SessionServer : public QObject, public Sessions {
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
	void addClient(ThinServerClient *client);

	/**
	 * @brief Create a new session
	 * @param id session ID
	 * @param idAlias session ID alias (empty if no alias set)
	 * @param protocolVersion client protocol version
	 * @param founder session founder username
	 * @return the newly created session
	 */
	std::tuple<Session*, QString> createSession(const QString &id, const QString &idAlias, const protocol::ProtocolVersion &protocolVersion, const QString &founder) override;

	/**
	 * @brief Create a new session by instantiating a template
	 * @param idAlias template alis
	 * @return nullptr if instantiation failed
	 */
	Session *createFromTemplate(const QString &idAlias);

	/**
	 * @brief Get descriptions of all sessions
	 */
	QJsonArray sessionDescriptions() const override;

	/**
	 * @brief Get the session with the specified ID
	 *
	 * @param id session ID
	 * @param load load from template if not live?
	 * @return session or null if not found
	 */
	Session *getSessionById(const QString &id, bool load) override;

	/**
	 * @brief Get the total number of connected users
	 */
	int totalUsers() const { return m_clients.size(); }

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
	 * @brief A user just disconnected
	 * @param count total number of logged in users
	 */
	void userCountChanged(int count);

	/**
	 * @brief Session with the given ID has just been destroyed
	 */
	void sessionEnded(const QString &id);

private slots:
	void removeClient(QObject *client);
	void onSessionAttributeChanged(Session *session);
	void cleanupSessions();

private:
	SessionHistory *initHistory(const QString &id, const QString alias, const protocol::ProtocolVersion &protocolVersion, const QString &founder);
	void initSession(Session *session);

	sessionlisting::Announcements *m_announcements;
	ServerConfig *m_config;
	TemplateLoader *m_tpls;
	QDir m_sessiondir;
	bool m_useFiledSessions;

	QList<Session*> m_sessions;
	QList<ThinServerClient*> m_clients;

#ifndef NDEBUG
	uint m_randomlag;
#endif
};

}

