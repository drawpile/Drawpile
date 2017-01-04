/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#include "core/point.h"

#include "../shared/net/control.h"
#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"

#include <QDebug>

using protocol::MessagePtr;

namespace net {

Client::Client(QObject *parent)
	: QObject(parent), m_myId(1), m_recordedChat(false)
{
	m_loopback = new LoopbackServer(this);
	m_server = m_loopback;
	m_isloopback = true;

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

	connect(server, &TcpServer::expectingBytes, this, &Client::expectingBytes);
	connect(server, &TcpServer::bytesReceived, this, &Client::bytesReceived);
	connect(server, &TcpServer::bytesSent, this, &Client::bytesSent);
	connect(server, &TcpServer::lagMeasured, this, &Client::lagMeasured);

	if(loginhandler->mode() == LoginHandler::HOST)
		loginhandler->setUserId(m_myId);

	emit serverConnected(loginhandler->url().host(), loginhandler->url().port());
	server->login(loginhandler);

	m_lastToolCtx = canvas::ToolContext();
}

void Client::disconnectFromServer()
{
	m_server->logout();
}

QString Client::sessionId() const
{
	return m_sessionId;
}

QUrl Client::sessionUrl(bool includeUser) const
{
	if(!isConnected())
		return QUrl();

	QUrl url = static_cast<const TcpServer*>(m_server)->url();
	url.setScheme("drawpile");
	if(!includeUser)
		url.setUserInfo(QString());
	url.setPath("/" + sessionId());
	return url;
}

void Client::handleConnect(const QString &sessionId, int userid, bool join)
{
	m_sessionId = sessionId;
	m_myId = userid;

	emit serverLoggedin(join);
}

void Client::handleDisconnect(const QString &message,const QString &errorcode, bool localDisconnect)
{
	Q_ASSERT(m_server != m_loopback);

	emit serverDisconnected(message, errorcode, localDisconnect);
	m_server->deleteLater();
	m_server = m_loopback;
	m_isloopback = true;
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
		qWarning("Context ID not set for message type %d", msg->type());
	}
#endif

	// Command type messages go to the local fork too
	if(msg->isCommand())
		emit drawingCommandLocal(msg);

	m_server->sendMessage(msg);
}

void Client::sendMessages(const QList<protocol::MessagePtr> &msgs)
{
	for(protocol::MessagePtr msg : msgs) {
		// TODO batch send
		sendMessage(msg);
	}

	if(isConnected() && m_server->uploadQueueBytes() > 1024 * 10)
		emit sendingBytes(m_server->uploadQueueBytes());
}

void Client::sendChat(const QString &message, bool preserve, bool announce, bool action)
{
	if(preserve || announce) {
		m_server->sendMessage(MessagePtr(new protocol::Chat(m_myId, message, announce, action)));

	} else {
		QJsonObject opts;
		if(action)
			opts["action"] = true;

		protocol::ServerCommand cmd {
			"chat",
			QJsonArray() << message,
			opts
		};

		m_server->sendMessage(MessagePtr(new protocol::Command(m_myId, cmd.toJson())));
	}
}

void Client::sendPinnedChat(const QString &message)
{
	m_server->sendMessage(protocol::Chat::pin(m_myId, message));
}

void Client::handleMessage(const protocol::MessagePtr &msg)
{
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
	case ServerReply::CHAT:
		// So that we don't need to handle chat messages in two places,
		// synthesize a ChatMessage for this one

		emit messageReceived(protocol::MessagePtr(new protocol::Chat(
			reply.reply.value("user").toInt(),
			reply.message,
			false, reply.reply.value("options").toObject().value("action").toBool()
		)));
		break;
	case ServerReply::MESSAGE:
	case ServerReply::ALERT:
	case ServerReply::ERROR:
	case ServerReply::RESULT:
		emit serverMessage(reply.message, reply.type == ServerReply::ALERT);
		break;
	case ServerReply::SESSIONCONF:
		emit sessionConfChange(reply.reply["config"].toObject());
		break;
	case ServerReply::SIZELIMITWARNING:
		qWarning() << "Session history size warning:" << reply.reply["maxSize"].toInt() - reply.reply["size"].toInt() << "bytes of space left!";
		emit serverHistoryLimitReceived(reply.reply["maxSize"].toInt());
		break;
	case ServerReply::RESET:
		handleResetRequest(reply);
		break;
	}
}

}
