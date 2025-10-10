// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/client.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/session.h"
#include "libserver/sessionhistory.h"
#include "libshared/net/messagequeue.h"
#include "libshared/net/servercmd.h"
#include "libshared/net/tcpmessagequeue.h"
#include "libshared/util/qtcompat.h"
#include <QPointer>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QStringList>
#include <QTcpSocket>
#include <QTimeZone>
#include <QTimer>
#ifdef HAVE_WEBSOCKETS
#	include <QWebSocket>
#	include "libshared/net/websocketmessagequeue.h"
#endif

namespace {
#ifdef HAVE_WEBSOCKETS
#	if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#		define COMPAT_WEBSOCKET_ERROR_SIGNAL(CLS) &CLS::errorOccurred
#	else
#		define COMPAT_WEBSOCKET_ERROR_SIGNAL(CLS)                             \
			QOverload<QAbstractSocket::SocketError>::of(&CLS::error)
#	endif
#endif
}

namespace server {

class ClientSocket {
	COMPAT_DISABLE_COPY_MOVE(ClientSocket)
public:
	ClientSocket() = default;
	virtual ~ClientSocket() = default;

	virtual QHostAddress peerAddress() const = 0;
	virtual QString errorString() const = 0;
	virtual void abort() = 0;

	virtual bool isBrowser() const = 0;
	virtual bool hasSslSupport() const = 0;
	virtual bool isSecure() const = 0;
	virtual void startTls() = 0;
};

class ClientTcpSocket final : public ClientSocket {
	COMPAT_DISABLE_COPY_MOVE(ClientTcpSocket)
public:
	ClientTcpSocket(QTcpSocket *tcpSocket)
		: m_tcpSocket(tcpSocket)
	{
		Q_ASSERT(tcpSocket);
	}

	virtual ~ClientTcpSocket() override = default;

	virtual QHostAddress peerAddress() const override
	{
		return m_tcpSocket->peerAddress();
	}

	virtual QString errorString() const override
	{
		return m_tcpSocket->errorString();
	}

	virtual void abort() override { m_tcpSocket->abort(); }

	virtual bool isBrowser() const override { return false; }

	virtual bool hasSslSupport() const override
	{
		return m_tcpSocket->inherits("QSslSocket");
	}

	virtual bool isSecure() const override
	{
		QSslSocket *sslSocket = qobject_cast<QSslSocket *>(m_tcpSocket);
		return sslSocket && sslSocket->isEncrypted();
	}

	virtual void startTls() override
	{
		QSslSocket *socket = qobject_cast<QSslSocket *>(m_tcpSocket);
		Q_ASSERT(socket);
		socket->startServerEncryption();
	}

private:
	QTcpSocket *m_tcpSocket;
};

#ifdef HAVE_WEBSOCKETS
class ClientWebSocket final : public ClientSocket {
	COMPAT_DISABLE_COPY_MOVE(ClientWebSocket)
public:
	ClientWebSocket(QWebSocket *webSocket, const QHostAddress &ip, bool browser)
		: m_webSocket(webSocket)
		, m_ip(ip)
		, m_browser(browser)
	{
		Q_ASSERT(webSocket);
	}

	virtual ~ClientWebSocket() override = default;

	virtual QHostAddress peerAddress() const override { return m_ip; }

	virtual QString errorString() const override
	{
		return m_webSocket->errorString();
	}

	virtual void abort() override { m_webSocket->abort(); }

	virtual bool isBrowser() const override { return m_browser; }

	virtual bool hasSslSupport() const override { return false; }

	virtual bool isSecure() const override { return false; }

	virtual void startTls() override { Q_ASSERT(false); }

private:
	QWebSocket *m_webSocket;
	QHostAddress m_ip;
	bool m_browser;
};
#endif

struct Client::Private {
	QPointer<Session> session;
	ClientSocket *socket;
	ServerLog *logger;

	net::MessageQueue *msgqueue;
	net::MessageList holdqueue;

	QString username;
	QString authId;
	QString sid;
	QString inviteSecret;
	QByteArray avatar;
	QSet<QString> flags;

	qint64 lastActive = 0;
	qint64 lastActiveDrawing = 0;

