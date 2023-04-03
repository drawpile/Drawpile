// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/drawdance/message.h"
#include "libclient/net/client.h"
#include "libclient/net/tcpserver.h"
#include "libclient/net/login.h"
#include "libclient/net/servercmd.h"
#include "libshared/util/qtcompat.h"

#ifdef Q_OS_ANDROID
#	include "libshared/util/androidutils.h"
#endif

#include <QDebug>

namespace net {

Client::Client(QObject *parent)
	: QObject(parent)
{
}

void Client::connectToServer(LoginHandler *loginhandler)
{
	Q_ASSERT(!isConnected());

	TcpServer *server = new TcpServer(this);
	m_server = server;

#ifdef Q_OS_ANDROID
	if(!m_wakeLock) {
		QString tag{QStringLiteral("Drawpile::TcpWake%1")
						.arg(
							reinterpret_cast<quintptr>(server),
							QT_POINTER_SIZE * 2, 16, QLatin1Char('0'))};
		m_wakeLock = new utils::AndroidWakeLock{"PARTIAL_WAKE_LOCK", tag};
	}

	if(!m_wifiLock) {
		QString tag{QStringLiteral("Drawpile::TcpWifi%1")
						.arg(
							reinterpret_cast<quintptr>(server),
							QT_POINTER_SIZE * 2, 16, QLatin1Char('0'))};
		m_wifiLock = new utils::AndroidWifiLock{"WIFI_MODE_FULL_LOW_LATENCY", tag};
	}
#endif

	connect(server, &TcpServer::loggingOut, this, &Client::serverDisconnecting);
	connect(server, &TcpServer::serverDisconnected, this, &Client::handleDisconnect);
	connect(server, &TcpServer::serverDisconnected, loginhandler, &LoginHandler::serverDisconnected);
	connect(server, &TcpServer::loggedIn, this, &Client::handleConnect);
	connect(server, &TcpServer::messagesReceived, this, &Client::handleMessages);

	connect(server, &TcpServer::bytesReceived, this, &Client::bytesReceived);
	connect(server, &TcpServer::bytesSent, this, &Client::bytesSent);
	connect(server, &TcpServer::lagMeasured, this, &Client::lagMeasured);

	connect(server, &TcpServer::gracefullyDisconnecting, this, [this](MessageQueue::GracefulDisconnect reason, const QString &message)
	{
		if(reason == MessageQueue::GracefulDisconnect::Kick) {
			emit youWereKicked(message);
			return;
		}

		QString chat;
		switch(reason) {
		case MessageQueue::GracefulDisconnect::Kick:
			emit youWereKicked(message);
			return;
		case MessageQueue::GracefulDisconnect::Error:
			chat = tr("A server error occurred!");
			break;
		case MessageQueue::GracefulDisconnect::Shutdown:
			chat = tr("The server is shutting down!");
			break;
		default:
			chat = "Unknown error";
		}

		if(!message.isEmpty())
			chat = QString("%1 (%2)").arg(chat, message);

		emit serverMessage(chat, true);
	});

	if(loginhandler->mode() == LoginHandler::Mode::HostRemote)
		loginhandler->setUserId(m_myId);

	emit serverConnected(loginhandler->url().host(), loginhandler->url().port());
	server->login(loginhandler);

	m_catchupTo = 0;
	m_caughtUp = 0;
	m_catchupProgress = 0;
}

void Client::disconnectFromServer()
{
	if(m_server)
		m_server->logout();
}

QUrl Client::sessionUrl(bool includeUser) const
{
	QUrl url = m_lastUrl;
	if(!includeUser)
		url.setUserInfo(QString());
	return url;
}

void Client::handleConnect(
	const QUrl &url, uint8_t userid, bool join, bool auth, bool moderator,
	bool supportsAutoReset, const protocol::ProtocolVersion &protocolVersion)
{
	m_lastUrl = url;
	m_myId = userid;
	m_moderator = moderator;
	m_isAuthenticated = auth;
	m_supportsAutoReset = supportsAutoReset;
	m_compatibilityMode = protocolVersion.isPastCompatible();

	emit serverLoggedIn(join, m_compatibilityMode);
}

void Client::handleDisconnect(const QString &message,const QString &errorcode, bool localDisconnect)
{
	Q_ASSERT(isConnected());

	m_compatibilityMode = false;
	emit serverDisconnected(message, errorcode, localDisconnect);
	m_server->deleteLater();
	m_server = nullptr;
	m_moderator = false;

#ifdef Q_OS_ANDROID
	delete m_wakeLock;
	delete m_wifiLock;
	m_wakeLock = nullptr;
	m_wifiLock = nullptr;
#endif
}

int Client::uploadQueueBytes() const
{
	if(m_server)
		return m_server->uploadQueueBytes();
	return 0;
}


void Client::sendMessage(const drawdance::Message &msg)
{
	sendMessages(1, &msg);
}

void Client::sendMessages(int count, const drawdance::Message *msgs)
{
	if(m_compatibilityMode) {
		QVector<drawdance::Message> compatibleMsgs = filterCompatibleMessages(count, msgs);
		sendCompatibleMessages(compatibleMsgs.count(), compatibleMsgs.constData());
	} else {
		sendCompatibleMessages(count, msgs);
	}
}

void Client::sendCompatibleMessages(int count, const drawdance::Message *msgs)
{
	if(count > 0) {
		emit drawingCommandsLocal(count, msgs);
		// Note: we could emit drawingCommandLocal only in connected mode,
		// but it's good to exercise the code path in local mode too
		// to make potential bugs more obvious.
		if(m_server) {
			m_server->sendMessages(count, msgs);
		} else {
			emit messagesReceived(count, msgs);
		}
	}
}

void Client::sendResetMessage(const drawdance::Message &msg)
{
	sendResetMessages(1, &msg);
}

void Client::sendResetMessages(int count, const drawdance::Message *msgs)
{
	if(m_compatibilityMode) {
		QVector<drawdance::Message> compatibleMsgs = filterCompatibleMessages(count, msgs);
		sendCompatibleResetMessages(compatibleMsgs.count(), compatibleMsgs.constData());
	} else {
		sendCompatibleResetMessages(count, msgs);
	}
}

void Client::sendCompatibleResetMessages(int count, const drawdance::Message *msgs)
{
	if(count > 0) {
		if(m_server) {
			m_server->sendMessages(count, msgs);
		} else {
			emit messagesReceived(count, msgs);
		}
	}
}

QVector<drawdance::Message> Client::filterCompatibleMessages(int count, const drawdance::Message *msgs)
{
	// Ideally, the client shouldn't be attempting to send any incompatible
	// messages in the first place, but we'll err on the side of caution. In
	// particular, a thick server will kick us out if we send a wrong message.
	QVector<drawdance::Message> compatibleMsgs;
	compatibleMsgs.reserve(count);
	for(int i = 0; i < count; ++i) {
		const drawdance::Message &msg = msgs[i];
		const drawdance::Message compatibleMsg = msg.makeBackwardCompatible();
		if(compatibleMsg.isNull()) {
			qWarning("Incompatible %s message", qUtf8Printable(msg.typeName()));
		} else {
			compatibleMsgs.append(compatibleMsg);
		}
	}
	return compatibleMsgs;
}

void Client::handleMessages(int count, const drawdance::Message *msgs)
{
	for(int i = 0; i < count; ++i) {
		const drawdance::Message &msg = msgs[i];
		switch(msg.type()) {
		case DP_MSG_SERVER_COMMAND:
			handleServerReply(ServerReply::fromMessage(msg));
			break;
		case DP_MSG_DATA:
			handleData(msg);
			break;
		default:
			break;
		}
	}
	emit messagesReceived(count, msgs);

	// The server can send a "catchup" message when there is a significant number
	// of messages queued. During login, we can show a progress bar and hide the canvas
	// to speed up the initial catchup phase.
	if(m_catchupTo>0) {
		m_caughtUp += count;
		if(m_caughtUp >= m_catchupTo) {
			qInfo("Catchup: caught up to %d messages", m_caughtUp);
			emit catchupProgress(100);
			m_catchupTo = 0;
		} else {
			int progress = 100 * m_caughtUp / m_catchupTo;
			if(progress != m_catchupProgress) {
				m_catchupProgress = progress;
				emit catchupProgress(progress);
			}
		}
	}
}

void Client::handleServerReply(const ServerReply &reply)
{
	switch(reply.type) {
	case ServerReply::ReplyType::Unknown:
		qWarning() << "Unknown server reply:" << reply.message << reply.reply;
		break;
	case ServerReply::ReplyType::Login:
		qWarning("got login message while in session!");
		break;
	case ServerReply::ReplyType::Message:
	case ServerReply::ReplyType::Alert:
	case ServerReply::ReplyType::Error:
	case ServerReply::ReplyType::Result:
		emit serverMessage(reply.message, reply.type == ServerReply::ReplyType::Alert);
		break;
	case ServerReply::ReplyType::Log: {
		QString time = QDateTime::fromString(reply.reply["timestamp"].toString(), Qt::ISODate).toLocalTime().toString(Qt::ISODate);
		QString user = reply.reply["user"].toString();
		QString msg = reply.message;
		if(user.isEmpty())
			emit serverLog(QStringLiteral("[%1] %2").arg(time, msg));
		else
			emit serverLog(QStringLiteral("[%1] %2: %3").arg(time, user, msg));
		} break;
	case ServerReply::ReplyType::SessionConf:
		emit sessionConfChange(reply.reply["config"].toObject());
		break;
	case ServerReply::ReplyType::SizeLimitWarning:
		// No longer used since 2.1.0. Replaced by RESETREQUEST
		break;
	case ServerReply::ReplyType::ResetRequest:
		emit autoresetRequested(reply.reply["maxSize"].toInt(), reply.reply["query"].toBool());
		break;
	case ServerReply::ReplyType::Status:
		emit serverStatusUpdate(reply.reply["size"].toInt());
		break;
	case ServerReply::ReplyType::Reset:
		handleResetRequest(reply);
		break;
	case ServerReply::ReplyType::Catchup:
		m_catchupTo = reply.reply["count"].toInt();
		m_caughtUp = 0;
		m_catchupProgress = 0;
		break;
	}
}

void Client::handleResetRequest(const ServerReply &msg)
{
	if(msg.reply["state"] == "init") {
		qDebug("Requested session reset");
		emit needSnapshot();

	} else if(msg.reply["state"] == "reset") {
		qDebug("Resetting session!");
		emit sessionResetted();

	} else {
		qWarning() << "Unknown reset state:" << msg.reply["state"].toString();
		qWarning() << msg.message;
	}
}

void Client::handleData(const drawdance::Message &msg)
{
	DP_MsgData *md = msg.toData();
	if(md && DP_msg_data_recipient(md) == m_myId) {
		int type = DP_msg_data_type(md);
		switch(type) {
		case DP_MSG_DATA_TYPE_USER_INFO:
			handleUserInfo(msg, md);
			break;
		default:
			qWarning("Unknown data message type %d", type);
			break;
		}

	}
}

void Client::Client::handleUserInfo(const drawdance::Message &msg, DP_MsgData *md)
{
	size_t size;
	const unsigned char *bytes = DP_msg_data_body(md, &size);
	QJsonParseError err;
	QJsonDocument json = QJsonDocument::fromJson(QByteArray::fromRawData(
		reinterpret_cast<const char *>(bytes), compat::castSize(size)), &err);
	if(json.isObject()) {
		QJsonObject info = json.object();
		QString type = info["type"].toString();
		if(type == "request_user_info") {
			emit userInfoRequested(msg.contextId());
		} else if(type == "user_info") {
			emit userInfoReceived(msg.contextId(), info);
		} else {
			qWarning("Unknown user info type '%s'", qUtf8Printable(type));
		}
	} else {
		qWarning("Could not parse JSON as an object: %s",
			qUtf8Printable(err.errorString()));
	}
}

}
