/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef DP_SERVER_LOGINHANDLER_H
#define DP_SERVER_LOGINHANDLER_H

#include <QObject>

#include "../util/logger.h"
#include "../net/message.h"

namespace server {

class Client;
class SessionState;

/**
 * @brief Perform the client login handshake
 *
 * The login process is as follows:
 * (client C connects to server S)
 * S: DRAWPILE <proto-major>
 *
 * - client should disconnect at this point if proto-major does not match -
 *
 * S: SESSION <proto-minor> [PASS]
 *  - or -
 * S: NOSESSION
 *
 * C: HOST <proto-minor> <userid> <username>
 *  - or -
 * C: JOIN <username> [;password]
 *
 * S: OK <userid>
 *  - or -
 * S: BADPASS / BADNAME / CLOSED
 */
class LoginHandler : public QObject
{
	Q_OBJECT
public:
	explicit LoginHandler(Client *client, SessionState *session, SharedLogger logger);

	void startLoginProcess();

signals:
	void sessionCreated(SessionState *session);

private slots:
	void handleLoginMessage(protocol::MessagePtr message);

private:
	void handleHostMessage(const QString &message);
	void handleJoinMessage(const QString &message);
	void send(const QString &msg);

	bool validateUsername(const QString &name);

	Client *_client;
	SessionState *_session;
	SharedLogger _logger;
};

}

#endif // LOGINHANDLER_H
