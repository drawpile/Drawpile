// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/sessionlistingmodel.h"
#include "libclient/net/loginsessions.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libshared/util/qtcompat.h"
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSize>
#include <QUrl>
#ifdef HAVE_TCPSOCKETS
#	include "cmake-config/config.h"
#else
#	include "libshared/util/whatismyip.h"
#endif

using sessionlisting::Session;

SessionListingModel::SessionListingModel(QObject *parent)
	: QAbstractItemModel(parent)
{
}

QModelIndex
SessionListingModel::index(int row, int column, const QModelIndex &parent) const
{
	if(row < 0 || column < 0 || column >= ColumnCount) {
		return QModelIndex();
	}

	if(parent.isValid() && isRootItem(parent)) {
		return createIndex(row, column, quintptr(parent.row() + 1));
	} else if(!parent.isValid() && row < m_listings.size()) {
		return createIndex(row, column, quintptr(0));
	}

	return QModelIndex();
}

QModelIndex SessionListingModel::parent(const QModelIndex &child) const
{
	if(!child.isValid() || isRootItem(child)) {
		return QModelIndex();
	}
	return createIndex(int(child.internalId() - 1), child.column());
}

int SessionListingModel::rowCount(const QModelIndex &parent) const
{
	if(!parent.isValid()) {
		return m_listings.size();
	}

	if(!isRootItem(parent) || parent.row() >= m_listings.size()) {
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
	if(minutes >= 60) {
		const auto hours = minutes / 60;
		if(hours >= 24) {
			const auto days = hours / 24;
			return SessionListingModel::tr("%1d%2h%3m")
				.arg(days)
				.arg(hours % 24)
				.arg(minutes % 60);
		} else {
			return SessionListingModel::tr("%1h%2m").arg(hours).arg(
				minutes % 60);
		}
	} else {
		return SessionListingModel::tr("%1m").arg(minutes);
	}
}

static QUrl sessionUrl(const Session &s)
{
	QString id = QUrl::toPercentEncoding(s.id);
	QUrl url;
	url.setHost(s.host);
#ifdef HAVE_TCPSOCKETS
	url.setScheme(QStringLiteral("drawpile"));
	if(s.port != cmake_config::proto::port()) {
		url.setPort(s.port);
	}
	url.setPath(QStringLiteral("/") + id);
#else
	url.setScheme(
		WhatIsMyIp::looksLikeLocalhost(s.host) ? QStringLiteral("ws")
											   : QStringLiteral("wss"));
	url.setPath(QStringLiteral("/drawpile-web/ws"));
	url.setQuery(QStringLiteral("session=") + id);
#endif
	return url;
}

QVariant SessionListingModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid())
		return QVariant();

	int i = listingIndex(index);
	const auto &listing = m_listings.at(i);

	if(isRootItem(index)) {
		if(index.column() != Message) {
			return QVariant();
		}

		switch(role) {
		case Qt::DisplayRole:
			return squashWhitespace(listing.name);
		case Qt::ToolTipRole:
			return listing.name;
		case SortKeyRole:
			return i;
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
	} else if(listing.offline()) {
		if(index.row() != 0) {
			return QVariant();
		}

		switch(role) {
		case Qt::DisplayRole:
		case Qt::ToolTipRole:
			if(index.column() == Message)
				return listing.message;
			break;
		case Qt::DecorationRole:
			if(index.column() == Message)
				return QIcon::fromTheme("dialog-information");
			break;
		case Qt::FontRole:
			if(index.column() == Message) {
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

		switch(role) {
		case Qt::DisplayRole:
		case Qt::ToolTipRole:
			switch(Column(index.column())) {
			case Title:
				return formatTitle(session, role == Qt::ToolTipRole);
			case Server:
				return role == Qt::DisplayRole ? squashWhitespace(session.host)
											   : session.host;
			case UserCount:
				if(session.users < 0) {
					return QVariant();
				} else if(role == Qt::ToolTipRole) {
					if(session.maxUsers > 0) {
						return tr("%1/%n users", nullptr, session.maxUsers)
							.arg(session.users);
					} else {
						return tr("%n users", nullptr, session.users);
					}
				} else {
					return QString::number(session.users);
				}
			case ActiveCount: {
				int n = session.activeDrawingUsers;
				if(role == Qt::ToolTipRole) {
					if(n < 0) {
						return tr("Unknown number of actively drawing users");
					} else {
						return tr("%n actively drawing user(s)", nullptr, n);
					}
				} else {
					if(n < 0) {
						return QVariant();
					} else {
						return QString::number(n);
					}
				}
			}
			case Owner:
				return role == Qt::DisplayRole ? squashWhitespace(session.owner)
											   : session.owner;
			case Uptime:
				return ageString(
					session.started.secsTo(QDateTime::currentDateTime()));
			case Version:
				if(role == Qt::ToolTipRole) {
					const auto version = session.protocol.versionName();
					if(session.protocol.isCurrent()) {
						return tr("Compatible");
					} else if(session.protocol.isPastCompatible()) {
						return tr("Requires compatibility mode (%1)")
							.arg(version);
					} else if(session.protocol.isFuture()) {
						return tr("Requires newer client (%1)").arg(version);
					} else {
						return tr("Incompatible (%1)")
							.arg(
								version.isEmpty() ? tr("unknown version")
												  : version);
					}
				}
				break;
			case ColumnCount:
				break;
			}
			break;
		case Qt::DecorationRole:
			switch(Column(index.column())) {
			case Version:
				if(session.protocol.isCurrent()) {
					return QIcon::fromTheme("state-ok");
				} else if(session.protocol.isPastCompatible()) {
					return QIcon::fromTheme("state-warning");
				} else {
					return QIcon::fromTheme("state-error");
				}
			case Title:
				if(!session.protocol.isCompatible()) {
					return QIcon::fromTheme("dontknow");
				} else if(isClosed(session)) {
					return QIcon::fromTheme("cards-block");
				} else if(session.password) {
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
			case Title:
				return session.title;
			case Server:
				return session.host;
			case UserCount:
				return session.users;
			case ActiveCount:
				return session.activeDrawingUsers;
			case Owner:
				return session.owner;
			case Uptime:
				return session.started;
			case Version:
				return session.protocol.asInteger();
			case ColumnCount:
				break;
			}
			break;
		case UrlRole:
			return sessionUrl(session);
		case UrlStringRole:
			return sessionUrl(session).toString();
		case IsPasswordedRole:
			return session.password;
		case IsNsfwRole:
			return isNsfm(session);
		case IsClosedRole:
			return isClosed(session);
		case IsListingRole:
			return true;
		case TitleRole:
			return session.title;
		case ServerRole:
			return session.host;
		case OwnerRole:
			return session.owner;
		case IsInactiveRole:
			return session.activeDrawingUsers == 0;
		case JoinableRole:
#ifndef HAVE_TCPSOCKETS
			if(!session.allowWeb) {
				return false;
			}
#endif
			return session.protocol.isCompatible();
		case JoinDenyReasonsRole: {
#ifdef HAVE_TCPSOCKETS
			bool webLoginBlocked = false;
#else
			bool webLoginBlocked = !session.allowWeb;
#endif
			return net::LoginSessionModel::getJoinDenyReasons(
				session.closed, false, webLoginBlocked,
				!session.protocol.isCompatible(), session.protocol.isFuture(),
				session.protocol.isPast());
		}
		case JoinDenyIcon:
			return net::LoginSessionModel::getJoinDenyIcon(
				!session.protocol.isCompatible());
		default:
			break;
		}
	}

	return QVariant();
}

QVariant SessionListingModel::headerData(
	int section, Qt::Orientation orientation, int role) const
{
	if(orientation != Qt::Horizontal) {
		return QVariant();
	}

	if(role == Qt::AccessibleTextRole && section == Version) {
		return tr("Compatibility");
	} else if(role != Qt::DisplayRole) {
		return QVariant();
	}

	switch(Column(section)) {
	case Title:
		return tr("Title");
	case Server:
		return tr("Server");
	case UserCount:
		return tr("Users");
	case ActiveCount:
		return tr("Active");
	case Owner:
		return tr("Owner");
	case Uptime:
		return tr("Age");
	case Version:
	case ColumnCount:
		break;
	}
	return QVariant();
}

Qt::ItemFlags SessionListingModel::flags(const QModelIndex &index) const
{
	if(isRootItem(index)) {
		return Qt::ItemIsEnabled;
	}

	const Listing &listing = m_listings.at(listingIndex(index));
	if(listing.offline()) {
		return Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
	}

	return QAbstractItemModel::flags(index) | Qt::ItemNeverHasChildren;
}

QSize SessionListingModel::span(const QModelIndex &index) const
{
	if(!index.isValid()) {
		return QSize(1, 1);
	}

	if(isRootItem(index)) {
		return QSize(ColumnCount - Message, 1);
	}

	const auto li = listingIndex(index);
	if(index.row() == 0 && li < m_listings.size() &&
	   m_listings.at(li).offline()) {
		return QSize(ColumnCount - Message, 1);
	}

	return QSize(1, 1);
}

QModelIndex SessionListingModel::primaryIndexOfUrl(const QUrl &url) const
{
	QVector<QModelIndex> indexes;
	int listingCount = m_listings.size();
	for(int listingIndex = 0; listingIndex < listingCount; ++listingIndex) {
		const Listing &listing = m_listings[listingIndex];
		int sessionCount = listing.sessions.count();
		for(int sessionIndex = 0; sessionIndex < sessionCount; ++sessionIndex) {
			const Session &session = listing.sessions[sessionIndex];
			if(sessionUrl(session) == url) {
				QModelIndex i = index(sessionIndex, 0, index(listingIndex, 0));
				bool hostedHere = session.host.compare(
									  listing.host, Qt::CaseInsensitive) == 0;
				if(hostedHere) {
					indexes.prepend(i);
				} else {
					indexes.append(i);
				}
			}
		}
	}
	return indexes.isEmpty() ? QModelIndex{} : indexes.first();
}

void SessionListingModel::setIcon(const QString &name, const QIcon &icon)
{
	m_icons[name] = icon;
}

void SessionListingModel::setMessage(
	const QString &name, const QString &host, const QString &message)
{
	for(int i = 0; i < m_listings.size(); ++i) {
		auto &listing = m_listings[i];
		if(listing.name == name) {
			const auto oldSize =
				listing.offline() ? 1 : listing.sessions.size();

			if(oldSize > 1)
				beginRemoveRows(createIndex(i, 0), 1, oldSize - 1);

			listing.host = host;
			listing.message = message;
			listing.sessions.clear();

			if(oldSize > 1)
				endRemoveRows();

			emit dataChanged(
				createIndex(i, 0), createIndex(i, ColumnCount - 1));
			const auto mi = createIndex(0, Message, quintptr(i + 1));
			emit dataChanged(mi, mi);
			return;
		}
	}

	beginInsertRows(QModelIndex(), m_listings.size(), m_listings.size());
	m_listings << Listing{name, host, message, {}};
	endInsertRows();
}

void SessionListingModel::setList(
	const QString &name, const QString &host, const QVector<Session> sessions)
{
	for(auto i = 0; i < m_listings.size(); ++i) {
		auto &listing = m_listings[i];
		if(listing.name == name) {
			const auto oldSize =
				listing.offline() ? 1 : listing.sessions.size();
			if(sessions.size() < oldSize) {
				beginRemoveRows(
					createIndex(i, 0), sessions.size(), oldSize - 1);
			} else if(sessions.size() > oldSize) {
				beginInsertRows(
					createIndex(i, 0), oldSize, sessions.size() - 1);
			}

			listing.host = host;
			listing.message = QString();
			listing.sessions = sessions;

			if(sessions.size() < oldSize) {
				endRemoveRows();
			} else if(sessions.size() > oldSize) {
				endInsertRows();
			}

			emit dataChanged(createIndex(i, 0), createIndex(i, 0));

			compat::sizetype changedSize = qMin(oldSize, sessions.size());
			if(changedSize > 0) {
				emit dataChanged(
					createIndex(0, 0, quintptr(i + 1)),
					createIndex(
						changedSize - 1, ColumnCount - 1, quintptr(i + 1)));
			}
			return;
		}
	}

	beginInsertRows(QModelIndex(), m_listings.size(), m_listings.size());
	m_listings << Listing{name, host, QString(), sessions};
	endInsertRows();
}

void SessionListingModel::clear()
{
	beginResetModel();
	m_listings.clear();
	endResetModel();
}

QString SessionListingModel::formatTitle(
	const Session &session, bool includeFlags) const
{
	QString title;
	if(session.title.isEmpty()) {
		title = tr("(untitled)");
	} else if(includeFlags) {
		title = session.title;
	} else {
		static QRegularExpression re(QStringLiteral("\\s+"));
		title = squashWhitespace(session.title);
	}

	if(includeFlags) {
		QStringList flags;
		if(!session.protocol.isCompatible()) {
			flags.append(tr("incompatible"));
		}
#ifndef HAVE_TCPSOCKETS
		if(!session.allowWeb) {
			flags.append(tr("joining from web not allowed"));
		}
#endif
		if(isClosed(session)) {
			flags.append(tr("closed"));
		}
		if(session.password) {
			flags.append(tr("password-protected"));
		}
		if(isNsfm(session)) {
			flags.append(tr("NSFM"));
		}
		if(!flags.isEmpty()) {
			return QStringLiteral("%1 (%2)").arg(
				title, flags.join(QStringLiteral(", ")));
		}
	}
	return title;
}

QString SessionListingModel::squashWhitespace(const QString &s)
{
	static QRegularExpression re(QStringLiteral("\\s+"));
	return s.trimmed().replace(re, QStringLiteral(" "));
}

bool SessionListingModel::isNsfm(const Session &session) const
{
	return session.nsfm || parentalcontrols::isNsfmTitle(session.title);
}

bool SessionListingModel::isClosed(const sessionlisting::Session &session) const
{
#ifndef HAVE_TCPSOCKETS
	if(!session.allowWeb) {
		return true;
	}
#endif
	return session.closed ||
		   (session.maxUsers > 0 && session.users >= session.maxUsers);
}
