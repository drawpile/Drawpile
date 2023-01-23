/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef DP_NET_TCPSERVER_H
#define DP_NET_TCPSERVER_H

#include "server.h"
#include "messagequeue.h"

#include <QUrl>

class QSslSocket;

namespace net {

class LoginHandler;

class TcpServer : public Server
{
	Q_OBJECT
	friend class LoginHandler;
public:
	explicit TcpServer(QObject *parent=nullptr);

	void login(LoginHandler *login);
	void logout() override;

	void sendMessage(const drawdance::Message &msg) override;
	void sendMessages(int count, const drawdance::Message *msgs) override;

	bool isLoggedIn() const override { return m_loginstate == nullptr; }

	int uploadQueueBytes() const override;

	void startTls();

	Security securityLevel() const override { return m_securityLevel; }
	QSslCertificate hostCertificate() const override;

	bool supportsPersistence() const override { return m_supportsPersistence; }
	bool supportsAbuseReports() const override { return m_supportsAbuseReports; }

	int artificialLagMs() const override { return m_msgqueue->artificalLagMs(); }
	void setArtificialLagMs(int msecs) override { m_msgqueue->setArtificialLagMs(msecs); }

signals:
	void loggedIn(const QUrl &url, uint8_t userid, bool join, bool auth, bool moderator, bool hasAutoreset);
	void loggingOut();
	void gracefullyDisconnecting(MessageQueue::GracefulDisconnect, const QString &message);
	void serverDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);

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
	drawdance::MessageList m_receiveBuffer;
	LoginHandler *m_loginstate;
	QString m_error, m_errorcode;
	Security m_securityLevel;
	bool m_localDisconnect;
	bool m_supportsPersistence;
	bool m_supportsAbuseReports;
};

}

#endif // TCPSERVER_H
