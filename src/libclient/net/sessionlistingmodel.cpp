// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/net/sessionlistingmodel.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "cmake-config/config.h"

#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QSize>
#include <QUrl>

using sessionlisting::Session;

SessionListingModel::SessionListingModel(QObject *parent)
	: QAbstractItemModel(parent)
{
}

QModelIndex SessionListingModel::index(int row, int column, const QModelIndex &parent) const
{
	if(row < 0 || column < 0 || column >= ColumnCount) {
		return QModelIndex();
	}

	if(parent.isValid() && isRootItem(parent)) {
		return createIndex(row, column, quintptr(parent.row() + 1));
	} else if (!parent.isValid() && row < m_listings.size()) {
		return createIndex(row, column, quintptr(0));
	}

	return QModelIndex();
}

QModelIndex SessionListingModel::parent(const QModelIndex &child) const
{
	if (!child.isValid() || isRootItem(child))
		return QModelIndex();

	return createIndex(int(child.internalId() - 1), child.column());
}

int SessionListingModel::rowCount(const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		return m_listings.size();
	}

	if (!isRootItem(parent) || parent.row() >= m_listings.size()) {
		return 0;
	}

	const auto &listing = m_listings.at(parent.row());
	return listing.offline() ? 1 : listing.sessions.size();
}

int SessionListingModel::columnCount(const QModelIndex &) const
{
	return ColumnCount;
}

static QString ageString(const qint64 seconds)
{
	const auto minutes = seconds / 60;
	if (minutes >= 60) {
		const auto hours = minutes / 60;
		if (hours >= 24) {
			const auto days = hours / 24;
			return SessionListingModel::tr("%1d%2h%3m")
				.arg(days)
				.arg(hours % 24)
				.arg(minutes % 60);
		} else {
			return SessionListingModel::tr("%1h%2m")
				.arg(hours)
				.arg(minutes % 60);
		}
	} else {
		return SessionListingModel::tr("%1m")
			.arg(minutes);
	}
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
	if (!index.isValid())
		return QVariant();

	const auto &listing = m_listings.at(listingIndex(index));

	if (isRootItem(index)) {
		if (index.column() != Message) {
			return QVariant();
		}

		switch (role) {
		case Qt::DisplayRole:
		case SortKeyRole:
			return listing.name;
		case Qt::DecorationRole:
			return m_icons[listing.name];
		case Qt::FontRole: {
			QFont font;
			font.setBold(true);
			return font;
		}
		case IsListingRole:
			return false;
		default:
			break;
		}
	} else if (listing.offline()) {
		if (index.row() != 0) {
			return QVariant();
		}

		switch(role) {
		case Qt::DisplayRole:
			if (index.column() == Message)
				return listing.message;
			break;
		case Qt::DecorationRole:
			if (index.column() == Message)
				return QIcon::fromTheme("dialog-information");
			break;
		case Qt::FontRole:
			if (index.column() == Message) {
				QFont font;
				font.setStyle(QFont::StyleItalic);
				return font;
			}
			break;
		case Qt::ForegroundRole:
			return QPalette().color(QPalette::Text);
		case IsListingRole:
			return false;
		default:
			break;
		}
	} else {
		if(index.row() >= listing.sessions.size())
			return QVariant();

		const auto &session = listing.sessions.at(index.row());

		switch (role) {
		case Qt::DisplayRole:
		case Qt::ToolTipRole:
			switch(Column(index.column())) {
			case Title:
				return session.title.isEmpty() ? tr("(untitled)") : session.title;
			case Server:
				return session.host;
			case UserCount:
				if (session.users < 0) {
					return QVariant();
			 	} else if (role == Qt::ToolTipRole) {
					return tr("%n users", nullptr, session.users);
				} else {
					return QString::number(session.users);
				}
			case Owner:
				return session.owner;
			case Uptime:
				return ageString(session.started.secsTo(QDateTime::currentDateTime()));
			case Version:
				if (role == Qt::ToolTipRole) {
					const auto version = session.protocol.versionName();
					if (session.protocol.isCurrent()) {
						return tr("Compatible");
					} else if (session.protocol.isPastCompatible()) {
						return tr("Requires compatibility mode (%1)").arg(version);
					} else if (session.protocol.isFuture()) {
						return tr("Requires newer client (%1)").arg(version);
					} else {
						return tr("Incompatible (%1)")
							.arg(version.isEmpty() ? tr("unknown version") : version);
					}
				}
				break;
			case ColumnCount: break;
			}
			break;
		case Qt::DecorationRole:
			switch (Column(index.column())) {
			case Version:
				if(session.protocol.isCurrent()) {
					return QIcon::fromTheme("state-ok");
				} else if(session.protocol.isPastCompatible()) {
					return QIcon::fromTheme("state-warning");
				} else {
					return QIcon::fromTheme("state-error");
				}
			case Title:
				if(session.password) {
					return QIcon::fromTheme("object-locked");
				} else if(isNsfm(session)) {
					return QIcon(":/icons/censored.svg");
				} else {
					QPixmap pm(64, 64);
					pm.fill(Qt::transparent);
					return QIcon(pm);
				}
			default:
				return QVariant();
			}
		case SortKeyRole:
			switch(Column(index.column())) {
			case Title: return session.title;
			case Server: return session.host;
			case UserCount: return session.users;
			case Owner: return session.owner;
			case Uptime: return session.started;
			case Version: return session.protocol.asInteger();
			case ColumnCount: break;
			}
			break;
		case UrlRole: return sessionUrl(session);
		case UrlStringRole: return sessionUrl(session).toString();
		case IsPasswordedRole: return session.password;
		case IsNsfwRole: return isNsfm(session);
		case IsListingRole: return true;
		case TitleRole: return session.title;
		case ServerRole: return session.host;
		case OwnerRole: return session.owner;
		default: break;
		}
	}

	return QVariant();
}

