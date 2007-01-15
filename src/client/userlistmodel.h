#ifndef USERLISTMODEL_H
#define USERLISTMODEL_H

#include <QAbstractListModel>
#include <QItemDelegate>

#include "netstate.h"

class UserListModel : public QAbstractListModel {
	Q_OBJECT
	public:
		UserListModel(QObject *parent=0) : QAbstractListModel(parent) {}
		~UserListModel() {}

		QVariant data(const QModelIndex& index, int role) const;
		int rowCount(const QModelIndex& parent) const;
		void addUser(const network::User& user);
		void changeUser(const network::User& user);
		bool hasUser(int id) const;
		void removeUser(int id);
		void clearUsers();

	private:
		network::UserList users_;
};

class UserListDelegate : public QItemDelegate {
	Q_OBJECT
	public:
		UserListDelegate(QObject *parent=0);

		//! Show or hide admin commands
		void setAdminMode(bool enable);

		void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
		QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);
	signals:
		void lockUser(int id, bool lock);
		void kickUser(int id);

	private:
		bool enableadmin_;
		QPixmap lock_, unlock_, kick_;
};

#endif

