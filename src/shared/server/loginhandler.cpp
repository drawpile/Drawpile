/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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
#include "serverconfig.h"

#include "../net/control.h"
#include "../util/logger.h"

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
	m_state = WAIT_FOR_IDENT;

	protocol::ServerReply greeting;
	greeting.type = protocol::ServerReply::LOGIN;
	greeting.message = "Drawpile server " DRAWPILE_VERSION;
	greeting.reply["version"] = DRAWPILE_PROTO_SERVER_VERSION;

	QJsonArray flags;

	if(m_server->config()->getConfigInt(config::SessionCountLimit) > 1)
		flags << "MULTI";
	if(!m_server->config()->getConfigString(config::HostPassword).isEmpty())
		flags << "HOSTP";
	if(m_server->config()->getConfigBool(config::EnablePersistence))
		flags << "PERSIST";
	if(m_client->hasSslSupport())
		flags << "TLS";
	if(m_server->mustSecure() && m_client->hasSslSupport()) {
		flags << "SECURE";
		m_state = WAIT_FOR_SECURE;
	}
	if(!m_server->config()->getConfigBool(config::AllowGuests))
		flags << "NOGUEST";

	greeting.reply["flags"] = flags;

	// Start by telling who we are
	send(greeting);

	// Client should disconnect upon receiving the above if the version number does not match
}

void LoginHandler::announceServerInfo()
{
	protocol::ServerReply greeting;
	greeting.type = protocol::ServerReply::LOGIN;
	greeting.message = "Welcome";
	greeting.reply["title"] = m_server->config()->getConfigString(config::ServerTitle);
	// TODO if message length exceeds maximum, split session list into multiple messages
	greeting.reply["sessions"] = m_server->sessionDescriptions();
	send(greeting);
}

void LoginHandler::announceSession(const QJsonObject &session)
{
	Q_ASSERT(session.contains("id"));

	if(m_state != WAIT_FOR_LOGIN)
		return;

	protocol::ServerReply greeting;
	greeting.type = protocol::ServerReply::LOGIN;
	greeting.message = "New session";

	QJsonArray s;
	s << session;
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

	const RegisteredUser userAccount = m_server->config()->getUserAccount(username, password);

	switch(userAccount.status) {
	case RegisteredUser::NotFound:
		if(m_server->config()->getConfigBool(config::AllowGuests)) {
			guestLogin(username);
			return;
		}
		// fall through to badpass if guest logins are disabled

	case RegisteredUser::BadPass:
		if(password.isEmpty()) {
			// No password: tell client that guest login is not possible (for this username)
			m_state = WAIT_FOR_IDENT;

			protocol::ServerReply identReply;
			identReply.type = protocol::ServerReply::RESULT;
			identReply.message = "Password needed";
			identReply.reply["state"] = "needPassword";
			send(identReply);

		} else {
			sendError("badPassword", "Incorrect password");
		}
		return;

	case RegisteredUser::Banned:
		sendError("banned", "This username is banned");
		return;

	case RegisteredUser::Ok: {
		// Yay, username and password were valid!
		m_client->setUsername(username);

		protocol::ServerReply identReply;
		identReply.type = protocol::ServerReply::RESULT;
		identReply.message = "Authenticated login OK!";
		identReply.reply["state"] = "identOk";
		identReply.reply["flags"] = QJsonArray::fromStringList(userAccount.flags);
		identReply.reply["ident"] = m_client->username();
		identReply.reply["guest"] = false;

		m_client->setAuthenticated(true);
		m_client->setModerator(userAccount.flags.contains("MOD"));
		m_hostPrivilege = userAccount.flags.contains("HOST");
		m_state = WAIT_FOR_LOGIN;

		send(identReply);
		announceServerInfo();
		break; }
	}
}

void LoginHandler::guestLogin(const QString &username)
{
	if(!m_server->config()->getConfigBool(config::AllowGuests)) {
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

static bool isValidSessionAlias(const QString &alias)
{
	if(alias.length() > 32 || alias.length() < 1)
		return false;

	for(int i=0;i<alias.length();++i) {
		const QChar c = alias.at(i);
		if(!(
			(c >= 'a' && c<'z') ||
			(c >= 'A' && c<'Z') ||
			(c >= '0' && c<'9') ||
			c=='-'
			))
			return false;
	}

	// To avoid confusion with real session IDs,
	// aliases may not be valid UUIDs.
	if(!QUuid(alias).isNull())
		return false;

	return true;
}

void LoginHandler::handleHostMessage(const protocol::ServerCommand &cmd)
{
	Q_ASSERT(!m_client->username().isEmpty());

	if(m_server->sessionCount() >= m_server->config()->getConfigInt(config::SessionCountLimit)) {
		sendError("closed", "This server is full");
		return;
	}

	QString sessionAlias = cmd.kwargs.value("alias").toString();
	protocol::ProtocolVersion protocolVersion = protocol::ProtocolVersion::fromString(cmd.kwargs.value("protocol").toString());

	if(!protocolVersion.isValid()) {
		sendError("syntax", "Unparseable protocol version");
		return;
	}

	int userId = cmd.kwargs.value("user_id").toInt();

	if(userId < 1 || userId>254) {
		sendError("syntax", "Invalid user ID (must be in range 1-254)");
		return;
	}

	// Check if session alias is available
	if(!sessionAlias.isEmpty()) {
		if(!isValidSessionAlias(sessionAlias)) {
			sendError("idInUse", "Invalid session alias");
			return;
		}
		if(m_server->getSessionById(sessionAlias)) {
			sendError("idInUse", "This session alias is already in use");
			return;
		}
	}

	QString host_password = cmd.kwargs.value("host_password").toString();
	if(host_password != m_server->config()->getConfigString(config::HostPassword) && !m_hostPrivilege) {
		sendError("badPassword", "Incorrect hosting password");
		return;
	}

	m_client->setId(userId);

	// Create a new session
	Session *session = m_server->createSession(QUuid::createUuid(), sessionAlias, protocolVersion, m_client->username());

	// Mark login phase as complete. No more login messages will be sent to this user
	protocol::ServerReply reply;
	reply.type = protocol::ServerReply::RESULT;
	reply.message = "Starting new session!";
	reply.reply["state"] = "host";

	QJsonObject joinInfo;
	joinInfo["id"] = sessionAlias.isEmpty() ? session->idString() : sessionAlias;
	joinInfo["user"] = userId;
	reply.reply["join"] = joinInfo;
	send(reply);

	m_complete = true;
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

	Session *session = m_server->getSessionById(sessionId);
	if(!session) {
		sendError("notFound", "Session not found!");
		return;
	}

	if(!m_client->isModerator()) {
		// Non-moderators have to obey access restrictions
		if(session->isClosed()) {
			sendError("closed", "This session is closed");
			return;
		}

		if(!session->checkPassword(cmd.kwargs.value("password").toString())) {
			sendError("badPassword", "Incorrect password");
			return;
		}
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
	joinInfo["id"] = session->idAlias().isEmpty() ? session->idString() : session->idAlias();
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

	// Note: the length check is not for any technical limitation, just to
	// make sure people don't use annoyinly long names. QString::length()
	// returns the number of UTF-16 QChars in the string, which might
	// not correspond to the actual number of logical characters, but
	// it should be close enough for this use case.
	if(username.length() > 22)
		return false;

	if(username.contains('"'))
		return false;

	return true;
}

}
