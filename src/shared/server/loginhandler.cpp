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
#include "sessiondesc.h"
#include "identitymanager.h"

#include "../net/login.h"
#include "../util/logger.h"
#include "../util/passwordhash.h"

#include "config.h"

#include <QStringList>
#include <QRegularExpression>
#include <QFutureWatcher>

namespace server {

LoginHandler::LoginHandler(Client *client, SessionServer *server) :
	QObject(client), _client(client), _server(server), _hostPrivilege(false), _complete(false)
{
	connect(client, SIGNAL(loginMessage(protocol::MessagePtr)), this,
			SLOT(handleLoginMessage(protocol::MessagePtr)));

	connect(server, &SessionServer::sessionChanged, this, &LoginHandler::announceSession);
	connect(server, &SessionServer::sessionEnded, this, &LoginHandler::announceSessionEnd);
}

void LoginHandler::startLoginProcess()
{
	QStringList flags;

	_state = WAIT_FOR_IDENT;

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
	if(_server->identityManager()) {
		flags << "IDENT";
		if(_server->identityManager()->isAuthorizedOnly())
			flags << "NOGUEST";
	}

	// Start by telling who we are
	send(QString("DRAWPILE %1 %2")
		 .arg(DRAWPILE_PROTO_MAJOR_VERSION)
		 .arg(flags.isEmpty() ? "-" : flags.join(","))
	);

	// Client should disconnect upon receiving the above if the version number does not match
}

void LoginHandler::announceServerInfo()
{
	// Send server title
	if(!_server->title().isEmpty())
		send("TITLE " + _server->title());

	// Tell about our session
	QList<SessionDescription> sessions = _server->sessions();

	if(sessions.isEmpty()) {
		send("NOSESSION");
	} else {
		for(const SessionDescription &session : sessions) {
			announceSession(session);
		}
	}
}

void LoginHandler::announceSession(const SessionDescription &session)
{
	if(_state != WAIT_FOR_LOGIN)
		return;

	Q_ASSERT(!session.id.isEmpty());

	QStringList flags;
	if(!session.passwordHash.isEmpty())
		flags << "PASS";

	if(session.closed)
		flags << "CLOSED";

	if(session.persistent)
		flags << "PERSIST";

	if(session.hibernating)
		flags << "ASLEEP";

	send(QString("SESSION %1 %2 %3 %4 \"%5\" ;%6")
			.arg(session.id)
			.arg(session.protoMinor)
			.arg(flags.isEmpty() ? "-" : flags.join(","))
			.arg(session.userCount)
			.arg(session.founder)
			.arg(session.title)
	);
}

void LoginHandler::announceSessionEnd(const QString &id)
{
	if(_state != WAIT_FOR_LOGIN)
		return;

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
		// Secure mode: wait for STARTTLS before doing anything
		if(message == "STARTTLS") {
			handleStarttls();
		} else {
			send("ERROR MUSTSECURE");
			logger::notice() << _client << "Didn't secure connection!";
			_client->disconnectError("must secure connection first");
		}

	} else if(_state == WAIT_FOR_IDENT) {
		// Wait for user identification before moving on to session listing
		if(message == "STARTTLS") {
			handleStarttls();
		} else if(message.startsWith("IDENT ")) {
			handleIdentMessage(message);
		} else {
			logger::notice() << _client << "Invalid login message";
			_client->disconnectError("invalid message");
		}

	} else if(_state == WAIT_FOR_IDENTITYMANAGER_REPLY) {
		// Client shouldn't shouldn't send anything in this state
		logger::notice() << _client << "Got login message while waiting for identity manager reply";
		_client->disconnectError("unexpected message");

	} else {
		if(message.startsWith("HOST ")) {
			handleHostMessage(message);
		} else if(message.startsWith("JOIN ")) {
			handleJoinMessage(message);
		} else {
			logger::notice() << _client << "Got invalid login message";
			_client->disconnectError("invalid message");
		}
	}
}

