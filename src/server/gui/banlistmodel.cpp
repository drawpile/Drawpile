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

#include "banlistmodel.h"

#include <QJsonObject>

namespace server {
namespace gui {

BanListModel::BanListModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

void BanListModel::setBanList(const QJsonArray &banlist)
{
	beginResetModel();
	m_banlist = banlist;
	endResetModel();
}

void BanListModel::addBanEntry(const QJsonObject &entry)
{
	beginInsertRows(QModelIndex(), m_banlist.size(), m_banlist.size());
	m_banlist << entry;
	endInsertRows();
}

void BanListModel::removeBanEntry(int id)
{
	for(int i=0;i<m_banlist.size();++i) {
		if(m_banlist.at(i).toObject()["id"].toInt() == id) {
			beginRemoveRows(QModelIndex(), i, i);
			m_banlist.removeAt(i);
			endRemoveRows();
			return;
		}
	}
}

int BanListModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return 4;
}

QVariant BanListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Vertical) {
		if(role == Qt::DisplayRole && section>=0 && section < m_banlist.size())
			return m_banlist[section].toObject()["id"].toInt();
		return QVariant();
	}

	if(role != Qt::DisplayRole)
		return QVariant();

	switch(section) {
	case 0: return tr("Address");
	case 1: return tr("Expires");
	case 2: return tr("Added");
	case 3: return tr("Comment");
	}

	return QVariant();
}

int BanListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_banlist.size();
}

QVariant BanListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row()<0 || index.row()>=m_banlist.size())
		return QVariant();

	if(role == Qt::UserRole)
		return m_banlist.at(index.row()).toObject()["id"].toInt();

	if(role != Qt::DisplayRole)
		return QVariant();

	const QJsonObject o = m_banlist.at(index.row()).toObject();

	switch(index.column()) {
	case 0:
		if(o["subnet"].toInt()>0)
			return QString("%1/%2").arg(o["ip"].toString()).arg(o["subnet"].toInt());
		else
			return o["ip"].toString();

	case 1: return o["expires"].toString();
	case 2: return o["added"].toString();
	case 3: return o["comment"].toString();
	}

	return QVariant();
}

}
}
