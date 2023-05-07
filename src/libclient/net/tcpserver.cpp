// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/net/tcpserver.h"
#include "libclient/net/login.h"
#include "libclient/net/messagequeue.h"
#include "libclient/net/servercmd.h"
#include "libshared/util/qtcompat.h"
#include "cmake-config/config.h"

#include <QDebug>
#include <QSslSocket>
#include <QSslConfiguration>

namespace net {

TcpServer::TcpServer(int timeoutSecs, QObject *parent) :
	Server(parent), m_loginstate(nullptr), m_securityLevel(NO_SECURITY),
	m_localDisconnect(false), m_supportsPersistence(false)
{
	m_socket = new QSslSocket(this);

	QSslConfiguration sslconf = m_socket->sslConfiguration();
	sslconf.setSslOption(QSsl::SslOptionDisableCompression, false);
	m_socket->setSslConfiguration(sslconf);

	m_msgqueue = new MessageQueue(m_socket, this);

	m_msgqueue->setIdleTimeout(timeoutSecs * 1000);
	m_msgqueue->setPingInterval(15 * 1000);

	connect(m_socket, &QSslSocket::disconnected, this, &TcpServer::handleDisconnect);
	connect(m_socket, compat::SocketError, this, &TcpServer::handleSocketError);
	connect(m_socket, &QSslSocket::stateChanged, this, [this](QAbstractSocket::SocketState state) {
		if(state==QAbstractSocket::ClosingState)
			emit loggingOut();
	});

	connect(m_msgqueue, &MessageQueue::messageAvailable, this, &TcpServer::handleMessage);
	connect(m_msgqueue, &MessageQueue::bytesReceived, this, &TcpServer::bytesReceived);
	connect(m_msgqueue, &MessageQueue::bytesSent, this, &TcpServer::bytesSent);
	connect(m_msgqueue, &MessageQueue::badData, this, &TcpServer::handleBadData);
	connect(m_msgqueue, &MessageQueue::pingPong, this, &TcpServer::lagMeasured);
	connect(m_msgqueue, &MessageQueue::gracefulDisconnect, this, &TcpServer::gracefullyDisconnecting);
}

void TcpServer::login(LoginHandler *login)
{
	m_loginstate = login;
	m_loginstate->setParent(this);
	m_loginstate->setServer(this);
	m_socket->connectToHost(login->url().host(), login->url().port(cmake_config::proto::port()));
}

void TcpServer::logout()
{
	m_localDisconnect = true;
	m_msgqueue->sendDisconnect(MessageQueue::GracefulDisconnect::Shutdown, QString());
}

void TcpServer::artificialDisconnect()
{
	m_socket->abort();
}

int TcpServer::uploadQueueBytes() const
{
	return m_msgqueue->uploadQueueBytes();
}

void TcpServer::sendMessage(const drawdance::Message &msg)
{
	m_msgqueue->send(msg);
}

void TcpServer::sendMessages(int count, const drawdance::Message *msgs)
{
	m_msgqueue->sendMultiple(count, msgs);
}

void TcpServer::handleMessage()
{
	m_msgqueue->receive(m_receiveBuffer);
	int count = m_receiveBuffer.count();
	if(count != 0) {
		if(m_loginstate) {
			// Drip feed messages one by one to the login handler,
			// since the inbox may contain messages not belonging to
			// the login handshake anymore.
			for(int i = 0; i < count; ++i) {
				const ServerReply sr = ServerReply::fromMessage(m_receiveBuffer[i]);
				const bool expectMoreLogin = m_loginstate->receiveMessage(sr);
				const int offset = i + 1;
				if(!expectMoreLogin && offset < count) {
					emit messagesReceived(
						count - offset, m_receiveBuffer.constData() + offset);
					break;
				}
			}
		} else {
			emit messagesReceived(count, m_receiveBuffer.constData());
		}
		m_receiveBuffer.clear();
	}
}

void TcpServer::handleBadData(int len, int type, int contextId)
{
	qWarning() << "Received" << len << "bytes of unknown or invalid message type" << type << "from context ID" << contextId;
	if(type < 64 || contextId == 0) {
		// If message type is Transparent, the bad data came from the server. Something is wrong for sure.
		m_error = tr("Received invalid data");
		m_socket->abort();
	} else {
		// Opaque messages are merely passed along by the server.
		// TODO autokick misbehaving clients?
	}
}

void TcpServer::handleDisconnect()
{
	emit serverDisconnected(m_error, m_errorcode, m_localDisconnect);
}

void TcpServer::handleSocketError()
{
	qWarning() << "Socket error:" << m_socket->errorString();

	if(m_socket->error() == QTcpSocket::RemoteHostClosedError)
		return;

	if(m_error.isEmpty())
		m_error = m_socket->errorString();
	if(m_socket->state() != QTcpSocket::UnconnectedState)
		m_socket->disconnectFromHost();
	else
		handleDisconnect();
}

void TcpServer::loginFailure(const QString &message, const QString &errorcode)
{
	qWarning() << "Login failed:" << message;
	m_error = message;
	m_errorcode = errorcode;
	m_localDisconnect = errorcode == "CANCELLED";
	m_socket->disconnectFromHost();
}

void TcpServer::loginSuccess()
{
	qDebug() << "logged in to session" << m_loginstate->url() << ". Got user id" << m_loginstate->userId();

	m_supportsPersistence = m_loginstate->supportsPersistence();
	m_supportsAbuseReports = m_loginstate->supportsAbuseReports();
	m_protocolVersion = m_loginstate->protocolVersion();

	emit loggedIn(
		m_loginstate->url(),
		m_loginstate->userId(),
		m_loginstate->mode() == LoginHandler::Mode::Join,
		m_loginstate->isAuthenticated(),
		m_loginstate->hasUserFlag("MOD"),
		!m_loginstate->sessionFlags().contains("NOAUTORESET"),
		m_protocolVersion);

	m_loginstate->deleteLater();
	m_loginstate = nullptr;
}

QSslCertificate TcpServer::hostCertificate() const
{
	return m_socket->peerCertificate();
}

}
