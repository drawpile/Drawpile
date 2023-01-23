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
#ifndef ZEROCONF_SERVER_DISCOVERY_H
#define ZEROCONF_SERVER_DISCOVERY_H

#include "libshared/listings/announcementapi.h"

#include <QVector>
#ifdef HAVE_DNSSD_BEFORE_5_84_0
#	include <KDNSSD/DNSSD/RemoteService>
#else
#	include <KDNSSD/RemoteService>
#endif

namespace KDNSSD {
	class ServiceBrowser;
}

class ZeroconfDiscovery : public QObject
{
	Q_OBJECT
public:
	ZeroconfDiscovery(QObject *parent=nullptr);

	//! Start discovery
	void discover();

	//! Check if Zeroconf is available
	static bool isAvailable();

signals:
	void serverListUpdated(QVector<sessionlisting::Session> servers);

private slots:
	void discoveryFinished();
	void addService(KDNSSD::RemoteService::Ptr service);
	void removeService(KDNSSD::RemoteService::Ptr service);

private:
	QVector<sessionlisting::Session> m_servers;
	KDNSSD::ServiceBrowser *m_browser;
};

#endif

