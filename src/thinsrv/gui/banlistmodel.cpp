// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/banlistmodel.h"

#include <QJsonObject>

namespace server {
namespace gui {

BanListModel::BanListModel(QObject *parent)
	: JsonListModel({
		{"address", tr("Address")},
		{"expires", tr("Expires")},
		{"added", tr("Added")},
		{"comment", tr("Comment")},
	}, parent)
{
}

void BanListModel::addBanEntry(const QJsonObject &entry)
{
	beginInsertRows(QModelIndex(), m_list.size(), m_list.size());
	m_list << entry;
	endInsertRows();
}

void BanListModel::removeBanEntry(int id)
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

QVariant BanListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Vertical) {
		if(role == Qt::DisplayRole && section>=0 && section < m_list.size())
			return m_list[section].toObject()["id"].toInt();
		return QVariant();
	}
	return JsonListModel::headerData(section, orientation, role);
}

QVariant BanListModel::getData(const QString &key, const QJsonObject &o) const
{
	if(key == "address") {
		if(o["subnet"].toInt()>0)
			return QString("%1/%2").arg(o["ip"].toString()).arg(o["subnet"].toInt());
		else
			return o["ip"].toString();
	} else
		return o.value(key).toVariant();
}

}
}
