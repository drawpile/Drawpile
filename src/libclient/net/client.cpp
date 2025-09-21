// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/client.h"
#include "libclient/net/login.h"
#include "libclient/net/message.h"
#include "libclient/net/server.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/qtcompat.h"
#include <QDebug>
#include <QLoggingCategory>
#include <QTimer>
#ifdef Q_OS_ANDROID
#	include "libshared/util/androidutils.h"
#endif

Q_LOGGING_CATEGORY(lcDpClient, "net.drawpile.client", QtWarningMsg)

namespace net {

Client::CommandHandler::~CommandHandler() {}

Client::Client(CommandHandler *commandHandler, QObject *parent)
	: QObject(parent)
	, m_commandHandler(commandHandler)
	, m_catchupTimer(new QTimer(this))
{
	m_catchupTimer->setSingleShot(true);
	m_catchupTimer->setTimerType(Qt::CoarseTimer);
	connect(m_catchupTimer, &QTimer::timeout, this, &Client::nudgeCatchup);
}

void Client::connectToServer(
	int timeoutSecs, int proxyMode, LoginHandler *loginhandler, bool builtin)
{
	Q_ASSERT(!isConnected());
	m_builtin = builtin;
	m_timeoutSecs = timeoutSecs;
	m_proxyMode = proxyMode;
	connectToServerInternal(loginhandler, false, false);
}

void Client::connectToServerInternal(
	net::LoginHandler *loginhandler, bool redirect, bool transparent)
{
	qCDebug(lcDpClient) << "Connect to" << loginhandler->url() << "redirect"
						<< redirect << "transparent" << transparent
						<< "connected" << isConnected();
	Q_ASSERT(!isConnected());

	m_server =
		Server::make(loginhandler->url(), m_timeoutSecs, m_proxyMode, this);
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

#	ifdef DRAWPILE_USE_CONNECT_SERVICE
	if(!m_connectServiceStarted) {
		m_connectServiceStarted = true;
		utils::startConnectService();
	}
#	endif
#endif

	m_connections.clear();
	m_connections.append(connect(
		m_server, &Server::initiatingConnection, this,
		&Client::setConnectionUrl));
	m_connections.append(connect(
		m_server, &Server::loggingOut, this, &Client::serverDisconnecting));
	m_connections.append(connect(
		m_server, &Server::serverDisconnected, this,
		&Client::handleDisconnect));
	m_connections.append(
		connect(m_server, &Server::loggedIn, this, &Client::handleConnect));
	m_connections.append(connect(
		m_server, &Server::bytesReceived, this, &Client::bytesReceived));
	m_connections.append(
		connect(m_server, &Server::bytesSent, this, &Client::bytesSent));
	m_connections.append(
		connect(m_server, &Server::lagMeasured, this, &Client::lagMeasured));
	m_connections.append(connect(
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
			emit serverMessage(chat, int(ServerMessageType::Alert));
		}));

	m_connections.append(connect(
		loginhandler, &LoginHandler::redirectRequested, this,
		&Client::handleRedirect));

	if(!redirect) {
		emit serverConnected(loginhandler->url());
	} else if(!transparent) {
		emit serverRedirected(loginhandler->url());
	}
	m_server->login(loginhandler);

	m_catchupTo = 0;
	m_caughtUp = 0;
	m_catchupProgress = 0;
}

void Client::disconnectFromServer()
{
	if(m_server) {
		qCDebug(lcDpClient, "Disconnect from connected server");
		m_server->logout();
	} else {
		qCDebug(lcDpClient, "Disconnect from server, but not connected");
	}
}

QUrl Client::sessionUrl(bool includeUser) const
{
	if(includeUser) {
		return m_lastUrl;
	} else {
		QUrl url = Server::stripInviteCodeFromUrl(m_lastUrl);
		url.setUserInfo(QString());
		return url;
	}
}

void Client::handleConnect(const LoggedInParams &params)
{
	m_lastUrl = params.url;
	m_myId = params.userId;
	m_userFlags = UserFlag::None;
	for(const QString &userFlag : params.userFlags) {
		if(userFlag == QStringLiteral("MOD")) {
			m_userFlags.setFlag(UserFlag::Mod);
		} else if(userFlag == QStringLiteral("WEBSESSION")) {
			m_userFlags.setFlag(UserFlag::WebSession);
		}
	}
	m_isAuthenticated = params.auth;
	m_supportsAutoReset = params.supportsAutoReset;
	m_supportsSkipCatchup = params.supportsSkipCatchup;
	m_compatibilityMode = params.compatibilityMode;
	m_historyIndex.clear();

	emit serverLoggedIn(params);
}

