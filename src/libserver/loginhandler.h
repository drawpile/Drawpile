// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_SERVER_LOGINHANDLER_H
#define DP_SERVER_LOGINHANDLER_H

#include "libshared/net/message.h"

#include <QObject>
#include <QStringList>
#include <QByteArray>

namespace protocol {
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
 * - Note. Server may send updates to session list and title until the client has made a choice -
 *
 * C: HOST or JOIN session
 *
 * S: OK or ERROR
 *
 * - if OK, the client is added to the session. If the client is hosting, initial state must be uploaded next. -
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
class LoginHandler final : public QObject
{
	Q_OBJECT
public:
	LoginHandler(Client *client, Sessions *sessions, ServerConfig *config);

	void startLoginProcess();

public slots:
	void announceSession(const QJsonObject &session);
	void announceSessionEnd(const QString &id);

private slots:
	void handleLoginMessage(protocol::MessagePtr message);

private:
	enum class State {
		WaitForSecure,
		WaitForIdent,
		WaitForLogin
	};

	void announceServerInfo();
	void handleIdentMessage(const protocol::ServerCommand &cmd);
	void handleHostMessage(const protocol::ServerCommand &cmd);
	void handleJoinMessage(const protocol::ServerCommand &cmd);
	void handleAbuseReport(const protocol::ServerCommand &cmd);
	void handleStarttls();
	void requestExtAuth();
	void guestLogin(const QString &username);
	void authLoginOk(const QString &username, const QString &authId, const QStringList &flags, const QByteArray &avatar, bool allowMod, bool allowHost);
	bool send(const protocol::ServerReply &cmd);
	void sendError(const QString &code, const QString &message);
	void extAuthGuestLogin(const QString &username);

	Client *m_client;
	Sessions *m_sessions;
	ServerConfig *m_config;

	State m_state = State::WaitForIdent;
	quint64 m_extauth_nonce = 0;
	bool m_hostPrivilege = false;
	bool m_complete = false;
};

}

#endif // LOGINHANDLER_H

