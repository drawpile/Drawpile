/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include <QDebug>
#include <QNetworkInterface>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMessageBox>

#include "whatismyip.h"

namespace {

bool isPublicAddress(const QHostAddress& address) {
	// This could be a bit more comprehensive
	if(address.protocol() == QAbstractSocket::IPv6Protocol) {
		if(address.scopeId()=="Global")
			return true;
		return false;
	} else {
		quint32 a = address.toIPv4Address();
		return !((a >> 24) == 10 ||
			(a >> 16) == 0xC0A8);
	}
}

bool addressSort(const QHostAddress& a1, const QHostAddress& a2)
{
	if(a1 == QHostAddress::LocalHost || a1 == QHostAddress::LocalHostIPv6)
		return false;
	return !isPublicAddress(a1);
}

}

WhatIsMyIp::WhatIsMyIp(QObject *parent) :
	QObject(parent), _querying(false)
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
	if(_querying)
		return;
	_querying = true;

	qDebug() << "Querying IP address via ipecho.net...";

	_net = new QNetworkAccessManager(this);
	connect(_net, SIGNAL(finished(QNetworkReply*)), this, SLOT(gotReply(QNetworkReply*)));
	_net->get(QNetworkRequest(QUrl("http://ipecho.net/plain")));
}

void WhatIsMyIp::gotReply(QNetworkReply *reply)
{
	if(reply->error() != QNetworkReply::NoError) {
		qWarning() << "ipecho.net error:" << reply->errorString();
		QMessageBox::warning(0, tr("IP lookup error"), reply->errorString());
	} else {
		qDebug() << "ipecho.net reply length =" << reply->readBufferSize();
		QByteArray buf = reply->read(64);
		qDebug() << "ipecho.net reply:" << buf;
		QHostAddress addr;
		if(!addr.setAddress(QString::fromUtf8(buf))) {
			qWarning() << "ipecho.net received invalid data:" << buf;
			QMessageBox::warning(0, tr("IP lookup error"), tr("Received invalid data"));
		} else {
			emit myAddressIs(addr.toString());
			_querying = false;
		}
	}

	_net->deleteLater();
	_net = 0;
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

/**
 * Attempt to discover the address most likely reachable from the
 * outside.
 * @return server hostname
 */
QString WhatIsMyIp::localAddress()
{
	QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
	QList<QHostAddress> alist;

	// Gather a list of acceptable addresses
	foreach (QNetworkInterface iface, list) {
		// Ignore inactive interfaces
		if(!(iface.flags() & QNetworkInterface::IsUp) ||
			!(iface.flags() & QNetworkInterface::IsRunning))
			continue;

		foreach (QNetworkAddressEntry entry, iface.addressEntries()) {
			// Ignore IPv6 addresses with scope ID, because QUrl doesn't accept them (last tested with Qt 5.1.1)
			QHostAddress a = entry.ip();
			if(a.scopeId().isEmpty())
				alist.append(a);
		}
	}

	if (alist.count() > 0) {
		qSort(alist.begin(), alist.end(), addressSort);
		QHostAddress a = alist.at(0);
		if(a.protocol() == QAbstractSocket::IPv6Protocol)
			return QString("[%1]").arg(a.toString());
		return a.toString();
	}
	return "127.0.0.1";
}
