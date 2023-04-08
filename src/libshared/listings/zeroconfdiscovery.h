// SPDX-License-Identifier: GPL-3.0-or-later

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

class ZeroconfDiscovery final : public QObject
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

