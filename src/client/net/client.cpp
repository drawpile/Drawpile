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
#include <QDebug>
#include <QImage>

#include "net/client.h"
#include "net/loopbackserver.h"
#include "net/tcpserver.h"
#include "net/utils.h"
#include "net/login.h"
#include "net/userlist.h"
#include "net/layerlist.h"
#include "net/aclfilter.h"

#include "core/point.h"

#include "../shared/net/control.h"
#include "../shared/net/annotation.h"
#include "../shared/net/image.h"
#include "../shared/net/layer.h"
#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/flow.h"
#include "../shared/net/pen.h"
#include "../shared/net/undo.h"
#include "../shared/net/recording.h"

using protocol::MessagePtr;

namespace net {

Client::Client(QObject *parent)
	: QObject(parent), m_myId(1), m_recordedChat(false)
{
	_loopback = new LoopbackServer(this);
	_server = _loopback;
	_isloopback = true;

	m_userlist = new UserListModel(this);
	m_layerlist = new LayerListModel(this);
	m_aclfilter = new AclFilter(m_userlist, m_layerlist, this);

	m_layerlist->setMyId(m_myId);
	m_aclfilter->reset(m_myId, true);

	connect(_loopback, &LoopbackServer::messageReceived, this, &Client::handleMessage);
	connect(m_layerlist, &LayerListModel::layerOrderChanged, this, &Client::sendLayerReorder);
	connect(m_aclfilter, &AclFilter::localOpChanged, this, &Client::opPrivilegeChange);
	connect(m_aclfilter, &AclFilter::localLockChanged, this, &Client::lockBitsChanged);
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
	m_layerlist->setMyId(userid);
	m_aclfilter->reset(m_myId, false);

	// Joining: we'll get the correct layer list from the server
	if(join)
		m_layerlist->clear();

	emit serverLoggedin(join);
}

void Client::handleDisconnect(const QString &message,const QString &errorcode, bool localDisconnect)
{
	Q_ASSERT(_server != _loopback);

	emit serverDisconnected(message, errorcode, localDisconnect);
	m_userlist->clearUsers();
	m_aclfilter->reset(m_myId, true);
	static_cast<TcpServer*>(_server)->deleteLater();
	_server = _loopback;
	_isloopback = true;
}

void Client::init()
{
	m_layerlist->clear();
	m_aclfilter->reset(m_myId, true);
}

bool Client::isLocalServer() const
{
	return _server->isLocal();
}

QString Client::myName() const
{
	return m_userlist->getUserById(m_myId).name;
}

int Client::uploadQueueBytes() const
{
	return _server->uploadQueueBytes();
}

void Client::sendCommand(protocol::MessagePtr msg)
{
	Q_ASSERT(msg->isCommand());
	emit drawingCommandLocal(msg);
	_server->sendMessage(msg);
}

void Client::sendCanvasResize(int top, int right, int bottom, int left)
{
	sendCommand(MessagePtr(new protocol::CanvasResize(
		m_myId,
		top, right, bottom, left
	)));
}

void Client::sendNewLayer(int id, int source, const QColor &fill, bool insert, bool copy, const QString &title)
{
	Q_ASSERT(id>0 && id<=0xffff);
	uint8_t flags = 0;
	if(copy) flags |= protocol::LayerCreate::FLAG_COPY;
	if(insert) flags |= protocol::LayerCreate::FLAG_INSERT;

	sendCommand(MessagePtr(new protocol::LayerCreate(m_myId, id, source, fill.rgba(), flags, title)));
}

void Client::sendLayerAttribs(int id, float opacity, paintcore::BlendMode::Mode blend)
{
	Q_ASSERT(id>=0 && id<=0xffff);
	sendCommand(MessagePtr(new protocol::LayerAttributes(m_myId, id, opacity*255, int(blend))));
}

void Client::sendLayerTitle(int id, const QString &title)
{
	Q_ASSERT(id>=0 && id<=0xffff);
	sendCommand(MessagePtr(new protocol::LayerRetitle(m_myId, id, title)));
}

void Client::sendLayerVisibility(int id, bool hide)
{
	// This one is actually a local only change
	m_layerlist->setLayerHidden(id, hide);
	emit layerVisibilityChange(id, hide);
}

void Client::sendDeleteLayer(int id, bool merge)
{
	Q_ASSERT(id>=0 && id<=0xffff);
	sendCommand(MessagePtr(new protocol::LayerDelete(m_myId, id, merge)));
}

void Client::sendLayerReorder(const QList<uint16_t> &ids)
{
	Q_ASSERT(ids.size()>0);
	sendCommand(MessagePtr(new protocol::LayerOrder(m_myId, ids)));
}

void Client::sendToolChange(const canvas::ToolContext &ctx)
{
	if(ctx != m_lastToolCtx) {
		sendCommand(brushToToolChange(m_myId, ctx.layer_id, ctx.brush));
		m_lastToolCtx = ctx;
		if(ctx.brush.blendingMode() != 0) // color is not used in erase mode
			emit sentColorChange(ctx.brush.color());
	}
}

void Client::sendStroke(const paintcore::Point &point)
{
	protocol::PenPointVector v(1);
	v[0] = pointToProtocol(point);
	sendCommand(MessagePtr(new protocol::PenMove(m_myId, v)));
}

void Client::sendStroke(const paintcore::PointVector &points)
{
	sendCommand(MessagePtr(new protocol::PenMove(m_myId, pointsToProtocol(points))));
}

void Client::sendPenup()
{
	sendCommand(MessagePtr(new protocol::PenUp(m_myId)));
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
void Client::sendImage(int layer, int x, int y, const QImage &image, paintcore::BlendMode::Mode mode)
{
	// Note: since we can't know how sendImage is used, we set skipempty to false when using the replace mode.
	QList<protocol::MessagePtr> msgs = putQImage(m_myId, layer, x, y, image, mode, mode!=paintcore::BlendMode::MODE_REPLACE);
	for(MessagePtr msg : msgs)
		sendCommand(msg);

	if(isConnected())
		emit sendingBytes(_server->uploadQueueBytes());
}

void Client::sendFillRect(int layer, const QRect &rect, const QColor &color, paintcore::BlendMode::Mode blend)
{
	sendCommand(MessagePtr(new protocol::FillRect(
		m_myId, layer,
		int(blend),
		rect.x(), rect.y(),
		rect.width(), rect.height(),
		color.rgba()
	)));
}

void Client::sendUndopoint()
{
	sendCommand(MessagePtr(new protocol::UndoPoint(m_myId)));
}

void Client::sendUndo(int actions, int override)
{
	Q_ASSERT(actions != 0);
	Q_ASSERT(actions >= -128 && actions <= 127);
	sendCommand(MessagePtr(new protocol::Undo(m_myId, override, actions)));
}

void Client::sendRedo(int actions, int override)
{
	sendUndo(-actions, override);
}

void Client::sendAnnotationCreate(int id, const QRect &rect)
{
	Q_ASSERT(id>0 && id <=0xffff);
	sendCommand(MessagePtr(new protocol::AnnotationCreate(
		m_myId,
		id,
		rect.x(),
		rect.y(),
		rect.width(),
		rect.height()
	)));
}

void Client::sendAnnotationReshape(int id, const QRect &rect)
{
	Q_ASSERT(id>0 && id <=0xffff);
	sendCommand(MessagePtr(new protocol::AnnotationReshape(
		m_myId,
		id,
		rect.x(),
		rect.y(),
		rect.width(),
		rect.height()
	)));
}

void Client::sendAnnotationEdit(int id, const QColor &bg, const QString &text)
{
	Q_ASSERT(id>0 && id <=0xffff);
	sendCommand(MessagePtr(new protocol::AnnotationEdit(
		m_myId,
		id,
		bg.rgba(),
		text
	)));
}

void Client::sendAnnotationDelete(int id)
{
	Q_ASSERT(id>0 && id <=0xffff);
	sendCommand(MessagePtr(new protocol::AnnotationDelete(m_myId, id)));
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
	if(announce || m_recordedChat || _isloopback) {
		_server->sendMessage(MessagePtr(new protocol::Chat(m_myId, message, announce, action)));
	} else {
		QJsonObject kwargs;
		if(action)
			kwargs["action"] = true;

		sendServerCommand("chat", QJsonArray() << message, kwargs);
	}
}

void Client::sendServerCommand(const QString &cmd, const QJsonArray &args, const QJsonObject &kwargs)
{
	protocol::ServerCommand c { cmd, args, kwargs };
	_server->sendMessage(MessagePtr(new protocol::Command(m_myId, c)));
}

void Client::sendLaserPointer(const QPointF &point, int trail)
{
	Q_ASSERT(trail>=0);
	_server->sendMessage(MessagePtr(new protocol::MovePointer(m_myId, point.x() * 4, point.y() * 4, trail)));
}

void Client::sendMarker(const QString &text)
{
	// Keep markers private
	handleMessage(MessagePtr(new protocol::Marker(m_myId, text)));
}

void Client::sendLockUser(int userid, bool lock)
{
	Q_ASSERT(userid>0 && userid<256);
	QList<uint8_t> ids = m_userlist->lockList();
	int oldLen = ids.size();
	if(lock) {
		if(!ids.contains(userid))
			ids.append(userid);
	} else {
		ids.removeAll(userid);
	}
	if(ids.size() != oldLen)
		_server->sendMessage(MessagePtr(new protocol::UserACL(m_myId, ids)));
}

void Client::sendOpUser(int userid, bool op)
{
	Q_ASSERT(userid>0 && userid<255);

	// List of current ops
	QList<uint8_t> ops = m_userlist->operatorList();

	if(op)
		ops.append(userid);
	else
		ops.removeOne(userid);

	_server->sendMessage(protocol::MessagePtr(new protocol::SessionOwner(m_myId, ops)));
}

void Client::sendKickUser(int userid)
{
	Q_ASSERT(userid>0 && userid<256);
	sendServerCommand("kick", QJsonArray() << userid);
}

void Client::sendSetSessionTitle(const QString &title)
{
	QJsonObject kwargs;
	kwargs["title"] = title;
	sendServerCommand("sessionconf", QJsonArray(), kwargs);
}

template<typename T> void setFlag(T &flags, T flag, bool on) {
	if(on)
		flags |= flag;
	else
		flags &= ~flag;
}

void Client::sendLockSession(bool lock)
{
	uint16_t flags = m_aclfilter->sessionAclFlags();
	setFlag(flags, protocol::SessionACL::LOCK_SESSION, lock);

	_server->sendMessage(MessagePtr(new protocol::SessionACL(m_myId, flags)));
}

void Client::sendLockLayerControls(bool lock, bool ownlayers)
{
	uint16_t flags = m_aclfilter->sessionAclFlags();
	setFlag(flags, protocol::SessionACL::LOCK_LAYERCTRL, lock);
	setFlag(flags, protocol::SessionACL::LOCK_OWNLAYERS, ownlayers);

	_server->sendMessage(MessagePtr(new protocol::SessionACL(m_myId, flags)));
}

void Client::sendCloseSession(bool close)
{
	QJsonObject kwargs;
	kwargs["closed"] = close;
	sendServerCommand("sessionconf", QJsonArray(), kwargs);
}

void Client::sendResetSession()
{
	sendServerCommand("reset-session");
}

void Client::sendLayerAcl(int layerid, bool locked, QList<uint8_t> exclusive)
{
	if(_isloopback) {
		// Allow layer locking in loopback mode. Exclusive access doesn't make any sense in this mode.
		_server->sendMessage(MessagePtr(new protocol::LayerACL(m_myId, layerid, locked, QList<uint8_t>())));

	} else {
		_server->sendMessage(MessagePtr(new protocol::LayerACL(m_myId, layerid, locked, exclusive)));
	}
}

void Client::playbackCommand(protocol::MessagePtr msg)
{
	if(_isloopback)
		_server->sendMessage(msg);
	else
		qWarning() << "tried to play back command in network mode";
}

void Client::endPlayback()
{
	m_userlist->clearUsers();
}

void Client::handleMessage(protocol::MessagePtr msg)
{
	// Filter messages
	if(!m_aclfilter->filterMessage(*msg)) {
		qDebug("Filtered message %d from %d", msg->type(), msg->contextId());
		return;
	}

	// Emit message as-is for recording
	emit messageReceived(msg);

	// Emit command stream messages for drawing
	if(msg->isCommand()) {
		emit drawingCommandReceived(msg);
		return;
	}

	// Handle meta messages here
	switch(msg->type()) {
	using namespace protocol;
	case MSG_COMMAND:
		handleServerCommand(msg.cast<Command>());
		break;
	case MSG_CHAT:
		handleChatMessage(msg.cast<Chat>());
		break;
	case MSG_USER_JOIN:
		handleUserJoin(msg.cast<UserJoin>());
		break;
	case MSG_USER_LEAVE:
		handleUserLeave(msg.cast<UserLeave>());
		break;
	case MSG_SESSION_OWNER:
	case MSG_USER_ACL:
	case MSG_SESSION_ACL:
	case MSG_LAYER_ACL:
		// Handled by the ACL filter
		break;
	case MSG_INTERVAL:
		/* intervals are used only when playing back recordings */
		break;
	case MSG_MOVEPOINTER:
		handleMovePointer(msg.cast<MovePointer>());
		break;
	case MSG_MARKER:
		handleMarkerMessage(msg.cast<Marker>());
		break;
	case MSG_DISCONNECT:
		handleDisconnectMessage(msg.cast<Disconnect>());
		break;
	default:
		qWarning() << "received unhandled meta command" << msg->type();
	}
}

void Client::handleResetRequest(const protocol::ServerReply &msg)
{
	if(msg.reply["state"] == "init") {
		qDebug("Requested session reset");
		emit needSnapshot();

	} else if(msg.reply["state"] == "reset") {
		qDebug("Resetting session!");
		m_aclfilter->reset(m_myId, false);
		emit sessionResetted();

	} else {
		qWarning() << "Unknown reset state:" << msg.reply["state"].toString();
		qWarning() << msg.message;
	}
}

void Client::handleChatMessage(const protocol::Chat &msg)
{
	QString username = m_userlist->getUsername(msg.contextId());
	emit chatMessageReceived(
		username,
		msg.message(),
		msg.isAnnouncement(),
		msg.isAction(),
		msg.contextId() == m_myId
	);
}

void Client::handleMarkerMessage(const protocol::Marker &msg)
{
	emit markerMessageReceived(
		m_userlist->getUsername(msg.contextId()),
		msg.text()
	);
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

	emit chatMessageReceived(tr("Server"), chat, false, false, false);
}

void Client::handleUserJoin(const protocol::UserJoin &msg)
{
	User u(msg.contextId(), msg.name(), msg.contextId() == m_myId, msg.isAuthenticated(), msg.isModerator());
	if(m_aclfilter->isLockedByDefault())
		u.isLocked = true;

	m_userlist->addUser(u);
	emit userJoined(msg.contextId(), msg.name());
}

void Client::handleUserLeave(const protocol::UserLeave &msg)
{
	QString name = m_userlist->getUserById(msg.contextId()).name;
	m_userlist->removeUser(msg.contextId());
	emit userLeft(name);
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
	case ServerReply::MESSAGE:
	case ServerReply::CHAT:
	case ServerReply::ALERT:
	case ServerReply::ERROR:
	case ServerReply::RESULT:
		emit chatMessageReceived(
			m_userlist->getUsername(reply.reply.value("user").toInt()),
			reply.message,
			false,
			reply.reply.value("options").toObject().value("action").toBool(),
			false);
		break;
	case ServerReply::SESSIONCONF:
		emit sessionConfChange(reply.reply["config"].toObject());
		break;
	case ServerReply::RESET:
		handleResetRequest(reply);
		break;
	}
}

void Client::handleMovePointer(const protocol::MovePointer &msg)
{
	emit userPointerMoved(msg.contextId(), QPointF(msg.x() / 4.0, msg.y() / 4.0), msg.persistence());
}

}
