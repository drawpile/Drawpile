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

#include "libclient/net/banlistmodel.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

namespace net {

BanlistModel::BanlistModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

int BanlistModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_banlist.size();
}

int BanlistModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	// Columns:
	// 0 - user
	// 1 - IP
	// 2 - banned by
	return 3;
}

QVariant BanlistModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row()>=0 && index.row() < m_banlist.size()) {
		if(role == Qt::DisplayRole) {
			const BanlistEntry &e = m_banlist.at(index.row());

			switch(index.column()) {
			case 0: return e.username;
			case 1: return e.ip;
			case 2: return e.bannedBy;
			}
		} else if(role == Qt::UserRole) {
			return m_banlist.at(index.row()).id;
		}
	}

	return QVariant();
}

QVariant BanlistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(role != Qt::DisplayRole || orientation != Qt::Horizontal)
		return QVariant();

	switch(section) {
	case 0: return tr("User");
	case 1: return tr("IP address");
	case 2: return tr("Banned by");
	}

	return QVariant();
}

void BanlistModel::clear()
{
	beginResetModel();
	m_banlist.clear();
	endResetModel();
}

void BanlistModel::updateBans(const QJsonArray &banlist)
{
	beginResetModel();
	m_banlist.clear();
	for(const QJsonValue &v : banlist) {
		QJsonObject b = v.toObject();
		m_banlist << BanlistEntry {
			b["id"].toInt(),
			b["username"].toString(),
			b["ip"].isString() ? b["ip"].toString() : QStringLiteral("***"),
			b["bannedBy"].toString()
		};
	}
	endResetModel();
}

}
