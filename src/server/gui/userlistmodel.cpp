/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "userlistmodel.h"

#include <QJsonObject>

namespace server {
namespace gui {

UserListModel::UserListModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

void UserListModel::setUserList(const QJsonArray &users)
{
	beginResetModel();
	m_users = users;
	endResetModel();
}

int UserListModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return 5;
}

QVariant UserListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation != Qt::Horizontal)
		return QVariant();

	if(role != Qt::DisplayRole)
		return QVariant();

	switch(section) {
	case 0: return tr("Session");
	case 1: return tr("ID");
	case 2: return tr("Name");
	case 3: return tr("Address");
	case 4: return tr("Features");
	}

	return QVariant();
}

int UserListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_users.size();
}

QVariant UserListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row()<0 || index.row()>=m_users.size())
		return QVariant();

	if(role != Qt::DisplayRole)
		return QVariant();

	QJsonObject u = m_users.at(index.row()).toObject();

	switch(index.column()) {
	case 0: return u["session"].toString();
	case 1: return u["id"].toInt();
	case 2: return u["name"].toString();
	case 3: return u["ip"].toString();
	case 4: {
		QStringList f;
		if(u["op"].toBool())
			f << "OP";
		if(u["mod"].toBool())
			f << "MOD";
		if(u["auth"].toBool())
			f << "Signed in";
		if(u["secure"].toBool())
			f << "TLS";
		return f.join(", ");
	}
	}

	return QVariant();
}

}
}
