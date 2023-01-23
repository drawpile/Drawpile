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

#include "thinsrv/gui/accountlistmodel.h"

#include <QJsonObject>

namespace server {
namespace gui {

AccountListModel::AccountListModel(QObject *parent)
	: JsonListModel({
		{"username", tr("Username")},
		{"locked", tr("Locked")},
		{"flags", tr("Flags")}
	},parent)
{
}

void AccountListModel::addAccount(const QJsonObject &entry)
{
	beginInsertRows(QModelIndex(), m_list.size(), m_list.size());
	m_list << entry;
	endInsertRows();
}

void AccountListModel::updateAccount(const QJsonObject &entry)
{
	const int id = entry["id"].toInt();
	for(int i=0;i<m_list.size();++i) {
		if(m_list.at(i).toObject()["id"].toInt() == id) {
			m_list[i] = entry;
			emit dataChanged(index(i, 0), index(i, columnCount()));
			return;
		}
	}

}

void AccountListModel::removeAccount(int id)
{
	for(int i=0;i<m_list.size();++i) {
		if(m_list.at(i).toObject()["id"].toInt() == id) {
			beginRemoveRows(QModelIndex(), i, i);
			m_list.removeAt(i);
			endRemoveRows();
			return;
		}
	}
}

QVariant AccountListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Vertical) {
		if(role == Qt::DisplayRole && section>=0 && section < m_list.size())
			return m_list[section].toObject()["id"].toInt();
		return QVariant();
	}

	return JsonListModel::headerData(section, orientation, role);
}

}
}
