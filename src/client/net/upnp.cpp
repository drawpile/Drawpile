/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
#include "utils/whatismyip.h"

#include <QThread>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

namespace net {

struct UPnPClient::Private {
	UPNPDev *devices;
	UPNPUrls urls;
	IGDdatas data;

	Private() : devices(0) {}
	~Private() {
		if(devices)
			freeUPNPDevlist(devices);
	}
};

UPnPClient::UPnPClient(QObject *parent)
	: QObject(parent), d(new Private)
{
}

UPnPClient::~UPnPClient()
{
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
bool UPnPClient::doDiscover()
{
	// Check if we have already done discovery
	if(d->devices)
		return true;

	int error;
	d->devices = upnpDiscover(2000, nullptr, nullptr, 0, 0, &error);
	if(!d->devices) {
		qWarning("UPnP: Error (%d) discovering devices!", error);
		return false;
	}


	char lanaddr[64];
	int i = UPNP_GetValidIGD(d->devices, &d->urls, &d->data, lanaddr, sizeof(lanaddr));
	if(i==1) {
		// Valid IGD found
		return true;
	} else {
		qWarning("UPnP: Valid IGD not found! (rval=%d)", i);
		return false;
	}
}

void UPnPClient::doActivateForward(int port)
{
	if(!doDiscover())
			return;

	QByteArray internalAddr = WhatIsMyIp::localAddress().toUtf8();
	QByteArray portstr = QByteArray::number(port);

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
	if(!doDiscover())
		return;

	QByteArray portstr = QByteArray::number(port);

	int r = UPNP_DeletePortMapping(
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
	char externalIpAddress[40];

	int r = UPNP_GetExternalIPAddress(
		d->urls.controlURL,
		d->data.first.servicetype,
		externalIpAddress);

	if(r != UPNPCOMMAND_SUCCESS) {
		qWarning("UPnP: external IP fetching failed.");

	} else {
		emit externalIp(QString::fromUtf8(externalIpAddress));
	}
}

}
