// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/server.h"
#include "libclient/net/client.h"
#include "libclient/net/login.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/whatismyip.h"
#include <QDebug>
#include <QMetaEnum>
#include <QUrlQuery>
#ifdef HAVE_TCPSOCKETS
#	include "libclient/net/tcpserver.h"
#endif
#ifdef HAVE_WEBSOCKETS
#	include "libclient/net/websocketserver.h"
#endif

namespace net {

Server *
Server::make(const QUrl &url, int timeoutSecs, int proxyMode, Client *client)
{
#if defined(HAVE_WEBSOCKETS) && defined(HAVE_TCPSOCKETS)
	if(url.scheme().startsWith(QStringLiteral("ws"), Qt::CaseInsensitive)) {
		return new WebSocketServer(timeoutSecs, proxyMode, client);
	} else {
		return new TcpServer(timeoutSecs, proxyMode, client);
	}
#elif defined(HAVE_TCPSOCKETS)
	Q_UNUSED(url);
	return new TcpServer(timeoutSecs, proxyMode, client);
#elif defined(HAVE_WEBSOCKETS)
	Q_UNUSED(url);
	return new WebSocketServer(timeoutSecs, proxyMode, client);
#else
#	error "No socket implementation compiled in."
#endif
}

QString Server::addSchemeToUserSuppliedAddress(const QString &remoteAddress)
{
	bool haveValidScheme =
		remoteAddress.startsWith(
			QStringLiteral("drawpile://"), Qt::CaseInsensitive) ||
		remoteAddress.startsWith(
			QStringLiteral("ws://"), Qt::CaseInsensitive) ||
		remoteAddress.startsWith(QStringLiteral("wss://"), Qt::CaseInsensitive);
	if(haveValidScheme) {
		return remoteAddress;
	} else {
		return QStringLiteral("drawpile://") + remoteAddress;
	}
}

QUrl Server::fixUpAddress(const QUrl &originalUrl, bool join)
{
#ifdef HAVE_TCPSOCKETS
	Q_UNUSED(join);
#else
	if(originalUrl.scheme().compare(
		   QStringLiteral("drawpile"), Qt::CaseInsensitive) == 0) {
		QUrl url = originalUrl;
		url.setScheme(
			WhatIsMyIp::looksLikeLocalhost(url.host()) ? QStringLiteral("ws")
													   : QStringLiteral("wss"));
		url.setPort(-1);

		QString path = url.path();
		url.setPath(QStringLiteral("/drawpile-web/ws"));

		if(join) {
			QString autoJoinId = extractAutoJoinId(path);
			if(!autoJoinId.isEmpty()) {
				QUrlQuery query(url);
				query.removeAllQueryItems(QStringLiteral("session"));
				query.addQueryItem(QStringLiteral("session"), autoJoinId);
				url.setQuery(query);
			}
		}

		qInfo(
			"Fixed up TCP URL '%s' to WebSocket URL '%s'",
			qUtf8Printable(originalUrl.toDisplayString()),
			qUtf8Printable(url.toDisplayString()));
		return url;
	}
#endif

#ifndef HAVE_WEBSOCKETS
	if(originalUrl.scheme().compare(
		   QStringLiteral("ws"), Qt::CaseInsensitive) == 0 ||
	   originalUrl.scheme().compare(
		   QStringLiteral("wss"), Qt::CaseInsensitive) == 0) {
		QUrl url = originalUrl;
		url.setScheme(QStringLiteral("drawpile://"));
		url.setPort(-1);
		url.setPath(QString());
		qInfo(
			"Fixed up WebSocket URL '%s' to TCP URL '%s'",
			qUtf8Printable(originalUrl.toDisplayString()),
			qUtf8Printable(url.toDisplayString()));
		return url;
	}
#endif

	return originalUrl;
}

QString Server::extractAutoJoinId(const QString &path)
{
	if(path.length() > 1) {
		QRegularExpression idre("\\A/?([a-zA-Z0-9:-]{1,64})/?\\z");
		QRegularExpressionMatch m = idre.match(path);
		if(m.hasMatch()) {
			return m.captured(1);
		}
	}
	return QString();
}

Server::Server(Client *client)
	: QObject(client)
	, m_client(client)
{
}

void Server::sendMessage(const net::Message &msg)
{
	messageQueue()->send(msg);
}

void Server::sendMessages(int count, const net::Message *msgs)
{
	messageQueue()->sendMultiple(count, msgs);
}

void Server::login(LoginHandler *login)
{
	m_loginstate = login;
	m_loginstate->setParent(this);
	m_loginstate->setServer(this);
	QUrl url = login->url();
	emit initiatingConnection(url);
	connectToHost(url);
}

void Server::logout()
{
	m_localDisconnect = true;
	messageQueue()->sendDisconnect(
		MessageQueue::GracefulDisconnect::Shutdown, QString());
}

int Server::uploadQueueBytes() const
{
	return messageQueue()->uploadQueueBytes();
}

void Server::handleDisconnect()
{
	emit serverDisconnected(
		m_error, m_errorcode, m_localDisconnect,
		!m_loginstate || m_loginstate->anyMessageReceived());
}

void Server::handleSocketStateChange(QAbstractSocket::SocketState state)
{
	switch(state) {
	case QAbstractSocket::ClosingState:
		emit loggingOut();
		break;
	case QAbstractSocket::UnconnectedState:
		handleDisconnect();
		break;
	default:
		break;
	}
}

void Server::handleSocketError()
{
	QString errorString = socketErrorStringWithCode();
	qWarning() << "Socket error:" << errorString;
	QAbstractSocket::SocketError errorCode = socketError();
	if(errorCode != QAbstractSocket::RemoteHostClosedError) {
		if(m_error.isEmpty()) {
			m_error = errorString;
			switch(errorCode) {
			case QAbstractSocket::ProxyAuthenticationRequiredError:
			case QAbstractSocket::ProxyConnectionRefusedError:
			case QAbstractSocket::ProxyConnectionClosedError:
			case QAbstractSocket::ProxyConnectionTimeoutError:
			case QAbstractSocket::ProxyNotFoundError:
			case QAbstractSocket::ProxyProtocolError:
				m_error += QStringLiteral("\n\n") +
						   tr("If you don't intend to use a proxy, you can "
							  "disable the network proxy in Drawpile's "
							  "preferences under the Network tab.");
				break;
			default:
				break;
			}
		}

		if(isConnected()) {
			disconnectFromHost();
		} else {
			handleDisconnect();
		}
	}
}

void Server::handleReadError()
{
	QString errorString = socketErrorStringWithCode();
	qWarning() << "Socket read error:" << errorString;
	if(m_error.isEmpty()) {
		if(errorString.isEmpty()) {
			m_error = tr("Network read error");
		} else {
			m_error = tr("Network read error: %1").arg(errorString);
		}
	}
}

void Server::handleWriteError()
{
	QString errorString = socketErrorStringWithCode();
	qWarning() << "Socket write error:" << errorString;
	if(m_error.isEmpty()) {
		if(errorString.isEmpty()) {
			m_error = tr("Network write error");
		} else {
			m_error = tr("Network write error: %1").arg(errorString);
		}
	}
}

void Server::handleTimeout(qint64 idleTimeout)
{
	qWarning() << "Message queue exceeded timeout of" << idleTimeout << "ms";
	if(m_error.isEmpty()) {
		m_error = tr("Network connection timed out");
	}
}

void Server::connectMessageQueue(MessageQueue *mq)
{
#ifdef __EMSCRIPTEN__
	// AutoConnection doesn't work here in Emscripten.
	Qt::ConnectionType connectionType = Qt::QueuedConnection;
#else
	Qt::ConnectionType connectionType = Qt::AutoConnection;
#endif
	connect(
		mq, &MessageQueue::messageAvailable, this, &Server::handleMessage,
		connectionType);
	connect(
		mq, &MessageQueue::bytesReceived, this, &Server::bytesReceived,
		connectionType);
	connect(
		mq, &MessageQueue::bytesSent, this, &Server::bytesSent, connectionType);
	connect(
		mq, &MessageQueue::badData, this, &Server::handleBadData,
		connectionType);
	connect(
		mq, &MessageQueue::readError, this, &Server::handleReadError,
		connectionType);
	connect(
		mq, &MessageQueue::writeError, this, &Server::handleWriteError,
		connectionType);
	connect(
		mq, &MessageQueue::timedOut, this, &Server::handleTimeout,
		connectionType);
	connect(
		mq, &MessageQueue::pingPong, this, &Server::lagMeasured,
		connectionType);
	connect(
		mq, &MessageQueue::gracefulDisconnect, this,
		&Server::gracefullyDisconnecting, connectionType);
}

void Server::handleMessage()
{
	messageQueue()->receive(m_receiveBuffer);
	int count = m_receiveBuffer.count();
	if(count != 0) {
		if(m_loginstate) {
			// Drip feed messages one by one to the login handler,
			// since the inbox may contain messages not belonging to
			// the login handshake anymore.
			for(int i = 0; i < count; ++i) {
				const ServerReply sr =
					ServerReply::fromMessage(m_receiveBuffer[i]);
				const bool expectMoreLogin = m_loginstate->receiveMessage(sr);
				const int offset = i + 1;
				if(!expectMoreLogin && offset < count) {
					m_client->handleMessages(
						count - offset, m_receiveBuffer.data() + offset);
					break;
				}
			}
		} else {
			m_client->handleMessages(count, m_receiveBuffer.data());
		}
		m_receiveBuffer.clear();
	}
}

void Server::handleBadData(int len, int type, int contextId)
{
	qWarning() << "Received" << len
			   << "bytes of unknown or invalid message type" << type
			   << "from context ID" << contextId;
	if(type < 64 || contextId == 0) {
		// If message type is Transparent, the bad data came from the server.
		// Something is wrong for sure.
		m_error = tr("Received invalid data");
		abortConnection();
	} else {
		// Opaque messages are merely passed along by the server.
		// TODO autokick misbehaving clients?
	}
}

void Server::loginSuccess()
{
	qDebug() << "logged in to session" << m_loginstate->url() << ". Got user id"
			 << m_loginstate->userId();

	m_supportsPersistence = m_loginstate->supportsPersistence();
	m_supportsCryptBanImpEx = m_loginstate->supportsCryptBanImEx();
	m_supportsModBanImpEx = m_loginstate->supportsModBanImEx();
	m_supportsAbuseReports = m_loginstate->supportsAbuseReports();
	messageQueue()->setContextId(m_loginstate->userId());

	emit loggedIn(
		m_loginstate->url(), m_loginstate->userId(),
		m_loginstate->mode() == LoginHandler::Mode::Join,
		m_loginstate->isAuthenticated(), m_loginstate->userFlags(),
		!m_loginstate->sessionFlags().contains("NOAUTORESET"),
		m_loginstate->compatibilityMode(), m_loginstate->joinPassword(),
		m_loginstate->authId());

	m_loginstate->deleteLater();
	m_loginstate = nullptr;
}

void Server::loginFailure(const QString &message, const QString &errorcode)
{
	qWarning() << "Login failed:" << message;
	m_error = message;
	m_errorcode = errorcode;
	m_localDisconnect = errorcode == "CANCELLED";
	disconnectFromHost();
}

QString Server::socketErrorStringWithCode() const
{
	int errorCode = int(socketError());
	QString errorString = socketErrorString();
	if(errorString.isEmpty()) {
		const char *errorCodeKey =
			QMetaEnum::fromType<QAbstractSocket::SocketError>().key(errorCode);
		QString errorCodeString = errorCodeKey ? QString::fromUtf8(errorCodeKey)
											   : QStringLiteral("Invalid");
		//: This is a network socket error message. %1 is an error code number,
		//: %2 is the English name for the error code.
		return tr("Socket error %1: %2").arg(errorCode).arg(errorCodeString);
	} else {
		//: This is a network socket error message. %1 is the error message, %2
		//: is an error code number.
		return tr("%1 (error %2)").arg(errorString).arg(errorCode);
	}
}

}
