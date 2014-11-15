/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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

#include <QObject>
#include <QUrl>

class QSslSocket;

namespace protocol {
    class MessageQueue;
}

namespace net {

class LoginHandler;

class TcpServer : public QObject, public Server
{
	Q_OBJECT
	friend class LoginHandler;
public:
	explicit TcpServer(QObject *parent = 0);

	void login(LoginHandler *login);
	void logout();

	void sendMessage(protocol::MessagePtr msg);
	void sendSnapshotMessages(QList<protocol::MessagePtr> msgs);

	bool isLoggedIn() const { return _loginstate == 0; }

	int uploadQueueBytes() const;

	void pauseInput(bool pause);

	void startTls();

	virtual Security securityLevel() const { return _securityLevel; }
	virtual QSslCertificate hostCertificate() const;

	QUrl url() const { return _url; }

signals:
	void loggedIn(QString sessionId, int userid, bool join);
	void loggingOut();
	void serverDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);

	void expectingBytes(int);
	void bytesReceived(int);
	void bytesSent(int);
	void messageReceived(protocol::MessagePtr message);

	void lagMeasured(qint64 lag);

protected:
	void loginFailure(const QString &message, const QString &errorcode);
	void loginSuccess();

private slots:
	void handleMessage();
	void handleBadData(int len, int type);
	void handleDisconnect();
	void handleSocketError();

private:
	QUrl _url;
	QSslSocket *_socket;
	protocol::MessageQueue *_msgqueue;
	LoginHandler *_loginstate;
	QString _error, _errorcode;
	Security _securityLevel;
	bool _localDisconnect;
	bool _paused;
};

}

#endif // TCPSERVER_H
