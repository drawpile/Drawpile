// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/tcpserver.h"
#include "cmake-config/config.h"
#include "libclient/net/login.h"
#include "libshared/net/tcpmessagequeue.h"
#include "libshared/util/qtcompat.h"
#include <QDebug>
#include <QSslConfiguration>
#include <QSslSocket>

namespace net {

TcpServer::TcpServer(int timeoutSecs, QObject *parent)
	: Server(parent)
{
	m_socket = new QSslSocket(this);

	QSslConfiguration sslconf = m_socket->sslConfiguration();
	sslconf.setSslOption(QSsl::SslOptionDisableCompression, false);
	m_socket->setSslConfiguration(sslconf);

	m_msgqueue = new TcpMessageQueue(m_socket, true, this);
	m_msgqueue->setIdleTimeout(timeoutSecs * 1000);
	m_msgqueue->setPingInterval(15 * 1000);

	connect(
		m_socket, &QSslSocket::disconnected, this,
		&TcpServer::handleDisconnect);
	connect(m_socket, compat::SocketError, this, &TcpServer::handleSocketError);
	connect(
		m_socket, &QSslSocket::stateChanged, this,
		&TcpServer::handleSocketStateChange);

	connectMessageQueue(m_msgqueue);
}

bool TcpServer::isWebSocket() const
{
	return false;
}

bool TcpServer::hasSslSupport() const
{
	return true;
}

QSslCertificate TcpServer::hostCertificate() const
{
	return m_socket->peerCertificate();
}

MessageQueue *TcpServer::messageQueue() const
{
	return m_msgqueue;
}

void TcpServer::connectToHost(const QUrl &url)
{
	m_socket->connectToHost(url.host(), url.port(cmake_config::proto::port()));
}

void TcpServer::disconnectFromHost()
{
	// When the socket is not yet connected, calling disconnectFromHost will set
	// the socket state back to UnconnectedState, but doesn't emit stateChanged
	// or disconnected signals. When aborting, the stateChanged gets emitted.
	switch(m_socket->state()) {
	case QAbstractSocket::UnconnectedState:
	case QAbstractSocket::HostLookupState:
	case QAbstractSocket::ConnectingState:
		abortConnection();
		break;
	default:
		m_socket->disconnectFromHost();
		break;
	}
}

void TcpServer::abortConnection()
{
	m_socket->abort();
}

bool TcpServer::isConnected() const
{
	return m_socket->state() != QAbstractSocket::UnconnectedState;
}

QAbstractSocket::SocketError TcpServer::socketError() const
{
	return m_socket->error();
}

QString TcpServer::socketErrorString() const
{
	return m_socket->errorString();
}

bool TcpServer::loginStartTls(LoginHandler *loginstate)
{
	connect(
		m_socket, &QSslSocket::encrypted, loginstate,
		&LoginHandler::tlsStarted);
	connect(
		m_socket,
		QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors),
		loginstate, &LoginHandler::tlsError);
	m_socket->startClientEncryption();
	return true;
}

bool TcpServer::loginIgnoreTlsErrors(const QList<QSslError> &ignore)
{
	m_socket->ignoreSslErrors(ignore);
	return true;
}

}
