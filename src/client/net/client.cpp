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

#include "statetracker.h"

#include "core/point.h"

#include "../shared/net/pen.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"
#include "../shared/net/annotation.h"
#include "../shared/net/snapshot.h"

using protocol::MessagePtr;

namespace net {

Client::Client(QObject *parent)
	: QObject(parent), _my_id(1)
{
	_loopback = new LoopbackServer(this);
	_server = _loopback;
	_isloopback = true;

	connect(
		_loopback,
		SIGNAL(messageReceived(protocol::MessagePtr)),
		this,
		SLOT(handleMessage(protocol::MessagePtr))
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

	connect(server, SIGNAL(serverDisconnected(QString)), this, SLOT(handleDisconnect(QString)));
	connect(server, SIGNAL(loggedIn(int)), this, SLOT(handleConnect(int)));
	connect(server, SIGNAL(messageReceived(protocol::MessagePtr)), this, SLOT(handleMessage(protocol::MessagePtr)));

	if(loginhandler->mode() == LoginHandler::HOST)
		loginhandler->setUserId(_my_id);

	qDebug() << "connecting to server...";
	server->login(loginhandler);
}

void Client::handleConnect(int userid)
{
	_my_id = userid;
}

void Client::handleDisconnect(const QString &message)
{
	qDebug() << "server disconnected" << message;
	_server = _loopback;
	_isloopback = true;
	emit serverDisconnected();
}

void Client::init()
{
	_loopback->reset();
}

bool Client::isLocalServer() const
{
	return _server->isLocal();
}

void Client::sendCanvasResize(const QSize &newsize)
{
	_server->sendMessage(MessagePtr(new protocol::CanvasResize(
		newsize.width(),
		newsize.height()
	)));
}

void Client::sendNewLayer(int id, const QColor &fill, const QString &title)
{
	Q_ASSERT(id>=0 && id<256);
	_server->sendMessage(MessagePtr(new protocol::LayerCreate(id, fill.rgba(), title)));
}

void Client::sendLayerAttribs(int id, float opacity, const QString &title)
{
	Q_ASSERT(id>=0 && id<256);
	_server->sendMessage(MessagePtr(new protocol::LayerAttributes(id, opacity*255, 0, title)));
}

void Client::sendDeleteLayer(int id, bool merge)
{
	Q_ASSERT(id>=0 && id<256);
	_server->sendMessage(MessagePtr(new protocol::LayerDelete(id, merge)));
}

void Client::sendLayerReorder(const QList<uint8_t> &ids)
{
	Q_ASSERT(ids.size()>0);
	_server->sendMessage(MessagePtr(new protocol::LayerOrder(ids)));
}

void Client::sendToolChange(const drawingboard::ToolContext &ctx)
{
	// TODO check if needs resending
	_server->sendMessage(MessagePtr(new protocol::ToolChange(
		_my_id,
		ctx.layer_id,
		ctx.brush.blendingMode(),
		(ctx.brush.subpixel() ? protocol::TOOL_MODE_SUBPIXEL : 0),
		ctx.brush.spacing(),
		ctx.brush.color(1.0).rgba(),
		ctx.brush.color(0.0).rgba(),
		ctx.brush.hardness(1.0) * 255,
		ctx.brush.hardness(0.0) * 255,
		ctx.brush.radius(1.0),
		ctx.brush.radius(0.0),
		ctx.brush.opacity(1.0) * 255,
		ctx.brush.opacity(0.0) * 255
	)));
}

namespace {

/**
 * Convert a dpcore::Point to network format. The
 * reverse operation for this is in statetracker.cpp
 * @param p
 * @return
 */
protocol::PenPoint point2net(const dpcore::Point &p)
{
	// The two least significant bits of the coordinate
	// are the fractional part.
	// The rest is the integer part with a bias of 128
	uint16_t x = (qMax(0, p.x() + 128) << 2) | (uint16_t(p.xFrac()*4) & 3);
	uint16_t y = (qMax(0, p.y() + 128) << 2) | (uint16_t(p.yFrac()*4) & 3);

	return protocol::PenPoint(x, y, p.pressure() * 255);
}

}

void Client::sendStroke(const dpcore::Point &point)
{
	protocol::PenPointVector v(1);
	v[0] = point2net(point);
	_server->sendMessage(MessagePtr(new protocol::PenMove(_my_id, v)));
}

void Client::sendStroke(const dpcore::PointVector &points)
{
	protocol::PenPointVector v(points.size());
	for(int i=0;i<points.size();++i)
		v[i] = point2net(points[i]);
	
	_server->sendMessage(MessagePtr(new protocol::PenMove(_my_id, v)));
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
	foreach(MessagePtr msg, putQImage(layer, x, y, image, blend))
		_server->sendMessage(msg);
}

void Client::sendAnnotationCreate(int id, const QRect &rect)
{
	Q_ASSERT(id>=0 && id < 256);
	_server->sendMessage(MessagePtr(new protocol::AnnotationCreate(id,
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
		id,
		bg.rgba(),
		text
	)));
}

void Client::sendAnnotationDelete(int id)
{
	Q_ASSERT(id>0 && id < 256);
	_server->sendMessage(MessagePtr(new protocol::AnnotationDelete(id)));
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

void Client::handleMessage(protocol::MessagePtr msg)
{
	if(msg->isCommand()) {
		emit drawingCommandReceived(msg);
		return;
	}
	// Not a command stream message? Must be a meta command then
	qDebug() << "handling" << msg->type();
	switch(msg->type()) {
	case protocol::MSG_SNAPSHOT:
		handleSnapshotRequest(msg.cast<protocol::SnapshotMode>());
		break;
	default:
		qWarning() << "received unhandled meta(?) command" << msg->type();
	}
}

void Client::handleSnapshotRequest(const protocol::SnapshotMode &msg)
{
	// The server should ever only send a REQUEST mode snapshot messages
	if(msg.mode() != protocol::SnapshotMode::REQUEST) {
		qWarning() << "received unhandled snapshot mode" << msg.mode() << "message.";
	}

	qDebug() << "need snapshot!";
	emit needSnapshot();
}

}
