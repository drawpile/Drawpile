/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef DP_NET_TCPSERVER_H
#define DP_NET_TCPSERVER_H

#include <QObject>

#include "server.h"

class QTcpSocket;

namespace protocol {
    class MessageQueue;
}

namespace net {

class LoginHandler;

class TcpServer : public QObject, public Server
{
	Q_OBJECT
public:
	explicit TcpServer(QObject *parent = 0);

	void login(LoginHandler *login);
	void logout();

	void sendMessage(protocol::MessagePtr msg);
	void sendSnapshotMessages(QList<protocol::MessagePtr> msgs);

	bool isLoggedIn() const { return _loginstate == 0; }

	int uploadQueueBytes() const;

	void pauseInput(bool pause);

signals:
	void loggedIn(int userid, bool join);
	void loggingOut();
	void serverDisconnected(const QString &message, bool localDisconnect);

	void expectingBytes(int);
	void bytesReceived(int);
	void bytesSent(int);
	void messageReceived(protocol::MessagePtr message);

protected:
	void loginFailure(const QString &message, bool cancelled=false) override;
	void loginSuccess() override;

private slots:
	void handleMessage();
	void handleBadData(int len, int type);
	void handleDisconnect();
	void handleSocketError();

private:
	QTcpSocket *_socket;
	protocol::MessageQueue *_msgqueue;
	LoginHandler *_loginstate;
	QString _error;
	bool _localDisconnect;
	bool _paused;
};

}

#endif // TCPSERVER_H
