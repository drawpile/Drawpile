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

#include "config.h"
#include "libclient/net/tcpserver.h"
#include "libclient/net/login.h"
#include "libclient/net/messagequeue.h"
#include "libclient/net/servercmd.h"
#include "libshared/util/qtcompat.h"

#include <QDebug>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QSettings>

namespace net {

TcpServer::TcpServer(QObject *parent) :
	Server(parent), m_loginstate(nullptr), m_securityLevel(NO_SECURITY),
	m_localDisconnect(false), m_supportsPersistence(false)
{
	m_socket = new QSslSocket(this);

	QSslConfiguration sslconf = m_socket->sslConfiguration();
	sslconf.setSslOption(QSsl::SslOptionDisableCompression, false);
	m_socket->setSslConfiguration(sslconf);

	m_msgqueue = new MessageQueue(m_socket, this);

	m_msgqueue->setIdleTimeout(QSettings().value("settings/server/timeout", 60).toInt() * 1000);
	m_msgqueue->setPingInterval(15 * 1000);

	connect(m_socket, &QSslSocket::disconnected, this, &TcpServer::handleDisconnect);
	connect(m_socket, compat::SocketError, this, &TcpServer::handleSocketError);
	connect(m_socket, &QSslSocket::stateChanged, this, [this](QAbstractSocket::SocketState state) {
		if(state==QAbstractSocket::ClosingState)
			emit loggingOut();
	});

	connect(m_msgqueue, &MessageQueue::messageAvailable, this, &TcpServer::handleMessage);
	connect(m_msgqueue, &MessageQueue::bytesReceived, this, &TcpServer::bytesReceived);
	connect(m_msgqueue, &MessageQueue::bytesSent, this, &TcpServer::bytesSent);
	connect(m_msgqueue, &MessageQueue::badData, this, &TcpServer::handleBadData);
	connect(m_msgqueue, &MessageQueue::pingPong, this, &TcpServer::lagMeasured);
	connect(m_msgqueue, &MessageQueue::gracefulDisconnect, this, &TcpServer::gracefullyDisconnecting);
}

void TcpServer::login(LoginHandler *login)
{
	m_loginstate = login;
	m_loginstate->setParent(this);
	m_loginstate->setServer(this);
	m_socket->connectToHost(login->url().host(), login->url().port(DRAWPILE_PROTO_DEFAULT_PORT));
}

void TcpServer::logout()
{
	m_localDisconnect = true;
	m_msgqueue->sendDisconnect(MessageQueue::GracefulDisconnect::Shutdown, QString());
}

int TcpServer::uploadQueueBytes() const
{
	return m_msgqueue->uploadQueueBytes();
}

void TcpServer::sendEnvelope(const Envelope &e)
{
	m_msgqueue->send(e);
}

void TcpServer::handleMessage()
{
	Envelope envelope = m_msgqueue->getPending();

	if(m_loginstate) {
		// Drip feed messages one by one to the login handler,
		// since the envelope may contain messages not belonging to
		// the login handshake anymore.
		while(!envelope.isEmpty()) {
			const ServerReply sr = ServerReply::fromEnvelope(envelope);
			const bool expectMoreLogin = m_loginstate->receiveMessage(sr);
			envelope = envelope.next();

			if(!expectMoreLogin) {
				if(!envelope.isEmpty())
					emit envelopeReceived(envelope);
				break;
			}
		}

	} else {
		emit envelopeReceived(envelope);
	}
}

void TcpServer::handleBadData(int len, int type, int contextId)
{
	qWarning() << "Received" << len << "bytes of unknown or invalid message type" << type << "from context ID" << contextId;
	if(type < 64 || contextId == 0) {
		// If message type is Transparent, the bad data came from the server. Something is wrong for sure.
		m_error = tr("Received invalid data");
		m_socket->abort();
	} else {
		// Opaque messages are merely passed along by the server.
		// TODO autokick misbehaving clients?
	}
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
	qDebug() << "logged in to session" << m_loginstate->url() << ". Got user id" << m_loginstate->userId();

	m_supportsPersistence = m_loginstate->supportsPersistence();
	m_supportsAbuseReports = m_loginstate->supportsAbuseReports();

	emit loggedIn(
		m_loginstate->url(),
		m_loginstate->userId(),
		m_loginstate->mode() == LoginHandler::Mode::Join,
		m_loginstate->isAuthenticated(),
		m_loginstate->hasUserFlag("MOD"),
		!m_loginstate->sessionFlags().contains("NOAUTORESET")
		);

	m_loginstate->deleteLater();
	m_loginstate = nullptr;
}

QSslCertificate TcpServer::hostCertificate() const
{
	return m_socket->peerCertificate();
}

}
