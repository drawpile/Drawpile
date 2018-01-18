/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2018 Calle Laakkonen

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

#include "sessionlistingmodel.h"
#include "utils/icon.h"
#include "config.h"

#include <QGuiApplication>
#include <QUrl>
#include <QDebug>

namespace sessionlisting {

SessionListingModel::SessionListingModel(QObject *parent)
	: QAbstractTableModel(parent), m_nsfmCount(0), m_nsfm(false), m_showPassworded(true)
{
}

int SessionListingModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_filtered.size();
}

int SessionListingModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;

	// Columns:
	// 0 - title
	// 1 - server
	// 2 - user count
	// 3 - owner
	// 4 - uptime
	return 5;
}

static QString ageString(const qint64 seconds)
{
	const int minutes = seconds / 60;
	return QGuiApplication::tr("%1h %2m").arg(minutes/60).arg(minutes%60);
}

QVariant SessionListingModel::data(const QModelIndex &index, int role) const
{
	if(index.row() < 0 || index.row() >= m_filtered.size())
		return QVariant();

	const Session &s = m_filtered.at(index.row());

	if(role == Qt::DisplayRole) {
		switch(index.column()) {
		case 0: return s.title.isEmpty() ? tr("(untitled)") : s.title;
		case 1: return s.host;
		case 2: return QString::number(s.users);
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
				return QIcon("builtin:nsfm.svg");
		}

	} else if(role == Qt::UserRole) {
		// User Role is used for sorting keys
		switch(index.column()) {
		case 0: return s.title;
		case 1: return s.host;
		case 2: return s.users;
		case 3: return s.owner;
		case 4: return s.started;
		}
	} else if(role == Qt::UserRole+1) {
		// User role+1 is used for the session URL
		return sessionUrl(index.row());
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

	return QVariant();
}

Qt::ItemFlags SessionListingModel::flags(const QModelIndex &index) const
{
	if(index.row() < 0 || index.row() >= m_filtered.size())
		return Qt::NoItemFlags;

	const Session &s = m_filtered.at(index.row());
	if(s.protocol.isCurrent())
		return QAbstractTableModel::flags(index);
	else
		return Qt::NoItemFlags;
}

void SessionListingModel::setList(const QList<sessionlisting::Session> sessions)
{
	m_sessions = sessions;
	filterSessionList();
}

void SessionListingModel::setShowNsfm(bool nsfm)
{
	if(m_nsfm != nsfm) {
		m_nsfm = nsfm;
		if(!m_sessions.isEmpty())
			filterSessionList();
	}
}

void SessionListingModel::setShowPassworded(bool show)
{
	if(m_showPassworded != show) {
		m_showPassworded = show;
		if(!m_sessions.isEmpty())
			filterSessionList();
	}
}

void SessionListingModel::filterSessionList()
{
	beginResetModel();
	m_filtered.clear();
	m_nsfmCount = 0;
	for(const sessionlisting::Session &s : m_sessions) {
		if(s.nsfm)
			++m_nsfmCount;
		if((!s.nsfm || m_nsfm) && (!s.password || m_showPassworded))
			m_filtered << s;
	}
	endResetModel();
}

QUrl SessionListingModel::sessionUrl(int index) const
{
	if(index<0 || index>=m_filtered.size())
		return QUrl();

	const Session &s = m_filtered.at(index);
	QUrl url;
	url.setScheme("drawpile");
	url.setHost(s.host);
	if(s.port != DRAWPILE_PROTO_DEFAULT_PORT)
		url.setPort(s.port);
	url.setPath("/" + s.id);
	return url;
}

}
