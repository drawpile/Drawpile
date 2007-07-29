/******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

******************************************************************************/

#include "network.h"
#include "../shared/templates.h"

namespace Network {

//! Qt specific network things
namespace Qt {

/*
QSet<QHostAddress> getExternalAddresses()
{
	QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
	const int requiredFlags = QNetworkInterface::IsUp
		|QNetworkInterface::IsRunning
		|QNetworkInterface::CanBroadcast;
	
	QSet<QHostAddress> set;
	
	QList<QNetworkAddressEntry> alist;
	foreach (QNetworkInterface iface, list)
	{
		int flags = iface.flags();
		
		if (!(fIsSet(flags, requiredFlags) and
			!fIsSet(flags, int(QNetworkInterface::IsLoopBack)) and
			!fIsSet(flags, int(QNetworkInterface::IsPointToPoint))))
			continue;
		
		alist = iface.addressEntries();
		
		foreach (QNetworkAddressEntry entry, alist)
		{
			switch (entry.ip().protocol())
			{
			case QAbstractSocket::IPv4Protocol:
				if (entry.netmask() == QHostAddress::Broadcast)
					set.insert(entry.ip());
				break;
			case QAbstractSocket::IPv6Protocol:
				if (entry.netmask().toString() == "FFFF:FFFF:FFFF:FFFF::")
					set.insert(entry.ip());
				break;
			default:
				;
			}
		}
	}
	
	return set;
}
*/

QHostAddress getExternalAddress()
{
	QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
	const int requiredFlags = QNetworkInterface::IsUp
		|QNetworkInterface::IsRunning
		|QNetworkInterface::CanBroadcast;
	
	QSet<QHostAddress> set;
	
	foreach (QNetworkInterface iface, list)
	{
		int flags = iface.flags();
		
		if (!(fIsSet(flags, requiredFlags) and
			!fIsSet(flags, int(QNetworkInterface::IsLoopBack)) and
			!fIsSet(flags, int(QNetworkInterface::IsPointToPoint))))
			continue;
		
		QList<QNetworkAddressEntry> alist = iface.addressEntries();
		
		foreach (QNetworkAddressEntry entry, alist)
		{
			switch (entry.ip().protocol())
			{
			case QAbstractSocket::IPv4Protocol:
				if (entry.netmask() == QHostAddress::Broadcast)
					set.insert(entry.ip());
				break;
			case QAbstractSocket::IPv6Protocol:
				if (entry.netmask().toString() == "FFFF:FFFF:FFFF:FFFF::")
					set.insert(entry.ip());
				break;
			default:
				;
			}
		}
	}
	
	if (set.count() >= 1)
		return *set.begin();
	else
		return QHostAddress();
}

} // Qt

} // Network
