// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_NET_TCPSERVER_H
#define DP_NET_TCPSERVER_H
#include "libclient/net/server.h"
#include "libshared/net/tcpmessagequeue.h"
#include <QUrl>

class QSslSocket;

namespace net {

class TcpServer final : public Server {
	Q_OBJECT
public:
	explicit TcpServer(int timeoutSecs, int proxyMode, Client *client);

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
	bool isWebSocket() const override;

private:
	QSslSocket *m_socket;
	TcpMessageQueue *m_msgqueue;
};

}

#endif // TCPSERVER_H
