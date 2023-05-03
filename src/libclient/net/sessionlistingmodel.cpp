// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/net/sessionlistingmodel.h"
#include "cmake-config/config.h"

#include <QGuiApplication>
#include <QIcon>
#include <QUrl>
#include <QDebug>

using sessionlisting::Session;

SessionListingModel::SessionListingModel(QObject *parent)
	: QAbstractItemModel(parent)
{
}

QModelIndex SessionListingModel::index(int row, int column, const QModelIndex &parent) const
{
	if(row < 0 || column < 0)
		return QModelIndex();

	if(parent.isValid()) {
		if(!parent.internalId()) {
			return createIndex(row, column, quintptr(parent.row()+1));
		}
	} else {
		if(row < m_listings.size())
			return createIndex(row, column);
	}

	return QModelIndex();
}

QModelIndex SessionListingModel::parent(const QModelIndex &child) const
{
	if(child.isValid() && child.internalId()>0) {
		return createIndex(int(child.internalId()-1), child.column());
	}

	return QModelIndex();
}

int SessionListingModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		if(parent.internalId()==0) {
			const Listing &l = m_listings.at(parent.row());
			return l.message.isEmpty() ? l.sessions.size() : 1;
		}

		return 0;
	}

	return m_listings.size();
}

int SessionListingModel::columnCount(const QModelIndex &) const
{
	return ColumnCount;
}

static QString ageString(const qint64 seconds)
{
	const auto minutes = seconds / 60;
	return QGuiApplication::tr("%1h %2m").arg(minutes/60).arg(minutes%60);
}

static QUrl sessionUrl(const Session &s)
{
	QUrl url;
	url.setScheme("drawpile");
	url.setHost(s.host);
	if(s.port != cmake_config::proto::port())
		url.setPort(s.port);
	url.setPath("/" + s.id);
	return url;
}

QVariant SessionListingModel::data(const QModelIndex &index, int role) const
{
	if(index.internalId() > 0) {
		const Listing &listing = m_listings.at(int(index.internalId() - 1));

		if(!listing.message.isEmpty()) {
			if(index.row() == 0 && index.column() == 0 && role == Qt::DisplayRole)
				return listing.message;
			return QVariant();
		}

		if(index.row() < 0 || index.row() >= listing.sessions.size())
			return QVariant();

		const Session &s = listing.sessions.at(index.row());

		if(role == Qt::DisplayRole) {
			switch(index.column()) {
			case ColumnTitle: return s.title.isEmpty() ? tr("(untitled)") : s.title;
			case ColumnServer: return s.host;
			case ColumnUsers: return s.users < 0 ? QVariant() : QString::number(s.users);
			case ColumnOwner: return s.owner;
			case ColumnUptime: return ageString(s.started.msecsTo(QDateTime::currentDateTime()) / 1000);
			}

		} else if(role == Qt::DecorationRole) {
			switch(index.column()) {
			case ColumnVersion:
				if(s.protocol.isCurrent()) {
					return QIcon::fromTheme("dialog-positive");
				} else if(s.protocol.isPastCompatible()) {
					return QIcon::fromTheme("dialog-warning");
				} else {
					return QIcon::fromTheme("dialog-error");
				}
			case ColumnStatus:
				if(!s.protocol.isCompatible()) {
					return QIcon::fromTheme("dontknow");
				} else if(s.password) {
					return QIcon::fromTheme("object-locked");
				} else {
					return QVariant{};
				}
			case ColumnTitle:
				return s.nsfm ? QIcon(":/icons/censored.svg") : QVariant{};
			default:
				return QVariant();
			}

		} else if(role == Qt::ToolTipRole) {
			switch(index.column()) {
			case ColumnVersion:
				if(s.protocol.isCurrent()) {
					return tr("Drawpile 2.2 (fully compatible)");
				} else if(s.protocol.isPastCompatible()) {
					return tr("Drawpile 2.1 (compatibility mode)");
				} else {
					return tr("%1 (incompatible)").arg(s.protocol.versionName());
				}
			case ColumnStatus:
				if(!s.protocol.isCompatible()) {
					return tr("Incompatible version");
				} else if(s.password) {
					return tr("Session password required");
				} else {
					return QVariant{};
				}
			case ColumnTitle:
				return s.nsfm ? tr("%1 (not safe for minors)").arg(s.title) : s.title;
			case ColumnServer:
				return s.host;
			case ColumnUsers:
				return tr("%n user(s)", "", s.users);
			case ColumnOwner:
				return s.owner;
			case ColumnUptime:
				return ageString(s.started.msecsTo(QDateTime::currentDateTime()) / 1000);
			default:
				return QVariant{};
			}

		} else if(role == SortKeyRole) {
			// User Role is used for sorting keys
			switch(index.column()) {
			case ColumnVersion: return s.protocol.asInteger();
			case ColumnStatus: return
				(s.protocol.isCompatible() ? 0x4 : 0x0) |
				(s.password ? 0x2 : 0x0) |
				(s.nsfm ? 0x1 : 0x0);
			case ColumnTitle: return s.title;
			case ColumnServer: return s.host;
			case ColumnUsers: return s.users;
			case ColumnOwner: return s.owner;
			case ColumnUptime: return s.started;
			}

		} else {
			// Direct data access roles
			switch(role) {
			case UrlRole: return sessionUrl(s);
			case IsPasswordedRole: return s.password;
			case IsNsfwRole: return s.nsfm;
			}
		}

	} else {
		if(role == Qt::DisplayRole) {
			if(index.column() == ColumnTitle)
				return m_listings.at(index.row()).name;
		} else if(role == SortKeyRole) {
			return index.row();
		}
	}

	return QVariant();
}

