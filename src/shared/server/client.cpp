/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include <QTcpSocket>
#include <QStringList>

#include "server.h"
#include "client.h"
#include "exceptions.h"

#include "../net/messagequeue.h"
#include "../net/login.h"
#include "../net/snapshot.h"

namespace server {

using protocol::MessagePtr;

Client::Client(Server *server, QTcpSocket *socket)
	: QObject(server),
	  _server(server),
	  _socket(socket),
	  _state(LOGIN), _substate(0),
	  _id(0)
{
	_msgqueue = new protocol::MessageQueue(socket, this);

	connect(_socket, SIGNAL(disconnected()), this, SLOT(socketDisconnect()));
	connect(_msgqueue, SIGNAL(messageAvailable()), this, SLOT(receiveMessages()));
	connect(_msgqueue, SIGNAL(badData(int,int)), this, SLOT(gotBadData(int,int)));

	// Client just connected, start by saying hello
	_msgqueue->send(MessagePtr(new protocol::Login(QString("DRAWPILE 0"))));
	// No password protection (TODO), so we are expecting a HOST/JOIN command
	_substate = 1;
}

Client::~Client()
{
	delete _socket;
}

QHostAddress Client::peerAddress() const
{
	return _socket->peerAddress();
}

void Client::receiveMessages()
{
	while(_msgqueue->isPending()) {
		MessagePtr msg = _msgqueue->getPending();

		switch(_state) {
		case LOGIN:
			handleLoginMessage(msg.cast<protocol::Login>());
			break;
		case WAIT_FOR_SYNC:
		case IN_SESSION:
			break;
		}
	}
}

void Client::gotBadData(int len, int type)
{
	_server->printError(QString("Received unknown message type #%1 of length %2 from %3").arg(type).arg(len).arg(peerAddress().toString()));
	_socket->disconnect();
}

void Client::socketDisconnect()
{
	emit disconnected(this);
}

/**
 * @brief Handle the login state
 *
 * The login process goes like this:
 * Server sends "DRAWPILE <version> [PASS]"
 * If password is required (PASS):
 * 1. Client responds with the password
 * Server sends BADPASS and disconnects or OK
 *
 * The client responds to OK (or initial hello) with "HOST ***" or "JOIN ***"
 * Server responds with BADNAME, NOSESSION, CLOSED or OK <userid>
 *
 * @param loginmsg login message
 */
void Client::handleLoginMessage(const protocol::Login &loginmsg)
{
	QString msg = loginmsg.message();
	QByteArray errormsg = "WHAT?";

	// TODO password protection
	try {
		switch(_substate) {
		case 0: /* expecting a password */
			// TODO
			break;
		case 1: /* expecting HOST or JOIN */
			if(msg.startsWith("HOST ")) {
				handleHostSession(msg);
				return;
			} else if(msg.startsWith("JOIN ")) {
				handleJoinSession(msg);
				return;
			}
		}
	} catch(const ProtocolViolation &pv) {
		errormsg = pv.what();
	}

	// Unexpected input
	_server->printError(QString("Error (%1) during login from %2").arg(QString(errormsg)).arg(peerAddress().toString()));
	_msgqueue->send(MessagePtr(new protocol::Login(errormsg)));
	_msgqueue->closeWhenReady();
}

void Client::handleHostSession(const QString &msg)
{
	// Cannot host if session has already started
	if(_server->isSessionStarted())
		throw ProtocolViolation("CLOSED");

	// Parse and validate command
	// Expected form is "HOST <version> <userid> <username>"
	QStringList tokens = msg.split(' ', QString::SkipEmptyParts);
	if(tokens.length() < 4)
		throw ProtocolViolation("WHAT?");

	bool ok;
	int minorVersion = tokens[1].toInt(&ok);
	if(!ok)
		throw ProtocolViolation("WHAT?");

	int userid = tokens[2].toInt(&ok);
	if(!ok || userid<1 || userid>255)
		throw ProtocolViolation("WHAT?");

	QString username = QStringList(tokens.mid(3)).join(' ');
	if(!validateUsername(username))
		throw ProtocolViolation("BADNAME");

	_id = userid;
	_username = username;
	// TODO set minor version
	emit loggedin(this);

	_msgqueue->send(MessagePtr(new protocol::Login(QString("OK %1").arg(_id))));
	_state = IN_SESSION;

	// Send request for initial state
	_msgqueue->send(MessagePtr(new protocol::SnapshotMode(protocol::SnapshotMode::REQUEST)));
}

void Client::handleJoinSession(const QString &msg)
{
	// Cannot join a session that hasn't started yet
	if(!_server->isSessionStarted())
		throw ProtocolViolation("NOSESSION");

	// Parse and validate command
	// Expected form is "JOIN <username>"
	QString username = msg.mid(msg.indexOf(' ') + 1).trimmed();
	if(!validateUsername(username))
		throw ProtocolViolation("BADNAME");

	_username = username;
	emit loggedin(this);
	// the server should have assigned the ID in response to the signal
	Q_ASSERT(_id>0);

	_msgqueue->send(MessagePtr(new protocol::Login(QString("OK %1").arg(_id))));
	_state = IN_SESSION;
}

bool Client::validateUsername(const QString &username)
{
	if(username.isEmpty())
		return false;

	// TODO check for duplicates
	return true;
}

}