	uint8_t id = 0;
	bool isOperator = false;
	bool isModerator = false;
	bool isTrusted = false;
	bool isAuthenticated = false;
	bool isMuted = false;
	bool isHoldLocked = false;
	bool isBanTriggered = false;
	bool isGhost = false;
	bool isMinorIncompatibility = false;
	bool gracefulDisconnect = false;
	bool quietDisconnect = false;
	CapabilityFlags capabilities = CapabilityFlag::None;
	ResetFlags resetFlags = ResetFlag::None;
	BanResult ban = BanResult::notBanned();

	Private(ClientSocket *socket_, ServerLog *logger_)
		: socket(socket_)
		, logger(logger_)
	{
		Q_ASSERT(socket);
		Q_ASSERT(logger);
	}

	~Private() { delete socket; }
};

Client::Client(
	QTcpSocket *tcpSocket, ServerLog *logger, bool decodeOpaque,
	QObject *parent)
	: QObject(parent)
	, d(new Private(new ClientTcpSocket(tcpSocket), logger))
{
	d->msgqueue = new net::TcpMessageQueue(tcpSocket, decodeOpaque, this);
	tcpSocket->setParent(this);
	connect(
		tcpSocket, &QAbstractSocket::disconnected, this,
		&Client::socketDisconnect);
	connect(
		tcpSocket, COMPAT_SOCKET_ERROR_SIGNAL(QTcpSocket), this,
		&Client::socketError);
	connect(
		d->msgqueue, &net::MessageQueue::messageAvailable, this,
		&Client::receiveMessages);
	connect(
		d->msgqueue, &net::MessageQueue::badData, this, &Client::gotBadData);
	connect(
		d->msgqueue, &net::MessageQueue::readError, this, &Client::readError);
	connect(
		d->msgqueue, &net::MessageQueue::writeError, this, &Client::writeError);
	connect(d->msgqueue, &net::MessageQueue::timedOut, this, &Client::timedOut);
}

#ifdef HAVE_WEBSOCKETS
Client::Client(
	QWebSocket *webSocket, const QHostAddress &ip, bool browser,
	ServerLog *logger, bool decodeOpaque, QObject *parent)
	: QObject(parent)
	, d(new Private(new ClientWebSocket(webSocket, ip, browser), logger))
{
	d->msgqueue = new net::WebSocketMessageQueue(webSocket, decodeOpaque, this);
	webSocket->setParent(this);
	connect(
		webSocket, &QWebSocket::disconnected, this, &Client::socketDisconnect);
	connect(
		webSocket, COMPAT_WEBSOCKET_ERROR_SIGNAL(QWebSocket), this,
		&Client::socketError);
	connect(
		d->msgqueue, &net::MessageQueue::messageAvailable, this,
		&Client::receiveMessages);
	connect(
		d->msgqueue, &net::MessageQueue::badData, this, &Client::gotBadData);
}
#endif

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
					(isModerator() ? DP_MSG_JOIN_FLAGS_MOD : 0) |
					(isMinorIncompatibility() ? DP_MSG_JOIN_FLAGS_OLD : 0);
	return net::makeJoinMessage(id(), flags, username(), avatar());
}

