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
#include <memory>
#include "network.h"
#include "netstate.h"

#include "../shared/protocol.h"

NetState::NetState(QObject *parent)
	: QObject(parent)
{
}

/**
 * Prepare the state machine for hosting a session
 * @param title session title
 * @param password session password. If empty, no password is required to join.
 */
void NetState::host(const QString& username, const QString& title,
		const QString& password)
{
	state_ = LOGIN;
	username_ = username;
	title_ = title;
	password_ = password;
}

void NetState::handleHostInfo(protocol::HostInfo *msg)
{
	// Handle host info
	qDebug() << "host info";
	// Reply with user info
	protocol::UserInfo *user = new protocol::UserInfo;
	user->event = protocol::user_event::Login;
	QByteArray name = username_.toUtf8();
	user->length = name.length();
	user->name = new char[name.length()];
	memcpy(user->name,name.constData(), name.length());

	net_->send(user);
}

