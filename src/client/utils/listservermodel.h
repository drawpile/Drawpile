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
	enum Option {
		NoOptions = 0,
		ShowLocal = 0x01, // show "Nearby" option (if KDNSSD is built in)
		ShowBlank = 0x02, // show a blank option (used to select no listing server)
	};
	Q_DECLARE_FLAGS(Options, Option)

	explicit ListServerModel(Options options, QObject *parent=nullptr);
	explicit ListServerModel(QObject *parent=nullptr) : ListServerModel(NoOptions, parent) {}

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;

	bool removeRows(int row, int count, const QModelIndex &parent);

	void addServer(const QString &name, const QString &url, const QString &description);

	//!  Set the favicon for the server with the given URL
	void setFavicon(const QString &url, const QImage &icon);

	//! Get all configured list servers
	static QVector<ListServer> listServers();

	//! Load server list from the settings file
	void loadServers();

	//! Save (modified) server list. This replaces the existing list
	void saveServers() const;

private:
	QVector<ListServer> m_servers;
	Options m_options;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ListServerModel::Options)

}

#endif // LISTSERVERMODEL_H
