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

#include "client.h"
#include "session.h"
#include "sessionhistory.h"
#include "opcommands.h"
#include "serverlog.h"
#include "serverconfig.h"

#include "../net/messagequeue.h"
#include "../net/control.h"
#include "../net/meta.h"

#include <QSslSocket>
#include <QStringList>

namespace server {

using protocol::MessagePtr;

Client::Client(QTcpSocket *socket, ServerLog *logger, QObject *parent)
	: QObject(parent),
	  m_session(nullptr),
	  m_socket(socket),
	  m_logger(logger),
	  m_historyPosition(-1),
	  m_id(0),
	  m_isOperator(false),
	  m_isModerator(false),
	  m_isAuthenticated(false),
	  m_isMuted(false)
{
	Q_ASSERT(socket);
	Q_ASSERT(logger);

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

QJsonObject Client::description(bool includeSession) const
{
	QJsonObject u;
	u["id"] = id();
	u["name"] = username();
	u["ip"] = peerAddress().toString();
	u["auth"] = isAuthenticated();
	u["op"] = isOperator();
	u["muted"] = isMuted();
	u["mod"] = isModerator();
	u["tls"] = isSecure();
	if(includeSession && m_session)
		u["session"] = m_session->idString();
	return u;
}

JsonApiResult Client::callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty())
		return JsonApiNotFound();

	if(method == JsonApiMethod::Delete) {
		disconnectKick("server operator");
		QJsonObject o;
		o["status"] = "ok";
		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};

	} else if(method == JsonApiMethod::Update) {
		QString msg = request["message"].toString();
		if(!msg.isEmpty())
			sendSystemChat(msg);

		if(request.contains("op")) {
			const bool op = request["op"].toBool();
			if(m_isOperator != op && m_session) {
				m_session->changeOpStatus(id(), op, "the server administrator");
			}
		}
		return JsonApiResult { JsonApiResult::Ok, QJsonDocument(description()) };

	} else if(method == JsonApiMethod::Get) {
		return JsonApiResult { JsonApiResult::Ok, QJsonDocument(description()) };

	} else {
		return JsonApiBadMethod();
	}
}

void Client::setSession(Session *session)
{
	Q_ASSERT(session);
	m_session = session;
	m_historyPosition = -1;

	// Enqueue the next batch (if available) when upload queue is empty
	connect(m_msgqueue, &protocol::MessageQueue::allSent, this, &Client::sendNextHistoryBatch);
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

void Client::sendNextHistoryBatch()
{
	// Only enqueue messages for uploading when upload queue is empty
	// and session is in a normal running state.
	// (We'll get another messagesAvailable signal when ready)
	if(m_session == nullptr || m_msgqueue->isUploading() || m_session->state() != Session::Running)
		return;

	m_session->historyCacheCleanup();

	QList<protocol::MessagePtr> batch;
	int batchLast;
	std::tie(batch, batchLast) = m_session->history()->getBatch(m_historyPosition);
	m_historyPosition = batchLast;
	m_msgqueue->send(batch);
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

		if(m_session == nullptr) {
			// No session? We must be in the login phase
			if(msg->type() == protocol::MSG_COMMAND)
				emit loginMessage(msg);
			else
				log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(
					QString("Got non-login message (type=%1) in login state").arg(msg->type())
					));

		} else {
			handleSessionMessage(msg);
		}
	}
}

void Client::gotBadData(int len, int type)
{
	log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(
		QString("Received unknown message type %1 of length %2").arg(type).arg(len)
		));
	m_socket->abort();
}

void Client::socketError(QAbstractSocket::SocketError error)
{
	if(error != QAbstractSocket::RemoteHostClosedError) {
		log(Log().about(Log::Level::Warn, Log::Topic::Status).message("Socket error: " + m_socket->errorString()));
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
		log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message("Received server-to-user only command " + msg->messageName()));
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
				log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message("Tried to change session ownership"));
				return;
			}

			QList<uint8_t> ids = msg.cast<protocol::SessionOwner>().ids();
			ids.append(m_id);
			ids = m_session->updateOwnership(ids, username());
			msg.cast<protocol::SessionOwner>().setIds(ids);
			break;
		}
		case protocol::MSG_CHAT: {
			if(isMuted())
				return;
			if(msg.cast<protocol::Chat>().isBypass()) {
				m_session->directToAll(msg);
				return;
			}
		}
		default: break;
	}

	// Rest of the messages are added to session history
	if(m_session->initUserId() == m_id)
		m_session->addToInitStream(msg);
	else if(isHoldLocked()) {
		if(!m_session->history()->isOutOfSpace())
			m_holdqueue.append(msg);
	}
	else
		m_session->addToHistory(msg);
}

void Client::disconnectKick(const QString &kickedBy)
{
	emit loggedOff(this);
	log(Log().about(Log::Level::Info, Log::Topic::Kick).message("Kicked by " + kickedBy));
	m_msgqueue->sendDisconnect(protocol::Disconnect::KICK, kickedBy);
}

void Client::disconnectError(const QString &message)
{
	emit loggedOff(this);
	log(Log().about(Log::Level::Warn, Log::Topic::Leave).message("Disconnected due to error: " + message));
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
		m_session->addToHistory(msg);
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

void Client::log(Log entry) const
{
	entry.user(m_id, m_socket->peerAddress(), m_username);
	if(m_session)
		m_session->log(entry);
	else
		m_logger->logMessage(entry);
}

}
