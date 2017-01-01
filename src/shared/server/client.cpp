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

#include "config.h"

#include "client.h"
#include "session.h"
#include "opcommands.h"

#include "../net/messagequeue.h"
#include "../net/control.h"
#include "../net/meta.h"
#include "../util/logger.h"

#include <QTcpSocket>
#include <QSslSocket>
#include <QStringList>

namespace server {

using protocol::MessagePtr;

Client::Client(QTcpSocket *socket, QObject *parent)
	: QObject(parent),
	  m_state(LOGIN),
	  m_session(nullptr),
	  m_socket(socket),
	  m_streampointer(0),
	  m_id(0),
	  m_isOperator(false),
	  m_isModerator(false),
	  m_isAuthenticated(false)
{
	m_msgqueue = new protocol::MessageQueue(socket, this);

	m_socket->setParent(this);

	connect(m_socket, &QAbstractSocket::disconnected, this, &Client::socketDisconnect);
	connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));
	connect(m_msgqueue, &protocol::MessageQueue::messageAvailable, this, &Client::receiveMessages);
	connect(m_msgqueue, &protocol::MessageQueue::badData, this, &Client::gotBadData);
}

Client::~Client()
{
}

protocol::MessagePtr Client::joinMessage() const
{
	return protocol::MessagePtr(new protocol::UserJoin(
			id(),
			(isAuthenticated() ? protocol::UserJoin::FLAG_AUTH : 0) | (isModerator() ? protocol::UserJoin::FLAG_MOD : 0),
			username()
	));
}

QString Client::toLogString() const {
	if(m_session)
		return QStringLiteral("#%1 [%2] %3@%4:").arg(QString::number(id()), peerAddress().toString(), username(), m_session->id().toString());
	else
		return QStringLiteral("[%1] %2@(lobby):").arg(peerAddress().toString(), username());
}

void Client::setSession(Session *session)
{
	m_session = session;

	m_state = IN_SESSION;
	m_streampointer = m_session->mainstream().offset();
}

void Client::setConnectionTimeout(int timeout)
{
	m_msgqueue->setIdleTimeout(timeout);
}

#ifndef NDEBUG
void Client::setRandomLag(uint lag)
{
	m_msgqueue->setRandomLag(lag);
}
#endif

QHostAddress Client::peerAddress() const
{
	return m_socket->peerAddress();
}

void Client::sendAvailableCommands()
{
	Q_ASSERT(m_state == IN_SESSION);

	// If we're at the moment initializing the session, skip any drawing
	// commands, because we just sent them.
	bool skipCommands = m_session->initUserId() == m_id;

	while(m_streampointer < m_session->mainstream().end()) {
		MessagePtr m = m_session->mainstream().at(m_streampointer++);
		if(!skipCommands || !m->isCommand())
			m_msgqueue->send(m);
	}
}

void Client::sendDirectMessage(protocol::MessagePtr msg)
{
	m_msgqueue->send(msg);
}

void Client::sendSystemChat(const QString &message)
{
	protocol::ServerReply msg {
		protocol::ServerReply::MESSAGE,
		message,
		QJsonObject()
	};

	m_msgqueue->send(MessagePtr(new protocol::Command(0, msg.toJson())));
}

void Client::receiveMessages()
{
	while(m_msgqueue->isPending()) {
		MessagePtr msg = m_msgqueue->getPending();

		if(m_state == LOGIN) {
			if(msg->type() == protocol::MSG_COMMAND)
				emit loginMessage(msg);
			else
				logger::notice() << this << "Got non-login message (type=" << msg->type() << ") in login state";

		} else {
			handleSessionMessage(msg);
		}
	}
}

void Client::gotBadData(int len, int type)
{
	logger::notice() << this << "Received unknown message type #" << type << "of length" << len;
	m_socket->abort();
}

void Client::socketError(QAbstractSocket::SocketError error)
{
	if(error != QAbstractSocket::RemoteHostClosedError) {
		logger::error() << this << "Socket error" << m_socket->errorString();
		m_socket->abort();
	}
}

void Client::socketDisconnect()
{
	emit loggedOff(this);
	this->deleteLater();
}

/**
 * @brief Handle messages in normal session mode
 *
 * This one is pretty simple. The message is validated to make sure
 * the client is authorized to send it, etc. and it is added to the
 * main message stream, from which it is distributed to all connected clients.
 * @param msg the message received from the client
 */
void Client::handleSessionMessage(MessagePtr msg)
{
	// Filter away server-to-client-only messages
	switch(msg->type()) {
	using namespace protocol;
	case MSG_USER_JOIN:
	case MSG_USER_LEAVE:
	case MSG_STREAMPOS:
		logger::notice() << this << "Got server-to-user only command" << msg->type();
		return;
	case MSG_DISCONNECT:
		// we don't do anything with disconnect notifications from the client
		return;
	default: break;
	}

	// Enforce origin context ID (except when uploading a snapshot)
	if(m_session->initUserId() != m_id)
		msg->setContextId(m_id);

	// Some meta commands affect the server too
	switch(msg->type()) {
		case protocol::MSG_COMMAND: {
			protocol::ServerCommand cmd = msg.cast<protocol::Command>().cmd();
			handleClientServerCommand(this, cmd.cmd, cmd.args, cmd.kwargs);
			return;
		}
		case protocol::MSG_SESSION_OWNER: {
			if(!isOperator()) {
				logger::warning() << this << "tried to change session ownership";
				return;
			}

			QList<uint8_t> ids = msg.cast<protocol::SessionOwner>().ids();
			ids.append(m_id);
			ids = m_session->updateOwnership(ids);
			msg.cast<protocol::SessionOwner>().setIds(ids);
			break;
		}
		default: break;
	}

	// Rest of the messages are added to session history
	if(m_session->initUserId() == m_id)
		m_session->addToInitStream(msg);
	else if(isHoldLocked())
		m_holdqueue.append(msg);
	else
		m_session->addToCommandStream(msg);
}

void Client::disconnectKick(const QString &kickedBy)
{
	emit loggedOff(this);
	logger::info() << this << "Kicked by" << kickedBy;
	m_msgqueue->sendDisconnect(protocol::Disconnect::KICK, kickedBy);
}

void Client::disconnectError(const QString &message)
{
	emit loggedOff(this);
	logger::info() << this << "Disconnecting due to error:" << message;
	m_msgqueue->sendDisconnect(protocol::Disconnect::ERROR, message);
}

void Client::disconnectShutdown()
{
	emit loggedOff(this);
	m_msgqueue->sendDisconnect(protocol::Disconnect::SHUTDOWN, QString());
}

bool Client::isHoldLocked() const
{
	Q_ASSERT(m_session);

	return m_session->state() != Session::Running;
}

void Client::enqueueHeldCommands()
{
	if(isHoldLocked())
		return;

	for(MessagePtr msg : m_holdqueue)
		m_session->addToCommandStream(msg);
	m_holdqueue.clear();
}

bool Client::hasSslSupport() const
{
	return m_socket->inherits("QSslSocket");
}

bool Client::isSecure() const
{
	QSslSocket *socket = qobject_cast<QSslSocket*>(m_socket);
	return socket && socket->isEncrypted();
}

void Client::startTls()
{
	QSslSocket *socket = qobject_cast<QSslSocket*>(m_socket);
	Q_ASSERT(socket);
	socket->startServerEncryption();
}

}
