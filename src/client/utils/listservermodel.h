/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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
#ifndef LISTSERVERMODEL_H
#define LISTSERVERMODEL_H

#include <QAbstractListModel>
#include <QIcon>

namespace sessionlisting {

struct ListServer {
	QIcon icon;
	QString iconName;
	QString name;
	QString url;
	QString description;
};

class ListServerModel : public QAbstractListModel
{
	Q_OBJECT
public:
	ListServerModel(bool showlocal, bool showBlank, QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;

	bool removeRows(int row, int count, const QModelIndex &parent);

	void addServer(const QString &name, const QString &url, const QString &description);

	//!  Set the favicon for the server with the given URL
	void setFavicon(const QString &url, const QImage &icon);

	//! Load server list from the settings file
	void loadServers();

	//! Save (modified) server list. This replaces the existing list
	void saveServers() const;

	QList<ListServer> servers() const { return m_servers; }

private:
	QList<ListServer> m_servers;
	bool m_showlocal;
	bool m_showBlank;
};

}

#endif // LISTSERVERMODEL_H
