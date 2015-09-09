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
#ifndef DP_NET_CLIENT_H
#define DP_NET_CLIENT_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

#include "core/point.h"
#include "core/blendmodes.h"
#include "net/server.h"
#include "../shared/net/message.h"
#include "canvas/statetracker.h" // for ToolContext


namespace paintcore {
	class Point;
}

namespace protocol {
	class Command;
	class Disconnect;
	struct ServerReply;
}

namespace net {
	
class LoopbackServer;
class LoginHandler;

/**
 * The client for accessing the drawing server.
 */
class Client : public QObject {
Q_OBJECT
public:
	Client(QObject *parent=0);
	~Client();

	/**
	 * @brief Connect to a remote server
	 * @param loginhandler the login handler to use
	 */
	void connectToServer(LoginHandler *loginhandler);

	/**
	 * @brief Disconnect from the remote server
	 */
	void disconnectFromServer();

	/**
	 * @brief Get the local user's user/context ID
	 * @return user ID
	 */
	int myId() const { return m_myId; }

	/**
	 * @brief Return the URL of the current session
	 *
	 * Returns an invalid URL not connected
	 */
	QUrl sessionUrl(bool includeUser=false) const;

	/**
	 * @brief Get the ID of the current session.
	 * @return
	 */
	QString sessionId() const;

	/**
	 * @brief Is the client connected to a local server?
	 *
	 * A local server is one that is running on this computer
	 * and thus has minimum latency.
	 * @return true if server is local
	 */
	bool isLocalServer() const;

	/**
	 * @brief Is the client connected by network?
	 * @return true if a network connection is open
	 */
	bool isConnected() const { return !_isloopback; }


	/**
	 * @brief Is the user connected and logged in?
	 * @return true if there is an active network connection and login process has completed
	 */
	bool isLoggedIn() const;

	/**
	 * @brief Get connection security level
	 */
	Server::Security securityLevel() const { return _server->securityLevel(); }

	/**
	 * @brief Get host certificate
	 *
	 * This is meaningful only if securityLevel != NO_SECURITY
	 */
	QSslCertificate hostCertificate() const { return _server->hostCertificate(); }

	/**
	 * @brief Get the number of bytes waiting to be sent
	 * @return upload queue length
	 */
	int uploadQueueBytes() const;

	/**
	 * @brief Whether to use recorded chat (Chat message) by default
	 *
	 * If set to false, chat messages are sent with ServerCommands and delivered
	 * only to the currently active users.
	 * @param recordedChat if true, chat messages are recorded in session history
	 */
	void setRecordedChatMode(bool recordedChat) { m_recordedChat = recordedChat; }

public slots:
	/**
	 * @brief Send a message to the server
	 *
	 * The context ID of the message is automatically set to the local user's ID.
	 *
	 * If this is a Command type message, drawingCommandLocal is emitted
	 * before the message is sent.
	 *
	 * TODO: replace all the other send* functions with this
	 * @param msg the message to send
	 */
	void sendMessage(protocol::MessagePtr msg);
	void sendMessages(const QList<protocol::MessagePtr> &msgs);

	void sendInitialSnapshot(const QList<protocol::MessagePtr> commands);

	/**
	 * @brief Send a chat message
	 *
	 * Depending on the "recorded chat" bit, this sends either a real Chat
	 * message (recorded) or a ServerCommand chat which bypasses the session history.
	 * @param message
	 * @param announce
	 * @param action
	 */
	void sendChat(const QString &message, bool announce, bool action);

signals:
	void messageReceived(protocol::MessagePtr msg);
	void drawingCommandLocal(protocol::MessagePtr msg);

	void needSnapshot();
	void sessionResetted();
	void sessionConfChange(const QJsonObject &config);

	void serverConnected(const QString &address, int port);
	void serverLoggedin(bool join);
	void serverDisconnecting();
	void serverDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);
	void youWereKicked(const QString &kickedBy);

	void serverMessage(const QString &message);

	void expectingBytes(int);
	void sendingBytes(int);
	void bytesReceived(int);
	void bytesSent(int);
	void lagMeasured(qint64);

	void sentColorChange(const QColor &color);
	void layerVisibilityChange(int id, bool hidden); // TODO refactor. This doesn't go through the network at all

private slots:
	void handleMessage(protocol::MessagePtr msg);
	void handleConnect(QString sessionId, int userid, bool join);
	void handleDisconnect(const QString &message, const QString &errorcode, bool localDisconnect);

private:
	void handleResetRequest(const protocol::ServerReply &msg);
	void handleServerCommand(const protocol::Command &msg);
	void handleDisconnectMessage(const protocol::Disconnect &msg);

	Server *_server;
	LoopbackServer *_loopback;

	QString m_sessionId;
	int m_myId;
	bool _isloopback;
	bool m_recordedChat;

	canvas::ToolContext m_lastToolCtx;
};

}

#endif
