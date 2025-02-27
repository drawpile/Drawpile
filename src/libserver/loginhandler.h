// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_SERVER_LOGINHANDLER_H
#define DP_SERVER_LOGINHANDLER_H
#include "libshared/net/message.h"
#include "libshared/net/protover.h"
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>

namespace net {
struct ServerCommand;
struct ServerReply;
}

namespace server {

class Client;
class Session;
class Sessions;
class ServerConfig;

/**
 * @brief Perform the client login handshake
 *
 * The login process is as follows:
 * (client C connects to [this] server S)
 *
 * S: Greeting (name and version info)
 *
 * - client should disconnect at this point if version does not match -
 *
 * C: STARTTLS (if "TLS" is in FEATURES)
 * S: STARTTLS (starts SSL handshake)
 *
 * C: IDENT username and password (or) IDENT extauth
 * S: IDENTIFIED OK or NEED PASSWORD, NEED EXTAUTH or ERROR
 *
 * S: SESSION LIST UPDATES
 *
 * - Note. Server may send updates to session list and title until the client
 * has made a choice -
 *
 * C: HOST or JOIN session
 *
 * S: OK or ERROR
 *
 * - if OK, the client is added to the session. If the client is hosting,
 * initial state must be uploaded next. -
 *
 * Notes:
 * ------
 *
 * Possible server feature flags:
 *    -       - no optional features supported
 *    MULTI   - this server supports multiple sessions
 *    TLS     - the server supports SSL/TLS encryption
 *    SECURE  - user must initiate encryption before login can proceed
 *    PERSIST - persistent sessions are supported
 *    IDENT   - non-guest access is supported
 *    NOGUEST - guest access is disabled (users must identify with password)
 *
 * Session ID is a string in the format [a-zA-Z0-9:-]{1,64}
 * If the ID was specified by the user (vanity ID), it is prefixed with '!'
 *
 */
class LoginHandler final : public QObject {
	Q_OBJECT
public:
	LoginHandler(Client *client, Sessions *sessions, ServerConfig *config);

	void startLoginProcess();

public slots:
	void announceSession(const QJsonObject &session);
	void announceSessionEnd(const QString &id);

private slots:
	void handleLoginMessage(const net::Message &msg);

private:
	static constexpr int MAX_PASSWORD_ATTEMPTS = 10;

	enum class State {
		WaitForSecure,
		WaitForClientInfo,
		WaitForLookup,
		WaitForIdent,
		WaitForLogin,
		Ignore,
	};
	enum class IdentIntent { Invalid, Unknown, Guest, Auth, ExtAuth };
	enum class HostPrivilege { None, Passworded, Any };

	class ClientInfoLogGuard;
	friend class ClientInfoLogGuard;

	void announceServerInfo();
	void handleClientInfoMessage(const net::ServerCommand &cmd);
	void handleLookupMessage(const net::ServerCommand &cmd);
	void handleIdentMessage(const net::ServerCommand &cmd);
	void handleHostMessage(const net::ServerCommand &cmd);
	void handleJoinMessage(const net::ServerCommand &cmd);
	void checkClientCapabilities(const net::ServerCommand &cmd);
	QJsonObject
	extractClientInfo(const QJsonObject &o, bool checkAuthenticated);
	bool compareClientInfo(const QJsonObject &info);
	void logClientInfo(const QJsonObject &info);
	void handleAbuseReport(const net::ServerCommand &cmd);
	void handleStarttls();
	void requestExtAuth();
	void guestLogin(
		const QString &username, IdentIntent intent,
		bool extAuthFallback = false);
	void authLoginOk(
		const QString &username, const QString &authId,
		const QStringList &flags, const QByteArray &avatar, bool allowMod,
		bool allowHost, bool allowGhost, bool allowBanExempt, bool allowWeb,
		bool allowWebHost, bool allowWebSession, bool allowPersist);
	bool send(const net::Message &msg);
	void sendError(
		const QString &code, const QString &message, bool disconnect = true);
	void extAuthGuestLogin(const QString &username, IdentIntent intent);

	bool needsLookup() const;

	static IdentIntent parseIdentIntent(const QString &s);
	static QString identIntentToString(IdentIntent intent);
	bool checkIdentIntent(
		IdentIntent intent, IdentIntent actual, bool extAuthFallback = false);

	bool verifySystemId(const QString &sid, bool required);

	static bool isValidSid(const QString &sid);

	bool verifyUserId(long long userId);

	void insertImplicitFlags(QSet<QString> &effectiveFlags);
	static QJsonArray flagSetToJson(const QSet<QString> &flags);

	HostPrivilege getHostPrivilege(const QSet<QString> &effectiveFlags) const;
	bool haveHostPrivilege(bool passworded) const;
	bool shouldAllowWebOnHost(
		const net::ServerCommand &cmd, const Session *session) const;

	Client *m_client;
	Sessions *m_sessions;
	ServerConfig *m_config;

	State m_state = State::WaitForIdent;
	QString m_minimumProtocolVersionString;
	protocol::ProtocolVersion m_minimumProtocolVersion;
	quint64 m_extauth_nonce = 0;
	HostPrivilege m_hostPrivilege = HostPrivilege::None;
	bool m_exemptFromBans = false;
	bool m_complete = false;
	bool m_requireMatchingHost;
	bool m_mandatoryLookup;
	QString m_lookup;
	QString m_lookupInviteSecret;
	int m_authPasswordAttempts = 0;
	int m_sessionPasswordAttempts = 0;
	QJsonObject m_clientInfo;
	QJsonObject m_lastLoggedClientInfo;
	Session *m_lastClientSession = nullptr;
};

}

#endif // LOGINHANDLER_H