QVariant SessionListingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation != Qt::Horizontal)
		return QVariant();

	if (role == Qt::AccessibleTextRole && section == Version) {
		return tr("Compatibility");
	} else if (role != Qt::DisplayRole) {
		return QVariant();
	}

	switch(Column(section)) {
	case Title: return tr("Title");
	case Server: return tr("Server");
	case UserCount: return tr("Users");
	case Owner: return tr("Owner");
	case Uptime: return tr("Age");
	case Version:
	case ColumnCount: {}
	}
	return QVariant();
}

Qt::ItemFlags SessionListingModel::flags(const QModelIndex &index) const
{
	if (isRootItem(index)) {
		return Qt::ItemIsEnabled;
	}

	const auto &listing = m_listings.at(listingIndex(index));
	if(listing.offline())
		return Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;

	const auto &session = listing.sessions.at(index.row());
	if(!session.protocol.isCompatible())
		return Qt::ItemNeverHasChildren;

	return QAbstractItemModel::flags(index) | Qt::ItemNeverHasChildren;
}

QSize SessionListingModel::span(const QModelIndex &index) const
{
	if (!index.isValid()) {
		return QSize(1, 1);
	}

	if (isRootItem(index)) {
		return QSize(ColumnCount - Message, 1);
	}

	const auto li = listingIndex(index);
	if (index.row() == 0 && li < m_listings.size() && m_listings.at(li).offline()) {
		return QSize(ColumnCount - Message, 1);
	}

	return QSize(1, 1);
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

void SessionListingModel::setIcon(const QString &name, const QIcon &icon)
{
	m_icons[name] = icon;
}

void SessionListingModel::setMessage(const QString &name, const QString &message)
{
	for(int i = 0; i < m_listings.size(); ++i) {
		auto &listing = m_listings[i];
		if (listing.name == name) {
			const auto oldSize = listing.offline() ? 1 : listing.sessions.size();

			if (oldSize > 1)
				beginRemoveRows(createIndex(i, 0), 1, oldSize - 1);

			listing.message = message;
			listing.sessions.clear();

			if(oldSize > 1)
				endRemoveRows();

			emit dataChanged(createIndex(i, 0), createIndex(i, ColumnCount - 1));
			const auto mi = createIndex(0, Message, quintptr(i + 1));
			emit dataChanged(mi, mi);
			return;
		}
	}

	beginInsertRows(QModelIndex(), m_listings.size(), m_listings.size());
	m_listings << Listing { name, message, {} };
	endInsertRows();
}

void SessionListingModel::setList(const QString &name, const QVector<Session> sessions)
{
	for (auto i = 0; i < m_listings.size(); ++i) {
		auto &listing = m_listings[i];
		if (listing.name == name) {
			const auto oldSize = listing.offline() ? 1 : listing.sessions.size();
			if (sessions.size() < oldSize)
				beginRemoveRows(createIndex(i, 0), sessions.size(), oldSize - 1);
			else if (sessions.size() > oldSize)
				beginInsertRows(createIndex(i, 0), oldSize, sessions.size() - 1);

			listing.message = QString();
			listing.sessions = sessions;

			if (sessions.size() < oldSize)
				endRemoveRows();
			else if (sessions.size() > oldSize)
				endInsertRows();

			emit dataChanged(createIndex(i, 0), createIndex(i, 0));
			emit dataChanged(
				createIndex(0, 0, quintptr(i + 1)),
				createIndex(sessions.size() - 1, ColumnCount - 1, quintptr(i + 1))
			);
			return;
		}
	}

	beginInsertRows(QModelIndex(), m_listings.size(), m_listings.size());
	m_listings << Listing { name, QString(), sessions };
	endInsertRows();
}

void SessionListingModel::clear()
{
	beginResetModel();
	m_listings.clear();
	endResetModel();
}

bool SessionListingModel::isNsfm(const Session &session) const
{
	return session.nsfm || parentalcontrols::isNsfmTitle(session.title);
}
