/*
   Drawpile - a collaborative drawing program.

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

#include "../net/message.h"

#include <QObject>
#include <QStringList>

namespace server {

class Client;
class SessionState;
class SessionServer;
struct SessionDescription;

/**
 * @brief Perform the client login handshake
 *
 * The login process is as follows:
 * (client C connects to [this] server S)
 *
 * S: DRAWPILE <proto-major> <FEATURES¹>
 *
 * - client should disconnect at this point if proto-major does not match -
 *
 * C: STARTTLS (if "TLS" is in FEATURES)
 * S: STARTTLS (starts SSL handshake)
 *
 * C: IDENT "<username>" [;<password⁵>]
 * S: IDENTIFIED <GUEST|USER> <flags⁶>
 *  - or -
 * S: NEEDPASS (cannot login as guest. Client may retry IDENT with a password attached)
 *  - or -
 * S: ERROR <BADNAME|BADPASS|BANNED>
 *
 * S: TITLE <server title> (if set)
 *
 * S: SESSION <id²> <proto-minor> <FLAGS³> <user-count> "<founder>" ;<title>
 *  - or -
 * S: NOSESSION [id⁴]
 *
 * - Note. Server may send updates to session list and title until the client has made a choice -
 *
 * C: HOST <*|session id⁷> <proto-minor> <userid> [;<server password⁸>]
 *  - or -
 * C: JOIN <id> [;<password>]
 *
 * S: OK <session id> <userid>
 *  - or -
 * S: ERROR <NOSESSION|NAMEINUSE|SYNTAX|CLOSED>
 *
 * Notes:
 * ------
 *
 * 1) Possible server features (comma separated list):
 *    -       - no optional features supported
 *    MULTI   - this server supports multiple sessions
 *    HOSTP   - a password is needed to host a session
 *    TLS     - the server supports SSL/TLS encryption
 *    SECURE  - user must initiate encryption before login can proceed
 *    PERSIST - persistent sessions are supported
 *    IDENT   - non-guest access is supported
 *    NOGUEST - guest access is disabled (users must identify with password)
 *
 * 2) ID is a string in the format [a-zA-Z0-9:-]{1,64}
 *    If the ID was specified by the user (vanity ID), it is prefixed with '!'
 *
 * 3) Set of comma delimited session flags:
 *    -       - no flags
 *    PASS    - password is needed to join
 *    CLOSED  - closed for new users
 *    PERSIST - this is a persistent session
 *
 * 4) If the ID is specified, this command indicates the session has just terminated.
 *    Otherwise, it means there are no available session at all.
 *
 * 5) If password is omitted the user logs in as a guest
 *
 * 6) Possible user flags (comma separated list) are:
 *    -     - no flags
 *    MOD   - user is a moderator
 *    HOST  - user may host sessions without providing the hosting password
 *
 * 7) The hosting user may request a specific vanity ID for the session, or use '*'
 *    and let the server decide.
 * 8) The server password must be provided if HOSTP was listed in server features.
 */
class LoginHandler : public QObject
{
	Q_OBJECT
public:
	LoginHandler(Client *client, SessionServer *server);

	void startLoginProcess();

private slots:
	void handleLoginMessage(protocol::MessagePtr message);

	void announceSession(const SessionDescription &session);
	void announceSessionEnd(const QString &id);

private:
	enum State {
		WAIT_FOR_SECURE,
		WAIT_FOR_IDENT,
		WAIT_FOR_IDENTITYMANAGER_REPLY,
		WAIT_FOR_LOGIN
	};

	void announceServerInfo();
	void handleIdentMessage(const QString &message);
	void handleHostMessage(const QString &message);
	void handleJoinMessage(const QString &message);
	void handleStarttls();
	void guestLogin(const QString &username);
	void send(const QString &msg);

	bool validateUsername(const QString &name);

	Client *_client;
	SessionServer *_server;

	State _state;
	bool _hostPrivilege;
	bool _complete;
};

}

#endif // LOGINHANDLER_H

