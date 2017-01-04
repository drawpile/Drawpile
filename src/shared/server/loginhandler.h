/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#ifndef DP_SERVER_LOGINHANDLER_H
#define DP_SERVER_LOGINHANDLER_H

#include "../net/message.h"

#include <QObject>
#include <QStringList>

namespace protocol {
	struct ServerCommand;
	struct ServerReply;
}

namespace server {

class Client;
class Session;
class SessionServer;
struct SessionDescription;

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
 * C: IDENT username and password
 * S: IDENTIFIED OK or NEED PASSWORD or ERROR
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
class LoginHandler : public QObject
{
	Q_OBJECT
public:
	LoginHandler(Client *client, SessionServer *server);

	void startLoginProcess();

private slots:
	void handleLoginMessage(protocol::MessagePtr message);

	void announceSession(const QJsonObject &session);
	void announceSessionEnd(const QString &id);

private:
	enum State {
		WAIT_FOR_SECURE,
		WAIT_FOR_IDENT,
		WAIT_FOR_LOGIN
	};

	void announceServerInfo();
	void handleIdentMessage(const protocol::ServerCommand &cmd);
	void handleHostMessage(const protocol::ServerCommand &cmd);
	void handleJoinMessage(const protocol::ServerCommand &cmd);
	void handleStarttls();
	void guestLogin(const QString &username);
	void send(const protocol::ServerReply &cmd);
	void sendError(const QString &code, const QString &message);

	bool validateUsername(const QString &name);

	Client *m_client;
	SessionServer *m_server;

	State m_state;
	bool m_hostPrivilege;
	bool m_complete;
};

}

#endif // LOGINHANDLER_H

