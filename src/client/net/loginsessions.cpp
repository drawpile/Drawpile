/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#include "loginsessions.h"
#include "utils/icon.h"

#include <QDebug>
#include <QPixmap>

namespace net {

LoginSessionModel::LoginSessionModel(QObject *parent) :
	QAbstractTableModel(parent), m_hideNsfm(false)
{
}

int LoginSessionModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_filtered.size();
}

int LoginSessionModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;

	// Columns:
	// 0 - closed/incompatible/password needed status icon
	// 1 - title
	// 2 - session founder name
	// 3 - user count

	return 4;
}

QVariant LoginSessionModel::data(const QModelIndex &index, int role) const
{
	if(index.row()<0 || index.row() >= m_filtered.size())
		return QVariant();

	const LoginSession &ls = m_filtered.at(index.row());

	if(role == Qt::DisplayRole) {
		switch(index.column()) {
		case 1: {
			QString title = ls.title.isEmpty() ? tr("(untitled)") : ls.title;
			if(!ls.alias.isEmpty())
				title = QStringLiteral("%1 [%2]").arg(title).arg(ls.alias);
			return title;
		}
		case 2: return ls.founder;
		case 3: return ls.userCount;
		}

	} else if(role == Qt::DecorationRole) {
		if(index.column()==0) {
			if(ls.closed)
				return icon::fromTheme("im-ban-user").pixmap(16, 16);
			else if(ls.needPassword)
				return icon::fromTheme("object-locked").pixmap(16, 16);
		}

	} else if(role == Qt::ToolTipRole) {
		if(ls.incompatible)
			return tr("Incompatible version");

	} else {
		switch(role) {
		case IdRole: return ls.id;
		case IdAliasRole: return ls.alias;
		case UserCountRole: return ls.userCount;
		case TitleRole: return ls.title;
		case FounderRole: return ls.founder;
		case NeedPasswordRole: return ls.needPassword;
		case PersistentRole: return ls.persistent;
		case ClosedRole: return ls.closed;
		case IncompatibleRole: return ls.incompatible;
		case JoinableRole: return !(ls.closed | ls.incompatible);
		case NsfmRole: return ls.nsfm;
		}
	}

	return QVariant();
}

Qt::ItemFlags LoginSessionModel::flags(const QModelIndex &index) const
{
	if(index.row()<0 || index.row() >= m_filtered.size())
		return 0;

	const LoginSession &ls = m_filtered.at(index.row());
	if(ls.incompatible || ls.closed)
		return Qt::NoItemFlags;
	else
		return QAbstractTableModel::flags(index);
}

QVariant LoginSessionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(role != Qt::DisplayRole || orientation != Qt::Horizontal)
		return QVariant();

	switch(section) {
	case 1: return tr("Title");
	case 2: return tr("Started by");
	case 3: return tr("Users");
	}

	return QVariant();
}

void LoginSessionModel::updateSession(const LoginSession &session)
{
	// If the session is already listed, update it in place
	for(int i=0;i<m_sessions.size();++i) {
		if(m_sessions.at(i).id == session.id) {
			m_sessions[i] = session;
			if(session.nsfm && m_hideNsfm)
				removeFiltered(session.id);
			else
				updateFiltered(session);
			emit filteredCountChanged();
			return;
		}
	}

	// Add a new session to the end of the list
	m_sessions << session;

	if(!m_hideNsfm || !session.nsfm)
		updateFiltered(session);
	emit filteredCountChanged();
}

void LoginSessionModel::updateFiltered(const LoginSession &session)
{
	for(int i=0;i<m_filtered.size();++i) {
		if(m_filtered.at(i).id == session.id) {
			m_filtered[i] = session;
			emit dataChanged(index(i, 0), index(i, columnCount()));
			return;
		}
	}

	beginInsertRows(QModelIndex(), m_filtered.size(), m_filtered.size());
	m_filtered << session;
	endInsertRows();
}

void LoginSessionModel::removeFiltered(const QString &id)
{
	for(int i=0;i<m_filtered.size();++i) {
		if(m_filtered.at(i).id == id) {
			beginRemoveRows(QModelIndex(), i, i);
			m_filtered.removeAt(i);
			endRemoveRows();
			break;
		}
	}
}

void LoginSessionModel::removeSession(const QString &id)
{
	for(int i=0;i<m_sessions.size();++i) {
		if(m_sessions.at(i).id == id) {
			m_sessions.removeAt(i);
			break;
		}
	}
	removeFiltered(id);
	emit filteredCountChanged();
}

void LoginSessionModel::setHideNsfm(bool hide)
{
	if(hide != m_hideNsfm) {
		m_hideNsfm  = hide;
		beginResetModel();
		if(hide) {
			m_filtered.clear();
			for(const LoginSession &ls : m_sessions)
				if(!ls.nsfm)
					m_filtered << ls;

		} else {
			m_filtered = m_sessions;
		}
		endResetModel();
		emit filteredCountChanged();
	}
}

}
