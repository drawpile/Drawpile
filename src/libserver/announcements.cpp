// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/announcements.h"
#include "libserver/announcable.h"

#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"

#include <QTimerEvent>

namespace sessionlisting {

Announcable::~Announcable() { }

Announcements::Announcements(server::ServerConfig *config, QObject *parent)
	: QObject(parent), m_config(config)
{
	m_timerId = startTimer(30 * 1000, Qt::VeryCoarseTimer);
}

void Announcements::announceSession(Announcable *session, const QUrl &listServer)
{
	Q_ASSERT(session);

	if(!listServer.isValid() || !m_config->isAllowedAnnouncementUrl(listServer)) {
		server::Log()
			.about(server::Log::Level::Warn, server::Log::Topic::PubList)
			.message("Announcement API URL not allowed: " + listServer.toString())
			.to(m_config->logger());
		session->sendListserverMessage(
			QStringLiteral("Listing on %1 is not allowed on this server")
				.arg(listServer.host()));
		return;
	}

	auto description = session->getSessionAnnouncement();

	// Don't announce twice at the same server
	if(findListing(listServer, session))
		return;

	// Make announcement
	m_announcements << Listing {
		listServer,
		session,
		Announcement {},
		QElapsedTimer(),
		false,
		{},
	};

	server::Log()
		.about(server::Log::Level::Info, server::Log::Topic::PubList)
		.session(session->id())
		.message("Announcing session at at " + listServer.toString())
		.to(m_config->logger());

	auto *response = sessionlisting::announceSession(listServer, description);

	connect(response, &AnnouncementApiResponse::finished, this, [listServer, session, description, response, this](const QVariant &result, const QString &message, const QString &error) {
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

			unlistSession(listing->session, listing->listServer, false);

			listing->session->sendListserverMessage(
				QStringLiteral("Listing on %1 failed: %2")
					.arg(listServer.host(), error));
			return;
		}

		QString successMessage;
		if(message.isEmpty()) {
			successMessage =
				QStringLiteral("This session is now listed at %1")
					.arg(listServer.host());
		} else {
			server::Log()
				.about(server::Log::Level::Info, server::Log::Topic::PubList)
				.message(message)
				.to(m_config->logger());
			successMessage =
				QStringLiteral("This session is now listed at %1: %2")
					.arg(listServer.host(), message);
		}
		listing->session->sendListserverMessage(successMessage);

		listing->announcement = result.value<sessionlisting::Announcement>();
		Q_ASSERT(listing->announcement.apiUrl == listing->listServer);
		listing->finishedListing = true;
		listing->description = description;
		listing->refreshTimer.start();

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
		const Listing &listing = i.next();
		if(
			(listServer.isEmpty() || listServer == listing.listServer) &&
			(!session || session == listing.session)
		)
		{
			changes.insert(listing.session);

			server::Log()
				.about(server::Log::Level::Info, server::Log::Topic::PubList)
				.session(listing.announcement.id)
				.message("Unlisting from " + listing.listServer.toString())
				.to(m_config->logger());

			if(listing.finishedListing && delist) {
				auto *response = sessionlisting::unlistSession(listing.announcement);
				connect(response, &AnnouncementApiResponse::finished, response, &AnnouncementApiResponse::deleteLater);
			}

			i.remove();
		}
	}

	for(const auto *changedSession : changes)
		emit announcementsChanged(changedSession);
}

Announcements::Listing *Announcements::findListing(const QUrl &listServer, const Announcable *session)
{
	for(Listing &listing : m_announcements) {
		if(listing.listServer == listServer && listing.session == session)
			return &listing;
	}
	return nullptr;
}

Announcements::Listing *Announcements::findListingById(const QUrl &listServer, int listingId)
{
	for(Listing &listing : m_announcements) {
		if(listing.listServer == listServer && listing.announcement.listingId == listingId)
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
	using Update = QPair<Announcement, Session>;

	QSet<QUrl> refreshServers;
	for(Listing &listing : m_announcements) {
		bool shouldRefresh =
			!refreshServers.contains(listing.listServer) &&
			listing.finishedListing &&
			(listing.refreshTimer.hasExpired(
				 listing.announcement.refreshInterval * 60 * 1000) ||
			 listing.session->hasUrgentAnnouncementChange(listing.description));
		if(shouldRefresh) {
			refreshServers.insert(listing.listServer);
		}
	}

	for(QUrl refreshServer : refreshServers) {
		QVector<Update> updates;
		for(Listing &listing : m_announcements) {
			if(listing.listServer == refreshServer) {
				updates.append(
					{listing.announcement,
					 listing.session->getSessionAnnouncement()});
				listing.refreshTimer.start();
			}
		}

		int updateCount = updates.size();
		if(updateCount == 0) {
			// Shouldn't happen.
			server::Log()
				.about(server::Log::Level::Error, server::Log::Topic::PubList)
				.message(QStringLiteral("No announcements for server %1")
							 .arg(refreshServer.toString()))
				.to(m_config->logger());
			continue;
		} else {
			server::Log()
				.about(server::Log::Level::Info, server::Log::Topic::PubList)
				.message(QStringLiteral("Refreshing %1 announcements at %2")
							 .arg(updateCount)
							 .arg(updates.first().first.apiUrl.toString()))
				.to(m_config->logger());
		}

		AnnouncementApiResponse *response =
			sessionlisting::refreshSessions(updates);

		connect(
			response, &AnnouncementApiResponse::finished,
			[this, refreshServer, updates, response](
				const QVariant &result, const QString &message,
				const QString &error) {
				response->deleteLater();

				if(!message.isEmpty()) {
					server::Log()
						.about(
							server::Log::Level::Info,
							server::Log::Topic::PubList)
						.message(message)
						.to(m_config->logger());
				}

				if(!error.isEmpty()) {
					// If bulk refresh fails, there is a serious problem
					// with the server. Remove all listings.
					server::Log()
						.about(
							server::Log::Level::Warn,
							server::Log::Topic::PubList)
						.message(QStringLiteral("%1: bulk refresh error: %2")
									 .arg(refreshServer.toString(), error))
						.to(m_config->logger());
					unlistSession(nullptr, refreshServer, false);
					return;
				}

				const QVariantHash results = result.toHash();
				const QVariantHash responses = results["responses"].toHash();
				const QVariantHash errors = results["errors"].toHash();
				for(const Update &pair : updates) {
					Q_ASSERT(pair.first.apiUrl == refreshServer);

					Listing *listing =
						findListingById(refreshServer, pair.first.listingId);
					if(!listing) {
						// Whoops! Looks like this session has ended
						continue;
					}

					listing->description = pair.second;

					// Remove missing listings
					const QString listingId =
						QString::number(listing->announcement.listingId);
					const QString resultValue =
						responses.value(listingId).toString();
					if(resultValue != QStringLiteral("ok")) {
						QString errorMessage = errors[listingId].toString();
						server::Log()
							.about(
								server::Log::Level::Warn,
								server::Log::Topic::PubList)
							.session(listing->announcement.id)
							.message(QStringLiteral("%1: %2 (%3)")
										 .arg(
											 listing->listServer.toString(),
											 resultValue,
											 errorMessage.isEmpty()
												 ? "no error message"
												 : errorMessage))
							.to(m_config->logger());

						QString announcementErrorMessage =
							QStringLiteral(
								"The session has been delisted from %1: %2")
								.arg(
									listing->listServer.host(),
									errorMessage.isEmpty() ? "no reason given"
														   : errorMessage);
						emit announcementError(
							listing->session, announcementErrorMessage);

						unlistSession(
							listing->session, listing->listServer, false);
					}
				}
			});
	}
}

QVector<Announcement> Announcements::getAnnouncements(const Announcable *session) const
{
	QVector<Announcement> list;
	for(const auto &listing : m_announcements) {
		if(listing.finishedListing && listing.session == session)
			list << listing.announcement;
	}
	return list;
}

}
