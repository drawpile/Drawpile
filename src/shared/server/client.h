/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
	Client(QTcpSocket *socket, SharedLogger logger, QObject *parent=0);
	~Client();

	//! Get the user's IP address
	QHostAddress peerAddress() const;

	/**
	 * @brief Assign this client to a session
	 * @param session
	 */
	void setSession(SessionState *session);

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
	bool isOperator() const { return _isOperator; }

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
	 * @param kickedBy user ID of the kicker
	 */
	void kick(int kickedBy=0);

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
	 * @brief Get this client's position in the message stream
	 * @return message stream index
	 */
	int streampointer() const { return _streampointer; }

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

	bool handleOperatorCommand(uint8_t ctxid, const QString &cmd);
	void handleUndoPoint();
	bool handleUndoCommand(protocol::Undo &undo);

	void sendOpWhoList();
	void sendOpServerStatus();

	void updateState(protocol::MessagePtr msg);

	void enqueueHeldCommands();
	void sendUpdatedAttrs();

	bool isLayerLocked(int layerid);

	SessionState *_session;
	SharedLogger _logger;

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

	//! Is this user locked? (by an operator)
	bool _userLock;

	//! User's barrier (snapshot sync) lock status
	enum {BARRIER_NOTLOCKED, BARRIER_WAIT, BARRIER_LOCKED } _barrierlock;
};

}

#endif
