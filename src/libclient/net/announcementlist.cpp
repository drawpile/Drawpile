// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/announcementlist.h"
#include "libclient/utils/listservermodel.h"

namespace net {

AnnouncementListModel::AnnouncementListModel(QObject *parent)
	: AnnouncementListModel({}, parent)
{
}

AnnouncementListModel::AnnouncementListModel(
	const QVector<QVariantMap> &data, QObject *parent)
	: QAbstractTableModel(parent)
{
	loadKnownServers(data);
}

QVariant AnnouncementListModel::data(const QModelIndex &index, int role) const
{
	using KnownServersConstIterator =
		QHash<QString, QPair<QIcon, QString>>::const_iterator;
	if(index.isValid() && index.column() == 0 && index.row() >= 0 &&
	   index.row() < m_announcements.size()) {
		if(role == Qt::DisplayRole) {
			const QString &url = m_announcements.at(index.row());
			KnownServersConstIterator it = m_knownServers.find(url);
			return it == m_knownServers.constEnd() ? url : it.value().second;
		} else if(role == Qt::DecorationRole) {
			const QString &url = m_announcements.at(index.row());
			KnownServersConstIterator it = m_knownServers.find(url);
			if(it != m_knownServers.constEnd()) {
				return m_knownServers[url].first;
			}
		} else if(role == Qt::UserRole) {
			return m_announcements.at(index.row());
		}
	}
	return QVariant();
}

int AnnouncementListModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_announcements.size();
}

int AnnouncementListModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : 1;
}

void AnnouncementListModel::setAnnouncements(const QStringList &urls)
{
	beginResetModel();
	m_announcements.clear();
	m_announcements.reserve(urls.size());
	for(const QString &url : urls) {
		if(!m_announcements.contains(url)) {
			m_announcements.append(url);
		}
	}
	endResetModel();
}

void AnnouncementListModel::addAnnouncement(const QString &url)
{
	// Check if the announcement is listed already
	for(int i = 0; i < m_announcements.count(); ++i) {
		if(m_announcements.at(i) == url) {
			m_announcements[i] = url;
			emit dataChanged(index(i, 0), index(i, columnCount()));
			return;
		}
	}

	// Append to the list
	const int pos = m_announcements.size();
	beginInsertRows(QModelIndex(), pos, pos);
	m_announcements.append(url);
	endInsertRows();
}

void AnnouncementListModel::removeAnnouncement(const QString &url)
{
	for(int pos = 0; pos < m_announcements.count(); ++pos) {
		if(m_announcements.at(pos) == url) {
			beginRemoveRows(QModelIndex(), pos, pos);
			m_announcements.removeAt(pos);
			endRemoveRows();
			break;
		}
	}
}

void AnnouncementListModel::clear()
{
	int size = m_announcements.size();
	if(size != 0) {
		beginRemoveRows(QModelIndex(), 0, size - 1);
		m_announcements.clear();
		endRemoveRows();
	}
}

void AnnouncementListModel::setKnownServers(const QVector<QVariantMap> &data)
{
	loadKnownServers(data);
	int count = m_announcements.size();
	if(count != 0) {
		emit dataChanged(createIndex(0, 0), createIndex(count - 1, 0));
	}
}

void AnnouncementListModel::loadKnownServers(const QVector<QVariantMap> &data)
{
	// Get list of known servers (url -> {icon, name})
	QVector<sessionlisting::ListServer> servers =
		sessionlisting::ListServerModel::listServers(data, true);
	m_knownServers.clear();
	for(const sessionlisting::ListServer &s : servers) {
		m_knownServers[s.url] = QPair<QIcon, QString>(s.icon, s.name);
	}
}

}
