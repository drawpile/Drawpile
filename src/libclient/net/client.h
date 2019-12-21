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
#ifndef DP_NET_CLIENT_H
#define DP_NET_CLIENT_H

#include "core/point.h"
#include "core/blendmodes.h"
#include "net/server.h"
#include "../libshared/net/message.h"

#include <QObject>
#include <QSslCertificate>
#include <QUrl>

class QJsonObject;
class QJsonArray;

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
	Client(QObject *parent=nullptr);
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
	uint8_t myId() const { return m_myId; }

	/**
	 * Return the URL of the current (or last connected) session
	 */
	QUrl sessionUrl(bool includeUser=false) const;

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
	bool isConnected() const { return !m_isloopback; }

	/**
	 * @brief Is the user connected and logged in?
	 * @return true if there is an active network connection and login process has completed
	 */
	bool isLoggedIn() const { return m_server->isLoggedIn(); }

	/**
	 * @brief Is teh user logged in as an authenticated user?
	 */
	bool isAuthenticated() const { return m_isAuthenticated; }

	/**
	 * @brief Is this user a moderator?
	 *
	 * Moderator status is a feature of the user account and cannot change during
	 * the connection.
	 */
	bool isModerator() const { return m_moderator; }

	/**
	 * @brief Get connection security level
	 */
	Server::Security securityLevel() const { return m_server->securityLevel(); }

	/**
	 * @brief Get host certificate
	 *
	 * This is meaningful only if securityLevel != NO_SECURITY
	 */
	QSslCertificate hostCertificate() const { return m_server->hostCertificate(); }

	/**
	 * @brief Does the server support persistent sessions?
	 *
	 * TODO for version 3.0: Change this to sessionSupportsPersistence
	 */
	bool serverSuppotsPersistence() const { return m_server->supportsPersistence(); }

	/**
	 * @brief Can the server receive abuse reports?
	 */
	bool serverSupportsReports() const { return m_server->supportsAbuseReports(); }

	bool sessionSupportsAutoReset() const { return m_supportsAutoReset; }

	/**
	 * @brief Get the number of bytes waiting to be sent
	 * @return upload queue length
	 */
	int uploadQueueBytes() const;

	//! Are we expecting more incoming data?
	bool isFullyCaughtUp() const { return m_catchupTo == 0; }

	/**
	 * @brief Whether to use recorded chat (Chat message) by default
	 *
	 * If set to false, chat messages are sent with ServerCommands and delivered
	 * only to the currently active users.
	 * @param recordedChat if true, chat messages are recorded in session history
	 */
	void setRecordedChatMode(bool recordedChat) { m_recordedChat = recordedChat; }

	/**
	 * Get the last session URL the client connected to.
	 */
	QUrl lastUrl() const { return m_lastUrl; }

public slots:
	/**
	 * @brief Send a message to the server
	 *
	 * The context ID of the message is automatically set to the local user's ID.
	 *
	 * If this is a Command type message, drawingCommandLocal is emitted
	 * before the message is sent.
	 *
	 * @param msg the message to send
	 */
	void sendMessage(const protocol::MessagePtr &msg);
	void sendMessages(const protocol::MessageList &msgs);

	//! Send messages as part of a session reset/init
	void sendResetMessages(const protocol::MessageList &msgs);

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

	void serverMessage(const QString &message, bool isAlert);
	void serverLog(const QString &message);

	void bytesReceived(int);
	void bytesSent(int);
	void lagMeasured(qint64);
	void autoresetRequested(int maxSize, bool query);
	void serverStatusUpdate(int historySize);

private slots:
	void handleMessage(const protocol::MessagePtr &msg);
	void handleConnect(const QUrl &url, uint8_t userid, bool join, bool auth, bool moderator, bool supportsAutoReset);
	void handleDisconnect(const QString &message, const QString &errorcode, bool localDisconnect);

private:
	void handleResetRequest(const protocol::ServerReply &msg);
	void handleServerCommand(const protocol::Command &msg);
	void handleDisconnectMessage(const protocol::Disconnect &msg);

	Server *m_server;
	LoopbackServer *m_loopback;

	QUrl m_lastUrl;
	uint8_t m_myId;
	bool m_isloopback;
	bool m_recordedChat;
	bool m_moderator;
	bool m_isAuthenticated;
	bool m_supportsAutoReset;

	int m_catchupTo;
	int m_caughtUp;
	int m_catchupProgress;
};

}

#endif
