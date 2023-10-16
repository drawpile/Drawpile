// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_NET_TCPSERVER_H
#define DP_NET_TCPSERVER_H
#include "libclient/net/server.h"
#include "libshared/net/messagequeue.h"
#include <QUrl>

class QSslSocket;

namespace net {

class LoginHandler;

class TcpServer final : public Server {
	Q_OBJECT
	friend class LoginHandler;

public:
	explicit TcpServer(int timeoutSecs, QObject *parent = nullptr);

	void login(LoginHandler *login);
	void logout() override;

	void sendMessage(const net::Message &msg) override;
	void sendMessages(int count, const net::Message *msgs) override;

	bool isLoggedIn() const override { return m_loginstate == nullptr; }

	int uploadQueueBytes() const override;

	void startTls();

	Security securityLevel() const override { return m_securityLevel; }
	QSslCertificate hostCertificate() const override;

	bool supportsPersistence() const override { return m_supportsPersistence; }

	bool supportsCryptBanImpEx() const override
	{
		return m_supportsCryptBanImpEx;
	}

	bool supportsModBanImpEx() const override
	{
		return m_supportsModBanImpEx;
	}

	bool supportsAbuseReports() const override
	{
		return m_supportsAbuseReports;
	}

	void setSmoothEnabled(bool smoothEnabled) override
	{
		m_msgqueue->setSmoothEnabled(smoothEnabled);
	}
	void setSmoothDrainRate(int smoothDrainRate) override
	{
		m_msgqueue->setSmoothDrainRate(smoothDrainRate);
	}

	int artificialLagMs() const override
	{
		return m_msgqueue->artificalLagMs();
	}
	void setArtificialLagMs(int msecs) override
	{
		m_msgqueue->setArtificialLagMs(msecs);
	}

	void artificialDisconnect() override;

signals:
	void loggedIn(
		const QUrl &url, uint8_t userid, bool join, bool auth, bool moderator,
		bool hasAutoreset, bool compatibilityMode, const QString &joinPassword);
	void loggingOut();
	void gracefullyDisconnecting(
		MessageQueue::GracefulDisconnect, const QString &message);
	void serverDisconnected(
		const QString &message, const QString &errorcode, bool localDisconnect);

	void bytesReceived(int);
	void bytesSent(int);

	void lagMeasured(qint64 lag);

protected:
	void loginFailure(const QString &message, const QString &errorcode);
	void loginSuccess();

private slots:
	void handleMessage();
	void handleBadData(int len, int type, int contextId);
	void handleDisconnect();
	void handleSocketError();

private:
	QSslSocket *m_socket;
	MessageQueue *m_msgqueue;
	net::MessageList m_receiveBuffer;
	LoginHandler *m_loginstate;
	QString m_error, m_errorcode;
	Security m_securityLevel;
	bool m_localDisconnect;
	bool m_supportsPersistence;
	bool m_supportsCryptBanImpEx;
	bool m_supportsModBanImpEx;
	bool m_supportsAbuseReports;
	bool m_compatibilityMode;
};

}

#endif // TCPSERVER_H
