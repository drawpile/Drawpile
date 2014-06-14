/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "webadmin.h"
#include "staticfileserver.h"
#include "qmhttp.h"
#include "api.h"

#include "../shared/util/logger.h"

namespace server {

Webadmin::Webadmin(QObject *parent)
	: QObject(parent), _server(new MicroHttpd(this))
{
}

void Webadmin::setBasicAuth(const QString &userpass)
{
	int sep = userpass.indexOf(':');
	if(sep<1) {
		logger::error() << "Invalid user:password pair for web admin:" << userpass;
	} else {
		QString user = userpass.left(sep);
		QString passwd = userpass.mid(sep+1);
		_server->setBasicAuth("drawpile-srv", user, passwd);
	}
}

void Webadmin::setWebappRoot(const QString &rootpath)
{
	_server->addRequestHandler("^/app(/.*)$", StaticFileServer(rootpath));
}

void Webadmin::setSessions(SessionServer *sessions)
{
	initWebAdminApi(_server, sessions);
}

bool Webadmin::setAccessSubnet(const QString &access)
{
	if(access == "all") {
		// special value
		_server->setAcceptPolicy([](const QHostAddress &) { return true; });
	} else {
		QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet(access);
		if(subnet.first.isNull())
			return false;

		_server->setAcceptPolicy([subnet](const QHostAddress &addr) {
			if(addr.isLoopback())
				return true;

			return addr.isInSubnet(subnet);
		});
	}
	return true;
}

void Webadmin::start(quint16 port)
{
	_server->listen(port);
	// TODO listenFd
}

}
