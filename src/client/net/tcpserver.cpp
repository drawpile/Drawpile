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
	connect(_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleSocketError()));
	connect(_msgqueue, SIGNAL(messageAvailable()), this, SLOT(handleMessage()));
	connect(_msgqueue, SIGNAL(bytesReceived(int)), this, SIGNAL(bytesReceived(int)));
	connect(_msgqueue, SIGNAL(bytesSent(int)), this, SIGNAL(bytesSent(int)));
	connect(_msgqueue, SIGNAL(badData(int,int)), this, SLOT(handleBadData(int,int)));
	connect(_msgqueue, SIGNAL(expectingBytes(int)), this, SIGNAL(expectingBytes(int)));
}

void TcpServer::login(LoginHandler *login)
{
	_loginstate = login;
	_loginstate->setParent(this);
	_loginstate->setServer(this);
	_socket->connectToHost(login->url().host(), login->url().port(protocol::DEFAULT_PORT));
}

void TcpServer::logout()
{
	_socket->close();
}

int TcpServer::uploadQueueBytes() const
{
	return _msgqueue->uploadQueueBytes();
}

void TcpServer::sendMessage(protocol::MessagePtr msg)
{
	_msgqueue->send(msg);
}

void TcpServer::sendSnapshotMessages(QList<protocol::MessagePtr> msgs)
{
	qDebug() << "sending" << msgs.length() << "snapshot messages";
	_msgqueue->sendSnapshot(msgs);
}

void TcpServer::handleMessage()
{
	while(_msgqueue->isPending()) {
		protocol::MessagePtr msg = _msgqueue->getPending();
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

void TcpServer::handleSocketError()
{
	qWarning() << "Socket error:" << _socket->errorString();
	_error = _socket->errorString();
	if(_socket->state() != QTcpSocket::UnconnectedState)
		_socket->close();
	else
		handleDisconnect();
}

void TcpServer::loginFailure(const QString &message)
{
	qWarning() << "Login failed:" << message;
	_error = message;
	_socket->close();
}

void TcpServer::loginSuccess()
{
	qDebug() << "logged in! Got user id" << _loginstate->userId();
	emit loggedIn(_loginstate->userId(), _loginstate->mode() == LoginHandler::JOIN);

	_loginstate->deleteLater();
	_loginstate = 0;
}

}
