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

#include "net/client.h"
#include "net/loopbackserver.h"
#include "net/tcpserver.h"
#include "net/login.h"
#include "net/internalmsg.h"
#include "net/envelope.h"
#include "net/servercmd.h"

#include <QDebug>

using protocol::MessagePtr;

namespace net {

Client::Client(QObject *parent)
	: QObject(parent), m_myId(1),
	  m_catchupTo(0), m_caughtUp(0), m_catchupProgress(0)
{
	m_loopback = new LoopbackServer(this);
	m_server = m_loopback;
	m_isloopback = true;
	m_moderator = false;
	m_isAuthenticated = false;

	connect(m_loopback, &LoopbackServer::envelopeReceived, this, &Client::handleEnvelope);
}

Client::~Client()
{
}

void Client::connectToServer(LoginHandler *loginhandler)
{
	Q_ASSERT(m_isloopback);

	TcpServer *server = new TcpServer(this);
	m_server = server;
	m_isloopback = false;

	connect(server, &TcpServer::loggingOut, this, &Client::serverDisconnecting);
	connect(server, &TcpServer::serverDisconnected, this, &Client::handleDisconnect);
	connect(server, &TcpServer::serverDisconnected, loginhandler, &LoginHandler::serverDisconnected);
	connect(server, &TcpServer::loggedIn, this, &Client::handleConnect);
	connect(server, &TcpServer::envelopeReceived, this, &Client::handleEnvelope);

	connect(server, &TcpServer::bytesReceived, this, &Client::bytesReceived);
	connect(server, &TcpServer::bytesSent, this, &Client::bytesSent);
	connect(server, &TcpServer::lagMeasured, this, &Client::lagMeasured);

	connect(server, &TcpServer::gracefullyDisconnecting, this, [this](MessageQueue::GracefulDisconnect reason, const QString &message)
	{
		if(reason == MessageQueue::GracefulDisconnect::Kick) {
			emit youWereKicked(message);
			return;
		}

		QString chat;
		switch(reason) {
		case MessageQueue::GracefulDisconnect::Kick:
			emit youWereKicked(message);
			return;
		case MessageQueue::GracefulDisconnect::Error:
			chat = tr("A server error occurred!");
			break;
		case MessageQueue::GracefulDisconnect::Shutdown:
			chat = tr("The server is shutting down!");
			break;
		default:
			chat = "Unknown error";
		}

		if(!message.isEmpty())
			chat = QString("%1 (%2)").arg(chat, message);

		emit serverMessage(chat, true);
	});

	if(loginhandler->mode() == LoginHandler::Mode::HostRemote)
		loginhandler->setUserId(m_myId);

	emit serverConnected(loginhandler->url().host(), loginhandler->url().port());
	server->login(loginhandler);

	m_catchupTo = 0;
	m_caughtUp = 0;
	m_catchupProgress = 0;
}

void Client::disconnectFromServer()
{
	m_server->logout();
}

QUrl Client::sessionUrl(bool includeUser) const
{
	QUrl url = m_lastUrl;
	if(!includeUser)
		url.setUserInfo(QString());
	return url;
}

void Client::handleConnect(const QUrl &url, uint8_t userid, bool join, bool auth, bool moderator, bool supportsAutoReset)
{
	m_lastUrl = url;
	m_myId = userid;
	m_moderator = moderator;
	m_isAuthenticated = auth;
	m_supportsAutoReset = supportsAutoReset;

	emit serverLoggedin(join);
}

void Client::handleDisconnect(const QString &message,const QString &errorcode, bool localDisconnect)
{
	Q_ASSERT(m_server != m_loopback);

	emit serverDisconnected(message, errorcode, localDisconnect);
	m_server->deleteLater();
	m_server = m_loopback;
	m_isloopback = true;
	m_moderator = false;
}

int Client::uploadQueueBytes() const
{
	return m_server->uploadQueueBytes();
}

void Client::sendEnvelope(const Envelope &envelope) {
	if(envelope.isEmpty())
		return;

	// When we build envelopes with Commands in them, they always start out with
	// a command, so it's enough to check just the first message.
	if(envelope.isCommand()) {
		emit drawingCommandLocal(envelope);
	}

	m_server->sendEnvelope(envelope);
}

void Client::sendResetEnvelope(const net::Envelope &resetImage)
{
	if(resetImage.isEmpty())
		return;

	m_server->sendEnvelope(resetImage);
}

void Client::handleEnvelope(const Envelope &envelope)
{
#if 0 // FIXME (internal messages no longer needed for this)
	if(m_catchupTo>0) {
		++m_caughtUp;
		if(m_caughtUp >= m_catchupTo) {
			emit messageReceived(protocol::ClientInternal::makeCatchup(100));
			m_catchupTo = 0;
		} else {
			int progress = 100 * m_caughtUp / m_catchupTo;
			if(progress != m_catchupProgress) {
				m_catchupProgress = progress;
				emit messageReceived(protocol::ClientInternal::makeCatchup(progress));
			}
		}
	}
#endif

	// Catch all Control messages here
	Envelope ctrl = envelope;
	bool allControl = true;
	while(!ctrl.isEmpty()) {
		if(ctrl.messageType() == protocol::MSG_COMMAND) {
			const ServerReply sr = ServerReply::fromEnvelope(ctrl);
			handleServerReply(sr);

		} else {
			allControl = false;
		}

		ctrl = ctrl.next();
	}

	// The paint engine will handle the rest and ignore the control messages
	if(!allControl)
		emit messageReceived(envelope);
}

void Client::handleServerReply(const ServerReply &reply)
{
	switch(reply.type) {
	case ServerReply::ReplyType::Unknown:
		qWarning() << "Unknown server reply:" << reply.message << reply.reply;
		break;
	case ServerReply::ReplyType::Login:
		qWarning("got login message while in session!");
		break;
	case ServerReply::ReplyType::Message:
	case ServerReply::ReplyType::Alert:
	case ServerReply::ReplyType::Error:
	case ServerReply::ReplyType::Result:
		emit serverMessage(reply.message, reply.type == ServerReply::ReplyType::Alert);
		break;
	case ServerReply::ReplyType::Log: {
		QString time = QDateTime::fromString(reply.reply["timestamp"].toString(), Qt::ISODate).toLocalTime().toString(Qt::ISODate);
		QString user = reply.reply["user"].toString();
		QString msg = reply.message;
		if(user.isEmpty())
			emit serverLog(QStringLiteral("[%1] %2").arg(time, msg));
		else
			emit serverLog(QStringLiteral("[%1] %2: %3").arg(time, user, msg));
		} break;
	case ServerReply::ReplyType::SessionConf:
		emit sessionConfChange(reply.reply["config"].toObject());
		break;
	case ServerReply::ReplyType::SizeLimitWarning:
		// No longer used since 2.1.0. Replaced by RESETREQUEST
		break;
	case ServerReply::ReplyType::ResetRequest:
		emit autoresetRequested(reply.reply["maxSize"].toInt(), reply.reply["query"].toBool());
		break;
	case ServerReply::ReplyType::Status:
		emit serverStatusUpdate(reply.reply["size"].toInt());
		break;
	case ServerReply::ReplyType::Reset:
		handleResetRequest(reply);
		break;
	case ServerReply::ReplyType::Catchup:
		m_catchupTo = reply.reply["count"].toInt();
		m_caughtUp = 0;
		m_catchupProgress = 0;
		break;
	}
}

void Client::handleResetRequest(const ServerReply &msg)
{
	if(msg.reply["state"] == "init") {
		qDebug("Requested session reset");
		emit needSnapshot();

	} else if(msg.reply["state"] == "reset") {
		qDebug("Resetting session!");
		emit sessionResetted();

	} else {
		qWarning() << "Unknown reset state:" << msg.reply["state"].toString();
		qWarning() << msg.message;
	}
}

}