QJsonObject Client::description(bool includeSession) const
{
	QJsonObject u;
	u["uid"] = uid();
	u["id"] = id();
	u["name"] = username();
	u["ip"] = peerAddress().toString();
	u["s"] = sid();
#if QT_CONFIG(timezone)
	QTimeZone utc = QTimeZone::utc();
#else
	QTimeZone utc = QTimeZone(QTimeZone::UTC);
#endif
	u["lastActive"] = QDateTime::fromMSecsSinceEpoch(d->lastActive, utc)
						  .toString(Qt::ISODate);
	u["lastActiveDrawing"] =
		QDateTime::fromMSecsSinceEpoch(d->lastActiveDrawing, utc)
			.toString(Qt::ISODate);
	u["auth"] = isAuthenticated();
	u["op"] = isOperator();
	u["trusted"] = isTrusted();
	u["muted"] = isMuted();
	u["mod"] = isModerator();
	u["tls"] = isSecure();
	u[QStringLiteral("browser")] = isBrowser();
	if(includeSession && d->session) {
		u["session"] = d->session->id();
	}
	QString state;
	switch(connectionState()) {
	case ConnectionState::Login:
		state = QStringLiteral("login");
		break;
	case ConnectionState::Session:
		state = QStringLiteral("session");
		break;
	case ConnectionState::GracefulDisconnect:
		state = QStringLiteral("graceful_disconnect");
		break;
	case ConnectionState::QuietDisconnect:
		state = QStringLiteral("quiet_disconnect");
		break;
	}
	u.insert(QStringLiteral("state"), state);
	u.insert(
		QStringLiteral("capabilities"),
		QJsonObject({
			{QStringLiteral("keepalive"),
			 hasCapability(CapabilityFlag::KeepAlive)},
			{QStringLiteral("thumbnail"),
			 hasCapability(CapabilityFlag::Thumbnail)},
		}));
	u.insert(QStringLiteral("oldv"), d->isMinorIncompatibility);
	return u;
}

JsonApiResult Client::callJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(path.size() == 1 && path[0] == QStringLiteral("thumbnail")) {
		return callThumbnailJsonApi(method, request);
	} else if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Delete) {
		return jsonApiKick(request[QStringLiteral("message")].toString());

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

JsonApiResult
Client::callThumbnailJsonApi(JsonApiMethod method, const QJsonObject &request)
{
	if(method == JsonApiMethod::Create) {
		if(id() == 0) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Invalid client id"));
		}

		if(!hasCapability(CapabilityFlag::Thumbnail)) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest,
				QStringLiteral("Client does not have thumbnail capabilities"));
		}

		int maxWidth = 0;
		QJsonValue maxWidthValue = request.value(QStringLiteral("maxWidth"));
		if(!maxWidthValue.isUndefined()) {
			if(maxWidthValue.isDouble()) {
				maxWidth = maxWidthValue.toInt();
			} else {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("maxWidth must be a number"));
			}
		}

		int maxHeight = 0;
		QJsonValue maxHeightValue = request.value(QStringLiteral("maxHeight"));
		if(!maxHeightValue.isUndefined()) {
			if(maxHeightValue.isDouble()) {
				maxHeight = maxHeightValue.toInt();
			} else {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("maxHeight must be a number"));
			}
		}

		int quality = 0;
		QJsonValue qualityValue = request.value(QStringLiteral("quality"));
		if(!qualityValue.isUndefined()) {
			if(qualityValue.isDouble()) {
				quality = qualityValue.toInt();
			} else {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("quality must be a number"));
			}
		}

		QString format = request.value(QStringLiteral("format")).toString();
		QString correlator = session()->startThumbnailGeneration(
			this, request.value(QStringLiteral("reason")).toString());
		if(correlator.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::Conflict,
				QStringLiteral(
					"This client is already generating a thumbnail"));
		}

		sendDirectMessage(
			net::ServerReply::makeThumbnail(
				correlator, maxWidth, maxHeight, quality, format));

		return JsonApiResult{
			JsonApiResult::Accepted,
			QJsonDocument(
				QJsonObject{{QStringLiteral("correlator"), correlator}})};

	} else if(method == JsonApiMethod::Delete) {
		if(session()->cancelThumbnailGeneration(this)) {
			return JsonApiResult{
				JsonApiResult::Ok,
				QJsonDocument(
					QJsonObject{
						{QStringLiteral("status"), QStringLiteral("ok")}})};
		} else {
			return JsonApiErrorResult(
				JsonApiResult::NotFound,
				QStringLiteral("Client is not generating a thumbnail"));
		}

	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult Client::jsonApiKick(const QString &message)
{
	if(message.toUtf8().length() > 100) {
		QJsonObject o{
			{QStringLiteral("status"), QStringLiteral("error")},
			{QStringLiteral("error"), QStringLiteral("message too long")}};
		return JsonApiResult{JsonApiResult::BadRequest, QJsonDocument(o)};
	}
	bool wasAlreadyDisconnecting = disconnectClient(
		Client::DisconnectionReason::Kick,
		QStringLiteral("the server administrator") +
			(message.isEmpty() ? QString()
							   : QStringLiteral(" (%1)").arg(message)),
		QStringLiteral("kicked via web admin"));
	if(wasAlreadyDisconnecting) {
		d->msgqueue->forceDisconnect();
	}
	QJsonObject o{
		{QStringLiteral("status"), QStringLiteral("ok")},
		{QStringLiteral("force"), wasAlreadyDisconnecting}};
	return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};
}