void LoginHandler::handleIdentMessage(const QString &message)
{
	const QRegularExpression re("\\IDENT \"([^\"]+)\"\\s*(?:;(.+))?\\z");
	auto m = re.match(message);
	if(!m.hasMatch()) {
		send("ERROR SYNTAX");
		_client->disconnectError("login error");
		return;
	}

	QString username = m.captured(1);
	QString password;
	if(m.lastCapturedIndex() == 2)
		password = m.captured(2);

	if(!validateUsername(username)) {
		send("ERROR BADNAME");
		_client->disconnectError("login error");
		return;
	}

	if(_server->identityManager()) {
		_state = WAIT_FOR_IDENTITYMANAGER_REPLY;
		IdentityResult *result = _server->identityManager()->checkLogin(username, password);
		connect(result, &IdentityResult::resultAvailable, [this, username, password](IdentityResult *result) {
			QString error;
			Q_ASSERT(result->status() != IdentityResult::INPROGRESS);
			switch(result->status()) {
			case IdentityResult::INPROGRESS: /* can't happen */ break;
			case IdentityResult::NOTFOUND:
				if(!_server->identityManager()->isAuthorizedOnly()) {
					guestLogin(username);
					break;
				}
				// fall through to badpass if guest logins are disabled
			case IdentityResult::BADPASS:
				if(password.isEmpty()) {
					// No password: tell client that guest login is not possible (for this username)
					_state = WAIT_FOR_IDENT;
					send("NEEDPASS");
					return;
				}
				error = "BADPASS";
				break;
			case IdentityResult::BANNED: error = "BANNED"; break;
			case IdentityResult::OK: {
				// Yay, username and password were valid!
				QString okstr = "IDENTIFIED USER ";
				if(result->flags().isEmpty())
					okstr += "-";
				else
					okstr += result->flags().join(",");

				if(validateUsername(result->canonicalName())) {
					_client->setUsername(result->canonicalName());

				} else {
					logger::warning() << "Identity manager gave us an invalid username:" << result->canonicalName();
					_client->setUsername(username);
				}

				_client->setAuthenticated(true);
				_client->setModerator(result->flags().contains("MOD"));
				_hostPrivilege = result->flags().contains("HOST");
				_state = WAIT_FOR_LOGIN;
				send(okstr);
				announceServerInfo();
				} break;
			}

			if(!error.isEmpty()) {
				send("ERROR " + error);
				_client->disconnectError("login error");
			}
		});

	} else {
		if(!password.isNull()) {
			// if we have no identity manager, we can't accept passwords
			send("ERROR NOIDENT");
			_client->disconnectError("login error");
			return;
		}
		guestLogin(username);
	}
}

void LoginHandler::guestLogin(const QString &username)
{
	if(_server->identityManager() && _server->identityManager()->isAuthorizedOnly()) {
		send("ERROR NOGUEST");
		_client->disconnectError("login error");
		return;
	}

	_client->setUsername(username);
	_state = WAIT_FOR_LOGIN;
	send("IDENTIFIED GUEST -");
	announceServerInfo();
}

void LoginHandler::handleHostMessage(const QString &message)
{
	Q_ASSERT(!_client->username().isEmpty());

	if(_server->sessionCount() >= _server->sessionLimit()) {
		send("ERROR CLOSED");
		_client->disconnectError("login error");
		return;
	}

	const QRegularExpression re("\\AHOST (\\d+) (\\d+)\\s*(?:;(.+))?\\z");
	auto m = re.match(message);
	if(!m.hasMatch()) {
		send("ERROR SYNTAX");
		_client->disconnectError("login error");
		return;
	}

	int minorVersion = m.captured(1).toInt();
	int userId = m.captured(2).toInt();

	QString password = m.captured(3);
	if(password != _server->hostPassword() && !_hostPrivilege) {
		send("ERROR BADPASS");
		_client->disconnectError("login error");
		return;
	}

	_client->setId(userId);

	// Mark login phase as complete. No more login messages will be sent to this user
	send(QString("OK %1").arg(userId));
	_complete = true;

	// Create a new session
	SessionState *session = _server->createSession(minorVersion, _client->username());

	session->joinUser(_client, true);

	deleteLater();
}

void LoginHandler::handleJoinMessage(const QString &message)
{
	Q_ASSERT(!_client->username().isEmpty());

	const QRegularExpression re("\\AJOIN ([a-zA-Z0-9:-]{1,64})\\s*(?:;(.+))?\\z");
	auto m = re.match(message);
	if(!m.hasMatch()) {
		send("ERROR SYNTAX");
		_client->disconnectError("login error");
		return;
	}

	QString sessionId = m.captured(1);
	SessionDescription sessiondesc = _server->getSessionDescriptionById(sessionId);
	if(sessiondesc.id==0) {
		send("ERROR NOSESSION");
		_client->disconnectError("login error");
		return;
	}

	if(sessiondesc.closed && !_client->isModerator()) {
		send("ERROR CLOSED");
		_client->disconnectError("login error");
		return;
	}

	QString password = m.captured(2);

	if(!passwordhash::check(password, sessiondesc.passwordHash) && !_client->isModerator()) {
		send("ERROR BADPASS");
		_client->disconnectError("login error");
		return;
	}

	// Just the username uniqueness check to go, we can wake up the session now
	// A freshly de-hibernated session will not have any users, so the last check
	// will never fail in that case.
	SessionState *session = _server->getSessionById(sessionId);
	if(!session) {
		// The session was just deleted from under us! (or de-hibernation failed)
		send("ERROR NOSESSION");
		_client->disconnectError("login error");
		return;
	}

	if(session->getClientByUsername(_client->username())) {
#ifdef NDEBUG
		send("ERROR NAMEINUSE");
		_client->disconnectError("login error");
		return;
#else
		// Allow identical usernames in debug builds, so I don't have to keep changing
		// the username when testing. There is no technical requirement for unique usernames;
		// the limitation is solely for the benefit of the human users.
		logger::warning() << "Username clash" << _client->username() << "for" << session << "ignored because this is a debug build.";
#endif
	}

	// Ok, join the session
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
	_state = WAIT_FOR_IDENT;
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
