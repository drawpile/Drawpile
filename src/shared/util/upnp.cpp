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

#include "upnp.h"
#include "../shared/util/whatismyip.h"

#include <QThread>

#define MINIUPNP_STATICLIB

#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnperrors.h>

struct UPnPClient::Private {
	UPNPDev *devices = nullptr;
	UPNPUrls urls;
	IGDdatas data;
};

UPnPClient::UPnPClient(QObject *parent)
	: QObject(parent), d(new Private)
{
}

UPnPClient::~UPnPClient()
{
	if(d->devices)
		freeUPNPDevlist(d->devices);
	delete d;
}

UPnPClient *UPnPClient::instance()
{
	static UPnPClient *c = nullptr;
	if(!c) {
		c = new UPnPClient;

		QThread *thread = new QThread;

		c->moveToThread(thread);
		connect(thread, &QThread::finished, c, &UPnPClient::deleteLater);
		thread->start();
	}

	return c;
}

// Thread safe interface
void UPnPClient::activateForward(int ip)
{
	QMetaObject::invokeMethod(this, "doActivateForward", Qt::QueuedConnection, Q_ARG(int, ip));
}

void UPnPClient::deactivateForward(int ip)
{
	QMetaObject::invokeMethod(this, "doDeactivateForward", Qt::QueuedConnection, Q_ARG(int, ip));
}

void UPnPClient::fetchExternalIp()
{
	QMetaObject::invokeMethod(this, "doFetchExternalIp", Qt::QueuedConnection);
}

// Thread internal functions
void UPnPClient::doDiscover()
{
	// Check if we have already done discovery
	if(d->devices)
		return;

	int error;
#if MINIUPNPC_API_VERSION < 14
	d->devices = upnpDiscover(2000, nullptr, nullptr, 0, 0, &error);
#else
	d->devices = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &error);
#endif
	if(!d->devices) {
		qWarning("UPnP: Error (%d) discovering devices!", error);
	}


	char lanaddr[64];
	UPNP_GetValidIGD(d->devices, &d->urls, &d->data, lanaddr, sizeof(lanaddr));
}

void UPnPClient::doActivateForward(int port)
{
	doDiscover();

	const QByteArray internalAddr = WhatIsMyIp::guessLocalAddress().toUtf8();
	const QByteArray portstr = QByteArray::number(port);

	int r = UPNP_AddPortMapping(
		d->urls.controlURL,
		d->data.first.servicetype,
		portstr.constData(), portstr.constData(),
		internalAddr.constData(),
		"Drawpile",
		"TCP",
		nullptr,
		"0");

	if(r == 501 || r == 718) {
		// Error... This can be caused by a forwarding entry that already exists
		// It's possible that clients that were shut down quickly (e.g. crashed)
		// didn't relinquish their mappings, so let's delete the old mapping and retry.
		// TODO a smarter approach would be to accept any random port and update our external IP
		// based on that.
		qWarning("UPnP: Couldn't add mapping. Trying to deactivate any existing and retrying...");

		doDeactivateForward(port);

		r = UPNP_AddPortMapping(
				d->urls.controlURL,
				d->data.first.servicetype,
				portstr.constData(), portstr.constData(),
				internalAddr.constData(),
				"Drawpile",
				"TCP",
				nullptr,
				"0");
	}

	if(r != UPNPCOMMAND_SUCCESS)
		qWarning("UPnP: error adding port mapping (code=%d): %s", r, strupnperror(r));
	else
		doFetchExternalIp();
}

void UPnPClient::doDeactivateForward(int port)
{
	doDiscover();

	const QByteArray portstr = QByteArray::number(port);

	const int r = UPNP_DeletePortMapping(
		d->urls.controlURL,
		d->data.first.servicetype,
		portstr.constData(),
		"TCP",
		nullptr);

	if(r != UPNPCOMMAND_SUCCESS)
		qWarning("UPnP: error removing port mapping (code=%d): %s", r, strupnperror(r));
}

void UPnPClient::doFetchExternalIp()
{
	doDiscover();

	char externalIpAddress[40];

	const int r = UPNP_GetExternalIPAddress(
		d->urls.controlURL,
		d->data.first.servicetype,
		externalIpAddress);

	if(r != UPNPCOMMAND_SUCCESS) {
		qWarning("UPnP: external IP fetching failed. (code=%d)", r);

	} else {
		emit externalIp(QString::fromUtf8(externalIpAddress));
	}
}
