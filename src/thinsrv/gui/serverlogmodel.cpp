// SPDX-License-Identifier: GPL-3.0-or-later

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

