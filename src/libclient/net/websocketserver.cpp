// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/websocketserver.h"
#include "libshared/net/websocketmessagequeue.h"
#include <QWebSocket>
#ifndef __EMSCRIPTEN__
#	include "libshared/net/proxy.h"
#	include <QNetworkProxy>
#endif

namespace {
#ifdef HAVE_WEBSOCKETS
#	if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
const auto WebSocketError = &QWebSocket::errorOccurred;
#	else
const auto WebSocketError =
	QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error);
#	endif
#endif
}

namespace net {

WebSocketServer::WebSocketServer(int timeoutSecs, int proxyMode, Client *client)
	: Server(client)
{
	m_socket = new QWebSocket(QString(), QWebSocketProtocol::Version13, this);
#ifdef __EMSCRIPTEN__
	Q_UNUSED(proxyMode);
#else
	if(shouldDisableProxy(proxyMode, m_socket->proxy(), true, false)) {
		m_socket->setProxy(QNetworkProxy::NoProxy);
	}
#endif

	m_msgqueue = new WebSocketMessageQueue(m_socket, true, this);
	m_msgqueue->setIdleTimeout(timeoutSecs * 1000);
	m_msgqueue->setPingInterval(15 * 1000);

#ifdef __EMSCRIPTEN__
	// AutoConnection doesn't work here in Emscripten.
	Qt::ConnectionType connectionType = Qt::QueuedConnection;
#else
	Qt::ConnectionType connectionType = Qt::AutoConnection;
#endif
	connect(
		m_socket, &QWebSocket::disconnected, this,
		&WebSocketServer::handleDisconnect, connectionType);
	connect(
		m_socket, WebSocketError, this, &WebSocketServer::handleSocketError,
		connectionType);
	connect(
		m_socket, &QWebSocket::stateChanged, this,
		&WebSocketServer::handleSocketStateChange, connectionType);

	connectMessageQueue(m_msgqueue);
}

bool WebSocketServer::hasSslSupport() const
{
	return false;
}

QSslCertificate WebSocketServer::hostCertificate() const
{
	return QSslCertificate();
}

MessageQueue *WebSocketServer::messageQueue() const
{
	return m_msgqueue;
}

void WebSocketServer::connectToHost(const QUrl &url)
{
	qDebug() << "WebSocketServer connecting to" << url;
	m_socket->open(url);
}

void WebSocketServer::disconnectFromHost()
{
	m_socket->close();
}

void WebSocketServer::abortConnection()
{
	m_socket->abort();
}

bool WebSocketServer::isConnected() const
{
	return m_socket->state() != QAbstractSocket::UnconnectedState;
}

QAbstractSocket::SocketError WebSocketServer::socketError() const
{
	return m_socket->error();
}

QString WebSocketServer::socketErrorString() const
{
	return m_socket->errorString();
}

bool WebSocketServer::loginStartTls(LoginHandler *loginstate)
{
	Q_UNUSED(loginstate);
	return false;
}

bool WebSocketServer::loginIgnoreTlsErrors(const QList<QSslError> &ignore)
{
	Q_UNUSED(ignore);
	return false;
}

bool WebSocketServer::isWebSocket() const
{
	return true;
}

}
