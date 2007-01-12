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

#include <QAbstractListModel>
#include <QListView>

#include "userlistwidget.h"
#include "netstate.h"

class UserListModel : public QAbstractListModel {
	public:
		QVariant data(const QModelIndex& index, int role = Qt::DisplayRole)
		{
			return users_.at(index.row()).name;
		}

		int rowCount(const QModelIndex& parent = QModelIndex()) const
		{
			return users_.count();
		}

		void addUser(const network::User& user)
		{
			users_.append(user);
		}

		void removeUser(int id)
		{
			for(network::UserList::iterator i = users_.begin();
					i!=users_.end();++i) {
				if(i->id == id) {
					i.remove();
					break;
				}
			}
		}

	private:
		network::UserList users_;
};

QVariant UserListModel::data(const QModelIndex& index, int role)
{
	return users_.at(index.row());
}

namespace widgets {

UserList::UserList(QWidget *parent)
	:QDockWidget(tr("Users"), parent)
{
	list_ = new QListView(this);
	setWidget(list_);
}

UserList::~UserList()
{
}

}

