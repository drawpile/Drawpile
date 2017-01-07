/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2015 Calle Laakkonen

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

#include "config.h"

#include "net/serverthread.h"

#include "builtinserver.h"
#include "../shared/util/logger.h"
#include "../shared/net/protover.h"

#include <QSettings>
#include <QDateTime>
#include <QDebug>

#ifdef HAVE_DNSSD
#include <KDNSSD/DNSSD/PublicService>
#endif
#ifdef HAVE_UPNP
#include "upnp.h"
#include "../shared/util/whatismyip.h"
#endif

namespace net {

ServerThread::ServerThread(QObject *parent)
	: QThread(parent), _deleteonexit(false), _portForwarded(false)
{
	QSettings cfg;
	_port = cfg.value("settings/server/port", DRAWPILE_PROTO_DEFAULT_PORT).toInt();
}

int ServerThread::startServer(const QString &title)
{
	_startmutex.lock();

	start();

	_starter.wait(&_startmutex);
	_startmutex.unlock();

#ifdef HAVE_DNSSD
	// Publish this server via DNS-SD.
	// Note: this code runs in the main thread
	if(_port>0) {
		QSettings cfg;
		if(cfg.value("settings/server/dnssd", true).toBool()) {
			qDebug("Announcing server (port %d) via DNSSD", _port);
			auto dnssd = new KDNSSD::PublicService(
				QString(),
				"_drawpile._tcp",
				_port,
				"local"
			);
			dnssd->setParent(this);

			QMap<QString,QByteArray> txt;
			txt["protocol"] = protocol::ProtocolVersion::current().asString().toUtf8();
			txt["started"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8();
			txt["title"] = title.toUtf8();

			dnssd->setTextData(txt);
			dnssd->publishAsync();
		}
	}
#endif

#ifdef HAVE_UPNP
	if(_port>0) {
		QSettings cfg;
		if(cfg.value("settings/server/upnp", true).toBool()) {
			QString myIp = WhatIsMyIp::guessLocalAddress();
			if(WhatIsMyIp::isMyPrivateAddress(myIp)) {
				qDebug() << "UPnP enabled: trying to forward" << _port << "to" << myIp;
				UPnPClient::instance()->activateForward(_port);
				_portForwarded = true;
			}
		}
	}
#endif
	return _port;
}

bool ServerThread::isOnDefaultPort() const
{
	return _port == DRAWPILE_PROTO_DEFAULT_PORT;
}

void ServerThread::run() {
	server::BuiltinServer server;
#ifdef NDEBUG
	logger::setLogLevel(logger::LOG_WARNING);
#else
	logger::setLogLevel(logger::LOG_DEBUG);
#endif

	connect(&server, SIGNAL(serverStopped()), this, SLOT(quit()));

	logger::info() << "Starting server";
	if(!server.start(_port)) {
		_port = 0;
		_error = server.errorString();
		_starter.wakeOne();
		return;
	}

	_port = server.port();
	_starter.wakeOne();

	exec();

#ifdef HAVE_UPNP
	if(_portForwarded) {
		qDebug() << "Removing port" << _port << "forwarding";
		UPnPClient::instance()->deactivateForward(_port);
	}
#endif

	logger::info() << "server thread exiting.";
	if(_deleteonexit)
		deleteLater();
}

}
