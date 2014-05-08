/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "loginhandler.h"
#include "client.h"
#include "session.h"
#include "sessionserver.h"

#include "../net/login.h"
#include "../util/logger.h"

#include "config.h"

#include <QStringList>
#include <QRegularExpression>

namespace server {

LoginHandler::LoginHandler(Client *client, SessionServer *server) :
	QObject(client), _client(client), _server(server), _complete(false)
{
	connect(client, SIGNAL(loginMessage(protocol::MessagePtr)), this,
			SLOT(handleLoginMessage(protocol::MessagePtr)));

	connect(server, SIGNAL(sessionChanged(SessionState*)), this, SLOT(announceSession(SessionState*)));
	connect(server, SIGNAL(sessionEnded(int)), this, SLOT(announceSessionEnd(int)));
}

void LoginHandler::startLoginProcess()
{
	QStringList flags;

	_state = WAIT_FOR_LOGIN;

	if(_server->sessionLimit()>1)
		flags << "MULTI";
	if(!_server->hostPassword().isEmpty())
		flags << "HOSTP";
	if(_server->allowPersistentSessions())
		flags << "PERSIST";
	if(_client->hasSslSupport())
		flags << "TLS";
	if(_server->mustSecure() && _client->hasSslSupport()) {
		flags << "SECURE";
		_state = WAIT_FOR_SECURE;
	}

	// Start by telling who we are
	send(QString("DRAWPILE %1 %2")
		 .arg(DRAWPILE_PROTO_MAJOR_VERSION)
		 .arg(flags.isEmpty() ? "-" : flags.join(","))
	);

	// Client should disconnect upon receiving the above if the version number does not match

	// Send server title
	if(!_server->title().isEmpty())
		send("TITLE " + _server->title());

	// Tell about our session
	QList<SessionState*> sessions = _server->sessions();

	if(sessions.isEmpty()) {
		send("NOSESSION");
	} else {
		for(SessionState *session : sessions) {
			announceSession(session);
		}
	}
}

void LoginHandler::announceSession(SessionState *session)
{
	QStringList flags;
	if(!session->password().isEmpty())
		flags << "PASS";

	if(session->isClosed())
		flags << "CLOSED";

	if(session->isPersistent())
		flags << "PERSIST";

	send(QString("SESSION %1 %2 %3 %4 \"%5\"")
			.arg(session->id())
			.arg(session->minorProtocolVersion())
			.arg(flags.isEmpty() ? "-" : flags.join(","))
			.arg(session->userCount())
			.arg(session->title())
	);
}

void LoginHandler::announceSessionEnd(int id)
{
	send(QString("NOSESSION %1").arg(id));
}

void LoginHandler::handleLoginMessage(protocol::MessagePtr msg)
{
	if(msg->type() != protocol::MSG_LOGIN) {
		logger::error() << "login handler was passed a non-login message!";
		return;
	}

	QString message = msg.cast<protocol::Login>().message();

	if(_state == WAIT_FOR_SECURE) {
		if(message == "STARTTLS") {
			handleStarttls();
		} else {
			send("ERROR MUSTSECURE");
			logger::warning() << "Client from" << _client->peerAddress() << "didn't secure connection!";
			_client->disconnectError("must secure connection first");
		}
	} else {
		if(message.startsWith("HOST ")) {
			handleHostMessage(message);
		} else if(message.startsWith("JOIN ")) {
			handleJoinMessage(message);
		} else if(message == "STARTTLS") {
			handleStarttls();
		} else {
			logger::warning() << "Got invalid login message from" << _client->peerAddress();
			_client->disconnectError("invalid message");
		}
	}
}

void LoginHandler::handleHostMessage(const QString &message)
{
	if(_server->sessionCount() >= _server->sessionLimit()) {
		send("ERROR CLOSED");
		_client->disconnectError("login error");
		return;
	}

	const QRegularExpression re("\\AHOST (\\d+) (\\d+) \"([^\"]+)\"\\s*(?:;(.+))?\\z");
	auto m = re.match(message);
	if(!m.hasMatch()) {
		send("ERROR SYNTAX");
		_client->disconnectError("login error");
		return;
	}

	int minorVersion = m.captured(1).toInt();
	int userId = m.captured(2).toInt();

	QString username = m.captured(3);
	if(!validateUsername(username)) {
		send("ERROR BADNAME");
		_client->disconnectError("login error");
		return;
	}

	QString password = m.captured(4);
	if(password != _server->hostPassword()) {
		send("ERROR BADPASS");
		_client->disconnectError("login error");
		return;
	}

	_client->setId(userId);
	_client->setUsername(username);

	// Mark login phase as complete. No more login messages will be sent to this user
	send(QString("OK %1").arg(userId));
	_complete = true;

	// Create a new session
	SessionState *session = _server->createSession(minorVersion);

	session->joinUser(_client, true);

	deleteLater();
}

void LoginHandler::handleJoinMessage(const QString &message)
{
	const QRegularExpression re("\\AJOIN (\\d+) \"([^\"]+)\"\\s*(?:;(.+))?\\z");
	auto m = re.match(message);
	if(!m.hasMatch()) {
		send("ERROR SYNTAX");
		_client->disconnectError("login error");
		return;
	}

	int sessionId = m.captured(1).toInt();
	SessionState *session = _server->getSessionById(sessionId);
	if(!session) {
		send("ERROR NOSESSION");
		_client->disconnectError("login error");
		return;
	}

	if(session->isClosed()) {
		send("ERROR CLOSED");
		_client->disconnectError("login error");
		return;
	}

	QString username = m.captured(2);
	QString password = m.captured(3);

	if(password != session->password()) {
		send("ERROR BADPASS");
		_client->disconnectError("login error");
		return;
	}

	if(!validateUsername(username)) {
		send("ERROR BADNAME");
		_client->disconnectError("login error");
		return;
	}

	bool nameInUse=false;
	for(const Client *c : session->clients()) {
		if(c->username().compare(username, Qt::CaseInsensitive)==0) {
			nameInUse=true;
			break;
		}
	}
	if(nameInUse) {
#ifdef NDEBUG
		send("ERROR NAMEINUSE");
		_client->disconnectError("login error");
		return;
#else
		// Allow identical usernames in debug builds, so I don't have to keep changing
		// the username when testing. There is no technical requirement for unique usernames;
		// the limitation is solely for the benefit of the human users.
		logger::warning() << "Username clash" << username << "for" << *session << "ignored because this is a debug build.";
#endif
	}

	// Ok, join the session
	_client->setUsername(username);
	session->assignId(_client);

	send(QString("OK %1").arg(_client->id()));
	_complete = true;

	session->joinUser(_client, false);

	deleteLater();
}

void LoginHandler::handleStarttls()
{
	if(!_client->hasSslSupport()) {
		// Note. Well behaved clients shouldn't send STARTTLS if TLS was not listed in server features.
		send("ERROR NOTLS");
		_client->disconnectError("login error");
		return;
	}

	if(_client->isSecure()) {
		send("ERROR ALREADYSECURE");
		_client->disconnectError("login error");
		return;
	}

	send("STARTTLS");
	_client->startTls();
	_state = WAIT_FOR_LOGIN;
}

void LoginHandler::send(const QString &msg)
{
	if(!_complete)
		_client->sendDirectMessage(protocol::MessagePtr(new protocol::Login(msg)));
}

bool LoginHandler::validateUsername(const QString &username)
{
	if(username.isEmpty())
		return false;

	if(username.contains('"'))
		return false;

	return true;
}

}