void Client::setSession(Session *session)
{
	d->session = session;
}

Session *Client::session()
{
	return d->session.data();
}

QString Client::uid() const
{
	return QString::number(reinterpret_cast<uintptr_t>(this), 16);
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

void Client::setAuthFlags(const QSet<QString> &flags)
{
	d->flags = flags;
}

QSet<QString> Client::authFlags() const
{
	return d->flags;
}

Client::CapabilityFlags Client::capabilities() const
{
	return d->capabilities;
}

bool Client::hasCapability(CapabilityFlag capability) const
{
	return d->capabilities.testFlag(capability);
}

void Client::setCapability(CapabilityFlag capability, bool on)
{
	d->capabilities.setFlag(capability, on);
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

const QString &Client::sid() const
{
	return d->sid;
}

void Client::setSid(const QString &sid)
{
	d->sid = sid;
}

bool Client::hasSid() const
{
	return !d->sid.isEmpty();
}

const QString &Client::inviteSecret() const
{
	return d->inviteSecret;
}

void Client::setInviteSecret(const QString &inviteSecret)
{
	d->inviteSecret = inviteSecret;
}

bool Client::hasInviteSecret() const
{
	return !d->inviteSecret.isEmpty();
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

void Client::setModerator(bool mod, bool ghost)
{
	d->isModerator = mod;
	d->isGhost = mod && ghost;
}

bool Client::isModerator() const
{
	return d->isModerator;
}

bool Client::isGhost() const
{
	return d->isGhost;
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

bool Client::isMinorIncompatibility() const
{
	return d->isMinorIncompatibility;
}

void Client::setMinorIncompatibility(bool minorIncompatibility)
{
	d->isMinorIncompatibility = minorIncompatibility;
}

bool Client::canManageWebSession() const
{
	return authFlags().contains(QStringLiteral("WEBSESSION"));
}

bool Client::isBanInProgress() const
{
	return d->ban.reaction != BanReaction::NotBanned;
}

void Client::applyBanExemption(bool exempt)
{
	bool shouldRevokeBan = exempt &&
						   d->ban.reaction != BanReaction::NotBanned &&
						   d->ban.isExemptable;
	if(shouldRevokeBan) {
		log(Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.user(d->id, d->socket->peerAddress(), QString())
				.message(QStringLiteral(
							 "%1 '%2' is banned (%3 %4), but user is exempt")
							 .arg(
								 d->ban.sourceType, d->ban.cause, d->ban.source,
								 QString::number(d->ban.sourceId))));
		d->ban = BanResult::notBanned();
	}
}

void Client::applyBan(const BanResult &ban)
{
	if(ban.reaction != BanReaction::NotBanned && !isBanInProgress()) {
		d->ban = ban;
	}
}

bool Client::triggerBan(bool early)
{
	bool shouldTrigger = !d->isBanTriggered && (!early || rollEarlyTrigger());
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

	disconnectClient(
		Client::DisconnectionReason::Error, message, QStringLiteral("banned"));
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
		QStringLiteral("Network error: no route to host"),
		QStringLiteral("NETERROR shadow ban"));
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

void Client::setKeepAliveTimeout(int timeout)
{
	d->msgqueue->setKeepAliveTimeout(timeout);
}

qint64 Client::lastActive() const
{
	return d->lastActive;
}

qint64 Client::lastActiveDrawing() const
{
	return d->lastActiveDrawing;
}

QHostAddress Client::peerAddress() const
{
	return d->socket->peerAddress();
}

void Client::sendDirectMessage(const net::Message &msg)
{
	if(!msg.isNull() && (!isAwaitingReset() || msg.isControl())) {
		d->msgqueue->send(msg);
	}
}

void Client::sendDirectMessages(const net::MessageList &msgs)
{
	if(isAwaitingReset()) {
		for(const net::Message &msg : msgs) {
			if(!msg.isNull() && msg.isControl()) {
				d->msgqueue->send(msg);
			}
		}
	} else {
		d->msgqueue->sendMultiple(msgs.size(), msgs.constData());
	}
}

bool Client::sendSystemChat(const QString &message, bool alert)
{
	net::Message msg = alert ? net::ServerReply::makeAlert(message)
							 : net::ServerReply::makeMessage(message);
	if(msg.isNull()) {
		return false;
	} else {
		d->msgqueue->send(msg);
		return true;
	}
}

void Client::receiveMessages()
{
	while(d->msgqueue->isPending()) {
		net::Message msg = d->msgqueue->shiftPending();
		if(msg.isNull()) {
			continue;
		}

		d->lastActive = QDateTime::currentMSecsSinceEpoch();
		if(msg.type() >= DP_MESSAGE_TYPE_RANGE_START_COMMAND) {
			d->lastActiveDrawing = d->lastActive;
		}

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

void Client::readError()
{
	log(Log()
			.about(Log::Level::Warn, Log::Topic::Status)
			.message(QStringLiteral("Socket read error: %1")
						 .arg(d->socket->errorString())));
	d->socket->abort();
}

void Client::writeError()
{
	log(Log()
			.about(Log::Level::Warn, Log::Topic::Status)
			.message(QStringLiteral("Socket write error: %1")
						 .arg(d->socket->errorString())));
	d->socket->abort();
}

void Client::timedOut(qint64 idleTimeout)
{
	log(Log()
			.about(Log::Level::Warn, Log::Topic::Status)
			.message(QStringLiteral("Message queue exceeded timeout of %1ms")
						 .arg(idleTimeout)));
	d->socket->abort();
}

void Client::socketDisconnect()
{
	emit loggedOff(this);
	this->deleteLater();
}

bool Client::disconnectClient(
	DisconnectionReason reason, const QString &message, const QString &details)
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

	QString logMessage;
	if(reason == DisconnectionReason::Kick) {
		if(details.isEmpty()) {
			logMessage = QStringLiteral("Kicked by %1").arg(message);
		} else {
			logMessage =
				QStringLiteral("Kicked by %1 (%2)").arg(message, details);
		}
	} else {
		if(details.isEmpty()) {
			logMessage = message;
		} else {
			logMessage = QStringLiteral("%1 (%2)").arg(message, details);
		}
	}

	log(Log().about(Log::Level::Info, topic).message(logMessage));

	emit loggedOff(this);
	d->gracefulDisconnect = true;
	d->quietDisconnect = false;
	return d->msgqueue->sendDisconnect(pr, message);
}

void Client::quietDisconnectClient()
{
	if(!d->gracefulDisconnect) {
		d->quietDisconnect = true;
		d->msgqueue->quietDisconnect();
	}
}

Client::ConnectionState Client::connectionState() const
{
	if(d->gracefulDisconnect) {
		return ConnectionState::GracefulDisconnect;
	} else if(d->quietDisconnect) {
		return ConnectionState::QuietDisconnect;
	} else if(d->session) {
		return ConnectionState::Session;
	} else {
		return ConnectionState::Login;
	}
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

void Client::setResetFlags(ResetFlags resetFlags)
{
	d->resetFlags = resetFlags;
}

Client::ResetFlags Client::resetFlags() const
{
	return d->resetFlags;
}

bool Client::isAwaitingReset() const
{
	return d->resetFlags.testFlag(ResetFlag::Awaiting) &&
		   !d->resetFlags.testFlag(ResetFlag::Streaming);
}

bool Client::isBrowser() const
{
	return d->socket->isBrowser();
}

bool Client::hasSslSupport() const
{
	return d->socket->hasSslSupport();
}

bool Client::isSecure() const
{
	return d->socket->isSecure();
}

void Client::startTls()
{
	d->socket->startTls();
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
	Log l = Log();
	setUserOnLog(l);
	return l;
}

Log &Client::setUserOnLog(Log &log) const
{
	return log.user(d->id, d->socket->peerAddress(), d->username);
}

}
