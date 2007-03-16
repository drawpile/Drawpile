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
#ifndef USERLISTWIDGET_H
#define USERLISTWIDGET_H

#include <QDockWidget>

class QListView;
class UserListModel;
class UserListDelegate;

namespace network {
	class User;
	class SessionState;
}

namespace widgets {

//! User list window
/**
 * A dock widget that displays a list of users, with session administration
 * controls.
 */
class UserList: public QDockWidget
{
	Q_OBJECT
	public:
		UserList(QWidget *parent=0);
		~UserList();

		void setAdminMode(bool enable);
		void setSession(network::SessionState *session);

	private:
		QListView *list_;
		UserListModel *model_;
		UserListDelegate *delegate_;
};

}

#endif

