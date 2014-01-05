/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QStringList>

#include "loginhandler.h"
#include "client.h"
#include "session.h"

#include "../net/login.h"

#include "config.h"

namespace server {

LoginHandler::LoginHandler(Client *client, SessionState *session, SharedLogger logger) :
	QObject(client), _client(client), _session(session), _logger(logger)
{
	connect(client, SIGNAL(loginMessage(protocol::MessagePtr)), this, SLOT(handleLoginMessage(protocol::MessagePtr)));
}

void LoginHandler::startLoginProcess()
{
	// Start by telling who we are
	send(QString("DRAWPILE %1").arg(DRAWPILE_PROTO_MAJOR_VERSION));
	// Client should disconnect upon receiving the above if the version number does not match

	// Tell about our session
	if(_session) {
		QString ses = QString("SESSION %1").arg(_session->minorProtocolVersion());
		if(!_session->password().isEmpty())
			ses.append(" PASS");

		send(ses);
	} else {
		send("NOSESSION");
	}
}

void LoginHandler::handleLoginMessage(protocol::MessagePtr msg)
{
	if(msg->type() != protocol::MSG_LOGIN) {
		_logger->logError("login handler was passed a non-login message!");
		return;
	}

	QString message = msg.cast<protocol::Login>().message();

	if(message.startsWith("HOST "))
		handleHostMessage(message);
	else if(message.startsWith("JOIN "))
		handleJoinMessage(message);
	else {
		_logger->logWarning(QString("Got invalid login message from %1").arg(_client->peerAddress().toString()));
		send("WHAT?");
		_client->kick(0);
	}
}

void LoginHandler::handleHostMessage(const QString &message)
{
	if(_session) {
		// protocol violation: session already in progress
		send("CLOSED");
		_client->kick(0);
		return;
	}

	QStringList tokens = message.split(' ', QString::SkipEmptyParts);
	if(tokens.size() < 4) {
		send("WHAT?");
		_client->kick(0);
		return;
	}

	bool ok;
	int minorVersion = tokens.at(1).toInt(&ok);
	if(!ok) {
		send("WHAT?");
		_client->kick(0);
		return;
	}

	int userId = tokens.at(2).toInt(&ok);
	if(!ok || userId<1 || userId>255) {
		send("WHAT?");
		_client->kick(0);
		return;
	}

	QString username = QStringList(tokens.mid(3)).join(' ');
	if(!validateUsername(username)) {
		send("BADNAME");
		_client->kick(0);
		return;
	}

	_client->setId(userId);
	_client->setUsername(username);

	send(QString("OK %1").arg(userId));

	// Create a new session
	SessionState *session = new SessionState(minorVersion, _logger);
	session->joinUser(_client, true);

	emit sessionCreated(session);

	deleteLater();
}

void LoginHandler::handleJoinMessage(const QString &message)
{
	if(!_session) {
		send("NOSESSION");
		_client->kick(0);
		return;
	}

	if(_session->userCount() >= _session->maxUsers()) {
		send("CLOSED");
		_client->kick(0);
		return;
	}

	QString username, password;
	int passwordstart = message.indexOf(';');
	if(passwordstart>0) {
		username = message.mid(5, passwordstart-5).trimmed();
		password = message.mid(passwordstart+1);
	} else {
		username = message.mid(5).trimmed();
	}

	if(!validateUsername(username)) {
		send("BADNAME");
		_client->kick(0);
		return;
	}

	if(password != _session->password()) {
		send("BADPASS");
		_client->kick(0);
		return;
	}

	// Ok, join the session
	_client->setUsername(username);
	_session->assignId(_client);

	send(QString("OK %1").arg(_client->id()));

	_session->joinUser(_client, false);

	deleteLater();
}

void LoginHandler::send(const QString &msg)
{
	_client->sendDirectMessage(protocol::MessagePtr(new protocol::Login(msg)));
}

bool LoginHandler::validateUsername(const QString &username)
{
	if(username.isEmpty())
		return false;

	// TODO check for duplicates
	return true;
}

}
