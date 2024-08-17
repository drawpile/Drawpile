// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_NET_WEBSOCKETSERVER_H
#define LIBCLIENT_NET_WEBSOCKETSERVER_H
#include "libclient/net/server.h"

class QWebSocket;

namespace net {

class WebSocketMessageQueue;

class WebSocketServer final : public Server {
	Q_OBJECT
public:
	explicit WebSocketServer(
		int timeoutSecs, int proxyMode, Client *client);

	bool isWebSocket() const override;

	bool hasSslSupport() const override;

	QSslCertificate hostCertificate() const override;

protected:
	MessageQueue *messageQueue() const override;
	void connectToHost(const QUrl &url) override;
	void disconnectFromHost() override;
	void abortConnection() override;
	bool isConnected() const override;
	QAbstractSocket::SocketError socketError() const override;
	QString socketErrorString() const override;
	bool loginStartTls(LoginHandler *loginstate) override;
	bool loginIgnoreTlsErrors(const QList<QSslError> &ignore) override;

private:
	QWebSocket *m_socket;
	WebSocketMessageQueue *m_msgqueue;
};

}

#endif
