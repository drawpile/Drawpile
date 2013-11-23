/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include <QDebug>
#include <QTcpSocket>

#include "tcpserver.h"
#include "login.h"

#include "../shared/net/messagequeue.h"
#include "../shared/net/constants.h"

namespace net {

TcpServer::TcpServer(QObject *parent) :
	QObject(parent), Server(false), _loginstate(0)
{
	_socket = new QTcpSocket(this);
	_msgqueue = new protocol::MessageQueue(_socket, this);

	connect(_socket, SIGNAL(disconnected()), this, SLOT(handleDisconnect()));
	connect(_msgqueue, SIGNAL(messageAvailable()), this, SLOT(handleMessage()));
	connect(_msgqueue, SIGNAL(bytesReceived(int)), this, SIGNAL(bytesReceived(int)));
	connect(_msgqueue, SIGNAL(badData(int,int)), this, SLOT(handleBadData(int,int)));
}

void TcpServer::login(LoginHandler *login)
{
	_loginstate = login;
	_loginstate->setParent(this);
	_loginstate->setServer(this);
	qDebug() << "connecting to" << login->url().host() << "port" << login->url().port(protocol::DEFAULT_PORT);
	_socket->connectToHost(login->url().host(), login->url().port(protocol::DEFAULT_PORT));
}

void TcpServer::sendMessage(protocol::MessagePtr msg)
{
	_msgqueue->send(msg);
}

void TcpServer::sendSnapshotMessages(QList<protocol::MessagePtr> msgs)
{
	_msgqueue->sendSnapshot(msgs);
}

void TcpServer::handleMessage()
{
	while(_msgqueue->isPending()) {
		protocol::MessagePtr msg = _msgqueue->getPending();
		qDebug() << "received message" << msg->type();
		if(_loginstate)
			_loginstate->receiveMessage(msg);
		else
			emit messageReceived(msg);
	}
}

void TcpServer::handleBadData(int len, int type)
{
	qWarning() << "Received" << len << "bytes of unknown message type" << type;
	_error = tr("Received invalid data");
	_socket->close();
}

void TcpServer::handleDisconnect()
{
	emit serverDisconnected(_error);
	deleteLater();
}

void TcpServer::loginFailure(const QString &message)
{
	qWarning() << "Login failed:" << message;
	_error = message;
	_socket->close();
}

void TcpServer::loginSuccess()
{
	qDebug() << "logged in! Got user id", _loginstate->userId();
	emit loggedIn(_loginstate->userId());

	_loginstate->deleteLater();
	_loginstate = 0;
}

}
