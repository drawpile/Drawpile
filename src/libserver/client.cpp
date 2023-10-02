// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/client.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/session.h"
#include "libserver/sessionhistory.h"
#include "libshared/net/messagequeue.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/qtcompat.h"
#include <QPointer>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QStringList>
#include <QTimer>

namespace server {

struct Client::Private {
	QPointer<Session> session;
	QTcpSocket *socket;
	ServerLog *logger;

	net::MessageQueue *msgqueue;
	net::MessageList holdqueue;

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
	bool isBanTriggered = false;
	BanResult ban = BanResult::notBanned();

	Private(QTcpSocket *socket_, ServerLog *logger_)
		: socket(socket_)
		, logger(logger_)
	{
		Q_ASSERT(socket);
		Q_ASSERT(logger);
	}
};

Client::Client(
	QTcpSocket *socket, ServerLog *logger, bool decodeOpaque, QObject *parent)
	: QObject(parent)
	, d(new Private(socket, logger))
{
	d->msgqueue = new net::MessageQueue(socket, decodeOpaque, this);
	d->socket->setParent(this);

	connect(
		d->socket, &QAbstractSocket::disconnected, this,
		&Client::socketDisconnect);
	connect(d->socket, compat::SocketError, this, &Client::socketError);
	connect(
		d->msgqueue, &net::MessageQueue::messageAvailable, this,
		&Client::receiveMessages);
	connect(
		d->msgqueue, &net::MessageQueue::badData, this, &Client::gotBadData);
}

Client::~Client()
{
	delete d;
}

net::MessageQueue *Client::messageQueue()
{
	return d->msgqueue;
}

net::Message Client::joinMessage() const
{
	uint8_t flags = (isAuthenticated() ? DP_MSG_JOIN_FLAGS_AUTH : 0) |
					(isModerator() ? DP_MSG_JOIN_FLAGS_MOD : 0);
	return net::makeJoinMessage(id(), flags, username(), avatar());
}

QJsonObject Client::description(bool includeSession) const
{
	QJsonObject u;
	u["id"] = id();
	u["name"] = username();
	u["ip"] = peerAddress().toString();
	u["lastActive"] = QDateTime::fromMSecsSinceEpoch(d->lastActive, Qt::UTC)
						  .toString(Qt::ISODate);
	u["auth"] = isAuthenticated();
	u["op"] = isOperator();
	u["muted"] = isMuted();
	u["mod"] = isModerator();
	u["tls"] = isSecure();
	if(includeSession && d->session) {
		u["session"] = d->session->id();
	}
	return u;
}

JsonApiResult Client::callJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Delete) {
		disconnectClient(Client::DisconnectionReason::Kick, "server operator");
		QJsonObject o;
		o["status"] = "ok";
		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};

	} else if(method == JsonApiMethod::Update) {
		QString msg = request["message"].toString();
		if(!msg.isEmpty()) {
			sendSystemChat(msg);
		}

		QString alert = request["alert"].toString();
		if(!alert.isEmpty()) {
			sendSystemChat(alert, true);
		}

		if(request.contains("op")) {
			const bool op = request["op"].toBool();
			if(d->isOperator != op && d->session) {
				d->session->changeOpStatus(
					id(), op, "the server administrator");
			}
		}

		if(request.contains("trusted")) {
			const bool trusted = request["trusted"].toBool();
			if(d->isTrusted != trusted && d->session) {
				d->session->changeTrustedStatus(
					id(), trusted, "the server administrator");
			}
		}

		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(description())};

	} else if(method == JsonApiMethod::Get) {
		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(description())};

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
	Q_ASSERT(d->id == 0 && id != 0); // ID is only assigned once
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
	return !isOperator() && isTrusted() && d->session &&
		   d->session->history()->hasFlag(SessionHistory::Deputies);
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
	return !d->authId.isEmpty();
}

void Client::setMuted(bool m)
{
	d->isMuted = m;
}

bool Client::isMuted() const
{
	return d->isMuted;
}

bool Client::isBanInProgress() const
{
	return d->ban.reaction != BanReaction::NotBanned;
}

bool Client::isImmuneToServerBans() const
{
	return d->ban.isExemptable && isModerator();
}

void Client::applyBan(const BanResult &ban)
{
	if(ban.reaction != BanReaction::NotBanned && !isBanInProgress()) {
		d->ban = ban;
	}
}

