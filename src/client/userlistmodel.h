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
#ifndef USERLISTMODEL_H
#define USERLISTMODEL_H

#include <QAbstractListModel>
#include <QItemDelegate>
#include <QList>

#if 0
namespace network {
	class SessionState;
}

/**
 * A list model to represent session users
 */
class UserListModel : public QAbstractListModel {
	Q_OBJECT
	public:
		UserListModel(QObject *parent=0);
		~UserListModel() {}

		void setSession(network::SessionState *session);

		QVariant data(const QModelIndex& index, int role) const;
		int rowCount(const QModelIndex& parent) const;

	private slots:
		void addUser(int id);
		void removeUser(int id);
		void updateUsers();

	private:
		network::SessionState *session_;
		QList<int> users_;
};

/**
 * A delegate to display a session user, optionally with buttons to lock or kick the user.
 */
class UserListDelegate : public QItemDelegate {
	Q_OBJECT
	public:
		UserListDelegate(QObject *parent=0);

		//! Show or hide admin commands
		void setAdminMode(bool enable);

		void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
		QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);

	private:
		bool enableadmin_;
};

#endif

#endif

