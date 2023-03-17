/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#include "libserver/client.h"
#include "libserver/session.h"
#include "libserver/sessionhistory.h"
#include "libserver/serverlog.h"
#include "libserver/serverconfig.h"

#include "libshared/net/messagequeue.h"
#include "libshared/net/control.h"
#include "libshared/net/meta.h"

#include <QSslSocket>
#include <QStringList>
#include <QPointer>

namespace server {

using protocol::MessagePtr;

struct Client::Private {
	QPointer<Session> session;
	QTcpSocket *socket;
	ServerLog *logger;

	protocol::MessageQueue *msgqueue;
	protocol::MessageList holdqueue;

	QString username;
	QString authId;
	QByteArray avatar;
	QStringList flags;

	qint64 lastActive = 0;

	uint8_t id = 0;
	bool isOperator = false;
	bool isModerator = false;
	bool isTrusted = false;
	bool isAuthenticated = false;
	bool isMuted = false;
	bool isHoldLocked = false;
	bool isAwaitingReset = false;

	Private(QTcpSocket *socket_, ServerLog *logger_)
		: socket(socket_), logger(logger_)
	{
		Q_ASSERT(socket);
		Q_ASSERT(logger);
	}
};

Client::Client(QTcpSocket *socket, ServerLog *logger, QObject *parent)
	: QObject(parent), d(new Private(socket, logger))
{
	d->msgqueue = new protocol::MessageQueue(socket, this);
	d->socket->setParent(this);

	connect(d->socket, &QAbstractSocket::disconnected, this, &Client::socketDisconnect);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	connect(d->socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &Client::socketError);
#else
	connect(d->socket, &QAbstractSocket::errorOccurred, this, &Client::socketError);
#endif
	connect(d->msgqueue, &protocol::MessageQueue::messageAvailable, this, &Client::receiveMessages);
	connect(d->msgqueue, &protocol::MessageQueue::badData, this, &Client::gotBadData);
}

Client::~Client()
{
	delete d;
}

protocol::MessageQueue *Client::messageQueue()
{
	return d->msgqueue;
}

protocol::MessagePtr Client::joinMessage() const
{
	return protocol::MessagePtr(new protocol::UserJoin(
			id(),
			(isAuthenticated() ? protocol::UserJoin::FLAG_AUTH : 0) | (isModerator() ? protocol::UserJoin::FLAG_MOD : 0),
			username(),
			avatar()
	));
}

QJsonObject Client::description(bool includeSession) const
{
	QJsonObject u;
	u["id"] = id();
	u["name"] = username();
	u["ip"] = peerAddress().toString();
	u["lastActive"] = QDateTime::fromMSecsSinceEpoch(d->lastActive, Qt::UTC).toString(Qt::ISODate);
	u["auth"] = isAuthenticated();
	u["op"] = isOperator();
	u["muted"] = isMuted();
	u["mod"] = isModerator();
	u["tls"] = isSecure();
	if(includeSession && d->session)
		u["session"] = d->session->id();
	return u;
}

JsonApiResult Client::callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty())
		return JsonApiNotFound();

	if(method == JsonApiMethod::Delete) {
		disconnectClient(Client::DisconnectionReason::Kick, "server operator");
		QJsonObject o;
		o["status"] = "ok";
		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};

	} else if(method == JsonApiMethod::Update) {
		QString msg = request["message"].toString();
		if(!msg.isEmpty())
			sendSystemChat(msg);

		if(request.contains("op")) {
			const bool op = request["op"].toBool();
			if(d->isOperator != op && d->session) {
				d->session->changeOpStatus(id(), op, "the server administrator");
			}
		}

		if(request.contains("trusted")) {
			const bool trusted = request["trusted"].toBool();
			if(d->isTrusted != trusted && d->session) {
				d->session->changeTrustedStatus(id(), trusted, "the server administrator");
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
	d->session = session;
}

Session *Client::session()
{
	return d->session.data();
}

void Client::setId(uint8_t id)
{
	Q_ASSERT(d->id==0 && id != 0); // ID is only assigned once
	d->id = id;
}

uint8_t Client::id() const
{
	return d->id;
}

void Client::setAuthFlags(const QStringList &flags)
{
	d->flags = flags;
}

QStringList Client::authFlags() const
{
	return d->flags;
}

void Client::setUsername(const QString &username)
{
	d->username = username;
}

const QString &Client::username() const
{
	return d->username;
}

void Client::setAvatar(const QByteArray &avatar)
{
	d->avatar = avatar;
}

const QByteArray &Client::avatar() const
{
	return d->avatar;
}

const QString &Client::authId() const
{
	return d->authId;
}

void Client::setAuthId(const QString &authId)
{
	d->authId = authId;
}

void Client::setOperator(bool op)
{
	d->isOperator = op;
}

bool Client::isOperator() const
{
	return d->isOperator || d->isModerator;
}

bool Client::isDeputy() const
{
	return !isOperator()
		&& isTrusted()
		&& d->session
		&& d->session->history()->hasFlag(SessionHistory::Deputies);
}

void Client::setModerator(bool mod)
{
	d->isModerator = mod;
}

bool Client::isModerator() const
{
	return d->isModerator;
}

bool Client::isTrusted() const
{
	return d->isTrusted;
}

void Client::setTrusted(bool trusted)
{
	d->isTrusted = trusted;
}

bool Client::isAuthenticated() const
{
	return! d->authId.isEmpty();
}

void Client::setMuted(bool m)
{
	d->isMuted = m;
}

bool Client::isMuted() const
{
	return d->isMuted;
}

void Client::setConnectionTimeout(int timeout)
{
	d->msgqueue->setIdleTimeout(timeout);
}

#ifndef NDEBUG
void Client::setRandomLag(uint lag)
{
	d->msgqueue->setRandomLag(lag);
}
#endif

QHostAddress Client::peerAddress() const
{
	return d->socket->peerAddress();
}

void Client::sendDirectMessage(protocol::MessagePtr msg)
{
	if(!d->isAwaitingReset || msg->isControl())
		d->msgqueue->send(msg);
}

void Client::sendDirectMessage(const protocol::MessageList &msgs)
{
	if(d->isAwaitingReset) {
		for(MessagePtr msg : msgs)
			if(msg->isControl())
				d->msgqueue->send(msg);
	} else {
		d->msgqueue->send(msgs);
	}
}

void Client::sendSystemChat(const QString &message)
{
	protocol::ServerReply msg {
		protocol::ServerReply::MESSAGE,
		message,
		QJsonObject()
	};

	d->msgqueue->send(MessagePtr(new protocol::Command(0, msg.toJson())));
}

void Client::receiveMessages()
{
	while(d->msgqueue->isPending()) {
		MessagePtr msg = d->msgqueue->getPending();

		d->lastActive = QDateTime::currentMSecsSinceEpoch();

		if(d->session.isNull()) {
			// No session? We must be in the login phase
			if(msg->type() == protocol::MSG_COMMAND)
				emit loginMessage(msg);
			else
				log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(
					QString("Got non-login message (type=%1) in login state").arg(msg->type())
					));

		} else {

			// Enforce origin ID, except when receiving a snapshot
			if(d->session->initUserId() != d->id)
				msg->setContextId(d->id);

			if(isHoldLocked())
				d->holdqueue << msg;
			else
				d->session->handleClientMessage(*this, msg);
		}
	}
}

void Client::gotBadData(int len, int type)
{
	log(Log().about(Log::Level::Warn, Log::Topic::RuleBreak).message(
		QString("Received unknown message type %1 of length %2").arg(type).arg(len)
		));
	d->socket->abort();
}

void Client::socketError(QAbstractSocket::SocketError error)
{
	if(error != QAbstractSocket::RemoteHostClosedError) {
		log(Log().about(Log::Level::Warn, Log::Topic::Status).message("Socket error: " + d->socket->errorString()));
		d->socket->abort();
	}
}

void Client::socketDisconnect()
{
	emit loggedOff(this);
	this->deleteLater();
}

void Client::disconnectClient(DisconnectionReason reason, const QString &message)
{
	protocol::Disconnect::Reason pr { protocol::Disconnect::OTHER };
	Log::Topic topic { Log::Topic::Leave };
	switch(reason) {
	case DisconnectionReason::Kick:
		pr = protocol::Disconnect::KICK;
		topic = Log::Topic::Kick;
		break;
	case DisconnectionReason::Error: pr = protocol::Disconnect::ERROR; break;
	case DisconnectionReason::Shutdown: pr = protocol::Disconnect::SHUTDOWN; break;
	}
	log(Log()
		.about(Log::Level::Info, topic)
		.message(message)
		);

	emit loggedOff(this);
	d->msgqueue->sendDisconnect(pr, message);
}

void Client::setHoldLocked(bool lock)
{
	d->isHoldLocked = lock;
	if(!lock) {
		for(MessagePtr msg : d->holdqueue)
			d->session->handleClientMessage(*this, msg);
		d->holdqueue.clear();
	}
}

bool Client::isHoldLocked() const
{
	return d->isHoldLocked;
}

void Client::setAwaitingReset(bool awaiting)
{
	d->isAwaitingReset = awaiting;
}

bool Client::isAwaitingReset() const
{
	return d->isAwaitingReset;
}

bool Client::hasSslSupport() const
{
	return d->socket->inherits("QSslSocket");
}

bool Client::isSecure() const
{
	QSslSocket *socket = qobject_cast<QSslSocket*>(d->socket);
	return socket && socket->isEncrypted();
}

void Client::startTls()
{
	QSslSocket *socket = qobject_cast<QSslSocket*>(d->socket);
	Q_ASSERT(socket);
	socket->startServerEncryption();
}

void Client::log(Log entry) const
{
	entry.user(d->id, d->socket->peerAddress(), d->username);
	if(d->session)
		d->session->log(entry);
	else
		d->logger->logMessage(entry);
}

Log Client::log() const {
	return Log().user(d->id, d->socket->peerAddress(), d->username);
}


}

