/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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
#include <QStringList>

#include "sessioninfo.h"
#include "network.h"
#include "hoststate.h"
#include "sessionstate.h"
#if 0
#include "../shared/protocol.h"
#endif

namespace network {

Session::Session(const QStringList& tokens)
{
	Q_ASSERT(tokens.size()==5);
	Q_ASSERT(tokens.at(0).compare("BOARD")==0);
	owner = tokens.at(1).toInt();
	title = tokens.at(2);
	width = tokens.at(3).toInt();
	height = tokens.at(4).toInt();
	maxusers = 255;
}

Session::Session(int o) :
	owner(o), title(""), width(0), height(0), maxusers(0) { }

QStringList Session::tokens() const {
	QStringList tk;
	tk << "BOARD" << QString::number(owner) << title <<
		QString::number(width) << QString::number(height);
	return tk;
}

User::User()
	: name_(QString()), id_(-1), locked_(false), owner_(0)
{}

User::User(const QString& name, int id, bool locked, SessionState *owner)
	: name_(name), id_(id), locked_(locked), owner_(owner)
{
}

User::User(SessionState *owner, const QStringList& tokens)
	: owner_(owner)
{
	Q_ASSERT(tokens.size()==4);
	Q_ASSERT(tokens.at(0).compare("USER")==0);
	id_ = tokens.at(1).toInt();
	name_ = tokens.at(2);
	locked_ = tokens.at(3).toInt();
}

void User::setLocked(bool lock)
{
	locked_ = lock;
}

bool User::isLocal() const
{
	return owner_->host()->localUser() == id_;
}

bool User::isOwner() const
{
	return owner_->info().owner == id_;
}

/**
 * Sends a request to the server to lock or unlock the user.
 * Only the session owner may use this command.
 * @param l lock or unlock
 */
void User::lock(bool l)
{
#if 0
	protocol::SessionEvent *msg = new protocol::SessionEvent(
			(l ? protocol::SessionEvent::Lock : protocol::SessionEvent::Unlock),
			id_,
			0 // aux (unused)
			);
	
	msg->session_id = owner_->info().id;
	
	owner_->host()->connection()->send(msg);

#endif
}

/**
 * Sends a request to the server to kick the user from the session.
 * Only the session owner may use this command.
 */
void User::kick()
{
#if 0
	protocol::SessionEvent *msg = new protocol::SessionEvent(
			protocol::SessionEvent::Kick,
			id_,
			0 // aux (unused)
			);
	
	msg->session_id = owner_->info().id;
	
	owner_->host()->connection()->send(msg);
#endif
}

}

