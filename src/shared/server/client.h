/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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

#include <QObject>
#include <QHostAddress>
#include <QTcpSocket>

#include "../net/message.h"
#include "../util/logger.h"

namespace protocol {
	class MessageQueue;
	class Login;
	class SnapshotMode;
	class Undo;
}

namespace server {

class SessionState;

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
	enum State {
		LOGIN,
		WAIT_FOR_SYNC,
		IN_SESSION
	};

public:
	explicit Client(QTcpSocket *socket, QObject *parent=0);
	~Client();

	//! Get the user's IP address
	QHostAddress peerAddress() const;

	/**
	 * @brief Assign this client to a session
	 * @param session
	 */
	void setSession(SessionState *session);
	SessionState *session() { return _session; }

	/**
	 * @brief Get the context ID of the client
	 *
	 * This is initially zero until the login process is complete.
	 *
	 * The ID is assigned by the session the client is joining, or by the client
	 * itself when hosting a new session
	 * @return client ID
	 */
	int id() const { return _id; }
	void setId(int id) { _id = id; }

	/**
	 * @brief Get the user name of this client
	 * @return user name
	 */
	const QString &username() const { return _username; }
	void setUsername(const QString &username) { _username = username; }

	/**
	 * @brief Does this user have session operator privileges?
	 * @return
	 */
	bool isOperator() const { return _isOperator || _isModerator; }

	/**
	 * @brief Is this user a moderator?
	 * Moderators can access any session, always have OP status and cannot be kicked by other users.
	 */
	bool isModerator() const { return _isModerator; }
	void setModerator(bool mod) { _isModerator = mod; }

	/**
	 * @brief Has this user been authenticated?
	 */
	bool isAuthenticated() const { return _isAuth; }
	void setAuthenticated(bool auth) { _isAuth = auth; }

	/**
	 * @brief Set connection idle timeout
	 * @param timeout timeout in milliseconds
	 */
	void setConnectionTimeout(int timeout);

	/**
	 * @brief Is this user locked individually?
	 * @return true if user lock is set
	 */
	bool isUserLocked() const { return _userLock; }

	/**
	 * @brief Is this client currently downloading the latest snapshot?
	 * @return
	 */
	bool isDownloadingLatestSnapshot() const;

	/**
	 * @brief Is this user currently uploading a snapshot?
	 * @return
	 */
	bool isUploadingSnapshot() const;

	/**
	 * @brief Request the client to generate a snapshot
	 *
	 * This causes the creation of a new snapshot point on the main stream.
	 */
	void requestSnapshot(bool forcenew);

	/**
	 * @brief Is this client's input queue on hold?
	 *
	 * Hold-lock is a type of lock where the input queue is merely put
	 * on hold (some non-drawing commands are still allowed though). Once
	 * the hold-lock is released, the queued commands will be processed.
	 * @return
	 */
	bool isHoldLocked() const;

	/**
	 * @brief Has this user been barrier locked?
	 *
	 * Barrier locking is used to synchronize users for snapshot generation
	 * @return
	 */
	bool isBarrierLocked() const { return _barrierlock == BARRIER_LOCKED; }

	/**
	 * @brief Is the client barrier locked or waiting for barrier lock?
	 * @return
	 */
	bool hasBarrierLock() const { return _barrierlock != BARRIER_NOTLOCKED; }

	/**
	 * @brief Is this client's input queue being ignored?
	 *
	 * Drop lock is a type of lock where all drawing commands are being
	 * dropped.
	 * @return
	 */
	bool isDropLocked() const;

	/**
	 * @brief Grant operator privileges to this user
	 */
	void grantOp();

	/**
	 * @brief Revoke this user's operator privileges
	 */
	void deOp();

	/**
	 * @brief Set the user specific lock on this user
	 */
	void lockUser();

	/**
	 * @brief Remove the user specific lock from this user
	 */
	void unlockUser();

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
	 * @brief Barrier lock this user
	 *
	 * If this user is currently drawing, the lock won't take place immediately
	 *
	 */
	void barrierLock();

	/**
	 * @brief Lift barrier lock
	 */
	void barrierUnlock();

	/**
	 * @brief Send a message directly to this client
	 *
	 * Note. Typically messages are sent via the main message stream. This is
	 * used during the login phase and for client specific notifications.
	 * @param msg
	 */
	void sendDirectMessage(protocol::MessagePtr msg);

	/**
	 * @brief Send a message from the server directly to this user
	 * @param message
	 */
	void sendSystemChat(const QString &message);

	/**
	 * @brief Get this client's position in the message stream
	 * @return message stream index
	 */
	int streampointer() const { return _streampointer; }

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
	 * @brief Send client attribute status update
	 *
	 * This is sent automatically by all functions that affect client
	 * status, but is also sent explicitly when the client first joins a session
	 */
	void sendUpdatedAttrs();

signals:
	void loginMessage(protocol::MessagePtr message);
	void disconnected(Client *client);
	void barrierLocked();

public slots:
	/**
	 * @brief Enqueue all available commands for sending
	 */
	void sendAvailableCommands();

	/**
	 * @brief A new snapshot was just created
	 */
	void snapshotNowAvailable();

private slots:
	void gotBadData(int len, int type);
	void receiveMessages();
	void receiveSnapshot();
	void socketError(QAbstractSocket::SocketError error);
	void socketDisconnect();

private:
	void handleSessionMessage(protocol::MessagePtr msg);
	void handleLoginMessage(const protocol::Login &msg);
	void handleLoginPassword(const QString &pass);
	void handleHostSession(const QString &msg);
	void handleJoinSession(const QString &msg);
	void handleSnapshotStart(const protocol::SnapshotMode &msg);

	void handleUndoPoint();
	bool handleUndoCommand(protocol::Undo &undo);

	void sendOpWhoList();
	void sendOpServerStatus();

	void updateState(protocol::MessagePtr msg);

	void enqueueHeldCommands();

	bool isLayerLocked(int layerid);

	SessionState *_session;

	QTcpSocket *_socket;
	protocol::MessageQueue *_msgqueue;
	QList<protocol::MessagePtr> _holdqueue;

	State _state;
	int _substate;
	bool _awaiting_snapshot;
	bool _uploading_snapshot;

	int _streampointer;
	int _substreampointer;

	int _id;
	QString _username;

	//! Does this user have operator privileges?
	bool _isOperator;

	//! Does this user have moderator privileges?
	bool _isModerator;

	//! Is this user authenticated (as opposed to a guest)
	bool _isAuth;

	//! Is this user locked? (by an operator)
	bool _userLock;

	//! User's barrier (snapshot sync) lock status
	enum {BARRIER_NOTLOCKED, BARRIER_WAIT, BARRIER_LOCKED } _barrierlock;
};

}

#endif
