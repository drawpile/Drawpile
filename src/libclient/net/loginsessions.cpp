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
			} else if(ls.isCompatibilityMode()) {
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
				return tr("%1 (incompatible)").arg(ls.version.description);
			} else if(ls.isCompatibilityMode()) {
				return tr("Drawpile 2.1 (compatibility mode)");
			} else {
				return tr("Drawpile 2.2 (fully compatible)");
			}
		case ColumnStatus:
			if(ls.isIncompatible()) {
				return tr("Incompatible version");
			} else if(ls.webLoginBlocked) {
#ifdef __EMSCRIPTEN__
				return tr("Closed (not allowed to join from the web browser)");
#else
				return tr("Closed (not allowed to join via WebSocket)");
#endif
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
			return isNsfm(ls) ? tr("Not suitable for minors (NSFM)")
							  : QVariant{};
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
		case IdRole:
			return ls.id;
		case IdAliasRole:
			return ls.alias;
		case AliasOrIdRole:
			return ls.idOrAlias();
		case UserCountRole:
			return ls.userCount;
		case TitleRole:
			return ls.title;
		case FounderRole:
			return ls.founder;
		case NeedPasswordRole:
			return ls.needPassword;
		case PersistentRole:
			return ls.persistent;
		case ClosedRole:
			return ls.isClosed();
		case IncompatibleRole:
			return !ls.version.compatible;
		case JoinableRole:
			return ls.isJoinable(m_moderatorMode);
		case NsfmRole:
			return isNsfm(ls);
		case CompatibilityModeRole:
			return ls.isCompatibilityMode();
		case InactiveRole:
			return ls.activeDrawingUserCount == 0;
		case JoinDenyReasonsRole: {
			QStringList reasons;
			if(ls.newLoginsBlocked) {
				//: "It" refers to a session that can't be joined.
				reasons.append(tr("It is full or closed."));
			}
			if(ls.guestLoginBlocked) {
				//: "It" refers to a session that can't be joined.
				reasons.append(tr("It requires an account."));
			}
			if(ls.webLoginBlocked) {
#ifdef __EMSCRIPTEN__
				//: "It" refers to a session that can't be joined.
				reasons.append(
					tr("It does not allow joining via web browser."));
#else
				//: "It" refers to a session that can't be joined.
				reasons.append(tr("It does not allow joining via WebSockets."));
#endif
			}
			if(ls.isIncompatible()) {
				if(ls.version.future) {
					reasons.append(
						//: "It" refers to a session that can't be joined.
						tr("It is hosted with a newer version of Drawpile, you "
						   "have to update. If there is no update available, "
						   "it may be hosted with a development version of "
						   "Drawpile."));
				} else if(ls.version.past) {
					//: "It" refers to a session that can't be joined.
					reasons.append(tr("It is hosted with an old, incompatible "
									  "version of Drawpile."));
				} else {
					//: "It" refers to a session that can't be joined.
					reasons.append(
						tr("It is hosted with an incompatible protocol."));
				}
			}
			return reasons;
		}
		case JoinDenyIcon:
			return QIcon::fromTheme(
				ls.isIncompatible() ? "dontknow" : "cards-block");
		default:
			return QVariant{};
		}
	}
}

QVariant LoginSessionModel::headerData(
	int section, Qt::Orientation orientation, int role) const
{
	if(role != Qt::DisplayRole || orientation != Qt::Horizontal) {
		return QVariant{};
	}

	switch(section) {
	case ColumnTitle:
		return tr("Title");
	case ColumnFounder:
		return tr("Started by");
	case ColumnUsers:
		return tr("Users");
	case ColumnActive:
		return tr("Active");
	default:
		return QVariant{};
	}
}

void LoginSessionModel::updateSession(const LoginSession &session)
{
	// If the session is already listed, update it in place
	for(int i = 0; i < m_sessions.size(); ++i) {
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
	for(int i = 0; i < m_sessions.size(); ++i) {
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

}
