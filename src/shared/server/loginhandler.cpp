/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include "../net/control.h"
#include "../util/logger.h"
#include "../util/passwordhash.h"

#include "config.h"

#include <QStringList>
#include <QRegularExpression>

namespace server {

LoginHandler::LoginHandler(Client *client, SessionServer *server) :
	QObject(client), m_client(client), m_server(server), m_hostPrivilege(false), m_complete(false)
{
	connect(client, &Client::loginMessage, this, &LoginHandler::handleLoginMessage);
	connect(server, &SessionServer::sessionChanged, this, &LoginHandler::announceSession);
	connect(server, &SessionServer::sessionEnded, this, &LoginHandler::announceSessionEnd);
}

void LoginHandler::startLoginProcess()
{
	protocol::ServerReply greeting;
	greeting.type = protocol::ServerReply::LOGIN;
	greeting.message = "Drawpile server " DRAWPILE_PROTO_STR;
	greeting.reply["version"] = DRAWPILE_PROTO_SERVER_VERSION;

	QJsonArray flags;

	if(m_server->sessionLimit()>1)
		flags << "MULTI";
	if(!m_server->hostPassword().isEmpty())
		flags << "HOSTP";
	if(m_server->allowPersistentSessions())
		flags << "PERSIST";
	if(m_client->hasSslSupport())
		flags << "TLS";
	if(m_server->mustSecure() && m_client->hasSslSupport()) {
		flags << "SECURE";
		m_state = WAIT_FOR_SECURE;
	}
	if(m_server->identityManager()) {
		flags << "IDENT";
		if(m_server->identityManager()->isAuthorizedOnly())
			flags << "NOGUEST";
	}

	greeting.reply["flags"] = flags;

	// Start by telling who we are
	m_state = WAIT_FOR_IDENT;
	send(greeting);

	// Client should disconnect upon receiving the above if the version number does not match
}

QJsonObject sessionDescription(const SessionDescription &session)
{
	Q_ASSERT(!session.id.isEmpty());
	QJsonObject o;
	o["id"] = session.id.id();
	o["protocol"] = session.protocolVersion;
	o["users"] = session.userCount;
	o["founder"] = session.founder;
	o["title"] = session.title;
	if(!session.passwordHash.isEmpty())
		o["password"] = true;
	if(session.closed)
		o["closed"] = true;
	if(session.persistent)
		o["persistent"] = true;
	//if(session.hibernating) // not currently implemented in this server
		//o["asleep"] = true;

	return o;
}

void LoginHandler::announceServerInfo()
{
	protocol::ServerReply greeting;
	greeting.type = protocol::ServerReply::LOGIN;
	greeting.message = "Welcome";

	const QList<SessionDescription> sessions = m_server->sessions();

	QJsonArray s;
	for(const SessionDescription &session : sessions) {
		s << sessionDescription(session);
	}

	greeting.reply["title"] = m_server->title();
	greeting.reply["sessions"] = s;
	send(greeting);
}

void LoginHandler::announceSession(const SessionDescription &session)
{
	if(m_state != WAIT_FOR_LOGIN)
		return;

	protocol::ServerReply greeting;
	greeting.type = protocol::ServerReply::LOGIN;
	greeting.message = "New session";

	QJsonArray s;
	s << sessionDescription(session);
	greeting.reply["sessions"] = s;

	send(greeting);
}

void LoginHandler::announceSessionEnd(const QString &id)
{
	if(m_state != WAIT_FOR_LOGIN)
		return;

	protocol::ServerReply greeting;
	greeting.type = protocol::ServerReply::LOGIN;
	greeting.message = "Session ended";

	QJsonArray s;
	s << id;
	greeting.reply["remove"] = s;

	send(greeting);
}

void LoginHandler::handleLoginMessage(protocol::MessagePtr msg)
{
	if(msg->type() != protocol::MSG_COMMAND) {
		logger::error() << "login handler was passed a non-login message!";
		return;
	}

	protocol::ServerCommand cmd = msg.cast<protocol::Command>().cmd();

	if(m_state == WAIT_FOR_SECURE) {
		// Secure mode: wait for STARTTLS before doing anything
		if(cmd.cmd == "startTls") {
			handleStarttls();
		} else {
			logger::notice() << m_client << "Didn't secure connection!";
			sendError("tlsRequired", "TLS required");
		}

	} else if(m_state == WAIT_FOR_IDENT) {
		// Wait for user identification before moving on to session listing
		if(cmd.cmd == "startTls") {
			handleStarttls();
		} else if(cmd.cmd == "ident") {
			handleIdentMessage(cmd);
		} else {
			logger::notice() << m_client << "Invalid login message";
			m_client->disconnectError("invalid message");
		}

	} else if(m_state == WAIT_FOR_IDENTITYMANAGER_REPLY) {
		// Client shouldn't shouldn't send anything in this state
		logger::notice() << m_client << "Got login message while waiting for identity manager reply";
		m_client->disconnectError("unexpected message");

	} else {
		if(cmd.cmd == "host") {
			handleHostMessage(cmd);
		} else if(cmd.cmd == "join") {
			handleJoinMessage(cmd);
		} else {
			logger::notice() << m_client << "Got invalid login message";
			m_client->disconnectError("invalid message");
		}
	}
}

void LoginHandler::handleIdentMessage(const protocol::ServerCommand &cmd)
{
	QString username, password;
	if(cmd.args.size()!=1 && cmd.args.size()!=2) {
		sendError("syntax", "Expected username and (optional) password");
		return;
	}

	username = cmd.args[0].toString();
	if(cmd.args.size()>1)
		password = cmd.args[1].toString();

	if(!validateUsername(username)) {
		sendError("badUsername", "Invalid username");
		return;
	}

	if(m_server->identityManager()) {
		m_state = WAIT_FOR_IDENTITYMANAGER_REPLY;
		IdentityResult *result = m_server->identityManager()->checkLogin(username, password);
		connect(result, &IdentityResult::resultAvailable, [this, username, password](IdentityResult *result) {
			QString errorcode, errorstr;
			Q_ASSERT(result->status() != IdentityResult::INPROGRESS);
			switch(result->status()) {
			case IdentityResult::INPROGRESS: /* can't happen */ break;
			case IdentityResult::NOTFOUND:
				if(!m_server->identityManager()->isAuthorizedOnly()) {
					guestLogin(username);
					break;
				}
				// fall through to badpass if guest logins are disabled
			case IdentityResult::BADPASS:
				if(password.isEmpty()) {
					// No password: tell client that guest login is not possible (for this username)
					m_state = WAIT_FOR_IDENT;

					protocol::ServerReply identReply;
					identReply.type = protocol::ServerReply::RESULT;
					identReply.message = "Password needed";
					identReply.reply["state"] = "needPassword";
					send(identReply);

					return;
				}
				errorcode = "badPassword";
				errorstr = "Incorrect password";
				break;

			case IdentityResult::BANNED:
				errorcode = "banned";
				errorstr = "This username is banned";
				break;

			case IdentityResult::OK: {
				// Yay, username and password were valid!
				if(validateUsername(result->canonicalName())) {
					m_client->setUsername(result->canonicalName());

				} else {
					logger::warning() << "Identity manager gave us an invalid username:" << result->canonicalName();
					m_client->setUsername(username);
				}

				protocol::ServerReply identReply;
				identReply.type = protocol::ServerReply::RESULT;
				identReply.message = "Authenticated login OK!";
				identReply.reply["state"] = "identOk";
				identReply.reply["flags"] = QJsonArray::fromStringList(result->flags());
				identReply.reply["ident"] = m_client->username();
				identReply.reply["guest"] = false;

				m_client->setAuthenticated(true);
				m_client->setModerator(result->flags().contains("MOD"));
				m_hostPrivilege = result->flags().contains("HOST");
				m_state = WAIT_FOR_LOGIN;

				send(identReply);
				announceServerInfo();
				} break;
			}

			if(!errorcode.isEmpty()) {
				sendError(errorcode, errorstr);
			}
		});

	} else {
		if(!password.isNull()) {
			// if we have no identity manager, we can't accept passwords
			sendError("noIdent", "This is a guest-only server");
			return;
		}
		guestLogin(username);
	}
}

void LoginHandler::guestLogin(const QString &username)
{
	if(m_server->identityManager() && m_server->identityManager()->isAuthorizedOnly()) {
		sendError("noGuest", "Guest logins not allowed");
		return;
	}

	m_client->setUsername(username);
	m_state = WAIT_FOR_LOGIN;
	
	protocol::ServerReply identReply;
	identReply.type = protocol::ServerReply::RESULT;
	identReply.message = "Guest login OK!";
	identReply.reply["state"] = "identOk";
	identReply.reply["flags"] = QJsonArray();
	identReply.reply["ident"] = m_client->username();
	identReply.reply["guest"] = true;
	send(identReply);

	announceServerInfo();
}

void LoginHandler::handleHostMessage(const protocol::ServerCommand &cmd)
{
	Q_ASSERT(!m_client->username().isEmpty());

	if(m_server->sessionCount() >= m_server->sessionLimit()) {
		sendError("closed", "This server is full");
		return;
	}

	QString sessionIdString = cmd.kwargs.value("id").toString();
	QString protocolVersion = cmd.kwargs.value("protocol").toString();
	int userId = cmd.kwargs.value("user_id").toInt();

	if(userId < 1 || userId>254) {
		sendError("syntax", "Invalid user ID (must be in range 1-254)");
		return;
	}

	// Check if session ID is available
	SessionId sessionId;
	if(sessionIdString.isEmpty())
		sessionId = SessionId::randomId();
	else
		sessionId = SessionId::customId(sessionIdString);

	if(!m_server->getSessionDescriptionById(sessionId).id.isEmpty()) {
		sendError("idInUse", "This session ID is already in use");
		return;
	}

	QString host_password = cmd.kwargs.value("host_password").toString();
	if(host_password != m_server->hostPassword() && !m_hostPrivilege) {
		sendError("badPassword", "Incorrect hosting password");
		return;
	}

	m_client->setId(userId);

	// Mark login phase as complete. No more login messages will be sent to this user
	protocol::ServerReply reply;
	reply.type = protocol::ServerReply::RESULT;
	reply.message = "Starting new session!";
	reply.reply["state"] = "host";
	QJsonObject joinInfo;
	joinInfo["id"] = sessionId.id();
	joinInfo["user"] = userId;
	reply.reply["join"] = joinInfo;
	send(reply);

	m_complete = true;

	// Create a new session
	Session *session = m_server->createSession(sessionId, protocolVersion, m_client->username());

	session->joinUser(m_client, true);

	deleteLater();
}

void LoginHandler::handleJoinMessage(const protocol::ServerCommand &cmd)
{
	Q_ASSERT(!m_client->username().isEmpty());
	if(cmd.args.size()!=1) {
		sendError("syntax", "Expected session ID");
		return;
	}

	QString sessionId = cmd.args.at(0).toString();
	SessionDescription sessiondesc = m_server->getSessionDescriptionById(sessionId);
	if(sessiondesc.id==0) {
		sendError("notFound", "Session not found!");
		return;
	}

	if(sessiondesc.closed && !m_client->isModerator()) {
		sendError("closed", "This session is closed");
		return;
	}

	QString password = cmd.kwargs.value("password").toString();

	if(!passwordhash::check(password, sessiondesc.passwordHash) && !m_client->isModerator()) {
		sendError("badPassword", "Incorrect password");
		return;
	}

	// Just the username uniqueness check to go, we can wake up the session now.
	// A freshly de-hibernated session will not have any users, so the last check
	// will never fail in that case.
	Session *session = m_server->getSessionById(sessionId);
	if(!session) {
		// The session was just deleted from under us! (or de-hibernation failed)
		sendError("notFound", "Session just went missing!");
		return;
	}

	if(session->getClientByUsername(m_client->username())) {
#ifdef NDEBUG
		sendError("nameInuse", "This username is already in use");
		return;
#else
		// Allow identical usernames in debug builds, so I don't have to keep changing
		// the username when testing. There is no technical requirement for unique usernames;
		// the limitation is solely for the benefit of the human users.
		logger::warning() << "Username clash" << m_client->username() << "for" << session << "ignored because this is a debug build.";
#endif
	}

	// Ok, join the session
	session->assignId(m_client);

	protocol::ServerReply reply;
	reply.type = protocol::ServerReply::RESULT;
	reply.message = "Joining a session!";
	reply.reply["state"] = "join";
	QJsonObject joinInfo;
	joinInfo["id"] = session->id().id();
	joinInfo["user"] = m_client->id();
	reply.reply["join"] = joinInfo;
	send(reply);

	m_complete = true;

	session->joinUser(m_client, false);

	deleteLater();
}

void LoginHandler::handleStarttls()
{
	if(!m_client->hasSslSupport()) {
		// Note. Well behaved clients shouldn't send STARTTLS if TLS was not listed in server features.
		sendError("noTls", "TLS not supported");
		return;
	}

	if(m_client->isSecure()) {
		sendError("alreadySecure", "Connection already secured"); // shouldn't happen normally
		return;
	}

	protocol::ServerReply reply;
	reply.type = protocol::ServerReply::LOGIN;
	reply.message = "Start TLS now!";
	reply.reply["startTls"] = true;
	send(reply);

	m_client->startTls();
	m_state = WAIT_FOR_IDENT;
}

void LoginHandler::send(const protocol::ServerReply &cmd)
{
	if(!m_complete)
		m_client->sendDirectMessage(protocol::MessagePtr(new protocol::Command(0, cmd)));
}

void LoginHandler::sendError(const QString &code, const QString &message)
{
	protocol::ServerReply r;
	r.type = protocol::ServerReply::ERROR;
	r.message = message;
	r.reply["code"] = code;
	send(r);
	m_client->disconnectError("Login error");
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
