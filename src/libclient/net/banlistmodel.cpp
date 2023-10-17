// SPDX-License-Identifier: GPL-3.0-or-later

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
	return parent.isValid() ? 0 : m_banlist.size();
}

int BanlistModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_showSensitive ? 5 : 3;
}

QVariant BanlistModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_banlist.size()) {
		if(role == Qt::DisplayRole || role == Qt::ToolTipRole) {
			const BanlistEntry &e = m_banlist.at(index.row());
			if(m_showSensitive) {
				switch(index.column()) {
				case 0:
					return e.id;
				case 1:
					return e.username;
				case 2:
					return e.ip;
				case 3:
					return QStringLiteral("%1@%2").arg(e.authId, e.sid);
				case 4:
					return e.bannedBy;
				}
			} else {
				switch(index.column()) {
				case 0:
					return e.id;
				case 1:
					return e.username;
				case 2:
					return e.bannedBy;
				}
			}
		} else if(role == Qt::UserRole) {
			return m_banlist.at(index.row()).id;
		}
	}

	return QVariant();
}

QVariant BanlistModel::headerData(
	int section, Qt::Orientation orientation, int role) const
{
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		if(m_showSensitive) {
			switch(section) {
			case 0:
				return tr("ID");
			case 1:
				return tr("User");
			case 2:
				return tr("IP address");
			case 3:
				return tr("Client info");
			case 4:
				return tr("Banned by");
			default:
				break;
			}
		} else {
			switch(section) {
			case 0:
				return tr("ID");
			case 1:
				return tr("User");
			case 2:
				return tr("Banned by");
			default:
				break;
			}
		}
	}
	return QVariant();
}

void BanlistModel::setShowSensitive(bool showSensitive)
{
	if(showSensitive != m_showSensitive) {
		beginResetModel();
		m_showSensitive = showSensitive;
		endResetModel();
	}
}

void BanlistModel::clear()
{
	beginResetModel();
	m_banlist.clear();
	m_showSensitive = false;
	endResetModel();
}

void BanlistModel::updateBans(const QJsonArray &banlist)
{
	beginResetModel();
	m_banlist.clear();
	for(const QJsonValue &v : banlist) {
		QJsonObject b = v.toObject();
		m_banlist.append({
			b["id"].toInt(),
			b["username"].toString(),
			b["ip"].toString(),
			b["extauthid"].toString(),
			b["s"].toString(),
			b["bannedBy"].toString(),
		});
	}
	endResetModel();
}

}
