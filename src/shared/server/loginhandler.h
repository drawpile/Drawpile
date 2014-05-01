/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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

#include <QObject>

#include "../net/message.h"

namespace server {

class Client;
class SessionState;
class SessionServer;

/**
 * @brief Perform the client login handshake
 *
 * The login process is as follows:
 * (client C connects to [this] server S)
 *
 * S: DRAWPILE <proto-major> <FEATURES¹> "server title"
 *
 * - client should disconnect at this point if proto-major does not match -
 *
 * S: TITLE "title" (if set)
 *
 * S: SESSION <id> <proto-minor> <FLAGS²> <user-count> "title"
 *  - or -
 * S: NOSESSION [id³]
 *
 * - Note. Server may send updates to session list and title until the client has made a choice.
 *
 * C: HOST <proto-minor> <userid> "<username>" [;server password⁴]
 *  - or -
 * C: JOIN <id> "<username>" [;password]
 *
 * S: OK <userid>
 *  - or -
 * S: ERROR <BADPASS|BADNAME|SESLIMIT|CLOSED>
 *
 * Notes:
 * ------
 *
 * 1) Possible server features (comma separated list):
 *    -       - no optional features supported
 *    MULTI   - this server supports multiple sessions
 *    HOSTP   - a password is needed to host a session
 *    SECURE  - the server supports encryption
 *    SECNOW  - the server requires encryption before login can proceed (implies SECURE)
 *    PERSIST - persistent sessions are supported
 *
 * 2) Set of comma delimited session flags:
 *    -       - no flags
 *    PASS    - password is needed to join
 *    CLOSED  - closed for new users
 *    PERSIST - this is a persistent session
 *
 * 3) If the ID is specified, this command indicates the session has just terminated.
 *    Otherwise, it means there are no available session at all.
 *
 * 4) The server password must be provided if HOSTP was listed in server features.
 */
class LoginHandler : public QObject
{
	Q_OBJECT
public:
	LoginHandler(Client *client, SessionServer *server);

	void startLoginProcess();

private slots:
	void handleLoginMessage(protocol::MessagePtr message);

	void announceSession(SessionState *session);
	void announceSessionEnd(int id);

private:
	void handleHostMessage(const QString &message);
	void handleJoinMessage(const QString &message);
	void send(const QString &msg);

	bool validateUsername(const QString &name);

	Client *_client;
	SessionServer *_server;

	bool _complete;
};

}

#endif // LOGINHANDLER_H

