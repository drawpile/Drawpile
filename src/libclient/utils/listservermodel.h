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
	bool readonly;
	bool publicListings;
	bool privateListings;
};

class ListServerModel final : public QAbstractListModel
{
	Q_OBJECT
public:
	enum ListServeroles {
		UrlRole = Qt::UserRole,
		DescriptionRole,
		PublicListRole,
		PrivateListRole
	};

	explicit ListServerModel(bool includeReadOnly, QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;

	bool removeRows(int row, int count, const QModelIndex &parent) override;
	bool moveRow(const QModelIndex &sourceParent, int sourceRow, const QModelIndex &destinationParent, int destinationChild);

	/**
	 * @brief Add a new server to the list
	 * @return false if an existing item was updated instead of added
	 */
	bool addServer(const QString &name, const QString &url, const QString &description, bool readonly, bool pub, bool priv);

	/**
	 * @brief Remove the server with the given URL
	 * @return false if no such server was found
	 */
	bool removeServer(const QString &url);

	//!  Set the favicon for the server with the given URL
	void setFavicon(const QString &url, const QImage &icon);

	//! Get all configured list servers
	static QVector<ListServer> listServers(bool includeReadOnly);

	//! Load server list from the settings file
	void loadServers(bool includeReadOnly);

	//! Save (modified) server list. This replaces the existing list
	void saveServers() const;

private:
	QVector<ListServer> m_servers;
};

}

#endif // LISTSERVERMODEL_H
