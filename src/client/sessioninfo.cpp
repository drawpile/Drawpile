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
#include "netstate.h"
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
	maxusers(info->limit)
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

}

