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

#include "sessionlistmodel.h"

#include <QJsonObject>

namespace server {
namespace gui {

SessionListModel::SessionListModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

void SessionListModel::setSessionList(const QJsonArray &sessions)
{
	beginResetModel();
	m_sessions = sessions;
	endResetModel();
}

int SessionListModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return 7;
}

QVariant SessionListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation != Qt::Horizontal)
		return QVariant();

	if(role != Qt::DisplayRole)
		return QVariant();

	switch(section) {
	case 0: return tr("ID");
	case 1: return tr("Alias");
	case 2: return tr("Title");
	case 3: return tr("Users");
	case 4: return tr("Options");
	case 5: return tr("Started");
	case 6: return tr("Size");
	}

	return QVariant();
}

int SessionListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_sessions.size();
}

QVariant SessionListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid() || index.row()<0 || index.row()>=m_sessions.size())
		return QVariant();

	if(role != Qt::DisplayRole)
		return QVariant();

	const QJsonObject s = m_sessions.at(index.row()).toObject();

	switch(index.column()) {
	case 0: return s["id"].toString();
	case 1: return s["alias"].toString();
	case 2: return s["title"].toString();
	case 3: return QString("%1/%2").arg(s["userCount"].toInt()).arg(s["maxUserCount"].toInt());
	case 4: {
		QStringList f;
		if(s["hasPassword"].toBool())
			f << "PASS";
		if(s["closed"].toBool())
			f << "CLOSED";
		if(s["nsfm"].toBool())
			f << "NSFM";
		if(s["persistent"].toBool())
			f << "PERSISTENT";
		return f.join(", ");
	}
	case 5: return s["startTime"].toString();
	case 6: return QString("%1 MB").arg(s["size"].toInt() / double(1024*1024),0, 'f', 2);
	}

	return QVariant();
}

}
}
