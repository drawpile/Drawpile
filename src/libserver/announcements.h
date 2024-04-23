// SPDX-License-Identifier: GPL-3.0-or-later

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
class Announcements final : public QObject
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
	void announceSession(Announcable *session, const QUrl &listServer);

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
	void announcementError(const Announcable *session, const QString &message);

protected:
	void timerEvent(QTimerEvent *event) override;

private:
	struct Listing {
		QUrl listServer;
		Announcable *session;
		Announcement announcement;

		QElapsedTimer refreshTimer;
		bool finishedListing;
		sessionlisting::Session description;
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
