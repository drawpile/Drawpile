/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "thinsrv/gui/jsonlistmodel.h"

#include <QJsonObject>

namespace server {
namespace gui {

JsonListModel::JsonListModel(const QVector<Column> &columns, QObject *parent)
	: QAbstractTableModel(parent), m_columns(columns)
{
}

void JsonListModel::setList(const QJsonArray &list)
{
	beginResetModel();
	m_list = list;
	endResetModel();
}

int JsonListModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_columns.size();
}

QVariant JsonListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < m_columns.size())
		return m_columns.at(section).title;

	return QVariant();
}

int JsonListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_list.size();
}

QVariant JsonListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row()<0 || index.row()>=m_list.size())
		return QVariant();

	if(role == Qt::UserRole)
		return getId(m_list.at(index.row()).toObject());

	if(role != Qt::DisplayRole)
		return QVariant();

	if(index.column() < 0 || index.column() > m_columns.size())
		return QVariant();

	const QJsonObject o = m_list.at(index.row()).toObject();
	return getData(m_columns[index.column()].key, o);
}

Qt::ItemFlags JsonListModel::flags(const QModelIndex &index) const
{
	if(!index.isValid() || index.row()<0 || index.row()>=m_list.size())
		return Qt::NoItemFlags;

	return getFlags(m_list.at(index.row()).toObject());
}

QVariant JsonListModel::getData(const QString &key, const QJsonObject &obj) const
{
	return obj[key].toVariant();
}

Qt::ItemFlags JsonListModel::getFlags(const QJsonObject &obj) const
{
	Q_UNUSED(obj);
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int JsonListModel::getId(const QJsonObject &obj) const
{
	return obj["id"].toInt();
}

}
}
