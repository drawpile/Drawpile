// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_SERVER_CLIENT_H
#define DP_SERVER_CLIENT_H
#include "libserver/jsonapi.h"
#include "libshared/net/message.h"
#include <QAbstractSocket>
#include <QObject>

class QHostAddress;
class QTcpSocket;
#ifdef HAVE_WEBSOCKETS
class QWebSocket;
#endif

namespace net {
class MessageQueue;
}

namespace server {

class Session;
class Log;
class ServerLog;
struct BanResult;

/**
 * @brief Server client
 *
 * This class represents a client that connects to the server.
 * A client is initially in a "lobby" state, until it finishes the login
 * handshake, at which point it is assigned to a session.
 */
class Client : public QObject {
	Q_OBJECT

public:
	~Client() override;

	//! Get the user's IP address
	QHostAddress peerAddress() const;

	/**
	 * @brief Assign this client to a session
	 * @param session
	 */
	void setSession(Session *session);
	Session *session();

	/**
	 * @brief Get the context ID of the client
	 *
	 * This is initially zero until the login process is complete.
	 *
	 * The ID is assigned by the session the client is joining, or by the client
	 * itself when hosting a new session
	 * @return client ID
	 */
	uint8_t id() const;
	void setId(uint8_t id);

	/**
	 * @brief Get an authenticated user's flags
	 *
	 * User flags come from the account system, either the built-in one or
	 * ext-auth.
	 */
	QStringList authFlags() const;
	void setAuthFlags(const QStringList &flags);

	/**
	 * @brief Get the user name of this client
	 * @return user name
	 */
	const QString &username() const;
	void setUsername(const QString &username);

	/**
	 * @brief Get this user's avatar.
	 *
	 * The avatar should be a PNG image.
	 * @return
	 */
	const QByteArray &avatar() const;
	void setAvatar(const QByteArray &avatar);

	/**
	 * @brief Get the user's authentication ID
	 *
	 * For internal servers, this is typically something like "_int_ID_".
	 * For ext-auth servers, this is the value returned in the "uid" field
	 * of the authentication token. It is unique only within the same ext-auth
	 * server, but should identify the user even when the username is changed.
	 */
	const QString &authId() const;
	void setAuthId(const QString &id);

	const QString &sid() const;
	void setSid(const QString &sid);

	/**
	 * @brief Does this user have session operator privileges?
	 * @return
	 */
	bool isOperator() const;
	void setOperator(bool op);

	/**
	 * @brief Is this user a deputy (but not an operator)
	 *
	 * Deputies have more limited permissions than operators.
	 */
	bool isDeputy() const;

	/**
	 * @brief Is this user a moderator?
	 * Moderators can access any session, always have OP status and cannot be
	 * kicked by other users.
	 */
	bool isModerator() const;
	bool isGhost() const;
	void setModerator(bool mod, bool ghost);

	/**
	 * @brief Is this a trusted user?
	 *
	 * The trust flag is granted by session operators. It's effects are purely
	 * clientside, but the server is aware of it so it can remember it for
	 * authenticated users.
	 */
	bool isTrusted() const;
	void setTrusted(bool trusted);

	/**
	 * @brief Has this user been authenticated (using either an internal account
	 * or ext-auth)?
	 */
	bool isAuthenticated() const;

	/**
	 * @brief Has this user been blocked from sending chat messages?
	 */
	bool isMuted() const;
	void setMuted(bool m);

	bool isBanInProgress() const;
	void applyBanExemption(bool exempt);
	void applyBan(const BanResult &ban);
	bool triggerBan(bool early);

	/**
	 * @brief Set connection idle timeout
	 * @param timeout timeout in milliseconds
	 */
	void setConnectionTimeout(int timeout);
	void setKeepAliveTimeout(int timeout);

	/**
	 * Get the timestamp of this client's last activity (i.e. non-keepalive
	 * message received)
	 *
	 * Returned value is given in milliseconds since Epoch.
	 */
	qint64 lastActive() const;

	/**
	 * Same as lastActive, but only for actual drawing commands.
	 */
	qint64 lastActiveDrawing() const;

	enum class DisconnectionReason {
		Kick,	  // kicked by an operator
		Error,	  // kicked due to some server or protocol error
		Shutdown, // the server is shutting down
	};

	/**
	 * @brief Disconnect this client
	 *
	 * Note. This does not immediately disconnect the client. Instead, a
	 * disconnect command is added to the client's upload queue.
	 *
	 * @param reason
	 * @param message
	 */
	void disconnectClient(DisconnectionReason reason, const QString &message);

	/**
	 * @brief Send a message directly to this client
	 *
	 * Note. Typically messages are sent via the shared session history. Direct
	 * messages are used during the login phase and for client specific
	 * notifications.
	 * @param msg
	 */
	void sendDirectMessage(const net::Message &msg);
	void sendDirectMessages(const net::MessageList &msgs);

	/**
	 * @brief Send a message from the server directly to this user
	 * @param message
	 */
	void sendSystemChat(const QString &message, bool alert = false);

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
	 * @brief Get a Join message for this user
	 */
	net::Message joinMessage() const;

	/**
	 * @brief Get a JSON object describing this user
	 *
	 * This is used by the admin API
	 */
	QJsonObject description(bool includeSession = true) const;

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
	JsonApiResult callJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request);

	/**
	 * @brief Divert incoming messages to a holding buffer
	 *
	 * When hold-locked, all incoming messages are saved in a buffer
	 * until the lock is released.
	 *
	 * @param lock
	 */
	void setHoldLocked(bool lock);
	bool isHoldLocked() const;

	/**
	 * @brief Block all messages sent to this user
	 *
	 * This state is set when a fresh reset is imminent and we don't want to
	 * send any messages to the client before that happens.
	 */
	void setAwaitingReset(bool awaiting);
	bool isAwaitingReset() const;

	/**
	 * @brief Write a log entry
	 *
	 * The user and session fields are filled in automatically.
	 * Note: this can only be used after the user has joined a session.
	 */
	void log(Log entry) const;
	Log log() const;

signals:
	/**
	 * @brief Message received while not part of a session
	 */
	void loginMessage(net::Message message);

	/**
	 * @brief This client is disconnecting
	 *
	 * This signal may be emitted twice: first when the client
	 * is instructed to disconnect (e.g. kicked)
	 * and the second time when the socket actually disconnects.
	 */
	void loggedOff(Client *client);

private slots:
	void gotBadData(int len, int type);
	void receiveMessages();
	void socketError(QAbstractSocket::SocketError error);
	void readError();
	void writeError();
	void timedOut(qint64 idleTimeout);
	void socketDisconnect();

protected:
	Client(
		QTcpSocket *tcpSocket, ServerLog *logger, bool decodeOpaque,
		QObject *parent);
#ifdef HAVE_WEBSOCKETS
	Client(
		QWebSocket *webSocket, const QHostAddress &ip, ServerLog *logger,
		bool decodeOpaque, QObject *parent);
#endif
	net::MessageQueue *messageQueue();

private:
	void handleSessionMessage(net::Message msg);
	static bool rollEarlyTrigger();
	void triggerNormalBan();
	void triggerNetError();
	void triggerGarbage();
	void triggerHang();
	void triggerTimer();

	struct Private;
	Private *d;
};

}

#endif
