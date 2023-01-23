/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "thinsrv/gui/sidebarmodel.h"

#include "thinsrv/gui/serversummarypage.h"
#include "thinsrv/gui/sessionlistpage.h"
#include "thinsrv/gui/userlistpage.h"
#include "thinsrv/gui/banlistpage.h"
#include "thinsrv/gui/sessionpage.h"
#include "thinsrv/gui/accountlistpage.h"
#include "thinsrv/gui/serverlogpage.h"

#include <QDebug>
#include <QBrush>
#include <QJsonArray>
#include <QJsonObject>

namespace server {
namespace gui {

SidebarModel::SidebarModel(QObject *parent)
	: QAbstractItemModel(parent)
{
	m_summarypages
		<< new ServersummaryPageFactory
		<< new SessionListPageFactory
		<< new UserListPageFactory
		<< new BanListPageFactory
		<< new AccountListPageFactory
		<< new ServerLogPageFactory;
}

SidebarModel::~SidebarModel()
{
	for(PageFactory *pf : m_summarypages)
		delete pf;
}

void SidebarModel::setSessionList(const QJsonArray &sessions)
{
	//qDebug() << "setSessionList" << sessions;
	QStringList ids;
	for(const QJsonValue &v : sessions) {
		const QJsonObject o = v.toObject();
		QString id = o["alias"].toString();
		if(id.isEmpty())
			id = o["id"].toString();

		ids << id;
	}

	// Remove missing sessions
	QMutableListIterator<PageFactory*> i(m_sessions);
	const QModelIndex section = createIndex(1, 0, quintptr(0));
	int row=0;
	while(i.hasNext()) {
		PageFactory *pf = i.next();
		if(!ids.contains(pf->title())) {
			qDebug() << "removing" << row;
			beginRemoveRows(section, row, row);
			delete pf;
			i.remove();
			endRemoveRows();
		} else {
			ids.removeAll(pf->title());
			++row;
		}
	}

	// Append new sessions
	if(!ids.isEmpty()) {
		row=m_sessions.size();
		qDebug() << "adding" << ids.size();
		beginInsertRows(section, row, row+ids.size()-1);
		for(const QString &id : ids) {
			m_sessions << new SessionPageFactory(id);
		}
		endInsertRows();
	}
}

QModelIndex SidebarModel::index(int row, int column, const QModelIndex &parent) const
{
	// The model tree has two levels:
	// Sections
	// Section specific pages
	// The leaf item ID is the section number (section row() + 1)

	if(parent.isValid()) {
		if(parent.internalId()==0)
			return createIndex(row, column, parent.row()+1);

	} else {
		return createIndex(row, column, quintptr(0));
	}
	return QModelIndex();
}

QModelIndex SidebarModel::parent(const QModelIndex &index) const
{
	if(!index.isValid() || index.internalId()==0)
		return QModelIndex();

	return createIndex(index.internalId()-1, index.column(), quintptr(0));
}

int SidebarModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		// Second level items (section pages) have no subitems
		if(parent.parent().isValid())
			return 0;

		// Only top-level sections have subitems
		switch(parent.row()) {
		case 0: return m_summarypages.size();
		case 1: return m_sessions.size();
		default: return 0;
		}
	} else {
		return 2;
	}
}

int SidebarModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return 1;
}

QVariant SidebarModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_UNUSED(section);
	Q_UNUSED(orientation);
	Q_UNUSED(role);
	return QVariant();
}

Qt::ItemFlags SidebarModel::flags(const QModelIndex &index) const
{
	if(index.isValid()) {
		if(index.parent().isValid())
			return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	}
	return Qt::NoItemFlags;
}

QVariant SidebarModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid())
		return QVariant();

	if(role == Qt::DisplayRole) {
		if(index.parent().isValid()) {
			switch(index.internalId()) {
			case 1:
				// Summary pages
				if(index.row()>=0 && index.row()<m_summarypages.size())
					return m_summarypages.at(index.row())->title();
				break;
			case 2:
				// Sessions
				if(index.row()>=0 && index.row()<m_sessions.size())
					return m_sessions.at(index.row())->title();
				break;
			}

		} else {
			// Top level categories
			switch(index.row()) {
			case 0: return tr("Server");
			case 1: return tr("Sesssions");
			}
		}
	} else if(role == PageFactoryRole) {
		if(index.parent().isValid()) {
			switch(index.internalId()) {
			case 1:
				// Summary pages
				if(index.row()>=0 && index.row()<m_summarypages.size())
					return QVariant::fromValue((void*)m_summarypages.at(index.row()));
				break;
			case 2:
				// Sessions
				if(index.row()>=0 && index.row()<m_sessions.size())
					return QVariant::fromValue((void*)m_sessions.at(index.row()));
				break;
			}
		}
	}

	return QVariant();
}

}

}
