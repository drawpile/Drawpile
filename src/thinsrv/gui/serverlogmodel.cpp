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

#include "thinsrv/gui/serverlogmodel.h"

#include <QDebug>

namespace server {
namespace gui {

ServerLogModel::ServerLogModel(QObject *parent)
	: JsonListModel({
		{"timestamp", tr("Time")},
		{"level", tr("Level")},
		{"topic", tr("Topic")},
		{"session", tr("Session")},
		{"user", tr("User")},
		{"message", tr("Message")},
	}, parent)
{
}

void ServerLogModel::addLogEntry(const QJsonObject &entry)
{
	beginInsertRows(QModelIndex(), 0, 0);
	m_list << entry;
	endInsertRows();
}

void ServerLogModel::addLogEntries(const QJsonArray &entries)
{
	if(entries.isEmpty())
		return;

	beginInsertRows(QModelIndex(), 0, entries.size()-1);
	for(int i=entries.size()-1;i>=0;--i)
		m_list << entries[i].toObject();
	endInsertRows();
}

QString ServerLogModel::lastTimestamp() const
{
	if(m_list.isEmpty())
		return QString();
	return m_list.last().toObject().value("timestamp").toString();
}

QVariant ServerLogModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row()<0 || index.row()>=m_list.size())
		return QVariant();

	if(role != Qt::DisplayRole)
		return QVariant();

	const QJsonObject &o = m_list.at(m_list.size() - index.row() - 1).toObject();
	return o.value(m_columns[index.column()].key).toVariant();
}

}
}

