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

#include <QListView>

#include "userlistwidget.h"
#include "userlistmodel.h"

namespace widgets {

UserList::UserList(QWidget *parent)
	:QDockWidget(tr("Users"), parent)
{
	list_ = new QListView(this);
	setWidget(list_);

	model_ = new UserListModel(this);
	list_->setModel(model_);
}

UserList::~UserList()
{
}

void UserList::addUser(const network::User& user)
{
	model_->addUser(user);
}

void UserList::removeUser(const network::User& user)
{
	model_->removeUser(user.id);
}

void UserList::clearUsers()
{
	model_->clearUsers();
}

}

