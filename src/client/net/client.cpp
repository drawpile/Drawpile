/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#include <QDebug>
#include <QImage>

#include "net/client.h"
#include "net/loopbackserver.h"
#include "net/tcpserver.h"
#include "net/utils.h"
#include "net/login.h"
#include "net/userlist.h"
#include "net/layerlist.h"

#include "statetracker.h"

#include "core/point.h"

#include "../shared/net/annotation.h"
#include "../shared/net/image.h"
#include "../shared/net/layer.h"
#include "../shared/net/meta.h"
#include "../shared/net/pen.h"
#include "../shared/net/snapshot.h"
#include "../shared/net/undo.h"

using protocol::MessagePtr;

namespace net {

Client::Client(QObject *parent)
	: QObject(parent), _my_id(1)
{
	_loopback = new LoopbackServer(this);
	_server = _loopback;
	_isloopback = true;
	_isOp = false;
	_isSessionLocked = false;
	_isUserLocked = false;

	_userlist = new UserListModel(this);
	_layerlist = new LayerListModel(this);

	connect(
		_loopback,
		SIGNAL(messageReceived(protocol::MessagePtr)),
		this,
		SLOT(handleMessage(protocol::MessagePtr))
	);

	connect(
		_layerlist,
		SIGNAL(layerOrderChanged(QList<uint8_t>)),
		this,
		SLOT(sendLayerReorder(QList<uint8_t>))
	);
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

	connect(server, SIGNAL(loggingOut()), this, SIGNAL(serverDisconnecting()));
	connect(server, SIGNAL(serverDisconnected(QString)), this, SLOT(handleDisconnect(QString)));
	connect(server, SIGNAL(loggedIn(int, bool)), this, SLOT(handleConnect(int, bool)));
	connect(server, SIGNAL(messageReceived(protocol::MessagePtr)), this, SLOT(handleMessage(protocol::MessagePtr)));

	connect(server, SIGNAL(expectingBytes(int)), this, SIGNAL(expectingBytes(int)));
	connect(server, SIGNAL(bytesReceived(int)), this, SIGNAL(bytesReceived(int)));
	connect(server, SIGNAL(bytesSent(int)), this, SIGNAL(bytesSent(int)));

	if(loginhandler->mode() == LoginHandler::HOST)
		loginhandler->setUserId(_my_id);

	emit serverConnected(loginhandler->url().host(), loginhandler->url().port());
	server->login(loginhandler);
}

void Client::disconnectFromServer()
{
	_server->logout();
}

bool Client::isLoggedIn() const
{
	return _server->isLoggedIn();
}

void Client::handleConnect(int userid, bool join)
{
	_my_id = userid;
	emit serverLoggedin(join);
}

void Client::handleDisconnect(const QString &message)
{
	emit serverDisconnected(message);
	_userlist->clearUsers();
	_layerlist->unlockAll();
	_server = _loopback;
	_isloopback = true;
	_isOp = false;
	_isSessionLocked = false;
	_isUserLocked = false;
	emit opPrivilegeChange(true); // user is always op in loopback mode
	emit sessionConfChange(false, false);
	emit lockBitsChanged();
}

void Client::init()
{
	_loopback->reset();
	_layerlist->clear();
}

bool Client::isLocalServer() const
{
	return _server->isLocal();
}

QString Client::myName() const
{
	return _userlist->getUserById(_my_id).name;
}

int Client::uploadQueueBytes() const
{
	return _server->uploadQueueBytes();
}

void Client::sendCanvasResize(const QSize &newsize)
{
	_server->sendMessage(MessagePtr(new protocol::CanvasResize(
		_my_id,
		newsize.width(),
		newsize.height()
	)));
}

void Client::sendNewLayer(int id, const QColor &fill, const QString &title)
{
	Q_ASSERT(id>=0 && id<256);
	_server->sendMessage(MessagePtr(new protocol::LayerCreate(_my_id, id, fill.rgba(), title)));
}

void Client::sendLayerAttribs(int id, float opacity, int blend)
{
	Q_ASSERT(id>=0 && id<256);
	_server->sendMessage(MessagePtr(new protocol::LayerAttributes(_my_id, id, opacity*255, blend)));
}

void Client::sendLayerTitle(int id, const QString &title)
{
	Q_ASSERT(id>=0 && id<256);
	_server->sendMessage(MessagePtr(new protocol::LayerRetitle(_my_id, id, title)));
}

void Client::sendLayerVisibility(int id, bool hide)
{
	// This one is actually a local only change
	_layerlist->setLayerHidden(id, hide);
	emit layerVisibilityChange(id, hide);
}

void Client::sendDeleteLayer(int id, bool merge)
{
	Q_ASSERT(id>=0 && id<256);
	_server->sendMessage(MessagePtr(new protocol::LayerDelete(_my_id, id, merge)));
}

void Client::sendLayerReorder(const QList<uint8_t> &ids)
{
	Q_ASSERT(ids.size()>0);
	_server->sendMessage(MessagePtr(new protocol::LayerOrder(_my_id, ids)));
}

void Client::sendToolChange(const drawingboard::ToolContext &ctx)
{
	// TODO check if needs resending
	_server->sendMessage(brushToToolChange(_my_id, ctx.layer_id, ctx.brush));
}

void Client::sendStroke(const paintcore::Point &point)
{
	protocol::PenPointVector v(1);
	v[0] = pointToProtocol(point);
	_server->sendMessage(MessagePtr(new protocol::PenMove(_my_id, v)));
}

void Client::sendStroke(const paintcore::PointVector &points)
{
	_server->sendMessage(MessagePtr(new protocol::PenMove(_my_id, pointsToProtocol(points))));
}

void Client::sendPenup()
{
	_server->sendMessage(MessagePtr(new protocol::PenUp(_my_id)));
}

/**
 * This one is a bit tricky, since the whole image might not fit inside
 * a single message. In that case, multiple PUTIMAGE commands will be sent.
 * 
 * @param layer layer onto which the image should be drawn
 * @param x image x coordinate
 * @param y imagee y coordinate
 * @param image image data
 */
void Client::sendImage(int layer, int x, int y, const QImage &image, bool blend)
{
	foreach(MessagePtr msg, putQImage(_my_id, layer, x, y, image, blend))
		_server->sendMessage(msg);

	emit sendingBytes(_server->uploadQueueBytes());
}

void Client::sendUndopoint()
{
	_server->sendMessage(MessagePtr(new protocol::UndoPoint(_my_id)));
}

void Client::sendUndo(int actions, int override)
{
	Q_ASSERT(actions != 0);
	Q_ASSERT(actions >= -128 && actions <= 127);
	_server->sendMessage(MessagePtr(new protocol::Undo(_my_id, override, actions)));
}

void Client::sendRedo(int actions, int override)
{
	sendUndo(-actions, override);
}

void Client::sendAnnotationCreate(int id, const QRect &rect)
{
	Q_ASSERT(id>=0 && id < 256);
	_server->sendMessage(MessagePtr(new protocol::AnnotationCreate(
		_my_id,
		id,
		qMax(0, rect.x()),
		qMax(0, rect.y()),
		rect.width(),
		rect.height()
	)));
}

void Client::sendAnnotationReshape(int id, const QRect &rect)
{
	Q_ASSERT(id>0 && id < 256);
	_server->sendMessage(MessagePtr(new protocol::AnnotationReshape(
		_my_id,
		id,
		qMax(0, rect.x()),
		qMax(0, rect.y()),
		rect.width(),
		rect.height()
	)));
}

void Client::sendAnnotationEdit(int id, const QColor &bg, const QString &text)
{
	Q_ASSERT(id>0 && id < 256);
	_server->sendMessage(MessagePtr(new protocol::AnnotationEdit(
		_my_id,
		id,
		bg.rgba(),
		text
	)));
}

void Client::sendAnnotationDelete(int id)
{
	Q_ASSERT(id>0 && id < 256);
	_server->sendMessage(MessagePtr(new protocol::AnnotationDelete(_my_id, id)));
}

/**
 * @brief Send the session initialization command stream
 * @param commands snapshot point commands
 */
void Client::sendSnapshot(const QList<protocol::MessagePtr> commands)
{
	// Send ACK to indicate the rest of the data is on its way
	_server->sendMessage(MessagePtr(new protocol::SnapshotMode(protocol::SnapshotMode::ACK)));

	// The actual snapshot data will be sent in parallel with normal session traffic
	_server->sendSnapshotMessages(commands);

	emit sendingBytes(_server->uploadQueueBytes());
}

void Client::sendChat(const QString &message)
{
	_server->sendMessage(MessagePtr(new protocol::Chat(0, message)));
}

/**
 * @brief Send a list of commands to initialize the session in local mode
 * @param commands
 */
void Client::sendLocalInit(const QList<protocol::MessagePtr> commands)
{
	Q_ASSERT(_isloopback);
	foreach(protocol::MessagePtr msg, commands)
		_loopback->sendMessage(msg);
}

void Client::sendLockUser(int userid, bool lock)
{
	Q_ASSERT(userid>0 && userid<256);
	QString cmd;
	if(lock)
		cmd = "/lock ";
	else
		cmd = "/unlock ";
	cmd += QString::number(userid);

	_server->sendMessage((MessagePtr(new protocol::Chat(0, cmd))));
}

void Client::sendKickUser(int userid)
{
	Q_ASSERT(userid>0 && userid<256);
	QString cmd = QString("/kick %1").arg(userid);
	_server->sendMessage((MessagePtr(new protocol::Chat(0, cmd))));
}

void Client::sendSetSessionTitle(const QString &title)
{
	_server->sendMessage(MessagePtr(new protocol::SessionTitle(_my_id, title)));
}

void Client::sendLockSession(bool lock)
{
	QString cmd;
	if(lock)
		cmd = "/lock";
	else
		cmd = "/unlock";

	_server->sendMessage(MessagePtr(new protocol::Chat(0, cmd)));
}

void Client::sendCloseSession(bool close)
{
	QString cmd;
	if(close)
		cmd = "/close";
	else
		cmd = "/open";

	_server->sendMessage(MessagePtr(new protocol::Chat(0, cmd)));
}

void Client::sendLayerAcl(int layerid, bool locked, QList<uint8_t> exclusive)
{
	if(_isloopback)
		qWarning() << "tried to send layer ACL in loopback mode!";
	else
		_server->sendMessage(MessagePtr(new protocol::LayerACL(_my_id, layerid, locked, exclusive)));
}

void Client::handleMessage(protocol::MessagePtr msg)
{
	// TODO should meta commands go here too for session recording purposes?
	if(msg->isCommand()) {
		emit drawingCommandReceived(msg);
		return;
	}
	// Not a command stream message? Must be a meta command then
	switch(msg->type()) {
	using namespace protocol;
	case MSG_SNAPSHOT:
		handleSnapshotRequest(msg.cast<SnapshotMode>());
		break;
	case MSG_CHAT:
		handleChatMessage(msg.cast<Chat>());
		break;
	case MSG_USER_JOIN:
		handleUserJoin(msg.cast<UserJoin>());
		break;
	case MSG_USER_ATTR:
		handleUserAttr(msg.cast<UserAttr>());
		break;
	case MSG_USER_LEAVE:
		handleUserLeave(msg.cast<UserLeave>());
		break;
	case MSG_SESSION_TITLE:
		emit sessionTitleChange(msg.cast<SessionTitle>().title());
		break;
	case MSG_SESSION_CONFIG:
		handleSessionConfChange(msg.cast<SessionConf>());
		break;
	case MSG_LAYER_ACL:
		handleLayerAcl(msg.cast<LayerACL>());
		break;
	default:
		qWarning() << "received unhandled meta command" << msg->type();
	}
}

void Client::handleSnapshotRequest(const protocol::SnapshotMode &msg)
{
	// The server should ever only send a REQUEST mode snapshot messages
	if(msg.mode() != protocol::SnapshotMode::REQUEST && msg.mode() != protocol::SnapshotMode::REQUEST_NEW) {
		qWarning() << "received unhandled snapshot mode" << msg.mode() << "message.";
		return;
	}

	emit needSnapshot(msg.mode() == protocol::SnapshotMode::REQUEST_NEW);
}

void Client::handleChatMessage(const protocol::Chat &msg)
{
	QString username;
	if(msg.contextId()!=0) {
		User user = _userlist->getUserById(msg.contextId());
		if(user.id==0)
			username = tr("User#%1").arg(msg.contextId());
		else
			username = user.name;
	}

	emit chatMessageReceived(username, msg.message(), msg.contextId() == _my_id);
}

void Client::handleUserJoin(const protocol::UserJoin &msg)
{
	_userlist->addUser(User(msg.contextId(), msg.name(), msg.contextId() == _my_id));
	emit userJoined(msg.name());
}

void Client::handleUserAttr(const protocol::UserAttr &msg)
{
	if(msg.contextId() == _my_id) {
		_isOp = msg.isOp();
		_isUserLocked = msg.isLocked();
		emit opPrivilegeChange(msg.isOp());
		emit lockBitsChanged();
	}
	_userlist->updateUser(msg.contextId(), msg.attrs());
}

void Client::handleUserLeave(const protocol::UserLeave &msg)
{
	QString name = _userlist->getUserById(msg.contextId()).name;
	_userlist->removeUser(msg.contextId());
	emit userLeft(name);
}

void Client::handleSessionConfChange(const protocol::SessionConf &msg)
{
	_isSessionLocked = msg.locked();
	emit sessionConfChange(msg.locked(), msg.closed());
	emit lockBitsChanged();
}

void Client::handleLayerAcl(const protocol::LayerACL &msg)
{
	_layerlist->updateLayerAcl(msg.id(), msg.locked(), msg.exclusive());
	emit lockBitsChanged();
}

}
