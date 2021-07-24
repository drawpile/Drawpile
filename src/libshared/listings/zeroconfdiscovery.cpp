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

#include "zeroconfdiscovery.h"

#include <QUrl>
#ifdef HAVE_DNSSD_BEFORE_5_84_0
#	include <KDNSSD/DNSSD/ServiceBrowser>
#else
#	include <KDNSSD/ServiceBrowser>
#endif

ZeroconfDiscovery::ZeroconfDiscovery(QObject *parent)
	: QObject(parent), m_browser(nullptr)
{
}

bool ZeroconfDiscovery::isAvailable()
{
	return KDNSSD::ServiceBrowser::isAvailable() == KDNSSD::ServiceBrowser::Working;
}

void ZeroconfDiscovery::discover()
{
	if(!m_browser) {
		m_browser = new KDNSSD::ServiceBrowser("_drawpile._tcp", true, "local");
		m_browser->setParent(this);

		connect(m_browser, &KDNSSD::ServiceBrowser::serviceAdded, this, &ZeroconfDiscovery::addService);
		connect(m_browser, &KDNSSD::ServiceBrowser::serviceRemoved, this, &ZeroconfDiscovery::removeService);
		connect(m_browser, &KDNSSD::ServiceBrowser::finished, this, &ZeroconfDiscovery::discoveryFinished);

		m_browser->startBrowse();
	}
}

void ZeroconfDiscovery::discoveryFinished()
{
	emit serverListUpdated(m_servers);
}

void ZeroconfDiscovery::addService(KDNSSD::RemoteService::Ptr service)
{
	const QHostAddress hostname = KDNSSD::ServiceBrowser::resolveHostName(service->hostName());
	QDateTime started = QDateTime::fromString(service->textData()["started"], Qt::ISODate);
	started.setTimeSpec(Qt::UTC);

	// If a host has both IPv4 and IPv6 addresses, the service may be announced on both.
	if(!hostname.isNull()) {
		const auto host = hostname.toString();
		for(const auto &s : m_servers) {
			if(s.host == host && s.port == service->port())
				return;
		}
	}

	m_servers << sessionlisting::Session {
		hostname.isNull() ? service->hostName() : hostname.toString(),
		service->port(),
		QString(), // no ID since this is actually a server
		protocol::ProtocolVersion::fromString(service->textData()["protocol"]),
		service->textData()["title"],
		-1, // user count is not advertised
		QStringList(), // nor are usernames
		false, // nor are any possible passwords
		false, // nor is there a serverwide NSFW flag
		sessionlisting::PrivacyMode::Public,
		QString(), // no owner tag either
		started
	};
}

void ZeroconfDiscovery::removeService(KDNSSD::RemoteService::Ptr service)
{
	const QHostAddress hostname = KDNSSD::ServiceBrowser::resolveHostName(service->hostName());
	const QString host = hostname.isNull() ? service->hostName() : hostname.toString();

	for(int i=0;i<m_servers.size();++i) {
		if(m_servers.at(i).host == host) {
			m_servers.removeAt(i);
			return;
		}
	}
}