bool Client::triggerBan(bool early)
{
	bool shouldTrigger = !d->isBanTriggered && !isImmuneToServerBans() &&
						 (!early || rollEarlyTrigger());
	if(shouldTrigger) {
		switch(d->ban.reaction) {
		case BanReaction::NotBanned:
			return false;
		case BanReaction::NormalBan:
		case BanReaction::Unknown:
			if(early) {
				return false;
			} else {
				triggerNormalBan();
				return true;
			}
		case BanReaction::NetError:
			triggerNetError();
			return true;
		case BanReaction::Garbage:
			triggerGarbage();
			return true;
		case BanReaction::Hang:
			triggerHang();
			return true;
		case BanReaction::Timer:
			triggerTimer();
			return false; // We don't disconnect yet.
		}
		qWarning("Invalid ban reaction %d", int(d->ban.reaction));
	}
	return false;
}

bool Client::rollEarlyTrigger()
{
	return QRandomGenerator::global()->generateDouble() >= 0.5;
}

void Client::triggerNormalBan()
{
	Q_ASSERT(!d->isBanTriggered);
	d->isBanTriggered = true;

	log(Log()
			.about(Log::Level::Warn, Log::Topic::Kick)
			.user(d->id, d->socket->peerAddress(), QString())
			.message(QStringLiteral("%1 '%2' is banned (%3 %4)")
						 .arg(
							 d->ban.sourceType, d->ban.cause, d->ban.source,
							 QString::number(d->ban.sourceId))));
	QString message;
	QJsonObject params;

	// We'll say that 99 years has sufficient permanence.
	QDate date = d->ban.expires.isValid() ? d->ban.expires.date() : QDate();
	bool isTemporary =
		date.isValid() && date.year() - QDate::currentDate().year() < 99;
	if(isTemporary) {
		message = QStringLiteral("Banned from this server until ") +
				  date.toString(Qt::ISODate);
	} else {
		message = QStringLiteral("Permanently banned from this server");
	}

	QString reason = d->ban.reason.trimmed();
	if(!reason.isEmpty()) {
		message += QStringLiteral(": ") + reason;
	}

	disconnectClient(Client::DisconnectionReason::Error, message);
}

void Client::triggerNetError()
{
	Q_ASSERT(!d->isBanTriggered);
	d->isBanTriggered = true;

	log(Log()
			.about(Log::Level::Warn, Log::Topic::Kick)
			.user(d->id, d->socket->peerAddress(), QString())
			.message(QStringLiteral("%1 '%2' is NETERROR banned (%3 %4)")
						 .arg(
							 d->ban.sourceType, d->ban.cause, d->ban.source,
							 QString::number(d->ban.sourceId))));

	disconnectClient(
		Client::DisconnectionReason::Error,
		QStringLiteral("Network error: no route to host"));
}

void Client::triggerGarbage()
{
	Q_ASSERT(!d->isBanTriggered);
	d->isBanTriggered = true;

	log(Log()
			.about(Log::Level::Warn, Log::Topic::Kick)
			.user(d->id, d->socket->peerAddress(), QString())
			.message(QStringLiteral("%1 '%2' is GARBAGE banned (%3 %4)")
						 .arg(
							 d->ban.sourceType, d->ban.cause, d->ban.source,
							 QString::number(d->ban.sourceId))));

	sendDirectMessage(net::ServerReply::makeResultGarbage());
}

void Client::triggerHang()
{
	Q_ASSERT(!d->isBanTriggered);
	d->isBanTriggered = true;

	log(Log()
			.about(Log::Level::Warn, Log::Topic::Kick)
			.user(d->id, d->socket->peerAddress(), QString())
			.message(QStringLiteral("%1 '%2' is HANG banned (%3 %4)")
						 .arg(
							 d->ban.sourceType, d->ban.cause, d->ban.source,
							 QString::number(d->ban.sourceId))));

	// Do absolutely nothing, just leave the client hanging until they leave.
}

void Client::triggerTimer()
{
	Q_ASSERT(!d->isBanTriggered);
	d->isBanTriggered = true;

	int msec = QRandomGenerator::global()->bounded(1000, 5000);
	log(Log()
			.about(Log::Level::Warn, Log::Topic::Kick)
			.user(d->id, d->socket->peerAddress(), QString())
			.message(QStringLiteral("%1 '%2' is TIMER banned (%3 %4), severing "
									"connection in %5ms")
						 .arg(
							 d->ban.sourceType, d->ban.cause, d->ban.source,
							 QString::number(d->ban.sourceId),
							 QString::number(msec))));
	QTimer::singleShot(msec, this, [this]() {
		d->socket->abort();
	});
}

