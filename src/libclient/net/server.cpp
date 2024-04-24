// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/server.h"
#include "libclient/net/login.h"
#include "libshared/net/servercmd.h"
#include <QDebug>
#ifdef HAVE_TCPSOCKETS
#	include "libclient/net/tcpserver.h"
#endif
#ifdef HAVE_WEBSOCKETS
#	include "libclient/net/websocketserver.h"
#endif

namespace net {

Server *Server::make(const QUrl &url, int timeoutSecs, QObject *parent)
{
#if defined(HAVE_WEBSOCKETS) && defined(HAVE_TCPSOCKETS)
	if(url.scheme().startsWith(QStringLiteral("ws"), Qt::CaseInsensitive)) {
		return new WebSocketServer(timeoutSecs, parent);
	} else {
		return new TcpServer(timeoutSecs, parent);
	}
#elif defined(HAVE_TCPSOCKETS)
	Q_UNUSED(url);
	return new TcpServer(timeoutSecs, parent);
#elif defined(HAVE_WEBSOCKETS)
	Q_UNUSED(url);
	return new WebSocketServer(timeoutSecs, parent);
#else
#	error "No socket implementation compiled in."
#endif
}

QString Server::addSchemeToUserSuppliedAddress(const QString &remoteAddress)
{
	bool haveValidScheme = false;
#ifdef HAVE_TCPSOCKETS
	haveValidScheme = haveValidScheme ||
					  remoteAddress.startsWith(
						  QStringLiteral("drawpile://"), Qt::CaseInsensitive);
#endif
#ifdef HAVE_WEBSOCKETS
	haveValidScheme =
		haveValidScheme ||
		remoteAddress.startsWith(
			QStringLiteral("ws://"), Qt::CaseInsensitive) ||
		remoteAddress.startsWith(QStringLiteral("wss://"), Qt::CaseInsensitive);
#endif

	if(haveValidScheme) {
		return remoteAddress;
	} else {
#if defined(HAVE_TCPSOCKETS)
		return QStringLiteral("drawpile://") + remoteAddress;
#elif defined(HAVE_WEBSOCKETS)
		bool looksLikeLocalhost =
			remoteAddress.startsWith(
				QStringLiteral("localhost"), Qt::CaseInsensitive) ||
			remoteAddress.startsWith("127.0.0.1") ||
			remoteAddress.startsWith("::1");
		QString scheme =
			looksLikeLocalhost ? QStringLiteral("ws") : QStringLiteral("wss");
		QUrl url(scheme + QStringLiteral("://") + remoteAddress);
		if(url.path().isEmpty() || url.path() == QStringLiteral("/")) {
			url.setPath(QStringLiteral("/drawpile-web/ws"));
		}
		return url.toString();
#endif
	}
}

Server::Server(QObject *parent)
	: QObject(parent)
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
	connectToHost(login->url());
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
	emit serverDisconnected(m_error, m_errorcode, m_localDisconnect);
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
	QString errorString = socketErrorString();
	qWarning() << "Socket error:" << errorString;
	if(socketError() != QAbstractSocket::RemoteHostClosedError) {
		if(m_error.isEmpty()) {
			m_error = socketErrorString();
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
	QString errorString = socketErrorString();
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
	QString errorString = socketErrorString();
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
	connect(mq, &MessageQueue::messageAvailable, this, &Server::handleMessage);
	connect(mq, &MessageQueue::bytesReceived, this, &Server::bytesReceived);
	connect(mq, &MessageQueue::bytesSent, this, &Server::bytesSent);
	connect(mq, &MessageQueue::badData, this, &Server::handleBadData);
	connect(mq, &MessageQueue::readError, this, &Server::handleReadError);
	connect(mq, &MessageQueue::writeError, this, &Server::handleWriteError);
	connect(mq, &MessageQueue::timedOut, this, &Server::handleTimeout);
	connect(mq, &MessageQueue::pingPong, this, &Server::lagMeasured);
	connect(
		mq, &MessageQueue::gracefulDisconnect, this,
		&Server::gracefullyDisconnecting);
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
					emit messagesReceived(
						count - offset, m_receiveBuffer.data() + offset);
					break;
				}
			}
		} else {
			emit messagesReceived(count, m_receiveBuffer.data());
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

}
