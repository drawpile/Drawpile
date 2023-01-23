/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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
#ifndef DRAWPILE_SERVER_ANNOUNCEMENTS_H
#define DRAWPILE_SERVER_ANNOUNCEMENTS_H

#include "libshared/listings/announcementapi.h"

#include <QObject>
#include <QVector>
#include <QElapsedTimer>

namespace server {
	class ServerConfig;
}

namespace sessionlisting {

class Announcable;

/**
 * @brief All session announcements made from this server
 */
class Announcements : public QObject
{
	Q_OBJECT
public:
	explicit Announcements(server::ServerConfig *config, QObject *parent = nullptr);

	/**
	 * @brief announceSession
	 * @param session
	 * @param listServer
	 * @param mode
	 */
	void announceSession(Announcable *session, const QUrl &listServer, PrivacyMode mode);

	/**
	 * @brief Unlist the session
	 *
	 * If a blank list server URL is passed, all listings for this
	 * session are removed.
	 *
	 * @param session
	 * @param listServer
	 * @param delist if false, the announcement is removed from only from the local list and no unlisting request is sent
	 */
	void unlistSession(Announcable *session, const QUrl &listServer=QUrl(), bool delist=true);

	/**
	 * @brief Return all active announcements for the given session
	 * @param session
	 * @return
	 */
	QVector<Announcement> getAnnouncements(const Announcable *session) const;

signals:
	//! There has been a change to the list of announcements for this session
	void announcementsChanged(const sessionlisting::Announcable *session);

protected:
	void timerEvent(QTimerEvent *event) override;

private:
	struct Listing {
		QUrl listServer;
		Announcable *session;
		Announcement announcement;

		QElapsedTimer refreshTimer;
		PrivacyMode mode; // undefined means the announcement hasn't finishe yet
	};

	Listing *findListing(const QUrl &listServer, const Announcable *session);
	Listing *findListingById(const QUrl &listServer, int id);

	void refreshListings();

	QVector<Listing> m_announcements;
	server::ServerConfig *m_config;

	int m_timerId;
};

}

#endif // ANNOUNCEMENTS_H