void Client::setConnectionTimeout(int timeout)
{
	d->msgqueue->setIdleTimeout(timeout);
}

QHostAddress Client::peerAddress() const
{
	return d->socket->peerAddress();
}

void Client::sendDirectMessage(const net::Message &msg)
{
	if(!d->isAwaitingReset || msg.isControl()) {
		d->msgqueue->send(msg);
	}
}

void Client::sendDirectMessages(const net::MessageList &msgs)
{
	if(d->isAwaitingReset) {
		for(const net::Message &msg : msgs) {
			if(msg.isControl()) {
				d->msgqueue->send(msg);
			}
		}
	} else {
		d->msgqueue->sendMultiple(msgs.size(), msgs.constData());
	}
}

void Client::sendSystemChat(const QString &message, bool alert)
{
	d->msgqueue->send(
		alert ? net::ServerReply::makeAlert(message)
			  : net::ServerReply::makeMessage(message));
}

void Client::receiveMessages()
{
	while(d->msgqueue->isPending()) {
		net::Message msg = d->msgqueue->shiftPending();
		d->lastActive = QDateTime::currentMSecsSinceEpoch();

		if(d->session.isNull()) {
			// No session? We must be in the login phase
			if(msg.type() == DP_MSG_SERVER_COMMAND) {
				emit loginMessage(msg);
			} else {
				log(Log()
						.about(Log::Level::Warn, Log::Topic::RuleBreak)
						.message(QStringLiteral("Got non-login message "
												"(type=%1) in login state")
									 .arg(msg.type())));
			}
		} else {
			// Enforce origin ID, except when receiving a snapshot
			if(d->session->initUserId() != d->id) {
				msg.setContextId(d->id);
			}

			if(isHoldLocked()) {
				d->holdqueue.append(msg);
			} else {
				d->session->handleClientMessage(*this, msg);
			}
		}
	}
}

void Client::gotBadData(int len, int type)
{
	log(Log()
			.about(Log::Level::Warn, Log::Topic::RuleBreak)
			.message(QString("Received unknown message type %1 of length %2")
						 .arg(type)
						 .arg(len)));
	d->socket->abort();
}

void Client::socketError(QAbstractSocket::SocketError error)
{
	if(error != QAbstractSocket::RemoteHostClosedError) {
		log(Log()
				.about(Log::Level::Warn, Log::Topic::Status)
				.message("Socket error: " + d->socket->errorString()));
		d->socket->abort();
	}
}

void Client::socketDisconnect()
{
	emit loggedOff(this);
	this->deleteLater();
}

void Client::disconnectClient(
	DisconnectionReason reason, const QString &message)
{
	net::MessageQueue::GracefulDisconnect pr{
		net::MessageQueue::GracefulDisconnect::Other};
	Log::Topic topic{Log::Topic::Leave};
	switch(reason) {
	case DisconnectionReason::Kick:
		pr = net::MessageQueue::GracefulDisconnect::Kick;
		topic = Log::Topic::Kick;
		break;
	case DisconnectionReason::Error:
		pr = net::MessageQueue::GracefulDisconnect::Error;
		break;
	case DisconnectionReason::Shutdown:
		pr = net::MessageQueue::GracefulDisconnect::Shutdown;
		break;
	}
	log(Log().about(Log::Level::Info, topic).message(message));

	emit loggedOff(this);
	d->msgqueue->sendDisconnect(pr, message);
}

void Client::setHoldLocked(bool lock)
{
	d->isHoldLocked = lock;
	if(!lock) {
		for(const net::Message &msg : d->holdqueue) {
			d->session->handleClientMessage(*this, msg);
		}
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
	QSslSocket *socket = qobject_cast<QSslSocket *>(d->socket);
	return socket && socket->isEncrypted();
}

void Client::startTls()
{
	QSslSocket *socket = qobject_cast<QSslSocket *>(d->socket);
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

Log Client::log() const
{
	return Log().user(d->id, d->socket->peerAddress(), d->username);
}


}
