/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
	: QObject(parent), board_(0), editor_(0), net_(0), session_(0)
{
	netstate_ = new network::HostState(this);
	connect(netstate_, SIGNAL(loggedin()), this, SIGNAL(loggedin()));
	connect(netstate_, SIGNAL(joined(int)), this, SLOT(sessionJoined(int)));
	connect(netstate_, SIGNAL(parted(int)), this, SLOT(sessionParted()));
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
 * Establish a connection with a server.
 * The connected signal will be emitted when connection is established
 * and loggedin when login is succesful.
 * @param address host address. Format: address[:port]
 * @param username username to use
 */
void Controller::connectHost(const QString& address, const QString& username)
{
	Q_ASSERT(net_ == 0);

	username_ = username;

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

/**
 * A new session is created and joined.
 * @param title session title
 * @param password session password. If empty, no password is set
 * @param image initial board image
 * @pre host connection must be established and user logged in.
 */
void Controller::hostSession(const QString& title, const QString& password,
		const QImage& image)
{
	netstate_->host(title, password, image.width(), image.height());
}

void Controller::disconnectHost()
{
	Q_ASSERT(net_);
	net_->disconnectHost();
}

/**
 * A session was joined
 */
void Controller::sessionJoined(int id)
{
	session_ = netstate_->session(id);
	emit joined();
}

/**
 * A session was left
 */
void Controller::sessionParted()
{
	session_ = 0;
	emit parted();
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
	netstate_->login(username_);
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