void Client::handleDisconnect(
	const QString &message, const QString &errorcode, bool localDisconnect,
	bool anyMessageReceived)
{
	bool connected = isConnected();
	qCDebug(lcDpClient) << "Handle disconnect connected" << connected
						<< "localDisconnect" << localDisconnect
						<< "anyMessageReceived" << anyMessageReceived
						<< "errorcode" << errorcode << "message" << message;
	if(connected) {
		emit serverDisconnected(
			message, errorcode, localDisconnect, anyMessageReceived);
		m_compatibilityMode = false;
		m_server->deleteLater();
		m_server = nullptr;
		m_userFlags = UserFlag::None;
#ifdef Q_OS_ANDROID
		delete m_wakeLock;
		delete m_wifiLock;
		m_wakeLock = nullptr;
		m_wifiLock = nullptr;
#	ifdef DRAWPILE_USE_CONNECT_SERVICE
		if(m_connectServiceStarted) {
			utils::stopConnectService();
		}
#	endif
#endif
	} else {
		// WebSockets may report multiple disconnects, sometimes amending error
		// information to them. So emit those too.
		emit serverDisconnectedAgain(message, errorcode);
	}
}

void Client::handleRedirect(
	const QSharedPointer<const net::LoginHostParams> &hostParams,
	const QString &autoJoinId, const QUrl &url, quint64 redirectNonce,
	const QStringList &redirectHistory, const QJsonObject &redirectData,
	bool late)
{
	bool connected = isConnected();
	qCDebug(lcDpClient) << "Handle redirect connected" << connected << "url"
						<< url << "redirectData" << redirectData;
	if(connected) {
		for(const QMetaObject::Connection &connection : m_connections) {
			disconnect(connection);
		}
		m_connections.clear();

		net::LoginHandler *loginhandler = new net::LoginHandler(
			hostParams, autoJoinId, url, redirectNonce, redirectHistory,
			redirectData, this);
		m_server->replaceWithRedirect(loginhandler, late);

		connect(
			m_server, &Server::serverDisconnected, m_server,
			&Server::deleteLater);
		m_server->logout();
		m_server = nullptr;

		connectToServerInternal(
			loginhandler, true,
			redirectData.value(QStringLiteral("transparent")).toBool());
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


void Client::sendCommands(int count, const net::Message *msgs)
{
	emit commandsAboutToSend();
	sendMessages(count, msgs);
}

void Client::sendMessage(const net::Message &msg)
{
	sendMessages(1, &msg);
}

void Client::sendMessages(int count, const net::Message *msgs)
{
	if(count > 0) {
		m_commandHandler->handleLocalCommands(count, msgs);
		// Note: we could only send only local commands here when offline, but
		// do the remote part anyway to not have to deal with two code paths.
		QVector<net::Message> matched = replaceLocalMatchMessages(count, msgs);
		int matchedCount = matched.size();
		if(matchedCount == 0) {
			sendRemoteMessages(count, msgs);
		} else {
			Q_ASSERT(matchedCount == count);
			sendRemoteMessages(matchedCount, matched.constData());
		}
	}
}

void Client::sendResetMessage(const net::Message &msg)
{
	sendResetMessages(1, &msg);
}

void Client::sendResetMessages(int count, const net::Message *msgs)
{
	if(count > 0) {
		sendRemoteMessages(count, msgs);
	}
}

void Client::sendLocalMessages(int count, const net::Message *msgs)
{
	if(count > 0) {
		m_commandHandler->handleLocalCommands(count, msgs);
	}
}

void Client::sendRemoteMessages(int count, const net::Message *msgs)
{
	Q_ASSERT(count > 0);
	if(m_server) {
		m_server->sendMessages(count, msgs);
	} else {
		m_commandHandler->handleCommands(count, msgs);
	}
}

QVector<net::Message>
Client::replaceLocalMatchMessages(int count, const net::Message *msgs)
{
	// If we're connected to a thick server, we don't want to send it unknown
	// message types because that'll get us kicked.
	bool shouldDisguiseAsPutImage = seemsConnectedToThickServer();
	for(int i = 0; i < count; ++i) {
		DP_Message *first =
			net::makeLocalMatchMessage(msgs[i], shouldDisguiseAsPutImage);
		if(first) {
			QVector<net::Message> matched;
			matched.reserve(count);
			for(int j = 0; j < i; ++j) {
				matched.append(msgs[j]);
			}
			matched.append(net::Message::noinc(first));

			for(int j = i + 1; j < count; ++j) {
				DP_Message *m = net::makeLocalMatchMessage(
					msgs[j], shouldDisguiseAsPutImage);
				matched.append(m ? net::Message::noinc(m) : msgs[j]);
			}

			return matched;
		}
	}
	return {};
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

	int handled = 0;
	for(int i = 0; i < count; ++i) {
		net::Message &msg = msgs[i];

		if(m_supportsSkipCatchup && msg.isAddedToHistory()) {
			m_historyIndex.incrementHistoryPos();
		}

		switch(msg.type()) {
		case DP_MSG_SERVER_COMMAND: {
			int handleCount = i - handled;
			if(handleCount > 0) {
				m_commandHandler->handleCommands(handleCount, msgs + handled);
			}
			handled = i;
			handleServerReply(ServerReply::fromMessage(msg), i);
			break;
		}
		case DP_MSG_DATA:
			handleData(msg);
			break;
		default:
			break;
		}
	}

	int handleCount = count - handled;
	if(handleCount > 0) {
		m_commandHandler->handleCommands(handleCount, msgs + handled);
	}

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

	if(m_supportsSkipCatchup) {
		// hidx is in messages that get added to the history.
		long long delta = 1LL;
		HistoryIndex hi = HistoryIndex::fromString(
			msg.reply.value(QStringLiteral("hidx")).toString());

		if(!hi.isValid()) {
			// hid* is in messages that not added to the history.
			delta = 0LL;
			hi = HistoryIndex::fromString(
				msg.reply.value(QStringLiteral("hid*")).toString());
		}

		if(hi.isValid()) {
			if(m_historyIndex.isValid() &&
			   m_historyIndex.sessionId() == hi.sessionId() &&
			   m_historyIndex.startId() == hi.startId()) {
				long long prevHistoryPos = m_historyIndex.historyPos();
				long long nextHistoryPos = hi.historyPos();
				if(prevHistoryPos + delta != nextHistoryPos) {
					// Miscounted messages somewhere, this would lead to a
					// desync when reconnecting and skipping catchup.
					qWarning(
						"Unexpected history position %lld at %lld (+%lld)",
						nextHistoryPos, prevHistoryPos, delta);
				}
			}
			m_historyIndex = hi;
		}
	}

	switch(msg.type) {
	case ServerReply::ReplyType::Unknown:
		qWarning() << "Unknown server reply:" << msg.message << reply;
		break;
	case ServerReply::ReplyType::Login:
		qWarning("got login message while in session!");
		break;
	case ServerReply::ReplyType::Message:
	case ServerReply::ReplyType::Error:
	case ServerReply::ReplyType::Result:
		emit serverMessage(
			translateMessage(reply), int(ServerMessageType::Message));
		break;
	case ServerReply::ReplyType::Alert: {
		bool preparing = reply[QStringLiteral("reset")].toString() ==
						 QStringLiteral("prepare");
		emit serverPreparingReset(preparing);
		emit serverMessage(
			translateMessage(reply),
			int(preparing ? ServerMessageType::ResetNotice
						  : ServerMessageType::Alert));
		break;
	}
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
	case ServerReply::ReplyType::ResetRequest: {
		int maxSize = reply[QStringLiteral("maxSize")].toInt();
		if(reply[QStringLiteral("query")].toBool()) {
			emit autoresetQueried(
				maxSize, reply[QStringLiteral("payload")].toString());
		} else {
			emit autoresetRequested(
				reply[QStringLiteral("maxSize")].toInt(),
				reply[QStringLiteral("correlator")].toString(),
				reply[QStringLiteral("stream")].toString());
		}
		break;
	}
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
	case ServerReply::ReplyType::StreamStart:
		emit streamResetStarted(reply[QStringLiteral("correlator")].toString());
		break;
	case ServerReply::ReplyType::StreamProgress:
		emit streamResetProgressed(reply[QStringLiteral("cancel")].toBool());
		break;
	case ServerReply::ReplyType::PasswordChange:
		emit sessionPasswordChanged(
			reply.value(QStringLiteral("password")).toString());
		break;
	case ServerReply::ReplyType::InviteCreated:
		emit inviteCodeCreated(
			reply.value(QStringLiteral("secret")).toString());
		break;
	case net::ServerReply::ReplyType::Thumbnail:
		if(reply.value(QStringLiteral("query")).toBool()) {
			emit thumbnailQueried(
				reply.value(QStringLiteral("payload")).toString());
		} else {
			emit thumbnailRequested(
				reply.value(QStringLiteral("correlator")).toString().toUtf8(),
				reply.value(QStringLiteral("maxWidth")).toInt(),
				reply.value(QStringLiteral("maxHeight")).toInt(),
				reply.value(QStringLiteral("quality")).toInt(),
				reply.value(QStringLiteral("format")).toString());
		}
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
		} else if(key == net::ServerReply::KEY_KICK_WEB_USERS) {
			QString by = params[QStringLiteral("by")].toString();
			if(by.isEmpty()) {
				return tr(
					"Session password removed by a server administrator. "
					"This server doesn't allow web browsers in public "
					"sessions, they will be disconnected.");
			} else {
				return tr("Session password removed by %1. This server doesn't "
						  "allow web browsers in public sessions, they will be "
						  "disconnected.")
					.arg(by);
			}
		} else if(key == net::ServerReply::KEY_OP_GIVE) {
			QString target = params[QStringLiteral("target")].toString();
			QString by = params[QStringLiteral("by")].toString();
			if(by.isEmpty()) {
				return tr("%1 made operator by the server.").arg(target);
			} else {
				return tr("%1 made operator by %2.").arg(target, by);
			}
		} else if(key == net::ServerReply::KEY_OP_GIVE_INVITE) {
			QString target = params[QStringLiteral("target")].toString();
			QString creator = params[QStringLiteral("creator")].toString();
			if(creator.isEmpty()) {
				return tr("%1 made operator via invite created by a server "
						  "administrator.")
					.arg(target);
			} else {
				return tr("%1 made operator via invite created by %2.")
					.arg(target, creator);
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
			return tr(
				"Session is out of space! To continue drawing, an "
				"operator must reset it to bring it down to a smaller "
				"size. This can be done via Session > Reset.");
		} else if(key == net::ServerReply::KEY_RESET_CANCEL) {
			return tr(
				"Session reset cancelled! An operator must unlock the "
				"canvas and reset the session manually.");
		} else if(key == net::ServerReply::KEY_RESET_FAILED) {
			return tr(
				"Session reset failed! An operator must unlock the "
				"canvas and reset the session manually.");
		} else if(key == net::ServerReply::KEY_RESET_PREPARE) {
			return tr(
				"Preparing for session reset! Please wait, the session "
				"should be available again shortly…");
		} else if(key == net::ServerReply::KEY_RESET_PREPARE_BY) {
			// %1 is the name of the operator resetting the canvas.
			return tr("Preparing for session reset by %1! Please wait, the "
					  "session should be available again shortly…")
				.arg(params[QStringLiteral("name")].toString());
		} else if(key == net::ServerReply::KEY_RESET_PREPARE_CURRENT) {
			// %1 is the name of the operator reverting the canvas.
			return tr("%1 is compressing the canvas! Please wait, the session "
					  "should be available again shortly…")
				.arg(params[QStringLiteral("name")].toString());
		} else if(key == net::ServerReply::KEY_RESET_PREPARE_EXTERNAL) {
			// %1 is the name of the operator replacing the canvas.
			return tr("%1 is replacing the canvas! Please wait, the session "
					  "should be available again shortly…")
				.arg(params[QStringLiteral("name")].toString());
		} else if(key == net::ServerReply::KEY_RESET_PREPARE_PAST) {
			// %1 is the name of the operator reverting the canvas.
			return tr("%1 is reverting the canvas to a previous state! Please "
					  "wait, the session should be available again shortly…")
				.arg(params[QStringLiteral("name")].toString());
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
		} else if(key == net::ServerReply::KEY_TRUST_GIVE_INVITE) {
			QString target = params[QStringLiteral("target")].toString();
			QString creator = params[QStringLiteral("creator")].toString();
			if(creator.isEmpty()) {
				return tr("%1 trusted via invite created by a server "
						  "administrator.")
					.arg(target);
			} else {
				return tr("%1 trusted via invite created by %2.")
					.arg(target, creator);
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
