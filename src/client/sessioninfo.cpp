/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#include "sessioninfo.h"
#include "network.h"
#include "hoststate.h"
#include "sessionstate.h"
#include "../shared/protocol.h"

namespace network {

Session::Session(const protocol::SessionInfo *info)
	: id(info->session_id),
	owner(info->owner),
	title(QString::fromUtf8(info->title)),
	width(info->width),
	height(info->height),
	mode(info->mode),
	maxusers(info->limit),
	protocollevel(info->level)
{
}

User::User()
	: name_(QString()), id_(-1), locked_(false), owner_(0)
{}

User::User(const QString& name, int id, bool locked, SessionState *owner)
	: name_(name), id_(id), locked_(locked), owner_(owner)
{
}

void User::setLocked(bool lock)
{
	locked_ = lock;
}

bool User::isLocal() const
{
	return owner_->host()->localUser().id() == id_;
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
	protocol::SessionEvent *msg = new protocol::SessionEvent(
			(l ? protocol::session_event::Lock : protocol::session_event::Unlock),
			id_,
			0 // aux (unused)
			);
	
	msg->session_id = owner_->info().id;
	
	owner_->host()->connection()->send(msg);

}

/**
 * Sends a request to the server to kick the user from the session.
 * Only the session owner may use this command.
 */
void User::kick()
{
	protocol::SessionEvent *msg = new protocol::SessionEvent(
			protocol::session_event::Kick,
			id_,
			0 // aux (unused)
			);
	
	msg->session_id = owner_->info().id;
	
	owner_->host()->connection()->send(msg);
}

}

