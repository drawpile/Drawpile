// SPDX-License-Identifier: GPL-3.0-or-later

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
