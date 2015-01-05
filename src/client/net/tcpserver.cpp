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

#include "config.h"
#include "tcpserver.h"
#include "login.h"

#include "../shared/net/messagequeue.h"
#include "../shared/net/flow.h"

#include <QDebug>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QSettings>

namespace net {

TcpServer::TcpServer(QObject *parent) :
	QObject(parent), Server(false), _loginstate(0), _securityLevel(NO_SECURITY),
	_localDisconnect(false)
{
	_socket = new QSslSocket(this);

	QSslConfiguration sslconf = _socket->sslConfiguration();
	sslconf.setSslOption(QSsl::SslOptionDisableCompression, false);
	_socket->setSslConfiguration(sslconf);

	_msgqueue = new protocol::MessageQueue(_socket, this);

	_msgqueue->setIdleTimeout(QSettings().value("settings/server/timeout", 60).toInt() * 1000);
	_msgqueue->setPingInterval(15 * 1000);

	connect(_socket, SIGNAL(disconnected()), this, SLOT(handleDisconnect()));
	connect(_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleSocketError()));
	connect(_socket, &QSslSocket::stateChanged, [this](QAbstractSocket::SocketState state) {
		if(state==QAbstractSocket::ClosingState)
			emit loggingOut();
	});

	connect(_msgqueue, SIGNAL(messageAvailable()), this, SLOT(handleMessage()));
	connect(_msgqueue, SIGNAL(bytesReceived(int)), this, SIGNAL(bytesReceived(int)));
	connect(_msgqueue, SIGNAL(bytesSent(int)), this, SIGNAL(bytesSent(int)));
	connect(_msgqueue, SIGNAL(badData(int,int)), this, SLOT(handleBadData(int,int)));
	connect(_msgqueue, SIGNAL(expectingBytes(int)), this, SIGNAL(expectingBytes(int)));
	connect(_msgqueue, SIGNAL(pingPong(qint64)), this, SIGNAL(lagMeasured(qint64)));
}

void TcpServer::login(LoginHandler *login)
{
	_url = login->url();
	_loginstate = login;
	_loginstate->setParent(this);
	_loginstate->setServer(this);
	_socket->connectToHost(login->url().host(), login->url().port(DRAWPILE_PROTO_DEFAULT_PORT));
}

void TcpServer::logout()
{
	_localDisconnect = true;
	_msgqueue->sendDisconnect(protocol::Disconnect::SHUTDOWN, QString());
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
	_socket->abort();
}

void TcpServer::handleDisconnect()
{
	emit serverDisconnected(_error, _errorcode, _localDisconnect);
	deleteLater();
}

void TcpServer::handleSocketError()
{
	qWarning() << "Socket error:" << _socket->errorString();

	if(_socket->error() == QTcpSocket::RemoteHostClosedError)
		return;

	if(_error.isEmpty())
		_error = _socket->errorString();
	if(_socket->state() != QTcpSocket::UnconnectedState)
		_socket->disconnectFromHost();
	else
		handleDisconnect();
}

void TcpServer::loginFailure(const QString &message, const QString &errorcode)
{
	qWarning() << "Login failed:" << message;
	_error = message;
	_errorcode = errorcode;
	_localDisconnect = errorcode == "CANCELLED";
	_socket->disconnectFromHost();
}

void TcpServer::loginSuccess()
{
	qDebug() << "logged in to session" << _loginstate->sessionId() << ". Got user id" << _loginstate->userId();
	emit loggedIn(_loginstate->sessionId(), _loginstate->userId(), _loginstate->mode() == LoginHandler::JOIN);

	_loginstate->deleteLater();
	_loginstate = 0;
}

QSslCertificate TcpServer::hostCertificate() const
{
	return _socket->peerCertificate();
}

}
