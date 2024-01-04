// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/net/loginsessions.h"
#include "libclient/parentalcontrols/parentalcontrols.h"

#include <QDebug>
#include <QIcon>
#include <QPixmap>

namespace net {

LoginSessionModel::LoginSessionModel(QObject *parent)
	: QAbstractTableModel(parent)
	, m_moderatorMode(false)
{
}

void LoginSessionModel::setModeratorMode(bool mod)
{
	if(mod != m_moderatorMode) {
		beginResetModel();
		m_moderatorMode = mod;
		endResetModel();
	}
}

int LoginSessionModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_sessions.size();
}

int LoginSessionModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : ColumnCount;
}

QVariant LoginSessionModel::data(const QModelIndex &index, int role) const
{
	if(index.row() < 0 || index.row() >= m_sessions.size()) {
		return QVariant{};
	}

	const LoginSession &ls = m_sessions.at(index.row());

	if(role == Qt::DisplayRole) {
		switch(index.column()) {
		case ColumnTitle: {
			QString title = ls.title.isEmpty() ? tr("(untitled)") : ls.title;
			if(ls.alias.isEmpty()) {
				return title;
			} else {
				return QStringLiteral("%1 [%2]").arg(title).arg(ls.alias);
			}
		}
		case ColumnFounder:
			return ls.founder;
		case ColumnUsers:
			return ls.userCount;
		case ColumnActive: {
			int n = ls.activeDrawingUserCount;
			if(n < 0) {
				return QVariant();
			} else {
				return n;
			}
		}
		default:
			return QVariant{};
		}

	} else if(role == Qt::DecorationRole) {
		switch(index.column()) {
		case ColumnVersion:
			if(ls.isIncompatible()) {
				return QIcon::fromTheme("state-error");
			} else if(ls.compatibilityMode) {
				return QIcon::fromTheme("state-warning");
			} else {
				return QIcon::fromTheme("state-ok");
			}
		case ColumnStatus:
			if(ls.isIncompatible()) {
				return QIcon::fromTheme("dontknow");
			} else if(ls.isClosed()) {
				return QIcon::fromTheme("cards-block");
			} else if(ls.needPassword) {
				return QIcon::fromTheme("object-locked");
			} else {
				return QVariant{};
			}
		case ColumnTitle:
			return isNsfm(ls) ? QIcon(":/icons/censored.svg") : QVariant{};
		default:
			return QVariant{};
		}

	} else if(role == Qt::ToolTipRole) {
		switch(index.column()) {
		case ColumnVersion:
			if(ls.isIncompatible()) {
				return tr("%1 (incompatible)").arg(ls.incompatibleSeries);
			} else if(ls.compatibilityMode) {
				return tr("Drawpile 2.1 (compatibility mode)");
			} else {
				return tr("Drawpile 2.2 (fully compatible)");
			}
		case ColumnStatus:
			if(ls.isIncompatible()) {
				return tr("Incompatible version");
			} else if(ls.guestLoginBlocked) {
				return tr("Closed (guest logins blocked)");
			} else if(ls.newLoginsBlocked) {
				return tr("Closed (new logins blocked)");
			} else if(ls.needPassword) {
				return tr("Session password required");
			} else {
				return QVariant{};
			}
		case ColumnTitle:
			return isNsfm(ls) ? tr("Not suitable for minors (NSFM)") : QVariant{};
		case ColumnActive: {
			int n = ls.activeDrawingUserCount;
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
		default:
			return QVariant{};
		}

	} else if(role == Qt::TextAlignmentRole) {
		return index.column() == ColumnUsers ? Qt::AlignCenter : QVariant{};

	} else {
		switch(role) {
		case IdRole: return ls.id;
		case IdAliasRole: return ls.alias;
		case AliasOrIdRole: return ls.idOrAlias();
		case UserCountRole: return ls.userCount;
		case TitleRole: return ls.title;
		case FounderRole: return ls.founder;
		case NeedPasswordRole: return ls.needPassword;
		case PersistentRole: return ls.persistent;
		case ClosedRole: return ls.isClosed();
		case IncompatibleRole: return !ls.incompatibleSeries.isEmpty();
		case JoinableRole: return (!ls.isClosed() || m_moderatorMode) && ls.incompatibleSeries.isEmpty();
		case NsfmRole: return isNsfm(ls);
		case CompatibilityModeRole: return ls.compatibilityMode;
		case InactiveRole: return ls.activeDrawingUserCount == 0;
		default: return QVariant{};
		}
	}
}

Qt::ItemFlags LoginSessionModel::flags(const QModelIndex &index) const
{
	if(index.row()<0 || index.row() >= m_sessions.size())
		return Qt::NoItemFlags;

	const LoginSession &ls = m_sessions.at(index.row());
	if(!ls.incompatibleSeries.isEmpty() || (ls.isClosed() && !m_moderatorMode))
		return Qt::NoItemFlags;
	else
		return QAbstractTableModel::flags(index);
}

QVariant LoginSessionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(role != Qt::DisplayRole || orientation != Qt::Horizontal) {
		return QVariant{};
	}

	switch(section) {
	case ColumnTitle: return tr("Title");
	case ColumnFounder: return tr("Started by");
	case ColumnUsers: return tr("Users");
	case ColumnActive: return tr("Active");
	default: return QVariant{};
	}
}

void LoginSessionModel::updateSession(const LoginSession &session)
{
	// If the session is already listed, update it in place
	for(int i=0;i<m_sessions.size();++i) {
		if(m_sessions.at(i).isIdOrAlias(session.idOrAlias())) {
			m_sessions[i] = session;
			emit dataChanged(index(i, 0), index(i, columnCount()));
			return;
		}
	}

	// Add a new session to the end of the list
	beginInsertRows(QModelIndex(), m_sessions.size(), m_sessions.size());
	m_sessions << session;
	endInsertRows();
}

void LoginSessionModel::removeSession(const QString &id)
{
	for(int i=0;i<m_sessions.size();++i) {
		if(m_sessions.at(i).isIdOrAlias(id)) {
			beginRemoveRows(QModelIndex(), i, i);
			m_sessions.removeAt(i);
			endRemoveRows();
			return;
		}
	}
}

bool LoginSessionModel::isNsfm(const LoginSession &session) const
{
	return session.nsfm || parentalcontrols::isNsfmTitle(session.title);
}

} // namespace net
