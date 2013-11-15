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

#include "client.h"
#include "loopbackserver.h"
#include "statetracker.h"

#include "core/point.h"

#include "../shared/net/pen.h"

namespace net {

Client::Client(QObject *parent)
	: QObject(parent), _my_id(0)
{
	_loopback = new LoopbackServer(this);
	_server = _loopback;
	
	connect(
		_loopback,
		SIGNAL(messageReceived(protocol::Message*)),
		this,
		SLOT(handleMessage(protocol::Message*))
	);
}

Client::~Client()
{
}

void Client::sendToolChange(const ToolContext &ctx)
{
	// TODO check if needs resending
	_server->sendMessage(new protocol::ToolChange(
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
	));
}

protocol::PenPoint point2net(const dpcore::Point &p)
{
	// TODO
	return protocol::PenPoint(p.x(), p.y(), p.pressure() * 255);
}

void Client::sendStroke(const dpcore::Point &point)
{
	protocol::PenPointVector v(1);
	v[0] = point2net(point);
	_server->sendMessage(new protocol::PenMove(_my_id, v));
}

void Client::sendStroke(const dpcore::PointVector &points)
{
	protocol::PenPointVector v(points.size());
	for(int i=0;i<points.size();++i)
		v[i] = point2net(points[i]);
	
	_server->sendMessage(new protocol::PenMove(_my_id, v));
}

void Client::sendPenup()
{
	_server->sendMessage(new protocol::PenUp(_my_id));
}

void Client::handleMessage(protocol::Message *msg)
{
	if(msg->isCommand()) {
		emit drawingCommandReceived(msg);
	}
}

}
