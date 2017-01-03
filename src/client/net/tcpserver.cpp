/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#include "../shared/net/control.h"

#include <QDebug>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QSettings>
#include <QApplication>

namespace net {

TcpServer::TcpServer(QObject *parent) :
	Server(true, parent), m_loginstate(nullptr), m_securityLevel(NO_SECURITY),
	m_localDisconnect(false), m_supportsPersistence(false)
{
	m_socket = new QSslSocket(this);

	QSslConfiguration sslconf = m_socket->sslConfiguration();
	sslconf.setSslOption(QSsl::SslOptionDisableCompression, false);
	m_socket->setSslConfiguration(sslconf);

	m_msgqueue = new protocol::MessageQueue(m_socket, this);
	m_msgqueue->setDecodeOpaque(true);

	m_msgqueue->setIdleTimeout(QSettings().value("settings/server/timeout", 60).toInt() * 1000);
	m_msgqueue->setPingInterval(15 * 1000);

	connect(m_socket, &QSslSocket::disconnected, this, &TcpServer::handleDisconnect);
	connect(m_socket, static_cast<void(QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::error), this, &TcpServer::handleSocketError);
	connect(m_socket, &QSslSocket::stateChanged, [this](QAbstractSocket::SocketState state) {
		if(state==QAbstractSocket::ClosingState)
			emit loggingOut();
	});

	connect(m_msgqueue, &protocol::MessageQueue::messageAvailable, this, &TcpServer::handleMessage);
	connect(m_msgqueue, &protocol::MessageQueue::bytesReceived, this, &TcpServer::bytesReceived);
	connect(m_msgqueue, &protocol::MessageQueue::bytesSent, this, &TcpServer::bytesSent);
	connect(m_msgqueue, &protocol::MessageQueue::badData, this, &TcpServer::handleBadData);
	connect(m_msgqueue, &protocol::MessageQueue::expectingBytes, this, &TcpServer::expectingBytes);
	connect(m_msgqueue, &protocol::MessageQueue::pingPong, this, &TcpServer::lagMeasured);
}

void TcpServer::login(LoginHandler *login)
{
	m_url = login->url();
	m_loginstate = login;
	m_loginstate->setParent(this);
	m_loginstate->setServer(this);
	m_socket->connectToHost(login->url().host(), login->url().port(DRAWPILE_PROTO_DEFAULT_PORT));
}

void TcpServer::logout()
{
	m_localDisconnect = true;
	m_msgqueue->sendDisconnect(protocol::Disconnect::SHUTDOWN, QString());
}

int TcpServer::uploadQueueBytes() const
{
	return m_msgqueue->uploadQueueBytes();
}

void TcpServer::sendMessage(const protocol::MessagePtr &msg)
{
	m_msgqueue->send(msg);
}

void TcpServer::handleMessage()
{
	while(m_msgqueue->isPending()) {
		protocol::MessagePtr msg = m_msgqueue->getPending();
		if(m_loginstate)
			m_loginstate->receiveMessage(msg);
		else
			emit messageReceived(msg);
	}
}

void TcpServer::handleBadData(int len, int type)
{
	qWarning() << "Received" << len << "bytes of unknown message type" << (unsigned int)type;
	m_error = tr("Received invalid data");
	m_socket->abort();
}

void TcpServer::handleDisconnect()
{
	emit serverDisconnected(m_error, m_errorcode, m_localDisconnect);
}

void TcpServer::handleSocketError()
{
	qWarning() << "Socket error:" << m_socket->errorString();

	if(m_socket->error() == QTcpSocket::RemoteHostClosedError)
		return;

	if(m_error.isEmpty())
		m_error = m_socket->errorString();
	if(m_socket->state() != QTcpSocket::UnconnectedState)
		m_socket->disconnectFromHost();
	else
		handleDisconnect();
}

void TcpServer::loginFailure(const QString &message, const QString &errorcode)
{
	qWarning() << "Login failed:" << message;
	m_error = message;
	m_errorcode = errorcode;
	m_localDisconnect = errorcode == "CANCELLED";
	m_socket->disconnectFromHost();
}

void TcpServer::loginSuccess()
{
	qDebug() << "logged in to session" << m_loginstate->sessionId() << ". Got user id" << m_loginstate->userId();

	m_supportsPersistence = m_loginstate->supportsPersistence();

	emit loggedIn(m_loginstate->sessionId(), m_loginstate->userId(), m_loginstate->mode() == LoginHandler::JOIN);

	m_loginstate->deleteLater();
	m_loginstate = nullptr;
}

QSslCertificate TcpServer::hostCertificate() const
{
	return m_socket->peerCertificate();
}

}
