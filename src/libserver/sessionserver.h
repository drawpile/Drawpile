// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/jsonapi.h"
#include "libserver/sessions.h"
#include "libshared/net/protover.h"
#include <QDir>
#include <QObject>

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
class SessionServer final : public QObject, public Sessions {
	Q_OBJECT
public:
	SessionServer(ServerConfig *config, QObject *parent = nullptr);

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
	std::tuple<Session *, QString> createSession(
		const QString &id, const QString &idAlias,
		const protocol::ProtocolVersion &protocolVersion,
		const QString &founder) override;

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
	JsonApiResult callSessionJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request);

	//! Like callSessionJsonApi, but for the user list
	JsonApiResult callUserJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request);

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
	void removeSession(Session *session);
	void removeClient(ThinServerClient *client);
	void onSessionAttributeChanged(Session *session);
	void cleanupSessions();

private:
	SessionHistory *initHistory(
		const QString &id, const QString alias,
		const protocol::ProtocolVersion &protocolVersion,
		const QString &founder);
	void initSession(Session *session);

	ThinServerClient *searchClientByPathUid(const QString &uid);

	sessionlisting::Announcements *m_announcements;
	ServerConfig *m_config;
	TemplateLoader *m_tpls = nullptr;
	QDir m_sessiondir;
	bool m_useFiledSessions = false;

	QList<Session *> m_sessions;
	QList<ThinServerClient *> m_clients;
};

}
