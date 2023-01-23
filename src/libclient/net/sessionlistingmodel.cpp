/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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

#include "libclient/net/sessionlistingmodel.h"
#include "libclient/utils/icon.h"
#include "config.h"

#include <QGuiApplication>
#include <QUrl>
#include <QDebug>

using sessionlisting::Session;

// Columns:
// 0 - title
// 1 - server
// 2 - user count
// 3 - owner
// 4 - uptime
static const int COLUMN_COUNT = 5;

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
	return COLUMN_COUNT;
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
	if(s.port != DRAWPILE_PROTO_DEFAULT_PORT)
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
			case 0: return s.title.isEmpty() ? tr("(untitled)") : s.title;
			case 1: return s.host;
			case 2: return s.users < 0 ? QVariant() : QString::number(s.users);
			case 3: return s.owner;
			case 4: return ageString(s.started.msecsTo(QDateTime::currentDateTime()) / 1000);
			}

		} else if(role == Qt::DecorationRole) {
			if(index.column() == 0) {
				if(!s.protocol.isCurrent())
					return icon::fromTheme("dontknow");
				else if(s.password)
					return icon::fromTheme("object-locked");
				else if(s.nsfm)
					return QIcon(":/icons/censored.svg");
			}

		} else if(role == Qt::ToolTipRole) {
			if(!s.protocol.isCurrent()) {
				QString ver;
				if(s.protocol.isFuture())
					ver = tr("New version");
				else
					ver = s.protocol.versionName();
				if(ver.isEmpty())
					ver = tr("Unknown version");

				return tr("Incompatible version (%1)").arg(ver);
			}

		} else if(role == SortKeyRole) {
			// User Role is used for sorting keys
			switch(index.column()) {
			case 0: return s.title;
			case 1: return s.host;
			case 2: return s.users;
			case 3: return s.owner;
			case 4: return s.started;
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
			if(index.column() == 0)
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
	case 0: return tr("Title");
	case 1: return tr("Server");
	case 2: return tr("Users");
	case 3: return tr("Owner");
	case 4: return tr("Age");
	}
	static_assert (COLUMN_COUNT == 5, "Update headerData()");

	return QVariant();
}

Qt::ItemFlags SessionListingModel::flags(const QModelIndex &index) const
{
	if(index.internalId()>0) {
		const Listing &l = m_listings.at(int(index.internalId() - 1));
		if(!l.message.isEmpty())
			return Qt::NoItemFlags;

		const Session &s = l.sessions.at(index.row());
		if(!s.protocol.isCurrent())
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
			if(sessions.size() < oldSize)
				beginRemoveRows(createIndex(i, 0), sessions.size(), oldSize-1);
			else if(sessions.size() > oldSize)
				beginInsertRows(createIndex(i, 0), oldSize, sessions.size()-1);

			m_listings[i].message = QString();
			m_listings[i].sessions = sessions;

			if(sessions.size() < oldSize)
				endRemoveRows();
			else if(sessions.size() > oldSize)
				endInsertRows();

			emit dataChanged(createIndex(i, 0), createIndex(i, 0));
			emit dataChanged(createIndex(0, 0, quintptr(i+1)), createIndex(sessions.size(), COLUMN_COUNT, quintptr(i+1)));
			return;
		}
	}

	beginInsertRows(QModelIndex(), m_listings.size(), m_listings.size());
	m_listings << Listing { listing, QString(), sessions };
	endInsertRows();
}

