#ifndef USERLISTMODEL_H
#define USERLISTMODEL_H

#include <QAbstractListModel>

#include "netstate.h"

class UserListModel : public QAbstractListModel {
	Q_OBJECT
	public:
		UserListModel(QObject *parent=0) : QAbstractListModel(parent) {}
		~UserListModel() {}

		QVariant data(const QModelIndex& index, int role) const
		{
			if(index.row() < 0 || index.row() >= users_.size())
				return QVariant();
			if(role == Qt::DisplayRole)
				return users_.at(index.row()).name;
			return QVariant();
		}

		int rowCount(const QModelIndex& parent) const
		{
			if(parent.isValid())
				return 0;
			return users_.count();
		}

		void addUser(const network::User& user)
		{
			beginInsertRows(QModelIndex(),users_.size(),users_.size());
			users_.append(user);
			endInsertRows();
		}

		void removeUser(int id)
		{
			for(int i=0;i<users_.size();++i) {
				if(users_.at(i).id == id) {
					beginRemoveRows(QModelIndex(),i,i);
					users_.removeAt(i);
					endRemoveRows();
					break;
				}
			}
		}

		void clearUsers()
		{
			if(users_.isEmpty()==false) {
				beginRemoveRows(QModelIndex(), 0, users_.size()-1);
				users_.clear();
				endRemoveRows();
			}
		}

	private:
		network::UserList users_;
};

#endif

