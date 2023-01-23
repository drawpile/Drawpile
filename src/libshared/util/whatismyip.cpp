/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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

#include "libshared/util/whatismyip.h"
#include "libshared/util/networkaccess.h"

#include <QDebug>
#include <QNetworkInterface>
#include <QNetworkReply>

#include <algorithm>

namespace {

bool isPublicAddress(const QHostAddress& address) {
	// This could be a bit more comprehensive
	if(address.protocol() == QAbstractSocket::IPv6Protocol) {
		return address.scopeId() == "Global";

	} else {
		const quint32 a = address.toIPv4Address();
		return !(
			(a >> 24) == 10 ||
			(a >> 24) == 127 ||
			(a >> 16) == 0xC0A8
			);
	}
}

bool addressSort(const QHostAddress& a1, const QHostAddress& a2)
{
	// Sort an IP address list so public addresses are at the beginning of the list
	uchar adr1[17], adr2[17];
	adr1[0] = (a1.isLoopback() ? 2 : 0) | (!isPublicAddress(a1) ? 1 : 0);
	adr2[0] = (a2.isLoopback() ? 2 : 0) | (!isPublicAddress(a2) ? 1 : 0);

	Q_IPV6ADDR ip6;
	ip6 = a1.toIPv6Address();
	memcpy(adr1+1, &ip6, 16);

	ip6 = a2.toIPv6Address();
	memcpy(adr2+1, &ip6, 16);

	return memcmp(adr1, adr2, 17) < 0 ;
}

}

WhatIsMyIp::WhatIsMyIp(QObject *parent) :
	QObject(parent), m_querying(false)
{
}

WhatIsMyIp *WhatIsMyIp::instance()
{
	static WhatIsMyIp *i;
	if(!i)
		i = new WhatIsMyIp();
	return i;
}

void WhatIsMyIp::discoverMyIp()
{
	if(m_querying)
		return;
	m_querying = true;

	auto *filedownload = new networkaccess::FileDownload(this);
	filedownload->setMaxSize(64);
	filedownload->start(QUrl("https://ipecho.net/plain"));

	connect(filedownload, &networkaccess::FileDownload::finished, this, [this, filedownload](const QString &errorMessage) {
		filedownload->deleteLater();
		if(!errorMessage.isEmpty()) {
			qWarning("ipecho.net error: %s", qPrintable(errorMessage));
			emit ipLookupError(errorMessage);
			return;
		}

		QByteArray buf = filedownload->file()->readAll();
		QHostAddress addr;
		if(!addr.setAddress(QString::fromUtf8(buf))) {
			qWarning() << "ipecho.net received invalid data:" << buf;
			emit ipLookupError(tr("Received invalid data"));
		} else {
			emit myAddressIs(addr.toString());
			m_querying = false;
		}
	});
}

bool WhatIsMyIp::isMyPrivateAddress(const QString &address)
{
	// Simple checks first
	if(address == "localhost")
		return true;

	QHostAddress addr;
	if(address.startsWith('[') && address.endsWith(']')) {
		if(!addr.setAddress(address.mid(1, address.length()-2)))
			return false;
	} else {
		if(!addr.setAddress(address))
			return false;
	}

	if(addr.isLoopback())
		return true;

	if(isPublicAddress(addr))
		return false;

	// Check all host addresses
	return QNetworkInterface::allAddresses().contains(addr);
}

bool WhatIsMyIp::isCGNAddress(const QString &address)
{
	QHostAddress addr;

	// remove port (if included). This breaks IPv6 addresses,
	// but it doesn't matter since CGN is only used with IPv4.
	int portsep = address.indexOf(':');
	if(portsep>0)
		addr = QHostAddress{address.left(portsep)};
	else
		addr = QHostAddress{address};

	if(addr.isNull()) {
		qWarning() << "unparseable address:" << address;
		return false;
	}

	return addr.isInSubnet(QHostAddress(QStringLiteral("100.64.0.0")), 10);
}

/**
 * Attempt to discover the address most likely reachable from the
 * outside.
 * @return server hostname
 */
QString WhatIsMyIp::guessLocalAddress()
{
	QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
	QList<QHostAddress> alist;

	// Gather a list of acceptable addresses
	for(const QNetworkInterface &iface : list) {
		// Ignore inactive interfaces
		if(!(iface.flags() & QNetworkInterface::IsUp) ||
			!(iface.flags() & QNetworkInterface::IsRunning))
			continue;

		for(const QNetworkAddressEntry &entry : iface.addressEntries()) {
			// Ignore IPv6 addresses with scope ID, because QUrl doesn't accept them (last tested with Qt 5.1.1)
			QHostAddress a = entry.ip();
			if(a.scopeId().isEmpty())
				alist.append(a);
		}
	}

	if (alist.count() > 0) {
		std::sort(alist.begin(), alist.end(), addressSort);

		QHostAddress a = alist.first();
		if(a.protocol() == QAbstractSocket::IPv6Protocol)
			return QString("[%1]").arg(a.toString());
		return a.toString();
	}

	return "127.0.0.1";
}

