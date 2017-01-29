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
#ifndef DP_SERVER_CLIENT_H
#define DP_SERVER_CLIENT_H

#include "../net/message.h"
#include "jsonapi.h"

#include <QObject>
#include <QTcpSocket>

class QHostAddress;

namespace protocol {
	class MessageQueue;
	class Command;
}

namespace server {

class Session;
class Log;
class ServerLog;

/**
 * @brief Server client
 *
 * This class represents a client that connects to the server.
 * A client is initially in a "lobby" state, until it finishes the login
 * handshake, at which point it is assigned to a session.
 */
class Client : public QObject
{
    Q_OBJECT

public:
	Client(QTcpSocket *socket, ServerLog *logger, QObject *parent=nullptr);
	~Client();

	//! Get the user's IP address
	QHostAddress peerAddress() const;

	/**
	 * @brief Assign this client to a session
	 * @param session
	 */
	void setSession(Session *session);
	Session *session() { return m_session; }

	/**
	 * @brief Get the context ID of the client
	 *
	 * This is initially zero until the login process is complete.
	 *
	 * The ID is assigned by the session the client is joining, or by the client
	 * itself when hosting a new session
	 * @return client ID
	 */
	int id() const { return m_id; }
	void setId(int id) { m_id = id; }

	/**
	 * @brief Get the user name of this client
	 * @return user name
	 */
	const QString &username() const { return m_username; }
	void setUsername(const QString &username) { m_username = username; }

	/**
	 * @brief Does this user have session operator privileges?
	 * @return
	 */
	bool isOperator() const { return m_isOperator || m_isModerator; }
	void setOperator(bool op) { m_isOperator = op; }

	/**
	 * @brief Is this user a moderator?
	 * Moderators can access any session, always have OP status and cannot be kicked by other users.
	 */
	bool isModerator() const { return m_isModerator; }
	void setModerator(bool mod) { m_isModerator = mod; }

	/**
	 * @brief Has this user been authenticated?
	 */
	bool isAuthenticated() const { return m_isAuthenticated; }
	void setAuthenticated(bool auth) { m_isAuthenticated = auth; }

	/**
	 * @brief Has this user been blocked from sending chat messages?
	 */
	bool isMuted() const { return m_isMuted; }
	void setMuted(bool m) { m_isMuted = m; }

	/**
	 * @brief Set connection idle timeout
	 * @param timeout timeout in milliseconds
	 */
	void setConnectionTimeout(int timeout);

#ifndef NDEBUG
	void setRandomLag(uint lag);
#endif

	/**
	 * @brief Kick this user off the server
	 * @param kickedBy username of the operator
	 */
	void disconnectKick(const QString &kickedBy);

	/**
	 * @brief Disconnect user due to an error
	 * @param message
	 */
	void disconnectError(const QString &message);

	/**
	 * @brief Disconnect user due to shutdown
	 */
	void disconnectShutdown();

	/**
	 * @brief Send a message directly to this client
	 *
	 * Note. Typically messages are sent via the shared session history. Direct
	 * messages are used during the login phase and for client specific notifications.
	 * @param msg
	 */
	void sendDirectMessage(protocol::MessagePtr msg);

	/**
	 * @brief Send a message from the server directly to this user
	 * @param message
	 */
	void sendSystemChat(const QString &message);

	/**
	 * @brief Get this client's position in the session history
	 * The returned index in the index of the last history message that
	 * is (or was) in the client's upload queue.
	 */
	int historyPosition() const { return m_historyPosition; }

	/**
	 * @brief Manually change the history position.
	 *
	 * Only time this needs to be done is during the session initialization
	 * phase when the hosting user must skip the history they themselves uploaded.
	 */
	void setHistoryPosition(int newpos) { m_historyPosition = newpos; }

	QString toLogString() const;

	/**
	 * @brief Does this client socket support SSL connections?
	 *
	 * Note. This means serverside support. The actual client might not support
	 * SSL.
	 * @return true if server has support for SSL for this client
	 */
	bool hasSslSupport() const;

	/**
	 * @brief Is this connection secure?
	 * @return
	 */
	bool isSecure() const;

	/**
	 * @brief Start SSL handshake
	 */
	void startTls();

	/**
	 * @brief Send all the messages that were held in queue
	 */
	void enqueueHeldCommands();

	/**
	 * @brief Get a Join message for this user
	 */
	protocol::MessagePtr joinMessage() const;

	/**
	 * @brief Get a JSON object describing this user
	 *
	 * This is used by the admin API
	 */
	QJsonObject description(bool includeSession=true) const;

	/**
	 * @brief Call the client's JSON administration API
	 *
	 * This is used by the HTTP admin API.
	 *
	 * @param method query method
	 * @param path path components
	 * @param request request body content
	 * @return JSON API response content
	 */
	JsonApiResult callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

	/**
	 * @brief Write a log entry
	 *
	 * The user and session fields are filled in automatically.
	 * Note: this can only be used after the user has joined a session.
	 */
	void log(Log entry) const;

signals:
	/**
	 * @brief Message received while not part of a session
	 */
	void loginMessage(protocol::MessagePtr message);

	/**
	 * @brief This client is disconnecting
	 *
	 * This signal may be emitted twice: first when the client
	 * is instructed to disconnect (e.g. kicked)
	 * and the second time when the socket actually disconnects.
	 */
	void loggedOff(Client *client);

public slots:
	void sendNextHistoryBatch();

private slots:
	void gotBadData(int len, int type);
	void receiveMessages();
	void socketError(QAbstractSocket::SocketError error);
	void socketDisconnect();

private:
	void handleSessionMessage(protocol::MessagePtr msg);
	bool isHoldLocked() const;

	Session *m_session;
	QTcpSocket *m_socket;
	ServerLog *m_logger;

	protocol::MessageQueue *m_msgqueue;
	QList<protocol::MessagePtr> m_holdqueue;
	int m_historyPosition;

	int m_id;
	QString m_username;

	bool m_isOperator;
	bool m_isModerator;
	bool m_isAuthenticated;
	bool m_isMuted;
};

}

#endif
