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

#include "serverlogmodel.h"

#include <QJsonArray>
#include <QDebug>

namespace server {
namespace gui {

ServerLogModel::ServerLogModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

void ServerLogModel::addLogEntry(const QJsonObject &entry)
{
	beginInsertRows(QModelIndex(), 0, 0);
	m_log << entry;
	endInsertRows();
}

void ServerLogModel::addLogEntries(const QJsonArray &entries)
{
	if(entries.isEmpty())
		return;

	beginInsertRows(QModelIndex(), 0, entries.size()-1);
	for(int i=entries.size()-1;i>=0;--i)
		m_log << entries[i].toObject();
	endInsertRows();
}

QString ServerLogModel::lastTimestamp() const
{
	if(m_log.isEmpty())
		return QString();
	return m_log.last().value("timestamp").toString();
}

int ServerLogModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return 6;
}

QVariant ServerLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QVariant();

	switch(section) {
	case 0: return tr("Time");
	case 1: return tr("Level");
	case 2: return tr("Topic");
	case 3: return tr("Session");
	case 4: return tr("User");
	case 5: return tr("Message");
	}

	return QVariant();
}

int ServerLogModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_log.size();
}

QVariant ServerLogModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row()<0 || index.row()>=m_log.size())
		return QVariant();

	if(role != Qt::DisplayRole)
		return QVariant();

	const QJsonObject &o = m_log.at(m_log.size() - index.row() - 1);

	switch(index.column()) {
	case 0: return o["timestamp"].toString();
	case 1: return o["level"].toString();
	case 2: return o["topic"].toString();
	case 3: return o["session"].toString();
	case 4: return o["user"].toString();
	case 5: return o["message"].toString();
	}

	return QVariant();
}

}
}

