/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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
#include "net/commands.h"
#include "net/internalmsg.h"

#include "core/point.h"

#include "../libshared/net/control.h"
#include "../libshared/net/meta.h"
#include "../libshared/net/meta2.h"

#include <QDebug>

using protocol::MessagePtr;

namespace net {

Client::Client(QObject *parent)
	: QObject(parent), m_myId(1), m_recordedChat(false),
	  m_catchupTo(0), m_caughtUp(0), m_catchupProgress(0)
{
	m_loopback = new LoopbackServer(this);
	m_server = m_loopback;
	m_isloopback = true;
	m_moderator = false;
	m_isAuthenticated = false;

	connect(m_loopback, &LoopbackServer::messageReceived, this, &Client::handleMessage);
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
	connect(server, &TcpServer::messageReceived, this, &Client::handleMessage);

	connect(server, &TcpServer::bytesReceived, this, &Client::bytesReceived);
	connect(server, &TcpServer::bytesSent, this, &Client::bytesSent);
	connect(server, &TcpServer::lagMeasured, this, &Client::lagMeasured);

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

bool Client::isLocalServer() const
{
	return m_server->isLocal();
}

int Client::uploadQueueBytes() const
{
	return m_server->uploadQueueBytes();
}

void Client::sendMessage(const protocol::MessagePtr &msg)
{
#ifndef NDEBUG
	if(!msg->isControl() && msg->contextId()==0) {
		qWarning("Context ID not set for message type #%d (%s)", msg->type(), qPrintable(msg->messageName()));
	}
#endif

	// Command type messages go to the local fork too
	if(msg->isCommand())
		emit drawingCommandLocal(msg);

	m_server->sendMessage(msg);
}

void Client::sendMessages(const protocol::MessageList &msgs)
{
	for(const protocol::MessagePtr &msg : msgs) {
#ifndef NDEBUG
		if(!msg->isControl() && msg->contextId()==0) {
			qWarning("Context ID not set for message type #%d (%s)", msg->type(), qPrintable(msg->messageName()));
		}
#endif
		if(msg->isCommand())
			emit drawingCommandLocal(msg);
	}
	m_server->sendMessages(msgs);
}

void Client::sendResetMessages(const protocol::MessageList &msgs)
{
	m_server->sendMessages(msgs);
}

void Client::handleMessage(const protocol::MessagePtr &msg)
{
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

	// Handle control messages here
	// (these are sent only by the server and are not stored in the session)
	if(msg->isControl()) {
		switch(msg->type()) {
		using namespace protocol;
		case MSG_COMMAND:
			handleServerCommand(msg.cast<Command>());
			break;
		case MSG_DISCONNECT:
			handleDisconnectMessage(msg.cast<Disconnect>());
			break;
		default:
			qWarning("Received unhandled control message %d", msg->type());
		}
		return;
	}

	// Rest of the messages are part of the session
	emit messageReceived(msg);
}

void Client::handleResetRequest(const protocol::ServerReply &msg)
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

void Client::handleDisconnectMessage(const protocol::Disconnect &msg)
{
	qDebug() << "Received disconnect notification! Reason =" << msg.reason() << "and message =" << msg.message();
	const QString message = msg.message();

	if(msg.reason() == protocol::Disconnect::KICK) {
		emit youWereKicked(message);
		return;
	}

	QString chat;
	if(msg.reason() == protocol::Disconnect::ERROR)
		chat = tr("A server error occurred!");
	else if(msg.reason() == protocol::Disconnect::SHUTDOWN)
		chat = tr("The server is shutting down!");
	else
		chat = "Unknown error";

	if(!message.isEmpty())
		chat += QString(" (%1)").arg(message);

	emit serverMessage(chat, true);
}

void Client::handleServerCommand(const protocol::Command &msg)
{
	using protocol::ServerReply;
	ServerReply reply = msg.reply();

	switch(reply.type) {
	case ServerReply::UNKNOWN:
		qWarning() << "Unknown server reply:" << reply.message << reply.reply;
		break;
	case ServerReply::LOGIN:
		qWarning("got login message while in session!");
		break;
	case ServerReply::MESSAGE:
	case ServerReply::ALERT:
	case ServerReply::ERROR:
	case ServerReply::RESULT:
		emit serverMessage(reply.message, reply.type == ServerReply::ALERT);
		break;
	case ServerReply::LOG: {
		QString time = QDateTime::fromString(reply.reply["timestamp"].toString(), Qt::ISODate).toLocalTime().toString(Qt::ISODate);
		QString user = reply.reply["user"].toString();
		QString msg = reply.message;
		if(user.isEmpty())
			emit serverLog(QStringLiteral("[%1] %2").arg(time, msg));
		else
			emit serverLog(QStringLiteral("[%1] %2: %3").arg(time, user, msg));
		} break;
	case ServerReply::SESSIONCONF:
		emit sessionConfChange(reply.reply["config"].toObject());
		break;
	case ServerReply::SIZELIMITWARNING:
		// No longer used since 2.1.0. Replaced by RESETREQUEST
		break;
	case ServerReply::RESETREQUEST:
		emit autoresetRequested(reply.reply["maxSize"].toInt(), reply.reply["query"].toBool());
		break;
	case ServerReply::STATUS:
		emit serverStatusUpdate(reply.reply["size"].toInt());
		break;
	case ServerReply::RESET:
		handleResetRequest(reply);
		break;
	case ServerReply::CATCHUP:
		m_catchupTo = reply.reply["count"].toInt();
		m_caughtUp = 0;
		m_catchupProgress = 0;
		break;
	}
}

}
