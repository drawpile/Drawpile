/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2019 Calle Laakkonen

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
#ifndef DP_NET_ANNOUNCEMENTLISTMODEL_H
#define DP_NET_ANNOUNCEMENTLISTMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QIcon>

namespace net {

/**
 * @brief Info about a session announcement
 */
struct Announcement {
	//! The URL of the announcement server
	QString url;

	//! Session room code (URL shortener type code, if provided by the server)
	QString roomcode;

	//! In private mode, the session is not visible in the public list, but a room code is still generated
	bool isPrivate;
};

/**
 * A list model to represent active session announcements.
 */
class AnnouncementListModel final : public QAbstractTableModel {
	Q_OBJECT
public:
	AnnouncementListModel(QObject *parent=nullptr);

	QVariant data(const QModelIndex& index, int role=Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex& parent=QModelIndex()) const override;
	int columnCount(const QModelIndex& parent=QModelIndex()) const override;

	/**
	 * @brief Add or update a listed announcement
	 *
	 * If an announcement with the same URL already exists in the list, it is updated in place.
	 */
	void addAnnouncement(const Announcement &a);

	/**
	 * @brief Remove an announcement from the list
	 *
	 * The announcement with the given server URL will be removed (if it is listed.)
	 */
	void removeAnnouncement(const QString &url);

	//! Clear the whole list
	void clear();

private:
	QVector<Announcement> m_announcements;
	QHash<QString,QPair<QIcon,QString>> m_knownServers;
};

}

Q_DECLARE_METATYPE(net::Announcement)

#endif