QVariant SessionListingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(role != Qt::DisplayRole || orientation != Qt::Horizontal)
		return QVariant();

	switch(section) {
	case ColumnVersion: return QVariant();
	case ColumnStatus: return QVariant();
	case ColumnTitle: return tr("Title");
	case ColumnServer: return tr("Server");
	case ColumnUsers: return tr("Users");
	case ColumnOwner: return tr("Owner");
	case ColumnUptime: return tr("Age");
	}
	static_assert (ColumnCount == 7, "Update headerData()");

	return QVariant();
}

Qt::ItemFlags SessionListingModel::flags(const QModelIndex &index) const
{
	if(index.internalId()>0) {
		const Listing &l = m_listings.at(int(index.internalId() - 1));
		if(!l.message.isEmpty())
			return Qt::NoItemFlags;

		const Session &s = l.sessions.at(index.row());
		if(!s.protocol.isCompatible())
			return Qt::NoItemFlags;
	}
	return QAbstractItemModel::flags(index);
}

QModelIndex SessionListingModel::indexOfListing(const QString &listing) const
{
	for(int i=0;i<m_listings.size();++i) {
		if(m_listings.at(i).name == listing) {
			qInfo("indexOf %s -> %d", qPrintable(listing), i);
			return createIndex(i, 0);
		}
	}

	return QModelIndex();
}

void SessionListingModel::setMessage(const QString &listing, const QString &message)
{
	for(int i=0;i<m_listings.size();++i) {
		if(m_listings.at(i).name == listing) {
			const int oldSize = m_listings[i].message.isEmpty() ? m_listings[i].sessions.size() : 1;
			if(oldSize > 1)
				beginRemoveRows(createIndex(i, 0), 1, oldSize-1);

			m_listings[i].message = message;
			m_listings[i].sessions.clear();

			if(oldSize > 1)
				endRemoveRows();

			emit dataChanged(createIndex(i, 0), createIndex(i, 0));
			const QModelIndex mi = createIndex(0, 0, quintptr(i+1));
			emit dataChanged(mi, mi);
			return;
		}
	}

	beginInsertRows(QModelIndex(), m_listings.size(), m_listings.size());
	m_listings << Listing { listing, message, QVector<sessionlisting::Session>() };
	endInsertRows();
}

void SessionListingModel::setList(const QString &listing, const QVector<Session> sessions)
{
	for(int i=0;i<m_listings.size();++i) {
		if(m_listings.at(i).name == listing) {
			const int oldSize = m_listings[i].message.isEmpty() ? m_listings[i].sessions.size() : 1;
			if(sessions.size() < oldSize) {
				beginRemoveRows(createIndex(i, 0), sessions.size(), oldSize-1);
			} else if(sessions.size() > oldSize) {
				beginInsertRows(createIndex(i, 0), oldSize, sessions.size()-1);
			}

			m_listings[i].message = QString();
			m_listings[i].sessions = sessions;

			if(sessions.size() < oldSize) {
				endRemoveRows();
			} else if(sessions.size() > oldSize) {
				endInsertRows();
			}

			emit dataChanged(createIndex(i, 0), createIndex(i, 0));
			emit dataChanged(createIndex(0, 0, quintptr(i+1)), createIndex(sessions.size(), ColumnCount, quintptr(i+1)));
			return;
		}
	}

	beginInsertRows(QModelIndex(), m_listings.size(), m_listings.size());
	m_listings << Listing { listing, QString(), sessions };
	endInsertRows();
}

