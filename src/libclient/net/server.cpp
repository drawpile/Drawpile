// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/server.h"
#include "libclient/net/login.h"
#include "libshared/net/servercmd.h"
#include <QDebug>
#ifdef HAVE_TCPSOCKETS
#	include "libclient/net/tcpserver.h"
#endif

namespace net {

Server *Server::make(const QUrl &url, int timeoutSecs, QObject *parent)
{
#if defined(HAVE_TCPSOCKETS)
	Q_UNUSED(url);
	return new TcpServer(timeoutSecs, parent);
#else
#	error "No socket implementation compiled in."
#endif
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
	if(state == QAbstractSocket::ClosingState) {
		emit loggingOut();
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

void Server::connectMessageQueue(MessageQueue *mq)
{
	connect(mq, &MessageQueue::messageAvailable, this, &Server::handleMessage);
	connect(mq, &MessageQueue::bytesReceived, this, &Server::bytesReceived);
	connect(mq, &MessageQueue::bytesSent, this, &Server::bytesSent);
	connect(mq, &MessageQueue::badData, this, &Server::handleBadData);
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
		m_loginstate->isAuthenticated(), m_loginstate->hasUserFlag("MOD"),
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
