/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#include "controller.h"
#include "board.h"
#include "brush.h"
#include "tools.h"
#include "boardeditor.h"
#include "network.h"
#include "netstate.h"

#include "../shared/protocol.defaults.h"

Controller::Controller(QObject *parent)
	: QObject(parent), board_(0), editor_(0), net_(0)
{
	netstate_ = new network::HostState(this);
}

Controller::~Controller()
{
}

void Controller::setModel(drawingboard::Board *board,
		interface::BrushSource *brush,
		interface::ColorSource *color)
{
	board_ = board;
	board_->addUser(0);
	editor_ = board->getEditor(true);
	editor_->setBrushSource(brush);
	editor_->setColorSource(color);
}

/**
 * @param address address
 * @param username username
 * @param title session tile
 * @param password
 */
void Controller::hostSession(const QString& address, const QString& username,
		const QString& title, const QString& password, const QImage& image)
{
	Q_ASSERT(username.isEmpty() == false);
	connectHost(address);
	netstate_->prepareHost(username, title, password,
			image.width(), image.height());
}

/**
 * Establish a connection with a server
 * @param address host address. Format: address[:port]
 * @param username username to use
 */
void Controller::connectHost(const QString& address)
{
	Q_ASSERT(net_ == 0);

	// Parse address
	address_ = address;
	quint16 port = protocol::default_port;
	QStringList addr = address.split(":", QString::SkipEmptyParts);
	if(addr.count()>1)
		port = addr[1].toInt();

	// Create network thread object
	net_ = new network::Connection(this);
	connect(net_,SIGNAL(connected()), this, SLOT(netConnected()));
	connect(net_,SIGNAL(disconnected(QString)), this, SLOT(netDisconnected(QString)));
	connect(net_,SIGNAL(error(QString)), this, SLOT(netError(QString)));
	connect(net_,SIGNAL(received()), netstate_, SLOT(receiveMessage()));

	// Connect to host
	netstate_->setConnection(net_);
	net_->connectHost(addr[0], port);
}

void Controller::disconnectHost()
{
	Q_ASSERT(net_);
	net_->disconnectHost();
}

void Controller::setTool(tools::Type tool)
{
	tool_ = tools::Tool::get(editor_,tool);
}

void Controller::penDown(const drawingboard::Point& point, bool isEraser)
{
	tool_->begin(point);
	if(tool_->readonly()==false)
		emit changed();
}

void Controller::penMove(const drawingboard::Point& point)
{
	tool_->motion(point);
}

void Controller::penUp()
{
	tool_->end();
}

void Controller::netConnected()
{
	netstate_->login();
	emit connected(address_);
}

void Controller::netDisconnected(const QString& message)
{
	qDebug() << "disconnect: " << message;
	emit disconnected();
	net_->wait();
	delete net_;
	net_ = 0;
}

void Controller::netError(const QString& message)
{
	qDebug() << "error: " << message;
}

