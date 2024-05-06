// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/client.h"
#include "libclient/document.h"
#include "libclient/net/login.h"
#include "libclient/net/message.h"
#include "libclient/net/server.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/qtcompat.h"
#include <QDebug>
#include <QTimer>
#ifdef Q_OS_ANDROID
#	include "libshared/util/androidutils.h"
#endif

namespace net {

Client::Client(Document *doc)
	: QObject(doc)
	, m_doc(doc)
	, m_catchupTimer(new QTimer(this))
{
	m_catchupTimer->setSingleShot(true);
	m_catchupTimer->setTimerType(Qt::CoarseTimer);
	connect(m_catchupTimer, &QTimer::timeout, this, &Client::nudgeCatchup);
}

void Client::connectToServer(
	int timeoutSecs, LoginHandler *loginhandler, bool builtin)
{
	Q_ASSERT(!isConnected());
	m_builtin = builtin;

	m_server = Server::make(loginhandler->url(), timeoutSecs, this);
	m_server->setSmoothDrainRate(m_smoothDrainRate);

#ifdef Q_OS_ANDROID
	if(!m_wakeLock) {
		QString tag{QStringLiteral("Drawpile::TcpWake%1")
						.arg(
							reinterpret_cast<quintptr>(m_server),
							QT_POINTER_SIZE * 2, 16, QLatin1Char('0'))};
		m_wakeLock = new utils::AndroidWakeLock{"PARTIAL_WAKE_LOCK", tag};
	}

	if(!m_wifiLock) {
		QString tag{QStringLiteral("Drawpile::TcpWifi%1")
						.arg(
							reinterpret_cast<quintptr>(m_server),
							QT_POINTER_SIZE * 2, 16, QLatin1Char('0'))};
		m_wifiLock =
			new utils::AndroidWifiLock{"WIFI_MODE_FULL_LOW_LATENCY", tag};
	}
#endif

	connect(
		m_server, &Server::initiatingConnection, this,
		&Client::setConnectionUrl);
	connect(m_server, &Server::loggingOut, this, &Client::serverDisconnecting);
	connect(
		m_server, &Server::serverDisconnected, this, &Client::handleDisconnect);
	connect(m_server, &Server::loggedIn, this, &Client::handleConnect);
	connect(m_server, &Server::bytesReceived, this, &Client::bytesReceived);
	connect(m_server, &Server::bytesSent, this, &Client::bytesSent);
	connect(m_server, &Server::lagMeasured, this, &Client::lagMeasured);
	connect(
		m_server, &Server::gracefullyDisconnecting, this,
		[this](
			MessageQueue::GracefulDisconnect reason, const QString &message) {
			QString chat;
			switch(reason) {
			case MessageQueue::GracefulDisconnect::Kick:
				if(message.isEmpty()) {
					chat = tr("You have been kicked.");
				} else {
					chat = tr("You have been kicked by %1.").arg(message);
				}
				break;
			case MessageQueue::GracefulDisconnect::Error:
				if(message.isEmpty()) {
					chat = tr("A server error occurred.");
				} else {
					chat = tr("A server error occurred: %1").arg(message);
				}
				break;
			case MessageQueue::GracefulDisconnect::Shutdown:
				if(message.isEmpty()) {
					chat = tr("The server is shutting down.");
				} else {
					chat =
						tr("The session has been shut down: %1").arg(message);
				}
				break;
			default:
				if(message.isEmpty()) {
					chat = tr("Disconnected.");
				} else {
					chat = tr("Disconnected: %1").arg(message);
				}
				break;
			}
			emit serverMessage(chat, true);
		});

	if(loginhandler->mode() == LoginHandler::Mode::HostRemote)
		loginhandler->setUserId(m_myId);

	emit serverConnected(
		loginhandler->url().host(), loginhandler->url().port());
	m_server->login(loginhandler);

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
	const QUrl &url, uint8_t userid, bool join, bool auth,
	const QStringList &userFlags, bool supportsAutoReset,
	bool compatibilityMode, const QString &joinPassword, const QString &authId)
{
	m_lastUrl = url;
	m_myId = userid;
	m_userFlags = UserFlag::None;
	for(const QString &userFlag : userFlags) {
		if(userFlag == QStringLiteral("MOD")) {
			m_userFlags.setFlag(UserFlag::Mod);
		} else if(userFlag == QStringLiteral("WEBSESSION")) {
			m_userFlags.setFlag(UserFlag::WebSession);
		}
	}
	m_isAuthenticated = auth;
	m_supportsAutoReset = supportsAutoReset;
	m_compatibilityMode = compatibilityMode;

	emit serverLoggedIn(join, m_compatibilityMode, joinPassword, authId);
}

void Client::handleDisconnect(
	const QString &message, const QString &errorcode, bool localDisconnect,
	bool anyMessageReceived)
{
	// WebSockets may report multiple disconnects, some with error messages,
	// some without. We just emit them all so the UI can report what it wants.
	emit serverDisconnected(
		message, errorcode, localDisconnect, anyMessageReceived);
	if(isConnected()) {
		m_compatibilityMode = false;
		m_server->deleteLater();
		m_server = nullptr;
		m_userFlags = UserFlag::None;
#ifdef Q_OS_ANDROID
		delete m_wakeLock;
		delete m_wifiLock;
		m_wakeLock = nullptr;
		m_wifiLock = nullptr;
#endif
	}
}

void Client::nudgeCatchup()
{
	if(m_catchupTo > 0) {
		qWarning(
			"Catchup stuck at %d%% with no message in %dms, nudging it",
			m_catchupProgress, m_catchupTimer->interval());
		if(++m_catchupProgress < 100) {
			emit catchupProgress(m_catchupProgress);
			m_catchupTimer->start(qMax(100, m_catchupTimer->interval() / 2));
		} else {
			finishCatchup("catchup stuck");
		}
	}
}

int Client::uploadQueueBytes() const
{
	if(m_server)
		return m_server->uploadQueueBytes();
	return 0;
}


void Client::sendMessage(const net::Message &msg)
{
	sendMessages(1, &msg);
}

void Client::sendMessages(int count, const net::Message *msgs)
{
	if(m_compatibilityMode) {
		QVector<net::Message> compatibleMsgs =
			filterCompatibleMessages(count, msgs);
		sendCompatibleMessages(
			compatibleMsgs.count(), compatibleMsgs.constData());
	} else {
		sendCompatibleMessages(count, msgs);
	}
}

void Client::sendCompatibleMessages(int count, const net::Message *msgs)
{
	if(count > 0) {
		emit m_doc->handleLocalCommands(count, msgs);
		// Note: we could emit drawingCommandLocal only in connected mode,
		// but it's good to exercise the code path in local mode too
		// to make potential bugs more obvious.
		if(m_server) {
			m_server->sendMessages(count, msgs);
		} else {
			m_doc->handleCommands(count, msgs);
		}
	}
}

void Client::sendResetMessage(const net::Message &msg)
{
	sendResetMessages(1, &msg);
}

void Client::sendResetMessages(int count, const net::Message *msgs)
{
	if(m_compatibilityMode) {
		QVector<net::Message> compatibleMsgs =
			filterCompatibleMessages(count, msgs);
		sendCompatibleResetMessages(
			compatibleMsgs.count(), compatibleMsgs.constData());
	} else {
		sendCompatibleResetMessages(count, msgs);
	}
}

void Client::sendCompatibleResetMessages(int count, const net::Message *msgs)
{
	if(count > 0) {
		if(m_server) {
			m_server->sendMessages(count, msgs);
		} else {
			m_doc->handleCommands(count, msgs);
		}
	}
}

QVector<net::Message>
Client::filterCompatibleMessages(int count, const net::Message *msgs)
{
	// Ideally, the client shouldn't be attempting to send any incompatible
	// messages in the first place, but we'll err on the side of caution. In
	// particular, a thick server will kick us out if we send a wrong message.
	QVector<net::Message> compatibleMsgs;
	compatibleMsgs.reserve(count);
	for(int i = 0; i < count; ++i) {
		const net::Message &msg = msgs[i];
		const net::Message compatibleMsg =
			net::makeMessageBackwardCompatible(msg);
		if(compatibleMsg.isNull()) {
			qWarning("Incompatible %s message", qUtf8Printable(msg.typeName()));
		} else {
			compatibleMsgs.append(compatibleMsg);
		}
	}
	return compatibleMsgs;
}

void Client::setConnectionUrl(const QUrl &url)
{
	m_connectionUrl = url;
}

void Client::handleMessages(int count, net::Message *msgs)
{
	if(m_catchupTo > 0) {
		m_catchupTimer->stop();
	}

	for(int i = 0; i < count; ++i) {
		net::Message &msg = msgs[i];
		switch(msg.type()) {
		case DP_MSG_SERVER_COMMAND:
			handleServerReply(ServerReply::fromMessage(msg), i);
			break;
		case DP_MSG_DATA:
			handleData(msg);
			break;
		case DP_MSG_DRAW_DABS_CLASSIC:
		case DP_MSG_DRAW_DABS_PIXEL:
		case DP_MSG_DRAW_DABS_PIXEL_SQUARE:
			if(m_compatibilityMode) {
				msg.setIndirectCompatFlag();
			}
			break;
		default:
			break;
		}
	}
	m_doc->handleCommands(count, msgs);

	// The server can send a "catchup" message when there is a significant
	// number of messages queued. During login, we can show a progress bar and
	// hide the canvas to speed up the initial catchup phase.
	if(m_catchupTo > 0) {
		m_caughtUp += count;
		if(m_caughtUp >= m_catchupTo) {
			// If we have a catchup key, wait for a caughtup message instead.
			if(m_catchupKey == -1) {
				finishCatchup("reached catchup count");
			}
			m_catchupTimer->start(CATCHUP_TIMER_MSEC);
		} else {
			int progress = qBound(0, 100 * m_caughtUp / m_catchupTo, 99);
			if(progress > m_catchupProgress) {
				m_catchupProgress = progress;
				emit catchupProgress(progress);
			}
			m_catchupTimer->start(CATCHUP_TIMER_MSEC);
		}
	}
}

void Client::handleServerReply(const ServerReply &msg, int handledMessageIndex)
{
	const QJsonObject &reply = msg.reply;
	switch(msg.type) {
	case ServerReply::ReplyType::Unknown:
		qWarning() << "Unknown server reply:" << msg.message << reply;
		break;
	case ServerReply::ReplyType::Login:
		qWarning("got login message while in session!");
		break;
	case ServerReply::ReplyType::Message:
	case ServerReply::ReplyType::Alert:
	case ServerReply::ReplyType::Error:
	case ServerReply::ReplyType::Result:
		emit serverMessage(
			translateMessage(reply), msg.type == ServerReply::ReplyType::Alert);
		break;
	case ServerReply::ReplyType::Log: {
		QString time =
			QDateTime::fromString(reply["timestamp"].toString(), Qt::ISODate)
				.toLocalTime()
				.toString(Qt::ISODate);
		QString user = reply["user"].toString();
		QString message = msg.message;
		if(user.isEmpty())
			emit serverLog(QStringLiteral("[%1] %2").arg(time, message));
		else
			emit serverLog(
				QStringLiteral("[%1] %2: %3").arg(time, user, message));
	} break;
	case ServerReply::ReplyType::SessionConf:
		emit sessionConfChange(reply["config"].toObject());
		break;
	case ServerReply::ReplyType::SizeLimitWarning:
		// No longer used since 2.1.0. Replaced by RESETREQUEST
		break;
	case ServerReply::ReplyType::ResetRequest:
		emit autoresetRequested(
			reply["maxSize"].toInt(), reply["query"].toBool());
		break;
	case ServerReply::ReplyType::Status:
		emit serverStatusUpdate(reply["size"].toInt());
		break;
	case ServerReply::ReplyType::Reset:
		handleResetRequest(msg);
		break;
	case ServerReply::ReplyType::Catchup:
		m_catchupTo = reply["count"].toInt();
		m_catchupKey =
			reply.contains("key") ? qMax(0, reply["key"].toInt()) : -1;
		m_server->setSmoothEnabled(false);
		qInfo(
			"Catching up to %d messages with key %d", m_catchupTo,
			m_catchupKey);
		m_caughtUp = 0;
		m_catchupProgress = 0;
		emit catchupProgress(0);
		// Shouldn't happen, but we'll handle catching up to nothing as well.
		if(m_catchupTo <= 0) {
			finishCatchup("caught up to nothing");
		}
		break;
	case ServerReply::ReplyType::CaughtUp:
		if(m_catchupTo > 0 && qMax(0, reply["key"].toInt()) == m_catchupKey) {
			finishCatchup(
				"received caughtup command from server", handledMessageIndex);
		}
		break;
	case ServerReply::ReplyType::BanImpEx:
		if(reply.contains(QStringLiteral("export"))) {
			emit bansExported(
				reply[QStringLiteral("export")].toString().toUtf8());
		} else if(reply.contains(QStringLiteral("imported"))) {
			emit bansImported(
				reply[QStringLiteral("total")].toInt(),
				reply[QStringLiteral("imported")].toInt());
		} else if(reply.contains(QStringLiteral("error"))) {
			emit bansImpExError(
				translateMessage(reply, QStringLiteral("error")));
		} else {
			qWarning() << "Unknown banimpex message:" << reply;
		}
		break;
	case ServerReply::ReplyType::OutOfSpace:
		emit sessionOutOfSpace();
		break;
	}
}

QString
Client::translateMessage(const QJsonObject &reply, const QString &fallbackKey)
{
	QJsonValue keyValue = reply[QStringLiteral("T")];
	if(keyValue.isString()) {
		QString key = keyValue.toString();
		QJsonObject params = reply[QStringLiteral("P")].toObject();
		if(key == net::ServerReply::KEY_BANEXPORT_MODONLY) {
			//: "Plain" meaning "not encrypted."
			return tr("Only moderators can export plain bans.");
		} else if(key == net::ServerReply::KEY_BANEXPORT_SERVERERROR) {
			return tr("Server error.");
		} else if(key == net::ServerReply::KEY_BANEXPORT_UNCONFIGURED) {
			return tr(
				"Exporting encrypted bans not configured on this server.");
		} else if(key == net::ServerReply::KEY_BANEXPORT_UNSUPPORTED) {
			return tr("Exporting encrypted bans not supported by this server.");
		} else if(key == net::ServerReply::KEY_BANIMPORT_CRYPTERROR) {
			return tr(
				"The server couldn't read the import data. This is likely "
				"because it was exported from a different server. You can only "
				"import bans into the same server they were exported from.");
		} else if(key == net::ServerReply::KEY_BANIMPORT_INVALID) {
			return tr("Invalid import data.");
		} else if(key == net::ServerReply::KEY_BANIMPORT_MALFORMED) {
			return tr("Malformed import data.");
		} else if(key == net::ServerReply::KEY_BANIMPORT_UNCONFIGURED) {
			return tr(
				"Importing encrypted bans not configured on this server.");
		} else if(key == net::ServerReply::KEY_BANIMPORT_UNSUPPORTED) {
			return tr("Importing encrypted bans not supported by this server.");
		} else if(key == net::ServerReply::KEY_BAN) {
			return tr("%1 banned by %2.")
				.arg(
					params[QStringLiteral("target")].toString(),
					params[QStringLiteral("by")].toString());
		} else if(key == net::ServerReply::KEY_KICK) {
			return tr("%1 kicked by %2.")
				.arg(
					params[QStringLiteral("target")].toString(),
					params[QStringLiteral("by")].toString());
		} else if(key == net::ServerReply::KEY_OP_GIVE) {
			QString target = params[QStringLiteral("target")].toString();
			QString by = params[QStringLiteral("by")].toString();
			if(by.isEmpty()) {
				return tr("%1 made operator by the server.").arg(target);
			} else {
				return tr("%1 made operator by %2.").arg(target, by);
			}
		} else if(key == net::ServerReply::KEY_OP_TAKE) {
			QString target = params[QStringLiteral("target")].toString();
			QString by = params[QStringLiteral("by")].toString();
			if(by.isEmpty()) {
				return tr("Operator status revoked from %1 by the server.")
					.arg(target);
			} else {
				return tr("Operator status revoked from %1 by %2.")
					.arg(target, by);
			}
		} else if(key == net::ServerReply::KEY_OUT_OF_SPACE) {
			return tr("Session is out of space! To continue drawing, an "
					  "operator must reset it to bring it down to a smaller "
					  "size. This can be done via Session > Reset.");
		} else if(key == net::ServerReply::KEY_RESET_CANCEL) {
			return tr("Session reset cancelled! An operator must unlock the "
					  "canvas and reset the session manually.");
		} else if(key == net::ServerReply::KEY_RESET_FAILED) {
			return tr("Session reset failed! An operator must unlock the "
					  "canvas and reset the session manually.");
		} else if(key == net::ServerReply::KEY_RESET_PREPARE) {
			return tr("Preparing for session reset! Please wait, the session "
					  "should be available again shortly…");
		} else if(key == net::ServerReply::KEY_TERMINATE_SESSION) {
			//: %1 is the name of the moderator.
			return tr("Session terminated by moderator (%1).")
				.arg(params[QStringLiteral("by")].toString());
		} else if(key == net::ServerReply::KEY_TERMINATE_SESSION_ADMIN) {
			//: %1 is the reason given.
			return tr("Session terminated by administrator: %1")
				.arg(params[QStringLiteral("reason")].toString());
		} else if(key == net::ServerReply::KEY_TERMINATE_SESSION_REASON) {
			//: %1 is the name of the moderator, %2 is the reason given.
			return tr("Session terminated by moderator (%1): %2")
				.arg(
					params[QStringLiteral("by")].toString(),
					params[QStringLiteral("reason")].toString());
		} else if(key == net::ServerReply::KEY_TRUST_GIVE) {
			QString target = params[QStringLiteral("target")].toString();
			QString by = params[QStringLiteral("by")].toString();
			if(by.isEmpty()) {
				return tr("%1 trusted by the server.").arg(target);
			} else {
				return tr("%1 trusted by %2.").arg(target, by);
			}
		} else if(key == net::ServerReply::KEY_TRUST_TAKE) {
			QString target = params[QStringLiteral("target")].toString();
			QString by = params[QStringLiteral("by")].toString();
			if(by.isEmpty()) {
				return tr("%1 untrusted by the server.").arg(target);
			} else {
				return tr("%1 untrusted by %2.").arg(target, by);
			}
		}
	}
	return reply[fallbackKey].toString();
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

void Client::handleData(const net::Message &msg)
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

void Client::Client::handleUserInfo(const net::Message &msg, DP_MsgData *md)
{
	size_t size;
	const unsigned char *bytes = DP_msg_data_body(md, &size);
	QJsonParseError err;
	QJsonDocument json = QJsonDocument::fromJson(
		QByteArray::fromRawData(
			reinterpret_cast<const char *>(bytes), compat::castSize(size)),
		&err);
	if(json.isObject()) {
		QJsonObject info = json.object();
		QString type = info["type"].toString();
		if(type == "request_user_info") {
			emit userInfoRequested(msg.contextId());
		} else if(type == "user_info") {
			emit userInfoReceived(msg.contextId(), info);
		} else if(type == "request_current_brush") {
			emit currentBrushRequested(
				msg.contextId(), info["correlator"].toString());
		} else if(type == "current_brush") {
			emit currentBrushReceived(msg.contextId(), info);
		} else {
			qWarning("Unknown user info type '%s'", qUtf8Printable(type));
		}
	} else {
		qWarning(
			"Could not parse JSON as an object: %s",
			qUtf8Printable(err.errorString()));
	}
}

void Client::setSmoothDrainRate(int smoothDrainRate)
{
	m_smoothDrainRate = smoothDrainRate;
	if(m_server) {
		m_server->setSmoothDrainRate(m_smoothDrainRate);
	}
}

void Client::finishCatchup(const char *reason, int handledMessageIndex)
{
	qInfo(
		"Caught up to %d/%d messages with key %d (%s)",
		m_caughtUp + handledMessageIndex, m_catchupTo, m_catchupKey, reason);
	m_catchupTo = 0;
	m_catchupTimer->stop();
	m_server->setSmoothEnabled(true);
	emit catchupProgress(100);
}

void Client::requestBanExport(bool plain)
{
	QJsonObject kwargs;
	if(plain) {
		kwargs[QStringLiteral("plain")] = true;
	}
	sendMessage(
		net::ServerCommand::make(QStringLiteral("export-bans"), {}, kwargs));
}

void Client::requestBanImport(const QString &bans)
{
	sendMessage(
		net::ServerCommand::make(QStringLiteral("import-bans"), {bans}));
}

void Client::requestUpdateAuthList(const QJsonArray &list)
{
	QJsonArray args;
	args.append(list);
	sendMessage(net::ServerCommand::make(QStringLiteral("auth-list"), args));
}

}
