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

#include "announcements.h"
#include "announcementapi.h"
#include "announcable.h"

#include "../server/serverconfig.h"
#include "../server/serverlog.h"

#include <QTimerEvent>

namespace sessionlisting {

Announcable::~Announcable() { }

Announcements::Announcements(server::ServerConfig *config, QObject *parent)
	: QObject(parent), m_config(config)
{
	m_timerId = startTimer(60 * 1000, Qt::VeryCoarseTimer);
}

void Announcements::announceSession(Announcable *session, const QUrl &listServer, PrivacyMode mode)
{
	Q_ASSERT(session);
	Q_ASSERT(mode != PrivacyMode::Undefined);

	if(!listServer.isValid() || !m_config->isAllowedAnnouncementUrl(listServer)) {
		server::Log()
			.about(server::Log::Level::Warn, server::Log::Topic::PubList)
			.message("Announcement API URL not allowed: " + listServer.toString())
			.to(m_config->logger());
		return;
	}

	auto description = session->getSessionAnnouncement();
	description.isPrivate = mode;

	// Don't announce twice at the same server
	if(findListing(listServer, session))
		return;

	// Make announcement
	m_announcements << Listing {
		listServer,
		session,
		Announcement {},
		QDeadlineTimer(),
		PrivacyMode::Undefined
	};

	server::Log()
		.about(server::Log::Level::Info, server::Log::Topic::PubList)
		.session(session->id())
		.message("Announcing session at at " + listServer.toString())
		.to(m_config->logger());

	auto *response = sessionlisting::announceSession(listServer, description);

	connect(response, &AnnouncementApiResponse::finished, this, [listServer, session, response, this](const QVariant &result, const QString &message, const QString &error) {
		response->deleteLater();

		Listing *listing = findListing(listServer, session);
		if(!listing) {
			// Whoops! Looks like this session has ended
			return;
		}

		if(!error.isEmpty()) {
			server::Log()
				.about(server::Log::Level::Warn, server::Log::Topic::PubList)
				.message(listServer.toString() + ": announcement failed: " + error)
				.to(m_config->logger());
			listing->session->sendListserverMessage(error);
			return;
		}

		if(!message.isEmpty()) {
			server::Log()
				.about(server::Log::Level::Info, server::Log::Topic::PubList)
				.message(message)
				.to(m_config->logger());
			listing->session->sendListserverMessage(message);
		}

		listing->announcement = result.value<sessionlisting::Announcement>();
		Q_ASSERT(listing->announcement.apiUrl == listing->listServer);
		listing->mode = listing->announcement.isPrivate ? PrivacyMode::Private : PrivacyMode::Public;
		listing->refreshTimer.setRemainingTime(listing->announcement.refreshInterval * 60 * 1000);

		emit announcementsChanged(listing->session);

		server::Log()
			.about(server::Log::Level::Info, server::Log::Topic::PubList)
			.message("Announced at: " + listServer.toString())
			.to(m_config->logger());
	});
}

void Announcements::unlistSession(Announcable *session, const QUrl &listServer, bool delist)
{
	QMutableVectorIterator<Listing> i(m_announcements);
	QSet<Announcable*> changes;

	while(i.hasNext()) {
		Listing &listing = i.next();
		if(
			(!listServer.isValid() || listServer == listing.listServer) &&
			(!session || session == listing.session)
			)
		{
			changes.insert(listing.session);

			server::Log()
				.about(server::Log::Level::Info, server::Log::Topic::PubList)
				.message("Unlisting: " + listing.announcement.id)
				.to(m_config->logger());

			if(listing.mode != PrivacyMode::Undefined && delist) {
				auto *response = sessionlisting::unlistSession(listing.announcement);
				connect(response, &AnnouncementApiResponse::finished, response, &AnnouncementApiResponse::deleteLater);
			}

			i.remove();
		}
	}

	for(const Announcable *session : changes)
		emit announcementsChanged(session);
}

Announcements::Listing *Announcements::findListing(const QUrl &listServer, const Announcable *session)
{
	for(Listing &listing : m_announcements) {
		if(listing.listServer == listServer && listing.session == session)
			return &listing;
	}
	return nullptr;
}

void Announcements::timerEvent(QTimerEvent *event)
{
	if(event->timerId() == m_timerId) {
		refreshListings();
	}
}

void Announcements::refreshListings()
{
	// TODO bulk updates
	for(Listing &listing : m_announcements) {
		if(listing.mode != PrivacyMode::Undefined && listing.refreshTimer.hasExpired()) {
			listing.refreshTimer.setRemainingTime(listing.announcement.refreshInterval * 60 * 1000);

			Session description = listing.session->getSessionAnnouncement();
			description.isPrivate = listing.mode;

			const QUrl listServer = listing.listServer;
			const Announcable *session = listing.session;

			auto *response = sessionlisting::refreshSession(listing.announcement, description);
			connect(response, &AnnouncementApiResponse::finished, [this, response, listServer, session](const QVariant &result, const QString &message, const QString &error) {
				Q_UNUSED(result)
				response->deleteLater();

				Listing *listing = findListing(listServer, session);
				if(!listing) {
					// Whoops! Looks like this session has ended
					return;
				}

				if(!message.isEmpty()) {
					server::Log()
						.about(server::Log::Level::Info, server::Log::Topic::PubList)
						.message(message)
						.to(m_config->logger());

					listing->session->sendListserverMessage(message);
				}

				if(!error.isEmpty()) {
					// Remove listing on error
					server::Log()
						.about(server::Log::Level::Warn, server::Log::Topic::PubList)
						.message(listServer.toString() + ": announcement error: " + error)
						.to(m_config->logger());

					unlistSession(listing->session, listing->listServer, false);
					listing->session->sendListserverMessage(error);
				}
			});
		}
	}
}

QVector<Announcement> Announcements::getAnnouncements(const Announcable *session) const
{
	QVector<Announcement> list;
	for(const auto &listing : m_announcements) {
		if(listing.mode != PrivacyMode::Undefined && listing.session == session)
			list << listing.announcement;
	}
	return list;
}

}
