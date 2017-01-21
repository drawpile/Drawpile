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

#include "accountlistmodel.h"

#include <QJsonObject>

namespace server {
namespace gui {

AccountListModel::AccountListModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

void AccountListModel::setAccountList(const QJsonArray &accounts)
{
	beginResetModel();
	m_accountlist = accounts;
	endResetModel();
}

void AccountListModel::addAccount(const QJsonObject &entry)
{
	beginInsertRows(QModelIndex(), m_accountlist.size(), m_accountlist.size());
	m_accountlist << entry;
	endInsertRows();
}

void AccountListModel::updateAccount(const QJsonObject &entry)
{
	const int id = entry["id"].toInt();
	for(int i=0;i<m_accountlist.size();++i) {
		if(m_accountlist.at(i).toObject()["id"].toInt() == id) {
			m_accountlist[i] = entry;
			emit dataChanged(index(i, 0), index(i, columnCount()));
			return;
		}
	}

}

void AccountListModel::removeAccount(int id)
{
	for(int i=0;i<m_accountlist.size();++i) {
		if(m_accountlist.at(i).toObject()["id"].toInt() == id) {
			beginRemoveRows(QModelIndex(), i, i);
			m_accountlist.removeAt(i);
			endRemoveRows();
			return;
		}
	}
}

QJsonObject AccountListModel::accountAt(int row) const
{
	return m_accountlist.at(row).toObject();
}

int AccountListModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return 3;
}

QVariant AccountListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Vertical) {
		if(role == Qt::DisplayRole && section>=0 && section < m_accountlist.size())
			return m_accountlist[section].toObject()["id"].toInt();
		return QVariant();
	}

	if(role != Qt::DisplayRole)
		return QVariant();

	switch(section) {
	case 0: return tr("Username");
	case 1: return tr("Locked");
	case 2: return tr("Flags");
	}

	return QVariant();
}

int AccountListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_accountlist.size();
}

QVariant AccountListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row()<0 || index.row()>=m_accountlist.size())
		return QVariant();

	if(role == Qt::UserRole)
		return m_accountlist.at(index.row()).toObject()["id"].toInt();

	if(role != Qt::DisplayRole)
		return QVariant();

	const QJsonObject o = m_accountlist.at(index.row()).toObject();

	switch(index.column()) {
	case 0: return o["username"].toString();
	case 1: return o["locked"].toBool();
	case 2: return o["flags"].toString();
	}

	return QVariant();
}

}
}
