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
	_loopback = new LoopbackServer(this);
	_server = _loopback;
	_isloopback = true;

	connect(_loopback, &LoopbackServer::messageReceived, this, &Client::handleMessage);
}

Client::~Client()
{
}

void Client::connectToServer(LoginHandler *loginhandler)
{
	Q_ASSERT(_isloopback);

	TcpServer *server = new TcpServer(this);
	_server = server;
	_isloopback = false;
	m_sessionId = loginhandler->sessionId(); // target host/join ID (if known already)

	connect(server, SIGNAL(loggingOut()), this, SIGNAL(serverDisconnecting()));
	connect(server, SIGNAL(serverDisconnected(QString, QString, bool)), this, SLOT(handleDisconnect(QString, QString, bool)));
	connect(server, SIGNAL(serverDisconnected(QString, QString, bool)), loginhandler, SLOT(serverDisconnected()));
	connect(server, SIGNAL(loggedIn(QString, int, bool)), this, SLOT(handleConnect(QString, int, bool)));
	connect(server, SIGNAL(messageReceived(protocol::MessagePtr)), this, SLOT(handleMessage(protocol::MessagePtr)));

	connect(server, SIGNAL(expectingBytes(int)), this, SIGNAL(expectingBytes(int)));
	connect(server, SIGNAL(bytesReceived(int)), this, SIGNAL(bytesReceived(int)));
	connect(server, SIGNAL(bytesSent(int)), this, SIGNAL(bytesSent(int)));
	connect(server, SIGNAL(lagMeasured(qint64)), this, SIGNAL(lagMeasured(qint64)));

	if(loginhandler->mode() == LoginHandler::HOST)
		loginhandler->setUserId(m_myId);

	emit serverConnected(loginhandler->url().host(), loginhandler->url().port());
	server->login(loginhandler);

	m_lastToolCtx = canvas::ToolContext();
}

void Client::disconnectFromServer()
{
	_server->logout();
}

bool Client::isLoggedIn() const
{
	return _server->isLoggedIn();
}

QString Client::sessionId() const
{
	return m_sessionId;
}

QUrl Client::sessionUrl(bool includeUser) const
{
	if(!isConnected())
		return QUrl();

	QUrl url = static_cast<const TcpServer*>(_server)->url();
	url.setScheme("drawpile");
	if(!includeUser)
		url.setUserInfo(QString());
	url.setPath("/" + sessionId());
	return url;
}

void Client::handleConnect(QString sessionId, int userid, bool join)
{
	m_sessionId = sessionId;
	m_myId = userid;

	emit serverLoggedin(join);
}

void Client::handleDisconnect(const QString &message,const QString &errorcode, bool localDisconnect)
{
	Q_ASSERT(_server != _loopback);

	emit serverDisconnected(message, errorcode, localDisconnect);
	static_cast<TcpServer*>(_server)->deleteLater();
	_server = _loopback;
	_isloopback = true;
}

bool Client::isLocalServer() const
{
	return _server->isLocal();
}

int Client::uploadQueueBytes() const
{
	return _server->uploadQueueBytes();
}

void Client::sendMessage(protocol::MessagePtr msg)
{
	msg->setContextId(m_myId);

	// Command type messages go to the local fork too
	if(msg->isCommand())
		emit drawingCommandLocal(msg);

	_server->sendMessage(msg);
}

void Client::sendMessages(const QList<protocol::MessagePtr> &msgs)
{
	for(protocol::MessagePtr msg : msgs) {
		// TODO batch send
		sendMessage(msg);
	}

	if(isConnected() && _server->uploadQueueBytes() > 1024 * 10)
		emit sendingBytes(_server->uploadQueueBytes());
}

/**
 * @brief Send the session initialization or reset command stream
 * @param commands snapshot point commands
 */
void Client::sendInitialSnapshot(const QList<protocol::MessagePtr> commands)
{
	// The actual snapshot data will be sent in parallel with normal session traffic
	_server->sendSnapshotMessages(commands);

	emit sendingBytes(_server->uploadQueueBytes());
}

void Client::sendChat(const QString &message, bool announce, bool action)
{
	_server->sendMessage(MessagePtr(new protocol::Chat(m_myId, message, announce, action)));
}

void Client::sendPinnedChat(const QString &message)
{
	_server->sendMessage(protocol::Chat::pin(m_myId, message));
}

void Client::handleMessage(protocol::MessagePtr msg)
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

	emit serverMessage(chat);
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
		emit serverMessage(reply.message);
		break;
	case ServerReply::SESSIONCONF:
		emit sessionConfChange(reply.reply["config"].toObject());
		break;
	case ServerReply::SIZELIMITWARNING:
		qWarning() << "Session history size warning:" << reply.reply["maxSize"].toInt() - reply.reply["size"].toInt() << "bytes of space left!";
		emit serverHistoryLimitReceived(reply.reply["maxSize"].toInt() - reply.reply["size"].toInt());
		break;
	case ServerReply::RESET:
		handleResetRequest(reply);
		break;
	}
}

}
