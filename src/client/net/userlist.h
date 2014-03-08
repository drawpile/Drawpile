/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

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
#ifndef DP_NET_USERLISTMODEL_H
#define DP_NET_USERLISTMODEL_H

#include <QAbstractListModel>
#include <QList>

namespace net {

/**
 * @brief Information about a user
 */
struct User {
	User() : id(0), name(QString()), isLocal(false), isOperator(false), isLocked(false) {}
	User(int id_, const QString &name_, bool local) : id(id_), name(name_), isLocal(local), isOperator(false), isLocked(false) {}

	int id;
	QString name;
	bool isLocal;
	bool isOperator;
	bool isLocked;
};

/**
 * A list model to represent session users.
 */
class UserListModel : public QAbstractListModel {
	Q_OBJECT
	public:
		UserListModel(QObject *parent=0);

		QVariant data(const QModelIndex& index, int role=Qt::DisplayRole) const;
		int rowCount(const QModelIndex& parent=QModelIndex()) const;

		void addUser(const User &user);
		void updateUser(int id, uchar attrs);
		void removeUser(int id);
		void clearUsers();

		/**
		 * @brief Get user info by ID
		 *
		 * This will return info about past users as well.
		 * @param id user id
		 * @return
		 */
		User getUserById(int id) const;

		/**
		 * @brief Get the name of the user with the given context ID
		 *
		 * If no such user exists, "User #X" is returned, where X is the ID number.
		 * @param id
		 * @return user name
		 */
		QString getUsername(int id) const;

	private:
		QVector<User> _users;
		QHash<int,User> _pastUsers;
};

}

Q_DECLARE_METATYPE(net::User)

#endif

