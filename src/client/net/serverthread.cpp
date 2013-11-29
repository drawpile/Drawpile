/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

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
*/
#include <QNetworkInterface>
#include <QTextStream>

#include <iostream>

#include "config.h"

#include "net/serverthread.h"

#include "../shared/server/server.h"
#include "../shared/server/server.h"

namespace {

bool isGlobal(const QHostAddress& address) {
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
	return !isGlobal(a1);
}

}

namespace net {

ServerThread::ServerThread(QObject *parent) : QThread(parent)
{
	_deleteonexit = false;
    _port = DRAWPILE_PROTO_DEFAULT_PORT;
}

/**
 * Attempt to discover the address most likely reachable from the
 * outside.
 * @return server hostname
 */
QString ServerThread::address()
{
	QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
	QList<QHostAddress> alist;
        
	// Gather a list of acceptable addresses
	foreach (QNetworkInterface iface, list) {
		if(!(iface.flags() & QNetworkInterface::IsUp) ||
				!(iface.flags() & QNetworkInterface::IsRunning))
			continue;

		QList<QNetworkAddressEntry> addresses = iface.addressEntries();

		foreach (QNetworkAddressEntry entry, addresses) {
			alist.append(entry.ip());
        }
	}

	if (alist.count() > 0) {
		qSort(alist.begin(), alist.end(), addressSort);
		return alist.at(0).toString();
	}
	return "127.0.0.1";
}

int ServerThread::startServer()
{
	_startmutex.lock();

	start();

	_starter.wait(&_startmutex);
	_startmutex.unlock();

	return _port;
}

bool ServerThread::isOnDefaultPort() const
{
	return _port == DRAWPILE_PROTO_DEFAULT_PORT;
}

void ServerThread::run() {
	server::Server server;
	server.setErrorStream(new QTextStream(stderr));

	connect(&server, SIGNAL(lastClientLeft()), this, SLOT(quit()));

	qDebug() << "starting server";
    if(!server.start(_port, true)) {
		qDebug() << "an error occurred";
		_port = 0;
		_starter.wakeOne();
		return;
	}

	_port = server.port();
	_starter.wakeOne();

	exec();

	qDebug() << "server thread exiting. Delete=" << _deleteonexit;
	if(_deleteonexit)
		deleteLater();
}

}
