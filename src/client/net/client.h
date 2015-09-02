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
	class Chat;
	class UserJoin;
	class SessionOwner;
	class UserLeave;
	class SessionConf;
	class LayerACL;
	class MovePointer;
	class Marker;
	class Disconnect;
	struct ServerReply;
}

namespace net {
	
class LoopbackServer;
class LoginHandler;
class UserListModel;
class LayerListModel;
class AclFilter;

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
	 * @brief Get the local user's username
	 * @return user name
	 */
	QString myName() const;

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
	 * @brief Get the user list
	 * @return user list model
	 */
	UserListModel *userlist() const { return m_userlist; }

	/**
	 * @brief Get the layer list
	 * @return layer list model
	 */
	LayerListModel *layerlist() const { return m_layerlist; }

	/**
	 * @brief Get the ACL filter
	 * @return
	 */
	AclFilter *aclFilter() const { return m_aclfilter; }

	//! Reinitialize after clearing out the old board
	void init();

	/**
	 * @brief Whether to use recorded chat (Chat message) by default
	 *
	 * If set to false, chat messages are sent with ServerCommands and delivered
	 * only to the currently active users.
	 * @param recordedChat if true, chat messages are recorded in session history
	 */
	void setRecordedChatMode(bool recordedChat) { m_recordedChat = recordedChat; }

public slots:
	// Layer changing
	void sendCanvasResize(int top, int right, int bottom, int left);
	void sendNewLayer(int id, int source, const QColor &fill, bool insert, bool copy, const QString &title);
	void sendLayerAttribs(int id, float opacity, paintcore::BlendMode::Mode blend);
	void sendLayerTitle(int id, const QString &title);
	void sendLayerVisibility(int id, bool hide);
	void sendLayerReorder(const QList<uint16_t> &ids);
	void sendDeleteLayer(int id, bool merge);

	// Drawing
	void sendToolChange(const canvas::ToolContext &ctx);
	void sendStroke(const paintcore::Point &point);
	void sendStroke(const paintcore::PointVector &points);
	void sendPenup();
	void sendImage(int layer, int x, int y, const QImage &image, paintcore::BlendMode::Mode mode=paintcore::BlendMode::MODE_NORMAL);
	void sendFillRect(int layer, const QRect &rect, const QColor &color, paintcore::BlendMode::Mode blend);

	// Undo/redo
	void sendUndopoint();
	void sendUndo(int actions=1, int override=0);
	void sendRedo(int actions=1, int override=0);

	// Annotations
	void sendAnnotationCreate(int id, const QRect &rect);
	void sendAnnotationReshape(int id, const QRect &rect);
	void sendAnnotationEdit(int id, const QColor &bg, const QString &text);
	void sendAnnotationDelete(int id);

	// Snapshot
	void sendInitialSnapshot(const QList<protocol::MessagePtr> commands);

	// Misc.
	void sendChat(const QString &message, bool announce, bool action);
	void sendServerCommand(const QString &cmd, const QJsonArray &args=QJsonArray(), const QJsonObject &kwargs=QJsonObject());
	void sendLaserPointer(const QPointF &point, int trail=0);
	void sendMarker(const QString &text);

	// Operator commands
	void sendLockUser(int userid, bool lock);
	void sendKickUser(int userid);
	void sendOpUser(int userid, bool op);
	void sendSetSessionTitle(const QString &title);
	void sendLayerAcl(int layerid, bool locked, QList<uint8_t> exclusive);
	void sendLockSession(bool lock);
	void sendLockLayerControls(bool lock, bool own);
	void sendCloseSession(bool close);
	void sendResetSession();

	// Recording
	void playbackCommand(protocol::MessagePtr msg);
	void endPlayback();

signals:
	void messageReceived(protocol::MessagePtr msg);
	void drawingCommandLocal(protocol::MessagePtr msg);
	void drawingCommandReceived(protocol::MessagePtr msg);
	void chatMessageReceived(const QString &user, const QString &message, bool announcement, bool action, bool me);
	void markerMessageReceived(const QString &user, const QString &message);
	void userPointerMoved(int ctx, const QPointF &point, int trail);

	void needSnapshot();
	void sessionResetted();

	void serverConnected(const QString &address, int port);
	void serverLoggedin(bool join);
	void serverDisconnecting();
	void serverDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);

	void userJoined(int id, const QString &name);
	void userLeft(const QString &name);
	void youWereKicked(const QString &kickedBy);

	void canvasLocked(bool locked);
	void opPrivilegeChange(bool op);
	void sessionConfChange(const QJsonObject &config);
	void lockBitsChanged();

	void layerVisibilityChange(int id, bool hidden);

	void expectingBytes(int);
	void sendingBytes(int);
	void bytesReceived(int);
	void bytesSent(int);
	void lagMeasured(qint64);

	void sentColorChange(const QColor &color);

private slots:
	void handleMessage(protocol::MessagePtr msg);
	void handleConnect(QString sessionId, int userid, bool join);
	void handleDisconnect(const QString &message, const QString &errorcode, bool localDisconnect);

private:
	void handleResetRequest(const protocol::ServerReply &msg);
	void handleChatMessage(const protocol::Chat &msg);
	void handleMarkerMessage(const protocol::Marker &msg);
	void handleUserJoin(const protocol::UserJoin &msg);
	void handleUserLeave(const protocol::UserLeave &msg);
	void handleServerCommand(const protocol::Command &msg);
	void handleMovePointer(const protocol::MovePointer &msg);
	void handleDisconnectMessage(const protocol::Disconnect &msg);

	void sendCommand(protocol::MessagePtr msg);

	Server *_server;
	LoopbackServer *_loopback;

	QString m_sessionId;
	int m_myId;
	bool _isloopback;
	bool m_recordedChat;
	UserListModel *m_userlist;
	LayerListModel *m_layerlist;
	AclFilter *m_aclfilter;

	canvas::ToolContext m_lastToolCtx;
};

}

#endif
