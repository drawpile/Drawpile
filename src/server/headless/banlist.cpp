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

#include "banlist.h"
#include "../shared/util/logger.h"

#include <QFile>

namespace server {

BanList::BanList(QObject *parent) : QObject(parent)
{

}

bool BanList::isBanned(const QHostAddress &address)
{
	if(_file.isModified())
		reloadBanList();

	for(const BannedIP &a : _banlist)
		if(address.isInSubnet(a))
			return true;

	return false;
}

void BanList::reloadBanList()
{
	// Expected file format:
	// # IPv4 address
	// 192.168.1.1
	// # IPv6 address
	// 2001:db8::1420:57ab
	// #IP address with netmask:
	// 192.168.1.1/32

	QSharedPointer<QFile> f = _file.open();
	if(!f->isOpen())
		return;

	QList<BannedIP> banlist;

	int linen = 0;
	while(true) {
		++linen;
		QByteArray line = f->readLine();
		if(line.isEmpty())
			break;
		line = line.trimmed();
		if(line.isEmpty() || line.startsWith('#'))
			continue;

		QString str = QString::fromLatin1(line);

		BannedIP ip;
		if(str.contains(QChar('/'))) {
			ip = QHostAddress::parseSubnet(str);
		} else {
			QHostAddress a(str);
			int subnet=0;
			switch(a.protocol()) {
			case QAbstractSocket::IPv4Protocol: subnet=32; break;
			case QAbstractSocket::IPv6Protocol: subnet=128; break;
			default: Q_ASSERT(false); break;
			}

			ip = BannedIP(a, subnet);
		}

		if(ip.first.isNull()) {
			logger::warning() << _file.path() << "Invalid IP address on line" << linen << ":" << str;
			continue;
		}

		banlist << ip;
	}

	_banlist = banlist;
}

}
